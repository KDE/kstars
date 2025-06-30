/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project:
    SPDX-FileCopyrightText: 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scheduler.h"

#include "ekos/scheduler/framingassistantui.h"
#include "ksnotification.h"
#include "ksmessagebox.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "Options.h"
#include "scheduleradaptor.h"
#include "schedulerjob.h"
#include "schedulerprocess.h"
#include "schedulermodulestate.h"
#include "schedulerutils.h"
#include "scheduleraltitudegraph.h"
#include "skymapcomposite.h"
#include "skycomponents/mosaiccomponent.h"
#include "skyobjects/mosaictiles.h"
#include "auxiliary/QProgressIndicator.h"
#include "dialogs/finddialog.h"
#include "ekos/manager.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/capture/placeholderpath.h"
#include "skyobjects/starobject.h"
#include "greedyscheduler.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "ksalmanac.h"

#include <KConfigDialog>
#include <KActionCollection>
#include <QFileDialog>
#include <QScrollBar>

#include <fitsio.h>
#include <ekos_scheduler_debug.h>
#include <indicom.h>
#include "ekos/capture/sequenceeditor.h"

// Qt version calming
#include <qtendl.h>

#define INDEX_LEAD      0
#define INDEX_FOLLOWER  1

#define BAD_SCORE                -1000
#define RESTART_GUIDING_DELAY_MS  5000

#define DEFAULT_MIN_ALTITUDE        15
#define DEFAULT_MIN_MOON_SEPARATION 0

// This is a temporary debugging printout introduced while gaining experience developing
// the unit tests in test_ekos_scheduler_ops.cpp.
// All these printouts should be eventually removed.
#define TEST_PRINT if (false) fprintf

namespace
{

// This needs to match the definition order for the QueueTable in scheduler.ui
enum QueueTableColumns
{
    NAME_COLUMN = 0,
    STATUS_COLUMN,
    CAPTURES_COLUMN,
    ALTITUDE_COLUMN,
    START_TIME_COLUMN,
    END_TIME_COLUMN,
};

}

namespace Ekos
{

Scheduler::Scheduler()
{
    // Use the default path and interface when running the scheduler.
    setupScheduler(ekosPathString, ekosInterfaceString);
}

Scheduler::Scheduler(const QString path, const QString interface,
                     const QString &ekosPathStr, const QString &ekosInterfaceStr)
{
    // During testing, when mocking ekos, use a special purpose path and interface.
    schedulerPathString = path;
    kstarsInterfaceString = interface;
    setupScheduler(ekosPathStr, ekosInterfaceStr);
}

void Scheduler::setupScheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr)
{
    setupUi(this);
    if (kstarsInterfaceString == "org.kde.kstars")
        prepareGUI();

    qRegisterMetaType<Ekos::SchedulerState>("Ekos::SchedulerState");
    qDBusRegisterMetaType<Ekos::SchedulerState>();

    m_moduleState.reset(new SchedulerModuleState());
    m_process.reset(new SchedulerProcess(moduleState(), ekosPathStr, ekosInterfaceStr));

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    // Get current KStars time and set seconds to zero
    QDateTime currentDateTime = SchedulerModuleState::getLocalTime();
    QTime currentTime         = currentDateTime.time();
    currentTime.setHMS(currentTime.hour(), currentTime.minute(), 0);
    currentDateTime.setTime(currentTime);

    // Set initial time for startup and completion times
    startupTimeEdit->setDateTime(currentDateTime);
    schedulerUntilValue->setDateTime(currentDateTime);

    // set up the job type selection combo box
    QStandardItemModel *model = new QStandardItemModel(leadFollowerSelectionCB);
    QStandardItem *item = new QStandardItem(i18n("Target"));
    model->appendRow(item);
    item = new QStandardItem(i18n("Follower"));
    QFont font;
    font.setItalic(true);
    item->setFont(font);
    model->appendRow(item);
    leadFollowerSelectionCB->setModel(model);

    sleepLabel->setPixmap(
        QIcon::fromTheme("chronometer").pixmap(QSize(32, 32)));
    changeSleepLabel("", false);

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi, 0);

    geo = KStarsData::Instance()->geo();

    //RA box should be HMS-style
    raBox->setUnits(dmsBox::HOURS);

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);
    connect(&m_DebounceTimer, &QTimer::timeout, this, &Scheduler::settleSettings);

    /* FIXME: Find a way to have multi-line tooltips in the .ui file, then move the widget configuration there - what about i18n? */

    queueTable->setToolTip(
        i18n("Job scheduler list.\nClick to select a job in the list.\nDouble click to edit a job with the left-hand fields.\n"));
    QTableWidgetItem *statusHeader       = queueTable->horizontalHeaderItem(SCHEDCOL_STATUS);
    QTableWidgetItem *altitudeHeader     = queueTable->horizontalHeaderItem(SCHEDCOL_ALTITUDE);
    QTableWidgetItem *startupHeader      = queueTable->horizontalHeaderItem(SCHEDCOL_STARTTIME);
    QTableWidgetItem *completionHeader   = queueTable->horizontalHeaderItem(SCHEDCOL_ENDTIME);
    QTableWidgetItem *captureCountHeader = queueTable->horizontalHeaderItem(SCHEDCOL_CAPTURES);

    if (statusHeader != nullptr)
        statusHeader->setToolTip(i18n("Current status of the job, managed by the Scheduler.\n"
                                      "If invalid, the Scheduler was not able to find a proper observation time for the target.\n"
                                      "If aborted, the Scheduler missed the scheduled time or encountered transitory issues and will reschedule the job.\n"
                                      "If complete, the Scheduler verified that all sequence captures requested were stored, including repeats."));
    if (altitudeHeader != nullptr)
        altitudeHeader->setToolTip(i18n("Current altitude of the target of the job.\n"
                                        "A rising target is indicated with an arrow going up.\n"
                                        "A setting target is indicated with an arrow going down."));
    if (startupHeader != nullptr)
        startupHeader->setToolTip(i18n("Startup time of the job, as estimated by the Scheduler.\n"
                                       "The altitude at startup, if available, is displayed too.\n"
                                       "Fixed time from user or culmination time is marked with a chronometer symbol."));
    if (completionHeader != nullptr)
        completionHeader->setToolTip(i18n("Completion time for the job, as estimated by the Scheduler.\n"
                                          "You may specify a fixed time to limit duration of looping jobs. "
                                          "A warning symbol indicates the altitude at completion may cause the job to abort before completion.\n"));
    if (captureCountHeader != nullptr)
        captureCountHeader->setToolTip(i18n("Count of captures stored for the job, based on its sequence job.\n"
                                            "This is a summary, additional specific frame types may be required to complete the job."));

    /* Set first button mode to add observation job from left-hand fields */
    setJobAddApply(true);

    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
    removeFromQueueB->setToolTip(
        i18n("Remove selected job from the observation list.\nJob properties are copied in the edition fields before removal."));
    removeFromQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    queueUpB->setIcon(QIcon::fromTheme("go-up"));
    queueUpB->setToolTip(i18n("Move selected job one line up in the list.\n"));
    queueUpB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueDownB->setIcon(QIcon::fromTheme("go-down"));
    queueDownB->setToolTip(i18n("Move selected job one line down in the list.\n"));
    queueDownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    evaluateOnlyB->setIcon(QIcon::fromTheme("system-reboot"));
    evaluateOnlyB->setToolTip(i18n("Reset state and force reevaluation of all observation jobs."));
    evaluateOnlyB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sortJobsB->setIcon(QIcon::fromTheme("transform-move-vertical"));
    sortJobsB->setToolTip(
        i18n("Reset state and sort observation jobs per altitude and movement in sky, using the start time of the first job.\n"
             "This action sorts setting targets before rising targets, and may help scheduling when starting your observation.\n"
             "Note the algorithm first calculates all altitudes using the same time, then evaluates jobs."));
    sortJobsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    mosaicB->setIcon(QIcon::fromTheme("zoom-draw"));
    mosaicB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    positionAngleSpin->setSpecialValueText("--");

    queueSaveAsB->setIcon(QIcon::fromTheme("document-save-as"));
    queueSaveAsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveB->setIcon(QIcon::fromTheme("document-save"));
    queueSaveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueLoadB->setIcon(QIcon::fromTheme("document-open"));
    queueLoadB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueAppendB->setIcon(QIcon::fromTheme("document-import"));
    queueAppendB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    loadSequenceB->setIcon(QIcon::fromTheme("document-open"));
    loadSequenceB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectStartupScriptB->setIcon(QIcon::fromTheme("document-open"));
    selectStartupScriptB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectShutdownScriptB->setIcon(
        QIcon::fromTheme("document-open"));
    selectShutdownScriptB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectFITSB->setIcon(QIcon::fromTheme("document-open"));
    selectFITSB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    startupB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    startupB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    shutdownB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    shutdownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // 2023-06-27 sterne-jaeger: For simplicity reasons, the repeat option
    // for all sequences is only active if we do consider the past
    schedulerRepeatEverything->setEnabled(Options::rememberJobProgress() == false);
    executionSequenceLimit->setEnabled(Options::rememberJobProgress() == false);
    executionSequenceLimit->setValue(Options::schedulerExecutionSequencesLimit());

    // disable creating follower jobs at the beginning
    leadFollowerSelectionCB->setEnabled(false);

    connect(startupB, &QPushButton::clicked, process().data(), &SchedulerProcess::runStartupProcedure);
    connect(shutdownB, &QPushButton::clicked, process().data(), &SchedulerProcess::runShutdownProcedure);

    connect(selectObjectB, &QPushButton::clicked, this, &Scheduler::selectObject);
    connect(epochCB, &QComboBox::currentTextChanged, this, &Scheduler::displayTargetCoords);
    connect(raBox, &QLineEdit::editingFinished, this, &Scheduler::readCoordsFromUI);
    connect(decBox, &QLineEdit::editingFinished, this, &Scheduler::readCoordsFromUI);
    connect(selectFITSB, &QPushButton::clicked, this, &Scheduler::selectFITS);
    connect(loadSequenceB, &QPushButton::clicked, this, &Scheduler::selectSequence);
    connect(selectStartupScriptB, &QPushButton::clicked, this, &Scheduler::selectStartupScript);
    connect(selectShutdownScriptB, &QPushButton::clicked, this, &Scheduler::selectShutdownScript);
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Scheduler::refreshOpticalTrain);

    connect(KStars::Instance()->actionCollection()->action("show_mosaic_panel"), &QAction::triggered, this, [this](bool checked)
    {
        mosaicB->setDown(checked);
    });
    connect(mosaicB, &QPushButton::clicked, this, []()
    {
        KStars::Instance()->actionCollection()->action("show_mosaic_panel")->trigger();
    });
    connect(addToQueueB, &QPushButton::clicked, [this]()
    {
        // add job from UI
        addJob();
    });
    connect(removeFromQueueB, &QPushButton::clicked, this, &Scheduler::removeJob);
    connect(queueUpB, &QPushButton::clicked, this, &Scheduler::moveJobUp);
    connect(queueDownB, &QPushButton::clicked, this, &Scheduler::moveJobDown);
    connect(evaluateOnlyB, &QPushButton::clicked, process().data(), &SchedulerProcess::startJobEvaluation);
    connect(sortJobsB, &QPushButton::clicked, this, &Scheduler::sortJobsPerAltitude);
    connect(queueTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Scheduler::queueTableSelectionChanged);
    connect(queueTable, &QAbstractItemView::clicked, this, &Scheduler::clickQueueTable);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Scheduler::loadJob);


    // These connections are looking for changes in the rows queueTable is displaying.
    connect(queueTable->verticalScrollBar(), &QScrollBar::valueChanged, [this]()
    {
        updateJobTable();
    });
    connect(queueTable->verticalScrollBar(), &QAbstractSlider::rangeChanged, [this]()
    {
        updateJobTable();
    });

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setCheckable(false);

    connect(startB, &QPushButton::clicked, this, &Scheduler::toggleScheduler);
    connect(pauseB, &QPushButton::clicked, this, &Scheduler::pause);

    connect(queueSaveAsB, &QPushButton::clicked, this, &Scheduler::saveAs);
    connect(queueSaveB, &QPushButton::clicked, this, &Scheduler::save);
    connect(queueLoadB, &QPushButton::clicked, this, [&]()
    {
        load(true);
    });
    connect(queueAppendB, &QPushButton::clicked, this, [&]()
    {
        load(false);
    });

    connect(schedulerTwilight, &QCheckBox::toggled, this, &Scheduler::checkTwilightWarning);

    // Connect to the state machine
    connect(moduleState().data(), &SchedulerModuleState::ekosStateChanged, this, &Scheduler::ekosStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::indiStateChanged, this, &Scheduler::indiStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::indiCommunicationStatusChanged, this,
            &Scheduler::indiCommunicationStatusChanged);
    connect(moduleState().data(), &SchedulerModuleState::schedulerStateChanged, this, &Scheduler::handleSchedulerStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::startupStateChanged, this, &Scheduler::startupStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::shutdownStateChanged, this, &Scheduler::shutdownStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::parkWaitStateChanged, this, &Scheduler::parkWaitStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::profilesChanged, this, &Scheduler::updateProfiles);
    connect(moduleState().data(), &SchedulerModuleState::currentPositionChanged, queueTable, &QTableWidget::selectRow);
    connect(moduleState().data(), &SchedulerModuleState::jobStageChanged, this, &Scheduler::updateJobStageUI);
    connect(moduleState().data(), &SchedulerModuleState::updateNightTime, this, &Scheduler::updateNightTime);
    connect(moduleState().data(), &SchedulerModuleState::currentProfileChanged, this, [&]()
    {
        schedulerProfileCombo->setCurrentText(moduleState()->currentProfile());
    });
    connect(schedulerProfileCombo, &QComboBox::currentTextChanged, process().data(), &SchedulerProcess::setProfile);
    // Connect to process engine
    connect(process().data(), &SchedulerProcess::schedulerStopped, this, &Scheduler::schedulerStopped);
    connect(process().data(), &SchedulerProcess::schedulerPaused, this, &Scheduler::handleSetPaused);
    connect(process().data(), &SchedulerProcess::shutdownStarted, this, &Scheduler::handleShutdownStarted);
    connect(process().data(), &SchedulerProcess::schedulerSleeping, this, &Scheduler::handleSchedulerSleeping);
    connect(process().data(), &SchedulerProcess::jobsUpdated, this, &Scheduler::handleJobsUpdated);
    connect(process().data(), &SchedulerProcess::targetDistance, this, &Scheduler::targetDistance);
    connect(process().data(), &SchedulerProcess::updateJobTable, this, &Scheduler::updateJobTable);
    connect(process().data(), &SchedulerProcess::clearJobTable, this, &Scheduler::clearJobTable);
    connect(process().data(), &SchedulerProcess::addJob, this, &Scheduler::addJob);
    connect(process().data(), &SchedulerProcess::changeCurrentSequence, this, &Scheduler::setSequence);
    connect(process().data(), &SchedulerProcess::jobStarted, this, &Scheduler::jobStarted);
    connect(process().data(), &SchedulerProcess::jobEnded, this, &Scheduler::jobEnded);
    connect(process().data(), &SchedulerProcess::syncGreedyParams, this, &Scheduler::syncGreedyParams);
    connect(process().data(), &SchedulerProcess::syncGUIToGeneralSettings, this, &Scheduler::syncGUIToGeneralSettings);
    connect(process().data(), &SchedulerProcess::changeSleepLabel, this, &Scheduler::changeSleepLabel);
    connect(process().data(), &SchedulerProcess::updateSchedulerURL, this, &Scheduler::updateSchedulerURL);
    connect(process().data(), &SchedulerProcess::interfaceReady, this, &Scheduler::interfaceReady);
    connect(process().data(), &SchedulerProcess::newWeatherStatus, this, &Scheduler::setWeatherStatus);
    // Connect geographical location - when it is available
    //connect(KStarsData::Instance()..., &LocationDialog::locationChanged..., this, &Scheduler::simClockTimeChanged);

    // Restore values for general settings.
    syncGUIToGeneralSettings();


    connect(errorHandlingButtonGroup, static_cast<void (QButtonGroup::*)(QAbstractButton *)>
            (&QButtonGroup::buttonClicked), [this](QAbstractButton * button)
    {
        Q_UNUSED(button)
        ErrorHandlingStrategy strategy = getErrorHandlingStrategy();
        Options::setErrorHandlingStrategy(strategy);
        errorHandlingStrategyDelay->setEnabled(strategy != ERROR_DONT_RESTART);
    });
    connect(errorHandlingStrategyDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [](int value)
    {
        Options::setErrorHandlingStrategyDelay(value);
    });
    connect(executionSequenceLimit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [](int value)
    {
        Options::setSchedulerExecutionSequencesLimit(value);
    });

    // Retiring the Classic algorithm.
    if (Options::schedulerAlgorithm() != ALGORITHM_GREEDY)
    {
        process()->appendLogText(
            i18n("Warning: The Classic scheduler algorithm has been retired. Switching you to the Greedy algorithm."));
        Options::setSchedulerAlgorithm(ALGORITHM_GREEDY);
    }

    // restore default values for scheduler algorithm
    setAlgorithm(Options::schedulerAlgorithm());

    connect(copySkyCenterB, &QPushButton::clicked, this, [this]()
    {
        SkyPoint center = SkyMap::Instance()->getCenterPoint();
        //center.deprecess(KStarsData::Instance()->updateNum());
        //center.catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        setTargetCoords(center.ra(), center.dec(), false);
    });
    copySkyCenterB->setIcon(QIcon::fromTheme("snap-orthogonal"));

    connect(copyMountTargetB, &QPushButton::clicked, this, [this]()
    {
        const SkyPoint coords = process()->mountCoords();

        if (coords.isValid())
            setTargetCoords(coords.ra(), coords.dec(), false);
    });
    copyMountTargetB->setIcon(QIcon(":/icons/ekos_mount_simple.png"));

    connect(editSequenceB, &QPushButton::clicked, this, [this]()
    {
        if (!m_SequenceEditor)
            m_SequenceEditor.reset(new SequenceEditor(this));

        m_SequenceEditor->show();
        m_SequenceEditor->raise();
    });

    m_JobUpdateDebounce.setSingleShot(true);
    m_JobUpdateDebounce.setInterval(1000);
    connect(&m_JobUpdateDebounce, &QTimer::timeout, this, [this]()
    {
        emit jobsUpdated(moduleState()->getJSONJobs());
    });

    altGraph->setState(moduleState());

    moduleState()->calculateDawnDusk();
    process()->loadProfiles();

    watchJobChanges(true);

    loadGlobalSettings();
    connectSettings();
    refreshOpticalTrain();
}

QString Scheduler::getCurrentJobName()
{
    return (activeJob() != nullptr ? activeJob()->getName() : "");
}

void Scheduler::prepareGUI ()
{
    KConfigDialog *dialog = new KConfigDialog(this, "schedulersettings", Options::self());

#ifdef Q_OS_MACOS
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    m_OpsOffsetSettings = new OpsOffsetSettings();
    KPageWidgetItem *page = dialog->addPage(m_OpsOffsetSettings, i18n("Offset"));
    page->setIcon(QIcon::fromTheme("configure"));

    m_OpsAlignmentSettings = new OpsAlignmentSettings();
    page = dialog->addPage(m_OpsAlignmentSettings, i18n("Alignment"));
    page->setIcon(QIcon::fromTheme("transform-move"));

    m_OpsJobsSettings = new OpsJobsSettings();
    page = dialog->addPage(m_OpsJobsSettings, i18n("Jobs"));
    page->setIcon(QIcon::fromTheme("view-calendar-workweek-symbolic"));

    m_OpsScriptsSettings = new OpsScriptsSettings();
    page = dialog->addPage(m_OpsScriptsSettings, i18n("Scripts"));
    page->setIcon(QIcon::fromTheme("document-properties"));
}
void Scheduler::watchJobChanges(bool enable)
{
    /* Don't double watch, this will cause multiple signals to be connected */
    if (enable == jobChangesAreWatched)
        return;

    /* These are the widgets we want to connect, per signal function, to listen for modifications */
    QLineEdit * const lineEdits[] =
    {
        nameEdit,
        groupEdit,
        raBox,
        decBox,
        fitsEdit,
        sequenceEdit,
        schedulerStartupScript,
        schedulerShutdownScript
    };

    QDateTimeEdit * const dateEdits[] =
    {
        startupTimeEdit,
        schedulerUntilValue
    };

    QComboBox * const comboBoxes[] =
    {
        schedulerProfileCombo,
        opticalTrainCombo,
        leadFollowerSelectionCB
    };

    QButtonGroup * const buttonGroups[] =
    {
        stepsButtonGroup,
        errorHandlingButtonGroup,
        startupButtonGroup,
        constraintButtonGroup,
        completionButtonGroup,
        startupProcedureButtonGroup,
        shutdownProcedureGroup
    };

    QAbstractButton * const buttons[] =
    {
        errorHandlingRescheduleErrorsCB,
        schedulerMoonSeparation,
        schedulerMoonAltitude,
        schedulerAltitude,
        schedulerHorizon
    };

    QSpinBox * const spinBoxes[] =
    {
        schedulerRepeatSequencesLimit,
        errorHandlingStrategyDelay
    };

    QDoubleSpinBox * const dspinBoxes[] =
    {
        schedulerMoonSeparationValue,
        schedulerMoonAltitudeMaxValue,
        schedulerAltitudeValue,
        positionAngleSpin,
    };

    if (enable)
    {
        /* Connect the relevant signal to setDirty. Note that we are not keeping the connection object: we will
         * only use that signal once, and there will be no leaks. If we were connecting multiple receiver functions
         * to the same signal, we would have to be selective when disconnecting. We also use a lambda to absorb the
         * excess arguments which cannot be passed to setDirty, and limit captured arguments to 'this'.
         * The main problem with this implementation compared to the macro method is that it is now possible to
         * stack signal connections. That is, multiple calls to WatchJobChanges will cause multiple signal-to-slot
         * instances to be registered. As a result, one click will produce N signals, with N*=2 for each call to
         * WatchJobChanges(true) missing its WatchJobChanges(false) counterpart.
         */
        for (auto * const control : lineEdits)
            connect(control, &QLineEdit::editingFinished, this, [this]()
        {
            setDirty();
        });
        for (auto * const control : dateEdits)
            connect(control, &QDateTimeEdit::editingFinished, this, [this]()
        {
            setDirty();
        });
        for (auto * const control : comboBoxes)
        {
            if (control == leadFollowerSelectionCB)
                connect(leadFollowerSelectionCB, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                        this, [this](int pos)
            {
                setJobManipulation(queueUpB->isEnabled() || queueDownB->isEnabled(), removeFromQueueB->isEnabled(), pos == INDEX_LEAD);
                setDirty();
            });
            else
                connect(control, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]()
            {
                setDirty();
            });
        }
        for (auto * const control : buttonGroups)
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            connect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, [this](int, bool)
#else
            connect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled), this, [this](int, bool)
#endif
        {
            setDirty();
        });
        for (auto * const control : buttons)
            connect(control, static_cast<void (QAbstractButton::*)(bool)>(&QAbstractButton::clicked), this, [this](bool)
        {
            setDirty();
        });
        for (auto * const control : spinBoxes)
            connect(control, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]()
        {
            setDirty();
        });
        for (auto * const control : dspinBoxes)
            connect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double)
        {
            setDirty();
        });
    }
    else
    {
        /* Disconnect the relevant signal from each widget. Actually, this method removes all signals from the widgets,
         * because we did not take care to keep the connection object when connecting. No problem in our case, we do not
         * expect other signals to be connected. Because we used a lambda, we cannot use the same function object to
         * disconnect selectively.
         */
        for (auto * const control : lineEdits)
            disconnect(control, &QLineEdit::editingFinished, this, nullptr);
        for (auto * const control : dateEdits)
            disconnect(control, &QDateTimeEdit::editingFinished, this, nullptr);
        for (auto * const control : comboBoxes)
            disconnect(control, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, nullptr);
        for (auto * const control : buttons)
            disconnect(control, static_cast<void (QAbstractButton::*)(bool)>(&QAbstractButton::clicked), this, nullptr);
        for (auto * const control : buttonGroups)
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            disconnect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, nullptr);
#else
            disconnect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::idToggled), this, nullptr);
#endif
        for (auto * const control : spinBoxes)
            disconnect(control, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, nullptr);
        for (auto * const control : dspinBoxes)
            disconnect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, nullptr);
    }

    jobChangesAreWatched = enable;
}

void Scheduler::handleConfigChanged()
{
    schedulerRepeatEverything->setEnabled(Options::rememberJobProgress() == false);
    executionSequenceLimit->setEnabled(Options::rememberJobProgress() == false);
}

void Scheduler::selectObject()
{
    if (FindDialog::Instance()->execWithParent(Ekos::Manager::Instance()) == QDialog::Accepted)
    {
        SkyObject *object = FindDialog::Instance()->targetObject();
        addObject(object);
    }
}

void Scheduler::addObject(SkyObject *object)
{
    if (object != nullptr)
    {
        QString finalObjectName(object->name());

        if (object->name() == "star")
        {
            StarObject *s = dynamic_cast<StarObject *>(object);

            if (s->getHDIndex() != 0)
                finalObjectName = QString("HD %1").arg(s->getHDIndex());
        }

        nameEdit->setText(finalObjectName);
        setTargetCoords(object->ra0(), object->dec0());

        setDirty();
    }
}

void Scheduler::selectFITS()
{
    auto url = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Select FITS/XISF Image"), dirPath,
                                           "FITS (*.fits *.fit);;XISF (*.xisf)");
    if (url.isEmpty())
        return;

    processFITSSelection(url);
}

void Scheduler::processFITSSelection(const QUrl &url)
{
    if (url.isEmpty())
        return;

    fitsURL = url;
    dirPath = QUrl(fitsURL.url(QUrl::RemoveFilename));
    fitsEdit->setText(fitsURL.toLocalFile());
    setDirty();

    const QString filename = fitsEdit->text();
    int status = 0;
    double ra = 0, dec = 0;
    dms raDMS, deDMS;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;

    if (fits_open_diskfile(&fptr, filename.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString::fromUtf8(error_status);
        return;
    }

    status = 0;
    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString::fromUtf8(error_status);
        return;
    }

    status = 0;
    char objectra_str[32] = {0};
    if (fits_read_key(fptr, TSTRING, "OBJCTRA", objectra_str, comment, &status))
    {
        if (fits_read_key(fptr, TDOUBLE, "RA", &ra, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            process()->appendLogText(i18n("FITS header: cannot find OBJCTRA (%1).", QString(error_status)));
            return;
        }

        raDMS.setD(ra);
    }
    else
    {
        raDMS = dms::fromString(objectra_str, false);
    }

    status = 0;
    char objectde_str[32] = {0};
    if (fits_read_key(fptr, TSTRING, "OBJCTDEC", objectde_str, comment, &status))
    {
        if (fits_read_key(fptr, TDOUBLE, "DEC", &dec, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            process()->appendLogText(i18n("FITS header: cannot find OBJCTDEC (%1).", QString(error_status)));
            return;
        }

        deDMS.setD(dec);
    }
    else
    {
        deDMS = dms::fromString(objectde_str, true);
    }

    setTargetCoords(raDMS, deDMS);

    char object_str[256] = {0};
    if (fits_read_key(fptr, TSTRING, "OBJECT", object_str, comment, &status))
    {
        QFileInfo info(filename);
        nameEdit->setText(info.completeBaseName());
    }
    else
    {
        nameEdit->setText(object_str);
    }
}

bool Scheduler::processCoordinates(dms &ra, dms &dec)
{

    bool raOk = false, decOk = false;
    ra = raBox->createDms(&raOk);
    dec = decBox->createDms(&decOk);

    if (raOk == false)
        process()->appendLogText(i18n("Warning: RA value %1 is invalid.", raBox->text()));

    if (decOk == false)
        process()->appendLogText(i18n("Warning: DEC value %1 is invalid.", decBox->text()));

    // success
    return (raOk && decOk);
}

void Scheduler::setSequence(const QString &sequenceFileURL)
{
    sequenceURL = QUrl::fromLocalFile(sequenceFileURL);

    if (sequenceFileURL.isEmpty())
        return;
    dirPath = QUrl(sequenceURL.url(QUrl::RemoveFilename));

    sequenceEdit->setText(sequenceURL.toLocalFile());

    setDirty();
}

void Scheduler::selectSequence()
{
    QString file = QFileDialog::getOpenFileName(Ekos::Manager::Instance(), i18nc("@title:window", "Select Sequence Queue"),
                   dirPath.toLocalFile(),
                   i18n("Ekos Sequence Queue (*.esq)"));

    setSequence(file);
}

void Scheduler::selectStartupScript()
{
    moduleState()->setStartupScriptURL(QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window",
                                       "Select Startup Script"),
                                       dirPath,
                                       i18n("Script (*)")));
    if (moduleState()->startupScriptURL().isEmpty())
        return;

    dirPath = QUrl(moduleState()->startupScriptURL().url(QUrl::RemoveFilename));

    moduleState()->setDirty(true);
    schedulerStartupScript->setText(moduleState()->startupScriptURL().toLocalFile());
}

void Scheduler::selectShutdownScript()
{
    moduleState()->setShutdownScriptURL(QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window",
                                        "Select Shutdown Script"),
                                        dirPath,
                                        i18n("Script (*)")));
    if (moduleState()->shutdownScriptURL().isEmpty())
        return;

    dirPath = QUrl(moduleState()->shutdownScriptURL().url(QUrl::RemoveFilename));

    moduleState()->setDirty(true);
    schedulerShutdownScript->setText(moduleState()->shutdownScriptURL().toLocalFile());
}

void Scheduler::addJob(SchedulerJob *job)
{
    if (0 <= jobUnderEdit)
    {
        // select the job currently being edited
        job = moduleState()->jobs().at(jobUnderEdit);
        // if existing, save it
        if (job != nullptr)
            saveJob(job);
        // in any case, reset editing
        resetJobEdit();
    }
    else
    {
        // remember the number of rows to select the first one appended
        int currentRow = moduleState()->currentPosition();

        //If no row is selected, the job will be appended at the end of the list, otherwise below the current selection
        if (currentRow < 0)
            currentRow = queueTable->rowCount();
        else
            currentRow++;

        /* If a job is being added, save fields into a new job */
        saveJob(job);

        // select the first appended row (if any was added)
        if (moduleState()->jobs().count() > currentRow)
            moduleState()->setCurrentPosition(currentRow);
    }

    emit jobsUpdated(moduleState()->getJSONJobs());
}

void Scheduler::updateJob(int index)
{
    if(index > 0)
    {
        auto job = moduleState()->jobs().at(index);
        // if existing, save it
        if (job != nullptr)
            saveJob(job);
        // in any case, reset editing
        resetJobEdit();

        emit jobsUpdated(moduleState()->getJSONJobs());

    }
}

bool Scheduler::fillJobFromUI(SchedulerJob *job)
{
    if (nameEdit->text().isEmpty())
    {
        process()->appendLogText(i18n("Warning: Target name is required."));
        return false;
    }

    if (sequenceEdit->text().isEmpty())
    {
        process()->appendLogText(i18n("Warning: Sequence file is required."));
        return false;
    }

    // Coordinates are required unless it is a FITS file
    if ((raBox->isEmpty() || decBox->isEmpty()) && fitsURL.isEmpty())
    {
        process()->appendLogText(i18n("Warning: Target coordinates are required."));
        return false;
    }

    // Read target coordinates from the UI
    if (readCoordsFromUI() == false)
        return false;

    /* Configure or reconfigure the observation job */
    fitsURL = QUrl::fromLocalFile(fitsEdit->text());

    // Get several job values depending on the state of the UI.

    StartupCondition startCondition = START_AT;
    if (asapConditionR->isChecked())
        startCondition = START_ASAP;

    CompletionCondition stopCondition = FINISH_AT;
    if (schedulerCompleteSequences->isChecked())
        stopCondition = FINISH_SEQUENCE;
    else if (schedulerRepeatSequences->isChecked())
        stopCondition = FINISH_REPEAT;
    else if (schedulerUntilTerminated->isChecked())
        stopCondition = FINISH_LOOP;

    double altConstraint = SchedulerJob::UNDEFINED_ALTITUDE;
    if (schedulerAltitude->isChecked())
        altConstraint = schedulerAltitudeValue->value();

    double moonSeparation = -1;
    if (schedulerMoonSeparation->isChecked())
        moonSeparation = schedulerMoonSeparationValue->value();

    double moonMaxAltitude = 90;
    if (schedulerMoonAltitude->isChecked())
        moonMaxAltitude = schedulerMoonAltitudeMaxValue->value();

    QString train = opticalTrainCombo->currentText() == "--" ? "" : opticalTrainCombo->currentText();

    // The reason for this kitchen-sink function is to separate the UI from the
    // job setup, to allow for testing.
    SchedulerUtils::setupJob(*job, nameEdit->text(), leadFollowerSelectionCB->currentIndex() == INDEX_LEAD, groupEdit->text(),
                             train, targetCoords.ra0(), targetCoords.dec0(),
                             KStarsData::Instance()->ut().djd(),
                             positionAngleSpin->value(), sequenceURL, fitsURL,

                             startCondition, startupTimeEdit->dateTime(),
                             stopCondition, schedulerUntilValue->dateTime(), schedulerRepeatSequencesLimit->value(),

                             altConstraint,
                             moonSeparation,
                             moonMaxAltitude,
                             schedulerTwilight->isChecked(),
                             schedulerHorizon->isChecked(),

                             schedulerTrackStep->isChecked(),
                             schedulerFocusStep->isChecked(),
                             schedulerAlignStep->isChecked(),
                             schedulerGuideStep->isChecked());

    // success
    updateJobTable(job);
    return true;
}

bool Scheduler::readCoordsFromUI()
{
    dms ra, dec;
    if (processCoordinates(ra, dec) == false)
    {
        // invalidate the target coordinates, targetCoords.isValid() will return false
        targetCoords = SkyPoint();
        return false;
    }

    // Take over the current coordinates if valid, taking the selected epoch into account
    setTargetCoords(ra, dec, epochCB->currentText() == "J2000");
    return true;
}

void Scheduler::saveJob(SchedulerJob *job)
{
    watchJobChanges(false);

    /* Create or Update a scheduler job, append below current selection */
    int currentRow = moduleState()->currentPosition() + 1;

    /* Add job to queue only if it is new, else reuse current row.
     * Make sure job is added at the right index, now that queueTable may have a line selected without being edited.
     */
    if (0 <= jobUnderEdit)
    {
        /* FIXME: jobUnderEdit is a parallel variable that may cause issues if it desyncs from moduleState()->currentPosition(). */
        if (jobUnderEdit != currentRow - 1)
        {
            qCWarning(KSTARS_EKOS_SCHEDULER) << "BUG: the observation job under edit does not match the selected row in the job table.";
        }

        /* Use the job in the row currently edited */
        job = moduleState()->jobs().at(jobUnderEdit);
        // try to fill the job from the UI and exit if it fails
        if (fillJobFromUI(job) == false)
        {
            watchJobChanges(true);
            return;
        }
    }
    else
    {
        if (job == nullptr)
        {
            /* Instantiate a new job, insert it in the job list and add a row in the table for it just after the row currently selected. */
            job = new SchedulerJob();
            // try to fill the job from the UI and exit if it fails
            if (fillJobFromUI(job) == false)
            {
                delete(job);
                watchJobChanges(true);
                return;
            }
        }
        /* Insert the job in the job list and add a row in the table for it just after the row currently selected. */
        moduleState()->mutlableJobs().insert(currentRow, job);
        insertJobTableRow(currentRow);
    }

    // update lead/follower relationships
    if (!job->isLead())
        job->setLeadJob(moduleState()->findLead(currentRow - 1));
    moduleState()->refreshFollowerLists();

    /* Verifications */
    // Warn user if a duplicated job is in the list - same target, same sequence
    // FIXME: Those duplicated jobs are not necessarily processed in the order they appear in the list!
    int numWarnings = 0;
    if (job->isLead())
    {
        foreach (SchedulerJob *a_job, moduleState()->jobs())
        {
            if (a_job == job || !a_job->isLead())
            {
                break;
            }
            else if (a_job->getName() == job->getName())
            {
                int const a_job_row = moduleState()->jobs().indexOf(a_job);

                /* FIXME: Warning about duplicate jobs only checks the target name, doing it properly would require checking storage for each sequence job of each scheduler job. */
                process()->appendLogText(i18n("Warning: job '%1' at row %2 has a duplicate target at row %3, "
                                              "the scheduler may consider the same storage for captures.",
                                              job->getName(), currentRow, a_job_row));

                /* Warn the user in case the two jobs are really identical */
                if (a_job->getSequenceFile() == job->getSequenceFile())
                {
                    if (a_job->getRepeatsRequired() == job->getRepeatsRequired() && Options::rememberJobProgress())
                        process()->appendLogText(i18n("Warning: jobs '%1' at row %2 and %3 probably require a different repeat count "
                                                      "as currently they will complete simultaneously after %4 batches (or disable option 'Remember job progress')",
                                                      job->getName(), currentRow, a_job_row, job->getRepeatsRequired()));
                }

                // Don't need to warn over and over.
                if (++numWarnings >= 1)
                {
                    process()->appendLogText(i18n("Skipped checking for duplicates."));
                    break;
                }
            }
        }
    }

    updateJobTable(job);

    /* We just added or saved a job, so we have a job in the list - enable relevant buttons */
    queueSaveAsB->setEnabled(true);
    queueSaveB->setEnabled(true);
    startB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
    setJobManipulation(true, true, job->isLead());
    checkJobInputComplete();

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 was saved.").arg(job->getName()).arg(currentRow + 1);

    watchJobChanges(true);

    if (SCHEDULER_LOADING != moduleState()->schedulerState())
    {
        process()->evaluateJobs(true);
    }
}

void Scheduler::syncGUIToJob(SchedulerJob *job)
{
    nameEdit->setText(job->getName());
    groupEdit->setText(job->getGroup());

    setTargetCoords(job->getTargetCoords().ra0(), job->getTargetCoords().dec0());

    // fitsURL/sequenceURL are not part of UI, but the UI serves as model, so keep them here for now
    fitsURL = job->getFITSFile().isEmpty() ? QUrl() : job->getFITSFile();
    fitsEdit->setText(fitsURL.toLocalFile());

    schedulerTrackStep->setChecked(job->getStepPipeline() & SchedulerJob::USE_TRACK);
    schedulerFocusStep->setChecked(job->getStepPipeline() & SchedulerJob::USE_FOCUS);
    schedulerAlignStep->setChecked(job->getStepPipeline() & SchedulerJob::USE_ALIGN);
    schedulerGuideStep->setChecked(job->getStepPipeline() & SchedulerJob::USE_GUIDE);

    switch (job->getFileStartupCondition())
    {
        case START_ASAP:
            asapConditionR->setChecked(true);
            break;

        case START_AT:
            startupTimeConditionR->setChecked(true);
            startupTimeEdit->setDateTime(job->getStartupTime());
            break;
    }

    if (job->getMinAltitude())
    {
        schedulerAltitude->setChecked(true);
        schedulerAltitudeValue->setValue(job->getMinAltitude());
    }
    else
    {
        schedulerAltitude->setChecked(false);
        schedulerAltitudeValue->setValue(DEFAULT_MIN_ALTITUDE);
    }

    if (job->getMinMoonSeparation() > 0)
    {
        schedulerMoonSeparation->setChecked(true);
        schedulerMoonSeparationValue->setValue(job->getMinMoonSeparation());
    }
    else
    {
        schedulerMoonSeparation->setChecked(false);
        schedulerMoonSeparationValue->setValue(DEFAULT_MIN_MOON_SEPARATION);
    }

    if (job->getMaxMoonAltitude() < 90)
    {
        schedulerMoonAltitude->setChecked(true);
        schedulerMoonAltitudeMaxValue->setValue(job->getMaxMoonAltitude());
    }
    else
    {
        schedulerMoonAltitude->setChecked(false);
    }

    schedulerTwilight->blockSignals(true);
    schedulerTwilight->setChecked(job->getEnforceTwilight());
    schedulerTwilight->blockSignals(false);

    schedulerHorizon->blockSignals(true);
    schedulerHorizon->setChecked(job->getEnforceArtificialHorizon());
    schedulerHorizon->blockSignals(false);

    if (job->isLead())
    {
        leadFollowerSelectionCB->setCurrentIndex(INDEX_LEAD);
    }
    else
    {
        leadFollowerSelectionCB->setCurrentIndex(INDEX_FOLLOWER);
    }

    if (job->getOpticalTrain().isEmpty())
        opticalTrainCombo->setCurrentIndex(0);
    else
        opticalTrainCombo->setCurrentText(job->getOpticalTrain());

    sequenceURL = job->getSequenceFile();
    sequenceEdit->setText(sequenceURL.toLocalFile());

    positionAngleSpin->setValue(job->getPositionAngle());

    switch (job->getCompletionCondition())
    {
        case FINISH_SEQUENCE:
            schedulerCompleteSequences->setChecked(true);
            break;

        case FINISH_REPEAT:
            schedulerRepeatSequences->setChecked(true);
            schedulerRepeatSequencesLimit->setValue(job->getRepeatsRequired());
            break;

        case FINISH_LOOP:
            schedulerUntilTerminated->setChecked(true);
            break;

        case FINISH_AT:
            schedulerUntil->setChecked(true);
            schedulerUntilValue->setDateTime(job->getFinishAtTime());
            break;
    }

    updateNightTime(job);
    setJobManipulation(true, true, job->isLead());
}

void Scheduler::syncGUIToGeneralSettings()
{
    schedulerParkDome->setChecked(Options::schedulerParkDome());
    schedulerParkMount->setChecked(Options::schedulerParkMount());
    schedulerCloseDustCover->setChecked(Options::schedulerCloseDustCover());
    schedulerWarmCCD->setChecked(Options::schedulerWarmCCD());
    schedulerUnparkDome->setChecked(Options::schedulerUnparkDome());
    schedulerUnparkMount->setChecked(Options::schedulerUnparkMount());
    schedulerOpenDustCover->setChecked(Options::schedulerOpenDustCover());
    setErrorHandlingStrategy(static_cast<ErrorHandlingStrategy>(Options::errorHandlingStrategy()));
    errorHandlingStrategyDelay->setValue(Options::errorHandlingStrategyDelay());
    errorHandlingRescheduleErrorsCB->setChecked(Options::rescheduleErrors());
    schedulerStartupScript->setText(moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile));
    schedulerShutdownScript->setText(moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile));

    if (process()->captureInterface() != nullptr)
    {
        QVariant hasCoolerControl = process()->captureInterface()->property("coolerControl");
        if (hasCoolerControl.isValid())
        {
            schedulerWarmCCD->setEnabled(hasCoolerControl.toBool());
            moduleState()->setCaptureReady(true);
        }
    }
}

void Scheduler::updateNightTime(SchedulerJob const *job)
{
    // select job from current position
    if (job == nullptr && moduleState()->jobs().size() > 0)
    {
        int const currentRow = moduleState()->currentPosition();
        if (0 <= currentRow && currentRow < moduleState()->jobs().size())
            job = moduleState()->jobs().at(currentRow);

        if (job == nullptr)
        {
            qCWarning(KSTARS_EKOS_SCHEDULER()) << "Cannot update night time, no matching job found at line" << currentRow;
            return;
        }
    }

    QDateTime const dawn = job ? job->getDawnAstronomicalTwilight() : moduleState()->Dawn();
    QDateTime const dusk = job ? job->getDuskAstronomicalTwilight() : moduleState()->Dusk();

    QChar const warning(dawn == dusk ? 0x26A0 : '-');
    nightTime->setText(i18n("%1 %2 %3", dusk.toString("hh:mm"), warning, dawn.toString("hh:mm")));
}

bool Scheduler::modifyJob(int index)
{
    // Reset Edit jobs
    jobUnderEdit = -1;

    if (index < 0)
        return false;

    queueTable->selectRow(index);
    auto modelIndex = queueTable->model()->index(index, 0);
    loadJob(modelIndex);
    return true;
}

void Scheduler::loadJob(QModelIndex i)
{
    if (jobUnderEdit == i.row())
        return;

    SchedulerJob * const job = moduleState()->jobs().at(i.row());

    if (job == nullptr)
        return;

    watchJobChanges(false);

    //job->setState(SCHEDJOB_IDLE);
    //job->setStage(SCHEDSTAGE_IDLE);
    syncGUIToJob(job);

    /* Turn the add button into an apply button */
    setJobAddApply(false);

    /* Disable scheduler start/evaluate buttons */
    startB->setEnabled(false);
    evaluateOnlyB->setEnabled(false);

    /* Don't let the end-user remove a job being edited */
    setJobManipulation(false, false, job->isLead());

    jobUnderEdit = i.row();
    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is currently edited.").arg(job->getName()).arg(
                                       jobUnderEdit + 1);

    watchJobChanges(true);
}

void Scheduler::updateSchedulerURL(const QString &fileURL)
{
    schedulerURL = QUrl::fromLocalFile(fileURL);
    // update save button tool tip
    queueSaveB->setToolTip("Save schedule to " + schedulerURL.fileName());
}

void Scheduler::queueTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)


    if (jobChangesAreWatched == false || selected.empty())
        // || (current.row() + 1) > moduleState()->jobs().size())
        return;

    const QModelIndex current = selected.indexes().first();
    // this should not happen, but avoids crashes
    if ((current.row() + 1) > moduleState()->jobs().size())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER()) << "Unexpected row number" << current.row() << "- ignoring.";
        return;
    }
    moduleState()->setCurrentPosition(current.row());
    SchedulerJob * const job = moduleState()->jobs().at(current.row());

    if (job != nullptr)
    {
        if (jobUnderEdit < 0)
            syncGUIToJob(job);
        else if (jobUnderEdit != current.row())
        {
            // avoid changing the UI values for the currently edited job
            process()->appendLogText(i18n("Stop editing of job #%1, resetting to original value.", jobUnderEdit + 1));
            resetJobEdit();
            syncGUIToJob(job);
        }
    }
    else nightTime->setText("-");
    altGraph->plot();
}

void Scheduler::clickQueueTable(QModelIndex index)
{
    if (index.isValid() && index.row() < moduleState()->jobs().count())
        setJobManipulation(true, true, moduleState()->jobs().at(index.row())->isLead());
    else
        setJobManipulation(index.isValid(), index.isValid(), leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);
}

void Scheduler::setJobAddApply(bool add_mode)
{
    if (add_mode)
    {
        addToQueueB->setIcon(QIcon::fromTheme("list-add"));
        addToQueueB->setToolTip(i18n("Use edition fields to create a new job in the observation list."));
        addToQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    }
    else
    {
        addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
        addToQueueB->setToolTip(i18n("Apply job changes."));
    }
    // check if the button should be enabled
    checkJobInputComplete();
}

void Scheduler::setJobManipulation(bool can_reorder, bool can_delete, bool is_lead)
{
    if (can_reorder)
    {
        int const currentRow = moduleState()->currentPosition();
        if (currentRow >= 0)
        {
            SchedulerJob *currentJob = moduleState()->jobs().at(currentRow);
            // Lead jobs may always be shifted, follower jobs only if there is another lead above its current one.
            queueUpB->setEnabled(0 < currentRow &&
                                 (currentJob->isLead() || (currentRow > 1 && moduleState()->findLead(currentRow - 2) != nullptr)));
            // Moving downward leads only if it is not the last lead in the list
            queueDownB->setEnabled(currentRow < queueTable->rowCount() - 1 &&
                                   (moduleState()->findLead(currentRow + 1, false) != nullptr));
        }
    }
    else
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
    }
    sortJobsB->setEnabled(can_reorder);
    removeFromQueueB->setEnabled(can_delete);

    nameEdit->setEnabled(is_lead);
    selectObjectB->setEnabled(is_lead);
    targetStarLabel->setVisible(is_lead);
    raBox->setEnabled(is_lead);
    decBox->setEnabled(is_lead);
    copySkyCenterB->setEnabled(is_lead);
    schedulerProfileCombo->setEnabled(is_lead);
    fitsEdit->setEnabled(is_lead);
    selectFITSB->setEnabled(is_lead);
    groupEdit->setEnabled(is_lead);
    schedulerTrackStep->setEnabled(is_lead);
    schedulerFocusStep->setEnabled(is_lead);
    schedulerAlignStep->setEnabled(is_lead);
    schedulerGuideStep->setEnabled(is_lead);
    startupGroup->setEnabled(is_lead);
    contraintsGroup->setEnabled(is_lead);

    // If there is a lead job above, allow creating follower jobs
    leadFollowerSelectionCB->setEnabled(moduleState()->findLead(queueTable->currentRow()) != nullptr);
    if (leadFollowerSelectionCB->isEnabled() == false)
        leadFollowerSelectionCB->setCurrentIndex(INDEX_LEAD);
}

bool Scheduler::reorderJobs(QList<SchedulerJob*> reordered_sublist)
{
    /* Add jobs not reordered at the end of the list, in initial order */
    foreach (SchedulerJob* job, moduleState()->jobs())
        if (!reordered_sublist.contains(job))
            reordered_sublist.append(job);

    if (moduleState()->jobs() != reordered_sublist)
    {
        /* Remember job currently selected */
        int const selectedRow = moduleState()->currentPosition();
        SchedulerJob * const selectedJob = 0 <= selectedRow ? moduleState()->jobs().at(selectedRow) : nullptr;

        /* Reassign list */
        moduleState()->setJobs(reordered_sublist);

        /* Refresh the table */
        for (SchedulerJob *job : moduleState()->jobs())
            updateJobTable(job);

        /* Reselect previously selected job */
        if (nullptr != selectedJob)
            moduleState()->setCurrentPosition(moduleState()->jobs().indexOf(selectedJob));

        return true;
    }
    else return false;
}

void Scheduler::moveJobUp()
{
    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int destinationRow;
    SchedulerJob *job = moduleState()->jobs().at(currentRow);

    if (moduleState()->jobs().at(currentRow)->isLead())
    {
        int const rows = 1 + job->followerJobs().count();
        // do nothing if there is no other lead job above the job and its follower jobs
        if (currentRow - rows < 0)
            return;

        // skip the previous lead job and its follower jobs
        destinationRow = currentRow - 1 - moduleState()->jobs().at(currentRow - rows)->followerJobs().count();
    }
    else
        destinationRow = currentRow - 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow < 0)
        return;

    if (moduleState()->jobs().at(currentRow)->isLead())
    {
        // remove the job and its follower jobs from the list
        moduleState()->mutlableJobs().removeOne(job);
        for (auto follower : job->followerJobs())
            moduleState()->mutlableJobs().removeOne(follower);

        // add it at the new place
        moduleState()->mutlableJobs().insert(destinationRow++, job);
        // add the follower jobs
        for (auto follower : job->followerJobs())
            moduleState()->mutlableJobs().insert(destinationRow++, follower);
        // update the modified positions
        for (int i = currentRow; i > destinationRow; i--)
            updateJobTable(moduleState()->jobs().at(i));
        // Move selection to destination row
        moduleState()->setCurrentPosition(destinationRow - job->followerJobs().count() - 1);
    }
    else
    {
        /* Swap jobs in the list */
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
        moduleState()->mutlableJobs().swapItemsAt(currentRow, destinationRow);
#else
        moduleState()->jobs().swap(currentRow, destinationRow);
#endif

        //Update the two table rows
        updateJobTable(moduleState()->jobs().at(currentRow));
        updateJobTable(moduleState()->jobs().at(destinationRow));

        /* Move selection to destination row */
        moduleState()->setCurrentPosition(destinationRow);
        // check if the follower job belongs to a new lead
        SchedulerJob *newLead = moduleState()->findLead(destinationRow, true);
        if (newLead != nullptr)
        {
            job->setLeadJob(newLead);
            moduleState()->refreshFollowerLists();
        }
    }

    setJobManipulation(true, true, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);

    /* Make list modified and evaluate jobs */
    moduleState()->setDirty(true);
    process()->evaluateJobs(true);
}

void Scheduler::moveJobDown()
{
    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int destinationRow;
    SchedulerJob *job = moduleState()->jobs().at(currentRow);

    if (moduleState()->jobs().at(currentRow)->isLead())
    {
        int const rows = 1 + job->followerJobs().count();
        // do nothing if there is no other lead job below the job and its follower jobs
        if (currentRow + rows >= moduleState()->jobs().count())
            return;

        // skip the next lead job and its follower jobs
        destinationRow = currentRow + 1 + moduleState()->jobs().at(currentRow + rows)->followerJobs().count();
    }
    else
        destinationRow = currentRow + 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow >= rowCount)
        return;

    if (moduleState()->jobs().at(currentRow)->isLead())
    {
        // remove the job and its follower jobs from the list
        moduleState()->mutlableJobs().removeOne(job);
        for (auto follower : job->followerJobs())
            moduleState()->mutlableJobs().removeOne(follower);

        // add it at the new place
        moduleState()->mutlableJobs().insert(destinationRow++, job);
        // add the follower jobs
        for (auto follower : job->followerJobs())
            moduleState()->mutlableJobs().insert(destinationRow++, follower);
        // update the modified positions
        for (int i = currentRow; i < destinationRow; i++)
            updateJobTable(moduleState()->jobs().at(i));
        // Move selection to destination row
        moduleState()->setCurrentPosition(destinationRow - job->followerJobs().count() - 1);
    }
    else
    {
        // Swap jobs in the list
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
        moduleState()->mutlableJobs().swapItemsAt(currentRow, destinationRow);
#else
        moduleState()->mutlableJobs().swap(currentRow, destinationRow);
#endif
        // Update the two table rows
        updateJobTable(moduleState()->jobs().at(currentRow));
        updateJobTable(moduleState()->jobs().at(destinationRow));
        // Move selection to destination row
        moduleState()->setCurrentPosition(destinationRow);
        // check if the follower job belongs to a new lead
        if (moduleState()->jobs().at(currentRow)->isLead())
        {
            job->setLeadJob(moduleState()->jobs().at(currentRow));
            moduleState()->refreshFollowerLists();
        }
    }

    setJobManipulation(true, true, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);

    /* Make list modified and evaluate jobs */
    moduleState()->setDirty(true);
    process()->evaluateJobs(true);
}

void Scheduler::updateJobTable(SchedulerJob *job)
{
    // handle full table update
    if (job == nullptr)
    {
        for (auto onejob : moduleState()->jobs())
            updateJobTable(onejob);
        altGraph->tickle();
        return;
    }

    const int row = moduleState()->jobs().indexOf(job);
    // Ignore unknown jobs
    if (row < 0)
        return;
    // ensure that the row in the table exists
    if (row >= queueTable->rowCount())
        insertJobTableRow(row - 1, false);

    QTableWidgetItem *nameCell = queueTable->item(row, static_cast<int>(SCHEDCOL_NAME));
    QTableWidgetItem *statusCell = queueTable->item(row, static_cast<int>(SCHEDCOL_STATUS));
    QTableWidgetItem *altitudeCell = queueTable->item(row, static_cast<int>(SCHEDCOL_ALTITUDE));
    QTableWidgetItem *startupCell = queueTable->item(row, static_cast<int>(SCHEDCOL_STARTTIME));
    QTableWidgetItem *completionCell = queueTable->item(row, static_cast<int>(SCHEDCOL_ENDTIME));
    QTableWidgetItem *captureCountCell = queueTable->item(row, static_cast<int>(SCHEDCOL_CAPTURES));

    // Only in testing.
    if (!nameCell) return;

    if (nullptr != nameCell)
    {
        nameCell->setText(job->isLead() ? job->getName() : "*");
        updateCellStyle(job, nameCell);
        if (nullptr != nameCell->tableWidget())
            nameCell->tableWidget()->resizeColumnToContents(nameCell->column());
    }

    if (nullptr != statusCell)
    {
        static QMap<SchedulerJobStatus, QString> stateStrings;
        static QString stateStringUnknown;
        if (stateStrings.isEmpty())
        {
            stateStrings[SCHEDJOB_IDLE] = i18n("Idle");
            stateStrings[SCHEDJOB_EVALUATION] = i18n("Evaluating");
            stateStrings[SCHEDJOB_SCHEDULED] = i18n("Scheduled");
            stateStrings[SCHEDJOB_BUSY] = i18n("Running");
            stateStrings[SCHEDJOB_INVALID] = i18n("Invalid");
            stateStrings[SCHEDJOB_COMPLETE] = i18n("Complete");
            stateStrings[SCHEDJOB_ABORTED] = i18n("Aborted");
            stateStrings[SCHEDJOB_ERROR] =  i18n("Error");
            stateStringUnknown = i18n("Unknown");
        }
        statusCell->setText(stateStrings.value(job->getState(), stateStringUnknown));
        updateCellStyle(job, statusCell);

        if (nullptr != statusCell->tableWidget())
            statusCell->tableWidget()->resizeColumnToContents(statusCell->column());
    }

    if (nullptr != startupCell)
    {
        auto time = (job->getState() == SCHEDJOB_BUSY) ? job->getStateTime() : job->getStartupTime();
        /* Display startup time if it is valid */
        if (time.isValid())
        {
            startupCell->setText(QString("%1%2%L3° %4")
                                 .arg(job->getAltitudeAtStartup() < job->getMinAltitude() ? QString(QChar(0x26A0)) : "")
                                 .arg(QChar(job->isSettingAtStartup() ? 0x2193 : 0x2191))
                                 .arg(job->getAltitudeAtStartup(), 0, 'f', 1)
                                 .arg(time.toString(startupTimeEdit->displayFormat())));
            job->setStartupFormatted(startupCell->text());

            switch (job->getFileStartupCondition())
            {
                /* If the original condition is START_AT/START_CULMINATION, startup time is fixed */
                case START_AT:
                    startupCell->setIcon(QIcon::fromTheme("chronometer"));
                    break;

                /* If the original condition is START_ASAP, startup time is informational */
                case START_ASAP:
                    startupCell->setIcon(QIcon());
                    break;

                default:
                    break;
            }
        }
        /* Else do not display any startup time */
        else
        {
            startupCell->setText("-");
            startupCell->setIcon(QIcon());
        }

        updateCellStyle(job, startupCell);

        if (nullptr != startupCell->tableWidget())
            startupCell->tableWidget()->resizeColumnToContents(startupCell->column());
    }

    if (nullptr != altitudeCell)
    {
        // FIXME: Cache altitude calculations
        bool is_setting = false;
        double const alt = SchedulerUtils::findAltitude(job->getTargetCoords(), QDateTime(), &is_setting);

        altitudeCell->setText(QString("%1%L2°")
                              .arg(QChar(is_setting ? 0x2193 : 0x2191))
                              .arg(alt, 0, 'f', 1));
        updateCellStyle(job, altitudeCell);
        job->setAltitudeFormatted(altitudeCell->text());

        if (nullptr != altitudeCell->tableWidget())
            altitudeCell->tableWidget()->resizeColumnToContents(altitudeCell->column());
    }

    if (nullptr != completionCell)
    {
        /* Display stop time if it is valid */
        if (job->getStopTime().isValid())
        {
            completionCell->setText(QString("%1%2%L3° %4")
                                    .arg(job->getAltitudeAtStop() < job->getMinAltitude() ? QString(QChar(0x26A0)) : "")
                                    .arg(QChar(job->isSettingAtStop() ? 0x2193 : 0x2191))
                                    .arg(job->getAltitudeAtStop(), 0, 'f', 1)
                                    .arg(job->getStopTime().toString(startupTimeEdit->displayFormat())));
            job->setEndFormatted(completionCell->text());

            switch (job->getCompletionCondition())
            {
                case FINISH_AT:
                    completionCell->setIcon(QIcon::fromTheme("chronometer"));
                    break;

                case FINISH_SEQUENCE:
                case FINISH_REPEAT:
                default:
                    completionCell->setIcon(QIcon());
                    break;
            }
        }
        /* Else do not display any completion time */
        else
        {
            completionCell->setText("-");
            completionCell->setIcon(QIcon());
        }

        updateCellStyle(job, completionCell);
        if (nullptr != completionCell->tableWidget())
            completionCell->tableWidget()->resizeColumnToContents(completionCell->column());
    }

    if (nullptr != captureCountCell)
    {
        switch (job->getCompletionCondition())
        {
            case FINISH_AT:
            // FIXME: Attempt to calculate the number of frames until end - requires detailed imaging time

            case FINISH_LOOP:
                // If looping, display the count of completed frames
                captureCountCell->setText(QString("%L1/-").arg(job->getCompletedCount()));
                break;

            case FINISH_SEQUENCE:
            case FINISH_REPEAT:
            default:
                // If repeating, display the count of completed frames to the count of requested frames
                captureCountCell->setText(QString("%L1/%L2").arg(job->getCompletedCount()).arg(job->getSequenceCount()));
                break;
        }

        QString tooltip = job->getProgressSummary();
        if (tooltip.size() == 0)
            tooltip = i18n("Count of captures stored for the job, based on its sequence job.\n"
                           "This is a summary, additional specific frame types may be required to complete the job.");
        captureCountCell->setToolTip(tooltip);

        updateCellStyle(job, captureCountCell);
        if (nullptr != captureCountCell->tableWidget())
            captureCountCell->tableWidget()->resizeColumnToContents(captureCountCell->column());
    }

    m_JobUpdateDebounce.start();
}

void Scheduler::insertJobTableRow(int row, bool above)
{
    const int pos = above ? row : row + 1;

    // ensure that there are no gaps
    if (row > queueTable->rowCount())
        insertJobTableRow(row - 1, above);

    queueTable->insertRow(pos);

    QTableWidgetItem *nameCell = new QTableWidgetItem();
    queueTable->setItem(row, static_cast<int>(SCHEDCOL_NAME), nameCell);
    nameCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    nameCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *statusCell = new QTableWidgetItem();
    queueTable->setItem(row, static_cast<int>(SCHEDCOL_STATUS), statusCell);
    statusCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    statusCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *captureCount = new QTableWidgetItem();
    queueTable->setItem(row, static_cast<int>(SCHEDCOL_CAPTURES), captureCount);
    captureCount->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    captureCount->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *startupCell = new QTableWidgetItem();
    queueTable->setItem(row, static_cast<int>(SCHEDCOL_STARTTIME), startupCell);
    startupCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    startupCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *altitudeCell = new QTableWidgetItem();
    queueTable->setItem(row, static_cast<int>(SCHEDCOL_ALTITUDE), altitudeCell);
    altitudeCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    altitudeCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *completionCell = new QTableWidgetItem();
    queueTable->setItem(row, static_cast<int>(SCHEDCOL_ENDTIME), completionCell);
    completionCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    completionCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void Scheduler::updateCellStyle(SchedulerJob *job, QTableWidgetItem *cell)
{
    QFont font(cell->font());
    font.setBold(job->getState() == SCHEDJOB_BUSY);
    font.setItalic(job->getState() == SCHEDJOB_BUSY);
    cell->setFont(font);
}

void Scheduler::resetJobEdit()
{
    if (jobUnderEdit < 0)
        return;

    SchedulerJob * const job = moduleState()->jobs().at(jobUnderEdit);
    Q_ASSERT_X(job != nullptr, __FUNCTION__, "Edited job must be valid");

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is not longer edited.").arg(job->getName()).arg(
                                       jobUnderEdit + 1);
    jobUnderEdit = -1;

    watchJobChanges(false);

    /* Revert apply button to add */
    setJobAddApply(true);

    /* Refresh state of job manipulation buttons */
    setJobManipulation(true, true, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);

    /* Restore scheduler operation buttons */
    evaluateOnlyB->setEnabled(true);
    startB->setEnabled(true);

    watchJobChanges(true);
    Q_ASSERT_X(jobUnderEdit == -1, __FUNCTION__, "No more edited/selected job after exiting edit mode");
}

void Scheduler::removeJob()
{
    int currentRow = moduleState()->currentPosition();

    watchJobChanges(false);
    if (moduleState()->removeJob(currentRow) == false)
    {
        watchJobChanges(true);
        return;
    }

    /* removing the job succeeded, update UI */
    /* Remove the job from the table */
    queueTable->removeRow(currentRow);

    /* If there are no job rows left, update UI buttons */
    if (queueTable->rowCount() == 0)
    {
        setJobManipulation(false, false, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);
        evaluateOnlyB->setEnabled(false);
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        startB->setEnabled(false);
        pauseB->setEnabled(false);
    }

    // Otherwise, clear the selection, leave the UI values holding the values of the removed job.
    // The position in the job list, where the job has been removed from, is still held in the module state.
    // This leaves the option directly adding the old values reverting the deletion.
    else
        queueTable->clearSelection();

    /* If needed, reset edit mode to clean up UI */
    if (jobUnderEdit >= 0)
        resetJobEdit();

    watchJobChanges(true);
    moduleState()->refreshFollowerLists();
    process()->evaluateJobs(true);
    updateJobTable();
    // disable moving and deleting, since selection is cleared
    setJobManipulation(false, false, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);
}

void Scheduler::removeOneJob(int index)
{
    moduleState()->setCurrentPosition(index);
    removeJob();
}
void Scheduler::toggleScheduler()
{
    if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
    {
        moduleState()->disablePreemptiveShutdown();
        process()->stop();
    }
    else
        process()->start();
}

void Scheduler::pause()
{
    moduleState()->setSchedulerState(SCHEDULER_PAUSED);
    process()->appendLogText(i18n("Scheduler pause planned..."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Resume Scheduler"));
}

void Scheduler::syncGreedyParams()
{
    process()->getGreedyScheduler()->setParams(
        errorHandlingRestartImmediatelyButton->isChecked(),
        errorHandlingRestartQueueButton->isChecked(),
        errorHandlingRescheduleErrorsCB->isChecked(),
        errorHandlingStrategyDelay->value(),
        errorHandlingStrategyDelay->value());
}

void Scheduler::handleShutdownStarted()
{
    KSNotification::event(QLatin1String("ObservatoryShutdown"), i18n("Observatory is in the shutdown process"),
                          KSNotification::Scheduler);
    weatherLabel->hide();
}

void Ekos::Scheduler::changeSleepLabel(QString text, bool show)
{
    sleepLabel->setToolTip(text);
    if (show)
        sleepLabel->show();
    else
        sleepLabel->hide();
}

void Scheduler::schedulerStopped()
{
    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_NOTHING).toLatin1().data());

    // Update job table rows for aborted ones (the others remain unchanged in their state)
    bool wasAborted = false;
    for (auto &oneJob : moduleState()->jobs())
    {
        if (oneJob->getState() == SCHEDJOB_ABORTED)
        {
            updateJobTable(oneJob);
            wasAborted = true;
        }
    }

    if (wasAborted)
        KSNotification::event(QLatin1String("SchedulerAborted"), i18n("Scheduler aborted."), KSNotification::Scheduler,
                              KSNotification::Alert);

    startupB->setEnabled(true);
    shutdownB->setEnabled(true);

    // If soft shutdown, we return for now
    if (moduleState()->preemptiveShutdown())
    {
        changeSleepLabel(i18n("Scheduler is in shutdown until next job is ready"));
        pi->stopAnimation();
        return;
    }

    changeSleepLabel("", false);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Start Scheduler"));
    pauseB->setEnabled(false);
    //startB->setText("Start Scheduler");

    queueLoadB->setEnabled(true);
    queueAppendB->setEnabled(true);
    addToQueueB->setEnabled(true);
    setJobManipulation(false, false, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);
    //mosaicB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
}


bool Scheduler::loadFile(const QUrl &path)
{
    return load(true, path.toLocalFile());
}

bool Scheduler::load(bool clearQueue, const QString &filename)
{
    QUrl fileURL;

    if (filename.isEmpty())
        fileURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Open Ekos Scheduler List"),
                                              dirPath,
                                              "Ekos Scheduler List (*.esl)");
    else
        fileURL = QUrl::fromLocalFile(filename);

    if (fileURL.isEmpty())
        return false;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return false;
    }

    dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    if (clearQueue)
        process()->removeAllJobs();
    // remember toe number of rows to select the first one appended
    const int row = moduleState()->jobs().count();

    // do not update while appending
    watchJobChanges(false);
    // try appending the jobs from the file to the job list
    const bool success = process()->appendEkosScheduleList(fileURL.toLocalFile());
    // turn on whatching
    watchJobChanges(true);

    if (success)
    {
        // select the first appended row (if any was added)
        if (moduleState()->jobs().count() > row)
            moduleState()->setCurrentPosition(row);

        /* Run a job idle evaluation after a successful load */
        process()->startJobEvaluation();

        return true;
    }

    return false;
}

void Scheduler::clearJobTable()
{
    if (jobUnderEdit >= 0)
        resetJobEdit();

    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);
}

void Scheduler::clearLog()
{
    process()->clearLog();
}

void Scheduler::saveAs()
{
    schedulerURL.clear();
    save();
}

bool Scheduler::saveFile(const QUrl &path)
{
    QUrl backupCurrent = schedulerURL;
    schedulerURL = path;

    if (save())
        return true;
    else
    {
        schedulerURL = backupCurrent;
        return false;
    }
}

bool Scheduler::save()
{
    QUrl backupCurrent = schedulerURL;

    if (schedulerURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || schedulerURL.toLocalFile().contains("/Temp"))
        schedulerURL.clear();

    // If no changes made, return.
    if (moduleState()->dirty() == false && !schedulerURL.isEmpty())
        return true;

    if (schedulerURL.isEmpty())
    {
        schedulerURL =
            QFileDialog::getSaveFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Save Ekos Scheduler List"), dirPath,
                                        "Ekos Scheduler List (*.esl)");
        // if user presses cancel
        if (schedulerURL.isEmpty())
        {
            schedulerURL = backupCurrent;
            return false;
        }

        dirPath = QUrl(schedulerURL.url(QUrl::RemoveFilename));

        if (schedulerURL.toLocalFile().contains('.') == 0)
            schedulerURL.setPath(schedulerURL.toLocalFile() + ".esl");
    }

    if (schedulerURL.isValid())
    {
        if ((process()->saveScheduler(schedulerURL)) == false)
        {
            KSNotification::error(i18n("Failed to save scheduler list"), i18n("Save"));
            return false;
        }

        // update save button tool tip
        queueSaveB->setToolTip("Save schedule to " + schedulerURL.fileName());
    }
    else
    {
        QString message = i18n("Invalid URL: %1", schedulerURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return false;
    }

    return true;
}

void Scheduler::checkJobInputComplete()
{
    // For object selection, all fields must be filled
    bool const nameSelectionOK = !raBox->isEmpty()  && !decBox->isEmpty() && !nameEdit->text().isEmpty();

    // For FITS selection, only the name and fits URL should be filled.
    bool const fitsSelectionOK = !nameEdit->text().isEmpty() && !fitsURL.isEmpty();

    // Sequence selection is required
    bool const seqSelectionOK = !sequenceEdit->text().isEmpty();

    // Finally, adding is allowed upon object/FITS and sequence selection
    bool const addingOK = (nameSelectionOK || fitsSelectionOK) && seqSelectionOK;

    addToQueueB->setEnabled(addingOK);
}

void Scheduler::setDirty()
{
    // check if all fields are filled to allow adding a job
    checkJobInputComplete();

    // ignore changes that are a result of syncGUIToJob() or syncGUIToGeneralSettings()
    if (jobUnderEdit < 0)
        return;

    moduleState()->setDirty(true);

    if (sender() == startupProcedureButtonGroup || sender() == shutdownProcedureGroup)
        return;

    // update state
    if (sender() == schedulerStartupScript)
        moduleState()->setStartupScriptURL(QUrl::fromUserInput(schedulerStartupScript->text()));
    else if (sender() == schedulerShutdownScript)
        moduleState()->setShutdownScriptURL(QUrl::fromUserInput(schedulerShutdownScript->text()));
}

void Scheduler::sortJobsPerAltitude()
{
    // We require a first job to sort, so bail out if list is empty
    if (moduleState()->jobs().isEmpty())
        return;

    // Don't reset current job
    // setCurrentJob(nullptr);

    // Don't reset scheduler jobs startup times before sorting - we need the first job startup time

    // Sort by startup time, using the first job time as reference for altitude calculations
    using namespace std::placeholders;
    QList<SchedulerJob*> sortedJobs = moduleState()->jobs();
    std::stable_sort(sortedJobs.begin() + 1, sortedJobs.end(),
                     std::bind(SchedulerJob::decreasingAltitudeOrder, _1, _2, moduleState()->jobs().first()->getStartupTime()));

    // If order changed, reset and re-evaluate
    if (reorderJobs(sortedJobs))
    {
        for (SchedulerJob * job : moduleState()->jobs())
            job->reset();

        process()->evaluateJobs(true);
    }
}

void Scheduler::resumeCheckStatus()
{
    disconnect(this, &Scheduler::weatherChanged, this, &Scheduler::resumeCheckStatus);
    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
    moduleState()->setupNextIteration(RUN_SCHEDULER);
}

ErrorHandlingStrategy Scheduler::getErrorHandlingStrategy()
{
    // The UI holds the state
    if (errorHandlingRestartQueueButton->isChecked())
        return ERROR_RESTART_AFTER_TERMINATION;
    else if (errorHandlingRestartImmediatelyButton->isChecked())
        return ERROR_RESTART_IMMEDIATELY;
    else
        return ERROR_DONT_RESTART;
}

void Scheduler::setErrorHandlingStrategy(ErrorHandlingStrategy strategy)
{
    errorHandlingStrategyDelay->setEnabled(strategy != ERROR_DONT_RESTART);

    switch (strategy)
    {
        case ERROR_RESTART_AFTER_TERMINATION:
            errorHandlingRestartQueueButton->setChecked(true);
            break;
        case ERROR_RESTART_IMMEDIATELY:
            errorHandlingRestartImmediatelyButton->setChecked(true);
            break;
        default:
            errorHandlingDontRestartButton->setChecked(true);
            break;
    }
}

// Can't use a SchedulerAlgorithm type for the arg here
// as the compiler is unhappy connecting the signals currentIndexChanged(int)
// or activated(int) to an enum.
void Scheduler::setAlgorithm(int algIndex)
{
    if (algIndex != ALGORITHM_GREEDY)
    {
        process()->appendLogText(
            i18n("Warning: The Classic scheduler algorithm has been retired. Switching you to the Greedy algorithm."));
        algIndex = ALGORITHM_GREEDY;
    }
    Options::setSchedulerAlgorithm(algIndex);

    groupLabel->setDisabled(false);
    groupEdit->setDisabled(false);
    queueTable->model()->setHeaderData(START_TIME_COLUMN, Qt::Horizontal, tr("Next Start"));
    queueTable->model()->setHeaderData(END_TIME_COLUMN, Qt::Horizontal, tr("Next End"));
    queueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void Scheduler::checkTwilightWarning(bool enabled)
{
    if (enabled)
        return;
    else
        process()->appendLogText(
            i18n("Turning off astronomical twilight check may cause the observatory to run during daylight. This can cause irreversible damage to your equipment!"));
    ;
}

void Scheduler::updateProfiles()
{
    schedulerProfileCombo->blockSignals(true);
    schedulerProfileCombo->clear();
    schedulerProfileCombo->addItems(moduleState()->profiles());
    schedulerProfileCombo->setCurrentText(moduleState()->currentProfile());
    schedulerProfileCombo->blockSignals(false);
}

void Scheduler::updateJobStageUI(SchedulerJobStage stage)
{
    /* Translated string cache - overkill, probably, and doesn't warn about missing enums like switch/case should ; also, not thread-safe */
    /* FIXME: this should work with a static initializer in C++11, but QT versions are touchy on this, and perhaps i18n can't be used? */
    static QMap<SchedulerJobStage, QString> stageStrings;
    static QString stageStringUnknown;
    if (stageStrings.isEmpty())
    {
        stageStrings[SCHEDSTAGE_IDLE] = i18n("Idle");
        stageStrings[SCHEDSTAGE_SLEWING] = i18n("Slewing");
        stageStrings[SCHEDSTAGE_SLEW_COMPLETE] = i18n("Slew complete");
        stageStrings[SCHEDSTAGE_FOCUSING] =
            stageStrings[SCHEDSTAGE_POSTALIGN_FOCUSING] = i18n("Focusing");
        stageStrings[SCHEDSTAGE_FOCUS_COMPLETE] =
            stageStrings[SCHEDSTAGE_POSTALIGN_FOCUSING_COMPLETE ] = i18n("Focus complete");
        stageStrings[SCHEDSTAGE_ALIGNING] = i18n("Aligning");
        stageStrings[SCHEDSTAGE_ALIGN_COMPLETE] = i18n("Align complete");
        stageStrings[SCHEDSTAGE_RESLEWING] = i18n("Repositioning");
        stageStrings[SCHEDSTAGE_RESLEWING_COMPLETE] = i18n("Repositioning complete");
        /*stageStrings[SCHEDSTAGE_CALIBRATING] = i18n("Calibrating");*/
        stageStrings[SCHEDSTAGE_GUIDING] = i18n("Guiding");
        stageStrings[SCHEDSTAGE_GUIDING_COMPLETE] = i18n("Guiding complete");
        stageStrings[SCHEDSTAGE_CAPTURING] = i18n("Capturing");
        stageStringUnknown = i18n("Unknown");
    }

    if (activeJob() == nullptr)
        jobStatus->setText(stageStrings[SCHEDSTAGE_IDLE]);
    else
        jobStatus->setText(QString("%1: %2").arg(activeJob()->getName(),
                           stageStrings.value(stage, stageStringUnknown)));

}

void Scheduler::interfaceReady(QDBusInterface *iface)
{
    if (iface == process()->mountInterface())
    {
        QVariant canMountPark = process()->mountInterface()->property("canPark");
        if (canMountPark.isValid())
        {
            schedulerUnparkMount->setEnabled(canMountPark.toBool());
            schedulerParkMount->setEnabled(canMountPark.toBool());
        }
        copyMountTargetB->setEnabled(true);
    }
    else if (iface == process()->capInterface())
    {
        QVariant canCapPark = process()->capInterface()->property("canPark");
        if (canCapPark.isValid())
        {
            schedulerCloseDustCover->setEnabled(canCapPark.toBool());
            schedulerOpenDustCover->setEnabled(canCapPark.toBool());
        }
        else
        {
            schedulerCloseDustCover->setEnabled(false);
            schedulerOpenDustCover->setEnabled(false);
        }
    }
    else if (iface == process()->domeInterface())
    {
        QVariant canDomePark = process()->domeInterface()->property("canPark");
        if (canDomePark.isValid())
        {
            schedulerUnparkDome->setEnabled(canDomePark.toBool());
            schedulerParkDome->setEnabled(canDomePark.toBool());
        }
    }
    else if (iface == process()->captureInterface())
    {
        QVariant hasCoolerControl = process()->captureInterface()->property("coolerControl");
        if (hasCoolerControl.isValid())
        {
            schedulerWarmCCD->setEnabled(hasCoolerControl.toBool());
        }
    }
}

void Scheduler::setWeatherStatus(ISD::Weather::Status status)
{
    TEST_PRINT(stderr, "sch%d @@@setWeatherStatus(%d)\n", __LINE__, static_cast<int>(status));
    ISD::Weather::Status newStatus = status;
    QString statusString;

    switch (newStatus)
    {
        case ISD::Weather::WEATHER_OK:
            statusString = i18n("Weather conditions are OK.");
            break;

        case ISD::Weather::WEATHER_WARNING:
            statusString = i18n("Warning: weather conditions are in the WARNING zone.");
            break;

        case ISD::Weather::WEATHER_ALERT:
            statusString = i18n("Caution: weather conditions are in the DANGER zone!");
            break;

        default:
            break;
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << statusString;

    if (moduleState()->weatherStatus() == ISD::Weather::WEATHER_OK)
        weatherLabel->setPixmap(
            QIcon::fromTheme("security-high")
            .pixmap(QSize(32, 32)));
    else if (moduleState()->weatherStatus() == ISD::Weather::WEATHER_WARNING)
    {
        weatherLabel->setPixmap(
            QIcon::fromTheme("security-medium")
            .pixmap(QSize(32, 32)));
        KSNotification::event(QLatin1String("WeatherWarning"), i18n("Weather conditions in warning zone"),
                              KSNotification::Scheduler, KSNotification::Warn);
    }
    else if (moduleState()->weatherStatus() == ISD::Weather::WEATHER_ALERT)
    {
        weatherLabel->setPixmap(
            QIcon::fromTheme("security-low")
            .pixmap(QSize(32, 32)));
        KSNotification::event(QLatin1String("WeatherAlert"),
                              i18n("Weather conditions are critical. Observatory shutdown is imminent"), KSNotification::Scheduler,
                              KSNotification::Alert);
    }
    else
        weatherLabel->setPixmap(QIcon::fromTheme("chronometer")
                                .pixmap(QSize(32, 32)));

    weatherLabel->show();
    weatherLabel->setToolTip(statusString);

    process()->appendLogText(statusString);

    emit weatherChanged(moduleState()->weatherStatus());
}

void Scheduler::handleSchedulerSleeping(bool shutdown, bool sleep)
{
    if (shutdown)
    {
        weatherLabel->hide();
    }
    if (sleep)
        changeSleepLabel(i18n("Scheduler is in sleep mode"));
}

void Scheduler::handleSchedulerStateChanged(SchedulerState newState)
{
    switch (newState)
    {
        case SCHEDULER_RUNNING:
            /* Update UI to reflect startup */
            pi->startAnimation();
            sleepLabel->hide();
            startB->setIcon(QIcon::fromTheme("media-playback-stop"));
            startB->setToolTip(i18n("Stop Scheduler"));
            pauseB->setEnabled(true);
            pauseB->setChecked(false);

            /* Disable edit-related buttons */
            queueLoadB->setEnabled(false);
            setJobManipulation(true, false, leadFollowerSelectionCB->currentIndex() == INDEX_LEAD);
            //mosaicB->setEnabled(false);
            evaluateOnlyB->setEnabled(false);
            startupB->setEnabled(false);
            shutdownB->setEnabled(false);
            break;

        default:
            break;
    }
    // forward the state chqnge
    emit newStatus(newState);
}

void Scheduler::handleSetPaused()
{
    pauseB->setCheckable(true);
    pauseB->setChecked(true);
}

void Scheduler::handleJobsUpdated(QJsonArray jobsList)
{
    syncGreedyParams();
    updateJobTable();
    altGraph->plot();

    emit jobsUpdated(jobsList);
}

bool Scheduler::importMosaic(const QJsonObject &payload)
{
    QScopedPointer<FramingAssistantUI> assistant(new FramingAssistantUI());
    return assistant->importMosaic(payload);
}

void Scheduler::startupStateChanged(StartupState state)
{
    jobStatus->setText(startupStateString(state));

    switch (moduleState()->startupState())
    {
        case STARTUP_IDLE:
            startupB->setIcon(QIcon::fromTheme("media-playback-start"));
            break;
        case STARTUP_COMPLETE:
            startupB->setIcon(QIcon::fromTheme("media-playback-start"));
            process()->appendLogText(i18n("Manual startup procedure completed successfully."));
            break;
        case STARTUP_ERROR:
            startupB->setIcon(QIcon::fromTheme("media-playback-start"));
            process()->appendLogText(i18n("Manual startup procedure terminated due to errors."));
            break;
        default:
            // in all other cases startup is running
            startupB->setIcon(QIcon::fromTheme("media-playback-stop"));
            break;
    }
}
void Scheduler::shutdownStateChanged(ShutdownState state)
{
    if (state == SHUTDOWN_COMPLETE || state == SHUTDOWN_IDLE
            || state == SHUTDOWN_ERROR)
    {
        shutdownB->setIcon(QIcon::fromTheme("media-playback-start"));
        pi->stopAnimation();
    }
    else
        shutdownB->setIcon(QIcon::fromTheme("media-playback-stop"));

    if (state == SHUTDOWN_IDLE)
        jobStatus->setText(i18n("Idle"));
    else
        jobStatus->setText(shutdownStateString(state));
}
void Scheduler::ekosStateChanged(EkosState state)
{
    if (state == EKOS_IDLE)
    {
        jobStatus->setText(i18n("Idle"));
        pi->stopAnimation();
    }
    else
        jobStatus->setText(ekosStateString(state));
}
void Scheduler::indiStateChanged(INDIState state)
{
    if (state == INDI_IDLE)
    {
        jobStatus->setText(i18n("Idle"));
        pi->stopAnimation();
    }
    else
        jobStatus->setText(indiStateString(state));

    refreshOpticalTrain();
}

void Scheduler::indiCommunicationStatusChanged(CommunicationStatus status)
{
    if (status == Success)
        refreshOpticalTrain();
}
void Scheduler::parkWaitStateChanged(ParkWaitState state)
{
    jobStatus->setText(parkWaitStateString(state));
}

SchedulerJob *Scheduler::activeJob()
{
    return moduleState()->activeJob();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setTargetCoords(const dms ra, const dms dec, bool isJ2000)
{
    if (isJ2000)
    {
        targetCoords.setRA0(ra);
        targetCoords.setDec0(dec);
        targetCoords.apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->updateNum()->julianDay());
    }
    else
    {
        targetCoords.setRA(ra);
        targetCoords.setDec(dec);
        SkyPoint J2000Coord(targetCoords.ra(), targetCoords.dec());
        J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
        targetCoords.setRA0(J2000Coord.ra());
        targetCoords.setDec0(J2000Coord.dec());
    }

    displayTargetCoords();
}
void Scheduler::displayTargetCoords()
{
    // do nothing if the coordinates are invalid
    if (targetCoords.isValid() == false)
        return;

    if (epochCB->currentText() == "J2000")
    {
        raBox->show(targetCoords.ra0());
        decBox->show(targetCoords.dec0());
    }
    else
    {
        raBox->show(targetCoords.ra());
        decBox->show(targetCoords.dec());
    }
}

void Scheduler::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItem("--");
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    opticalTrainCombo->blockSignals(false);
};

}
