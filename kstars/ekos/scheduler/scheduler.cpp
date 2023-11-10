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
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"

#include <KConfigDialog>
#include <KActionCollection>

#include <fitsio.h>
#include <ekos_scheduler_debug.h>
#include <indicom.h>

// Qt version calming
#include <qtendl.h>

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

// Functions to make human-readable debug messages for the various enums.

QString commStatusString(Ekos::CommunicationStatus state)
{
    switch(state)
    {
        case Ekos::Idle:
            return "Idle";
        case Ekos::Pending:
            return "Pending";
        case Ekos::Success:
            return "Success";
        case Ekos::Error:
            return "Error";
    }
    return QString("????");
}

QString schedulerStateString(Ekos::SchedulerState state)
{
    switch(state)
    {
        case Ekos::SCHEDULER_IDLE:
            return "SCHEDULER_IDLE";
        case Ekos::SCHEDULER_STARTUP:
            return "SCHEDULER_STARTUP";
        case Ekos::SCHEDULER_RUNNING:
            return "SCHEDULER_RUNNING";
        case Ekos::SCHEDULER_PAUSED:
            return "SCHEDULER_PAUSED";
        case Ekos::SCHEDULER_SHUTDOWN:
            return "SCHEDULER_SHUTDOWN";
        case Ekos::SCHEDULER_ABORTED:
            return "SCHEDULER_ABORTED";
        case Ekos::SCHEDULER_LOADING:
            return "SCHEDULER_LOADING";
    }
    return QString("????");
}

void Scheduler::printStates(const QString &label)
{
    TEST_PRINT(stderr, "%s",
               QString("%1 %2 %3%4 %5 %6 %7 %8 %9\n")
               .arg(label)
               .arg(timerStr(moduleState()->timerState()))
               .arg(schedulerStateString(moduleState()->schedulerState()))
               .arg((moduleState()->timerState() == RUN_JOBCHECK && activeJob() != nullptr) ?
                    QString("(%1 %2)").arg(SchedulerJob::jobStatusString(activeJob()->getState()))
                    .arg(SchedulerJob::jobStageString(activeJob()->getStage())) : "")
               .arg(ekosStateString(moduleState()->ekosState()))
               .arg(indiStateString(moduleState()->indiState()))
               .arg(startupStateString(moduleState()->startupState()))
               .arg(shutdownStateString(moduleState()->shutdownState()))
               .arg(parkWaitStateString(moduleState()->parkWaitState())).toLatin1().data());
}

QDateTime Scheduler::Dawn, Scheduler::Dusk, Scheduler::preDawnDateTime;

// This is the initial conditions that need to be set before starting.
void Scheduler::init()
{
    // This is needed to get wakeupScheduler() to call start() and startup,
    // instead of assuming it is already initialized (if preemptiveShutdown was not set).
    // The time itself is not used.
    moduleState()->enablePreemptiveShutdown(SchedulerModuleState::getLocalTime());

    moduleState()->setIterationSetup(false);
    moduleState()->setupNextIteration(RUN_WAKEUP, 10);
}

// Setup the main loop and start.
void Scheduler::start()
{

    foreach (auto j, moduleState()->jobs())
        j->enableGraphicsUpdates(false);

    // New scheduler session shouldn't inherit ABORT or ERROR states from the last one.
    foreach (auto j, moduleState()->jobs())
        j->setState(SchedulerJob::JOB_IDLE);
    init();
    foreach (auto j, moduleState()->jobs())
        j->enableGraphicsUpdates(true);

    iterate();
}

// This is the main scheduler loop.
// Run an iteration, get the sleep time, sleep for that interval, and repeat.
void Scheduler::iterate()
{
    const int msSleep = runSchedulerIteration();
    if (msSleep < 0)
        return;

    connect(&moduleState()->iterationTimer(), &QTimer::timeout, this, &Scheduler::iterate, Qt::UniqueConnection);
    moduleState()->iterationTimer().setSingleShot(true);
    moduleState()->iterationTimer().start(msSleep);
}

bool Scheduler::currentlySleeping()
{
    return moduleState()->iterationTimer().isActive() && moduleState()->timerState() == RUN_WAKEUP;
}

int Scheduler::runSchedulerIteration()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (moduleState()->startMSecs() == 0)
        moduleState()->setStartMSecs(now);

    printStates(QString("\nrunScheduler Iteration %1 @ %2")
                .arg(moduleState()->increaseSchedulerIteration())
                .arg((now - moduleState()->startMSecs()) / 1000.0, 1, 'f', 3));

    SchedulerTimerState keepTimerState = moduleState()->timerState();

    // TODO: At some point we should require that timerState and timerInterval
    // be explicitly set in all iterations. Not there yet, would require too much
    // refactoring of the scheduler. When we get there, we'd exectute the following here:
    // timerState = RUN_NOTHING;    // don't like this comment, it should always set a state and interval!
    // timerInterval = -1;
    moduleState()->setIterationSetup(false);
    switch (keepTimerState)
    {
        case RUN_WAKEUP:
            wakeUpScheduler();
            break;
        case RUN_SCHEDULER:
            checkStatus();
            break;
        case RUN_JOBCHECK:
            checkJobStage();
            break;
        case RUN_SHUTDOWN:
            checkShutdownState();
            break;
        case RUN_NOTHING:
            moduleState()->setTimerInterval(-1);
            break;
    }
    if (!moduleState()->iterationSetup())
    {
        // See the above TODO.
        // Since iterations aren't yet always set up, we repeat the current
        // iteration type if one wasn't set up in the current iteration.
        // qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler iteration never set up.";
        moduleState()->setTimerInterval(moduleState()->updatePeriodMs());
        TEST_PRINT(stderr, "Scheduler iteration never set up--repeating %s with %d...\n",
                   timerStr(moduleState()->timerState()).toLatin1().data(), moduleState()->timerInterval());
    }
    printStates(QString("End iteration, sleep %1: ").arg(moduleState()->timerInterval()));
    return moduleState()->timerInterval();
}

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

    qRegisterMetaType<Ekos::SchedulerState>("Ekos::SchedulerState");
    qDBusRegisterMetaType<Ekos::SchedulerState>();

    m_moduleState.reset(new SchedulerModuleState());
    m_process = new SchedulerProcess(moduleState());

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    // Get current KStars time and set seconds to zero
    QDateTime currentDateTime = SchedulerModuleState::getLocalTime();
    QTime currentTime         = currentDateTime.time();
    currentTime.setHMS(currentTime.hour(), currentTime.minute(), 0);
    currentDateTime.setTime(currentTime);

    // Set initial time for startup and completion times
    startupTimeEdit->setDateTime(currentDateTime);
    completionTimeEdit->setDateTime(currentDateTime);

    m_GreedyScheduler = new GreedyScheduler();

    // Set up DBus interfaces
    new SchedulerAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(schedulerPathString);
    if (!QDBusConnection::sessionBus().registerObject(schedulerPathString, this))
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Scheduler failed to register with dbus");
    process()->setEkosInterface(new QDBusInterface(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr,
                                QDBusConnection::sessionBus(), this));

    process()->setIndiInterface(new QDBusInterface(kstarsInterfaceString, INDIPathString, INDIInterfaceString,
                                QDBusConnection::sessionBus(), this));

    // Example of connecting DBus signals
    //connect(ekosInterface, SIGNAL(indiStatusChanged(Ekos::CommunicationStatus)), this, SLOT(setINDICommunicationStatus(Ekos::CommunicationStatus)));
    //connect(ekosInterface, SIGNAL(ekosStatusChanged(Ekos::CommunicationStatus)), this, SLOT(setEkosCommunicationStatus(Ekos::CommunicationStatus)));
    //connect(ekosInterface, SIGNAL(newModule(QString)), this, SLOT(registerNewModule(QString)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "newModule", this,
                                          SLOT(registerNewModule(QString)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "newDevice", this,
                                          SLOT(registerNewDevice(QString, int)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "indiStatusChanged",
                                          this, SLOT(setINDICommunicationStatus(Ekos::CommunicationStatus)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "ekosStatusChanged",
                                          this, SLOT(setEkosCommunicationStatus(Ekos::CommunicationStatus)));

    sleepLabel->setPixmap(
        QIcon::fromTheme("chronometer").pixmap(QSize(32, 32)));
    sleepLabel->hide();

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi, 0);

    geo = KStarsData::Instance()->geo();

    //RA box should be HMS-style
    raBox->setUnits(dmsBox::HOURS);

    /* FIXME: Find a way to have multi-line tooltips in the .ui file, then move the widget configuration there - what about i18n? */

    queueTable->setToolTip(
        i18n("Job scheduler list.\nClick to select a job in the list.\nDouble click to edit a job with the left-hand fields."));

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
    // for all sequences is only active of we do consider the past
    repeatSequenceCB->setEnabled(Options::rememberJobProgress() == false);
    executionSequenceLimit->setEnabled(Options::rememberJobProgress() == false);
    repeatSequenceCB->setChecked(Options::schedulerRepeatSequences());
    executionSequenceLimit->setValue(Options::schedulerExecutionSequencesLimit());

    connect(startupB, &QPushButton::clicked, process(), &SchedulerProcess::runStartupProcedure);
    connect(shutdownB, &QPushButton::clicked, process(), &SchedulerProcess::runShutdownProcedure);

    connect(selectObjectB, &QPushButton::clicked, this, &Scheduler::selectObject);
    connect(selectFITSB, &QPushButton::clicked, this, &Scheduler::selectFITS);
    connect(loadSequenceB, &QPushButton::clicked, this, &Scheduler::selectSequence);
    connect(selectStartupScriptB, &QPushButton::clicked, this, &Scheduler::selectStartupScript);
    connect(selectShutdownScriptB, &QPushButton::clicked, this, &Scheduler::selectShutdownScript);

    connect(KStars::Instance()->actionCollection()->action("show_mosaic_panel"), &QAction::triggered, this, [this](bool checked)
    {
        mosaicB->setDown(checked);
    });
    connect(mosaicB, &QPushButton::clicked, this, []()
    {
        KStars::Instance()->actionCollection()->action("show_mosaic_panel")->trigger();
    });
    connect(addToQueueB, &QPushButton::clicked, this, &Scheduler::addJob);
    connect(removeFromQueueB, &QPushButton::clicked, this, &Scheduler::removeJob);
    connect(queueUpB, &QPushButton::clicked, this, &Scheduler::moveJobUp);
    connect(queueDownB, &QPushButton::clicked, this, &Scheduler::moveJobDown);
    connect(evaluateOnlyB, &QPushButton::clicked, this, &Scheduler::startJobEvaluation);
    connect(sortJobsB, &QPushButton::clicked, this, &Scheduler::sortJobsPerAltitude);
    connect(queueTable->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            &Scheduler::queueTableSelectionChanged);
    connect(queueTable, &QAbstractItemView::clicked, this, &Scheduler::clickQueueTable);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Scheduler::loadJob);


    // These connections are looking for changes in the rows queueTable is displaying.
    connect(queueTable->verticalScrollBar(), &QScrollBar::valueChanged, this, &Scheduler::updateTable);
    connect(queueTable->verticalScrollBar(), &QAbstractSlider::rangeChanged, this, &Scheduler::updateTable);

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

    connect(twilightCheck, &QCheckBox::toggled, this, &Scheduler::checkTwilightWarning);

    // Connect simulation clock scale
    connect(KStarsData::Instance()->clock(), &SimClock::scaleChanged, this, &Scheduler::simClockScaleChanged);
    connect(KStarsData::Instance()->clock(), &SimClock::timeChanged, this, &Scheduler::simClockTimeChanged);

    // Connect to the state machine
    connect(moduleState().data(), &SchedulerModuleState::ekosStateChanged, this, &Scheduler::ekosStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::indiStateChanged, this, &Scheduler::indiStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::startupStateChanged, this, &Scheduler::startupStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::shutdownStateChanged, this, &Scheduler::shutdownStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::parkWaitStateChanged, this, &Scheduler::parkWaitStateChanged);
    connect(moduleState().data(), &SchedulerModuleState::profilesChanged, this, &Scheduler::updateProfiles);
    connect(moduleState().data(), &SchedulerModuleState::currentProfileChanged, this, [&]()
    {
        schedulerProfileCombo->setCurrentText(moduleState()->currentProfile());
    });
    // Connect to process engine
    connect(process().data(), &SchedulerProcess::newLog, this, &Scheduler::appendLogText);
    connect(process().data(), &SchedulerProcess::stopScheduler, this, &Scheduler::stop);
    connect(process().data(), &SchedulerProcess::stopCurrentJobAction, this, &Scheduler::stopCurrentJobAction);
    connect(process().data(), &SchedulerProcess::findNextJob, this, &Scheduler::findNextJob);
    connect(process().data(), &SchedulerProcess::getNextAction, this, &Scheduler::getNextAction);
    // Connect geographical location - when it is available
    //connect(KStarsData::Instance()..., &LocationDialog::locationChanged..., this, &Scheduler::simClockTimeChanged);

    // Restore values for general settings.
    syncGUIToGeneralSettings();
    trackStepCheck->setChecked(Options::schedulerTrackStep());
    focusStepCheck->setChecked(Options::schedulerFocusStep());
    guideStepCheck->setChecked(Options::schedulerGuideStep());
    alignStepCheck->setChecked(Options::schedulerAlignStep());
    altConstraintCheck->setChecked(Options::schedulerAltitude());
    artificialHorizonCheck->setChecked(Options::schedulerHorizon());
    moonSeparationCheck->setChecked(Options::schedulerMoonSeparation());
    weatherCheck->setChecked(Options::schedulerWeather());
    twilightCheck->setChecked(Options::schedulerTwilight());
    minMoonSeparation->setValue(Options::schedulerMoonSeparationValue());
    minAltitude->setValue(Options::schedulerAltitudeValue());

    // Save new default values for scheduler checkboxes.
    connect(parkDomeCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerParkDome(checked);
    });
    connect(parkMountCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerParkMount(checked);
    });
    connect(capCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerCloseDustCover(checked);
    });
    connect(warmCCDCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerWarmCCD(checked);
    });
    connect(unparkDomeCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerUnparkDome(checked);
    });
    connect(unparkMountCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerUnparkMount(checked);
    });
    connect(uncapCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerOpenDustCover(checked);
    });
    connect(trackStepCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerTrackStep(checked);
    });
    connect(focusStepCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerFocusStep(checked);
    });
    connect(guideStepCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerGuideStep(checked);
    });
    connect(alignStepCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerAlignStep(checked);
    });
    connect(altConstraintCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerAltitude(checked);
    });
    connect(artificialHorizonCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerHorizon(checked);
    });
    connect(moonSeparationCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerMoonSeparation(checked);
    });
    connect(weatherCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerWeather(checked);
    });
    connect(twilightCheck, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerTwilight(checked);
    });
    connect(minMoonSeparation, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        Options::setSchedulerMoonSeparationValue(minMoonSeparation->value());
    });
    connect(minAltitude, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        Options::setSchedulerAltitudeValue(minAltitude->value());
    });
    connect(repeatSequenceCB, &QPushButton::clicked, [](bool checked)
    {
        Options::setSchedulerRepeatSequences(checked);
    });
    connect(executionSequenceLimit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]()
    {
        Options::setSchedulerExecutionSequencesLimit(executionSequenceLimit->value());
    });

    // save new default values for error handling strategy

    connect(errorHandlingRescheduleErrorsCB, &QPushButton::clicked, [](bool checked)
    {
        Options::setRescheduleErrors(checked);
    });
    connect(errorHandlingButtonGroup, static_cast<void (QButtonGroup::*)(QAbstractButton *)>
            (&QButtonGroup::buttonClicked), [this](QAbstractButton * button)
    {
        Q_UNUSED(button)
        ErrorHandlingStrategy strategy = getErrorHandlingStrategy();
        Options::setErrorHandlingStrategy(strategy);
        errorHandlingDelaySB->setEnabled(strategy != ERROR_DONT_RESTART);
    });
    connect(errorHandlingDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [](int value)
    {
        Options::setErrorHandlingStrategyDelay(value);
    });

    // Retiring the Classic algorithm.
    if (Options::schedulerAlgorithm() != ALGORITHM_GREEDY)
    {
        appendLogText(i18n("Warning: The Classic scheduler algorithm has been retired. Switching you to the Greedy algorithm."));
        Options::setSchedulerAlgorithm(ALGORITHM_GREEDY);
    }

    // restore default values for scheduler algorithm
    setAlgorithm(Options::schedulerAlgorithm());

    connect(copySkyCenterB, &QPushButton::clicked, this, [this]()
    {
        SkyPoint center = SkyMap::Instance()->getCenterPoint();
        //center.deprecess(KStarsData::Instance()->updateNum());
        center.catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        raBox->show(center.ra0());
        decBox->show(center.dec0());
    });

    connect(KConfigDialog::exists("settings"), &KConfigDialog::settingsChanged, this, &Scheduler::applyConfig);

    calculateDawnDusk();
    updateNightTime();

    process()->loadProfiles();

    watchJobChanges(true);
}

QString Scheduler::getCurrentJobName()
{
    return (activeJob() != nullptr ? activeJob()->getName() : "");
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
        startupScript,
        shutdownScript
    };

    QDateTimeEdit * const dateEdits[] =
    {
        startupTimeEdit,
        completionTimeEdit
    };

    QComboBox * const comboBoxes[] =
    {
        schedulerProfileCombo,
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
        errorHandlingRescheduleErrorsCB
    };

    QSpinBox * const spinBoxes[] =
    {
        repeatsSpin,
        errorHandlingDelaySB
    };

    QDoubleSpinBox * const dspinBoxes[] =
    {
        minMoonSeparation,
        minAltitude,
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
            connect(control, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]()
        {
            setDirty();
        });
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

void Scheduler::appendLogText(const QString &text)
{
    /* FIXME: user settings for log length */
    int const max_log_count = 2000;
    if (m_LogText.size() > max_log_count)
        m_LogText.removeLast();

    m_LogText.prepend(i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                            SchedulerModuleState::getLocalTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_SCHEDULER) << text;

    emit newLog(text);
}

void Scheduler::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Scheduler::applyConfig()
{
    calculateDawnDusk();
    updateNightTime();
    repeatSequenceCB->setEnabled(Options::rememberJobProgress() == false);
    executionSequenceLimit->setEnabled(Options::rememberJobProgress() == false);

    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
    {
        evaluateJobs(true);
    }
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
        raBox->show(object->ra0());
        decBox->show(object->dec0());

        addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
        //mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);

        setDirty();
    }
}

void Scheduler::selectFITS()
{
    fitsURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Select FITS/XISF Image"), dirPath,
                                          "FITS (*.fits *.fit);;XISF (*.xisf)");
    if (fitsURL.isEmpty())
        return;

    dirPath = QUrl(fitsURL.url(QUrl::RemoveFilename));

    fitsEdit->setText(fitsURL.toLocalFile());

    if (nameEdit->text().isEmpty())
        nameEdit->setText(fitsURL.fileName());

    addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
    //mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);

    processFITSSelection();

    setDirty();
}

void Scheduler::processFITSSelection()
{
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
            appendLogText(i18n("FITS header: cannot find OBJCTRA (%1).", QString(error_status)));
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
            appendLogText(i18n("FITS header: cannot find OBJCTDEC (%1).", QString(error_status)));
            return;
        }

        deDMS.setD(dec);
    }
    else
    {
        deDMS = dms::fromString(objectde_str, true);
    }

    raBox->show(raDMS);
    decBox->show(deDMS);

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

void Scheduler::setSequence(const QString &sequenceFileURL)
{
    sequenceURL = QUrl::fromLocalFile(sequenceFileURL);

    if (sequenceFileURL.isEmpty())
        return;
    dirPath = QUrl(sequenceURL.url(QUrl::RemoveFilename));

    sequenceEdit->setText(sequenceURL.toLocalFile());

    // For object selection, all fields must be filled
    if ((raBox->isEmpty() == false && decBox->isEmpty() == false && nameEdit->text().isEmpty() == false)
            // For FITS selection, only the name and fits URL should be filled.
            || (nameEdit->text().isEmpty() == false && fitsURL.isEmpty() == false))
    {
        addToQueueB->setEnabled(true);
        //mosaicB->setEnabled(true);
    }

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
    startupScript->setText(moduleState()->startupScriptURL().toLocalFile());
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
    shutdownScript->setText(moduleState()->shutdownScriptURL().toLocalFile());
}

void Scheduler::addJob()
{
    if (0 <= jobUnderEdit)
    {
        /* If a job is being edited, reset edition mode as all fields are already transferred to the job */
        resetJobEdit();
    }
    else
    {
        /* If a job is being added, save fields into a new job */
        saveJob();
        /* There is now an evaluation for each change, so don't duplicate the evaluation now */
        // jobEvaluationOnly = true;
        // evaluateJobs();
    }
    emit jobsUpdated(getJSONJobs());
}

void Scheduler::saveJob()
{
    if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
    {
        appendLogText(i18n("Warning: You cannot add or modify a job while the scheduler is running."));
        return;
    }

    if (nameEdit->text().isEmpty())
    {
        appendLogText(i18n("Warning: Target name is required."));
        return;
    }

    if (sequenceEdit->text().isEmpty())
    {
        appendLogText(i18n("Warning: Sequence file is required."));
        return;
    }

    // Coordinates are required unless it is a FITS file
    if ((raBox->isEmpty() || decBox->isEmpty()) && fitsURL.isEmpty())
    {
        appendLogText(i18n("Warning: Target coordinates are required."));
        return;
    }

    bool raOk = false, decOk = false;
    dms /*const*/ ra(raBox->createDms(&raOk));
    dms /*const*/ dec(decBox->createDms(&decOk));

    if (raOk == false)
    {
        appendLogText(i18n("Warning: RA value %1 is invalid.", raBox->text()));
        return;
    }

    if (decOk == false)
    {
        appendLogText(i18n("Warning: DEC value %1 is invalid.", decBox->text()));
        return;
    }

    watchJobChanges(false);

    /* Create or Update a scheduler job */
    int currentRow = queueTable->currentRow();
    SchedulerJob * job = nullptr;

    /* If no row is selected for insertion, append at end of list. */
    if (currentRow < 0)
        currentRow = queueTable->rowCount();

    /* Add job to queue only if it is new, else reuse current row.
     * Make sure job is added at the right index, now that queueTable may have a line selected without being edited.
     */
    if (0 <= jobUnderEdit)
    {
        /* FIXME: jobUnderEdit is a parallel variable that may cause issues if it desyncs from queueTable->currentRow(). */
        if (jobUnderEdit != currentRow)
            qCWarning(KSTARS_EKOS_SCHEDULER) << "BUG: the observation job under edit does not match the selected row in the job table.";

        /* Use the job in the row currently edited */
        job = moduleState()->jobs().at(currentRow);
    }
    else
    {
        /* Instantiate a new job, insert it in the job list and add a row in the table for it just after the row currently selected. */
        job = new SchedulerJob();
        moduleState()->mutlableJobs().insert(currentRow, job);
        queueTable->insertRow(currentRow);
    }

    const bool savedUpdate = job->graphicsUpdatesEnabled();
    job->enableGraphicsUpdates(false);

    /* Configure or reconfigure the observation job */

    job->setDateTimeDisplayFormat(startupTimeEdit->displayFormat());
    fitsURL = QUrl::fromLocalFile(fitsEdit->text());

    // Get several job values depending on the state of the UI.

    StartupCondition startCondition = START_AT;
    if (asapConditionR->isChecked())
        startCondition = START_ASAP;

    CompletionCondition stopCondition = FINISH_AT;
    if (sequenceCompletionR->isChecked())
        stopCondition = FINISH_SEQUENCE;
    else if (repeatCompletionR->isChecked())
        stopCondition = FINISH_REPEAT;
    else if (loopCompletionR->isChecked())
        stopCondition = FINISH_LOOP;

    double altConstraint = SchedulerJob::UNDEFINED_ALTITUDE;
    if (altConstraintCheck->isChecked())
        altConstraint = minAltitude->value();

    double moonConstraint = -1;
    if (moonSeparationCheck->isChecked())
        moonConstraint = minMoonSeparation->value();

    // The reason for this kitchen-sink function is to separate the UI from the
    // job setup, to allow for testing.
    process()->setupJob(*job, nameEdit->text(), groupEdit->text(), ra, dec,
                        KStarsData::Instance()->ut().djd(),
                        positionAngleSpin->value(), sequenceURL, fitsURL,

                        startCondition, startupTimeEdit->dateTime(),
                        stopCondition, completionTimeEdit->dateTime(), repeatsSpin->value(),

                        altConstraint,
                        moonConstraint,
                        weatherCheck->isChecked(),
                        twilightCheck->isChecked(),
                        artificialHorizonCheck->isChecked(),

                        trackStepCheck->isChecked(),
                        focusStepCheck->isChecked(),
                        alignStepCheck->isChecked(),
                        guideStepCheck->isChecked());


    /* Verifications */
    // Warn user if a duplicated job is in the list - same target, same sequence
    // FIXME: Those duplicated jobs are not necessarily processed in the order they appear in the list!
    int numWarnings = 0;
    foreach (SchedulerJob *a_job, moduleState()->jobs())
    {
        if (a_job == job)
        {
            break;
        }
        else if (a_job->getName() == job->getName())
        {
            int const a_job_row = a_job->getNameCell() ? a_job->getNameCell()->row() + 1 : 0;

            /* FIXME: Warning about duplicate jobs only checks the target name, doing it properly would require checking storage for each sequence job of each scheduler job. */
            appendLogText(i18n("Warning: job '%1' at row %2 has a duplicate target at row %3, "
                               "the scheduler may consider the same storage for captures.",
                               job->getName(), currentRow, a_job_row));

            /* Warn the user in case the two jobs are really identical */
            if (a_job->getSequenceFile() == job->getSequenceFile())
            {
                if (a_job->getRepeatsRequired() == job->getRepeatsRequired() && Options::rememberJobProgress())
                    appendLogText(i18n("Warning: jobs '%1' at row %2 and %3 probably require a different repeat count "
                                       "as currently they will complete simultaneously after %4 batches (or disable option 'Remember job progress')",
                                       job->getName(), currentRow, a_job_row, job->getRepeatsRequired()));
            }

            // Don't need to warn over and over.
            if (++numWarnings >= 1)
            {
                appendLogText(i18n("Skipped checking for duplicates."));
                break;
            }
        }
    }

    if (-1 == jobUnderEdit)
    {
        QTableWidgetItem *nameCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_NAME), nameCell);
        nameCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        nameCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *statusCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_STATUS), statusCell);
        statusCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        statusCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *captureCount = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_CAPTURES), captureCount);
        captureCount->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        captureCount->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *startupCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_STARTTIME), startupCell);
        startupCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        startupCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *altitudeCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_ALTITUDE), altitudeCell);
        altitudeCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        altitudeCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *completionCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_ENDTIME), completionCell);
        completionCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        completionCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }

    setJobStatusCells(currentRow);

    /* We just added or saved a job, so we have a job in the list - enable relevant buttons */
    queueSaveAsB->setEnabled(true);
    queueSaveB->setEnabled(true);
    startB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
    setJobManipulation(true, true);

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 was saved.").arg(job->getName()).arg(currentRow + 1);
    job->enableGraphicsUpdates(savedUpdate);
    job->updateJobCells();

    watchJobChanges(true);

    if (SCHEDULER_LOADING != moduleState()->schedulerState())
    {
        evaluateJobs(true);
    }
}

void Scheduler::syncGUIToJob(SchedulerJob *job)
{
    nameEdit->setText(job->getName());
    groupEdit->setText(job->getGroup());

    raBox->show(job->getTargetCoords().ra0());
    decBox->show(job->getTargetCoords().dec0());

    // fitsURL/sequenceURL are not part of UI, but the UI serves as model, so keep them here for now
    fitsURL = job->getFITSFile().isEmpty() ? QUrl() : job->getFITSFile();
    sequenceURL = job->getSequenceFile();
    fitsEdit->setText(fitsURL.toLocalFile());
    sequenceEdit->setText(sequenceURL.toLocalFile());

    positionAngleSpin->setValue(job->getPositionAngle());

    trackStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_TRACK);
    focusStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_FOCUS);
    alignStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_ALIGN);
    guideStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_GUIDE);

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

    if (job->hasMinAltitude())
    {
        altConstraintCheck->setChecked(true);
        minAltitude->setValue(job->getMinAltitude());
    }
    else
    {
        altConstraintCheck->setChecked(false);
        minAltitude->setValue(DEFAULT_MIN_ALTITUDE);
    }

    if (job->getMinMoonSeparation() >= 0)
    {
        moonSeparationCheck->setChecked(true);
        minMoonSeparation->setValue(job->getMinMoonSeparation());
    }
    else
    {
        moonSeparationCheck->setChecked(false);
        minMoonSeparation->setValue(DEFAULT_MIN_MOON_SEPARATION);
    }

    weatherCheck->setChecked(job->getEnforceWeather());

    twilightCheck->blockSignals(true);
    twilightCheck->setChecked(job->getEnforceTwilight());
    twilightCheck->blockSignals(false);

    artificialHorizonCheck->blockSignals(true);
    artificialHorizonCheck->setChecked(job->getEnforceArtificialHorizon());
    artificialHorizonCheck->blockSignals(false);

    switch (job->getCompletionCondition())
    {
        case FINISH_SEQUENCE:
            sequenceCompletionR->setChecked(true);
            break;

        case FINISH_REPEAT:
            repeatCompletionR->setChecked(true);
            repeatsSpin->setValue(job->getRepeatsRequired());
            break;

        case FINISH_LOOP:
            loopCompletionR->setChecked(true);
            break;

        case FINISH_AT:
            timeCompletionR->setChecked(true);
            completionTimeEdit->setDateTime(job->getCompletionTime());
            break;
    }

    updateNightTime(job);

    setJobManipulation(true, true);
}

void Scheduler::syncGUIToGeneralSettings()
{
    parkDomeCheck->setChecked(Options::schedulerParkDome());
    parkMountCheck->setChecked(Options::schedulerParkMount());
    capCheck->setChecked(Options::schedulerCloseDustCover());
    warmCCDCheck->setChecked(Options::schedulerWarmCCD());
    unparkDomeCheck->setChecked(Options::schedulerUnparkDome());
    unparkMountCheck->setChecked(Options::schedulerUnparkMount());
    uncapCheck->setChecked(Options::schedulerOpenDustCover());
    setErrorHandlingStrategy(static_cast<ErrorHandlingStrategy>(Options::errorHandlingStrategy()));
    errorHandlingDelaySB->setValue(Options::errorHandlingStrategyDelay());
    errorHandlingRescheduleErrorsCB->setChecked(Options::rescheduleErrors());
    startupScript->setText(moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile));
    shutdownScript->setText(moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile));

    if (process()->captureInterface() != nullptr)
    {
        QVariant hasCoolerControl = process()->captureInterface()->property("coolerControl");
        if (hasCoolerControl.isValid())
        {
            warmCCDCheck->setEnabled(hasCoolerControl.toBool());
            moduleState()->setCaptureReady(true);
        }
    }
}

void Scheduler::updateNightTime(SchedulerJob const *job)
{
    if (job == nullptr)
    {
        int const currentRow = queueTable->currentRow();
        if (0 < currentRow)
            job = moduleState()->jobs().at(currentRow);
    }

    QDateTime const dawn = job ? job->getDawnAstronomicalTwilight() : Dawn;
    QDateTime const dusk = job ? job->getDuskAstronomicalTwilight() : Dusk;

    QChar const warning(dawn == dusk ? 0x26A0 : '-');
    nightTime->setText(i18n("%1 %2 %3", dusk.toString("hh:mm"), warning, dawn.toString("hh:mm")));
}

void Scheduler::loadJob(QModelIndex i)
{
    if (jobUnderEdit == i.row())
        return;

    if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
    {
        appendLogText(i18n("Warning: you cannot add or modify a job while the scheduler is running."));
        return;
    }

    SchedulerJob * const job = moduleState()->jobs().at(i.row());

    if (job == nullptr)
        return;

    watchJobChanges(false);

    //job->setState(SchedulerJob::JOB_IDLE);
    //job->setStage(SchedulerJob::STAGE_IDLE);
    syncGUIToJob(job);

    /* Turn the add button into an apply button */
    setJobAddApply(false);

    /* Disable scheduler start/evaluate buttons */
    startB->setEnabled(false);
    evaluateOnlyB->setEnabled(false);

    /* Don't let the end-user remove a job being edited */
    setJobManipulation(false, false);

    jobUnderEdit = i.row();
    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is currently edited.").arg(job->getName()).arg(
                                       jobUnderEdit + 1);

    watchJobChanges(true);
}

void Scheduler::queueTableSelectionChanged(QModelIndex current, QModelIndex previous)
{
    Q_UNUSED(previous)

    // prevent selection when not idle
    if (moduleState()->schedulerState() != SCHEDULER_IDLE)
        return;

    if (current.row() < 0 || (current.row() + 1) > moduleState()->jobs().size())
        return;

    SchedulerJob * const job = moduleState()->jobs().at(current.row());

    if (job != nullptr)
    {
        resetJobEdit();
        syncGUIToJob(job);
    }
    else nightTime->setText("-");
}

void Scheduler::clickQueueTable(QModelIndex index)
{
    setJobManipulation(index.isValid(), index.isValid());
}

void Scheduler::setJobAddApply(bool add_mode)
{
    if (add_mode)
    {
        addToQueueB->setIcon(QIcon::fromTheme("list-add"));
        addToQueueB->setToolTip(i18n("Use edition fields to create a new job in the observation list."));
        //addToQueueB->setStyleSheet(QString());
        addToQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    }
    else
    {
        addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
        addToQueueB->setToolTip(i18n("Apply job changes."));
        //addToQueueB->setStyleSheet("background-color:orange;}");
        addToQueueB->setEnabled(true);
    }
}

void Scheduler::setJobManipulation(bool can_reorder, bool can_delete)
{
    bool can_edit = (moduleState()->schedulerState() == SCHEDULER_IDLE);

    if (can_reorder)
    {
        int const currentRow = queueTable->currentRow();
        queueUpB->setEnabled(can_edit && 0 < currentRow);
        queueDownB->setEnabled(can_edit && currentRow < queueTable->rowCount() - 1);
    }
    else
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
    }
    sortJobsB->setEnabled(can_edit && can_reorder);
    removeFromQueueB->setEnabled(can_edit && can_delete);
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
        int const selectedRow = queueTable->currentRow();
        SchedulerJob * const selectedJob = 0 <= selectedRow ? moduleState()->jobs().at(selectedRow) : nullptr;

        /* Reassign list */
        moduleState()->setJobs(reordered_sublist);

        /* Reassign status cells for all jobs, and reset them */
        for (int row = 0; row < moduleState()->jobs().size(); row++)
            setJobStatusCells(row);

        /* Reselect previously selected job */
        if (nullptr != selectedJob)
            queueTable->selectRow(moduleState()->jobs().indexOf(selectedJob));

        return true;
    }
    else return false;
}

void Scheduler::moveJobUp()
{
    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int const destinationRow = currentRow - 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow < 0)
        return;

    /* Swap jobs in the list */
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
    moduleState()->mutlableJobs().swapItemsAt(currentRow, destinationRow);
#else
    moduleState()->jobs().swap(currentRow, destinationRow);
#endif

    /* Reassign status cells */
    setJobStatusCells(currentRow);
    setJobStatusCells(destinationRow);

    /* Move selection to destination row */
    queueTable->selectRow(destinationRow);
    setJobManipulation(true, true);

    /* Jobs are now sorted, so reset all later jobs */
    for (int row = destinationRow; row < moduleState()->jobs().size(); row++)
        moduleState()->jobs().at(row)->reset();

    /* Make list modified and evaluate jobs */
    moduleState()->setDirty(true);
    evaluateJobs(true);
}

void Scheduler::moveJobDown()
{
    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int const destinationRow = currentRow + 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow == rowCount)
        return;

    /* Swap jobs in the list */
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
    moduleState()->mutlableJobs().swapItemsAt(currentRow, destinationRow);
#else
    moduleState()->mutlableJobs().swap(currentRow, destinationRow);
#endif

    /* Reassign status cells */
    setJobStatusCells(currentRow);
    setJobStatusCells(destinationRow);

    /* Move selection to destination row */
    queueTable->selectRow(destinationRow);
    setJobManipulation(true, true);

    /* Jobs are now sorted, so reset all later jobs */
    for (int row = currentRow; row < moduleState()->jobs().size(); row++)
        moduleState()->jobs().at(row)->reset();

    /* Make list modified and evaluate jobs */
    moduleState()->setDirty(true);
    evaluateJobs(true);
}

void Scheduler::updateTable()
{
    foreach (SchedulerJob* job, moduleState()->jobs())
        job->updateJobCells();
}

void Scheduler::setJobStatusCells(int row)
{
    if (row < 0 || moduleState()->jobs().size() <= row)
        return;

    SchedulerJob * const job = moduleState()->jobs().at(row);

    job->setNameCell(queueTable->item(row, static_cast<int>(SCHEDCOL_NAME)));
    job->setStatusCell(queueTable->item(row, static_cast<int>(SCHEDCOL_STATUS)));
    job->setCaptureCountCell(queueTable->item(row, static_cast<int>(SCHEDCOL_CAPTURES)));
    job->setAltitudeCell(queueTable->item(row, static_cast<int>(SCHEDCOL_ALTITUDE)));
    job->setStartupCell(queueTable->item(row, static_cast<int>(SCHEDCOL_STARTTIME)));
    job->setCompletionCell(queueTable->item(row, static_cast<int>(SCHEDCOL_ENDTIME)));
    job->updateJobCells();
    emit jobsUpdated(getJSONJobs());
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
    setJobManipulation(true, true);

    /* Restore scheduler operation buttons */
    evaluateOnlyB->setEnabled(true);
    startB->setEnabled(true);

    Q_ASSERT_X(jobUnderEdit == -1, __FUNCTION__, "No more edited/selected job after exiting edit mode");
}

void Scheduler::removeJob()
{
    int currentRow = queueTable->currentRow();

    /* Don't remove a row that is not selected */
    if (currentRow < 0)
        return;

    /* Grab the job currently selected */
    SchedulerJob * const job = moduleState()->jobs().at(currentRow);
    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is being deleted.").arg(job->getName()).arg(currentRow + 1);

    /* Remove the job from the table */
    queueTable->removeRow(currentRow);

    /* If there are no job rows left, update UI buttons */
    if (queueTable->rowCount() == 0)
    {
        setJobManipulation(false, false);
        evaluateOnlyB->setEnabled(false);
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        startB->setEnabled(false);
        pauseB->setEnabled(false);
    }

    /* Else update the selection */
    else
    {
        if (currentRow > queueTable->rowCount())
            currentRow = queueTable->rowCount() - 1;

        loadJob(queueTable->currentIndex());
        queueTable->selectRow(currentRow);
    }

    /* If needed, reset edit mode to clean up UI */
    if (jobUnderEdit >= 0)
        resetJobEdit();

    /* And remove the job object */
    moduleState()->mutlableJobs().removeOne(job);
    delete (job);

    moduleState()->setDirty(true);
    evaluateJobs(true);
    emit jobsUpdated(getJSONJobs());
    updateTable();
}

void Scheduler::removeOneJob(int index)
{
    queueTable->selectRow(index);
    removeJob();
}
void Scheduler::toggleScheduler()
{
    if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
    {
        moduleState()->disablePreemptiveShutdown();
        stop();
    }
    else
        start();
}

void Scheduler::stop()
{
    if (moduleState()->schedulerState() != SCHEDULER_RUNNING)
        return;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is stopping...";

    // Stop running job and abort all others
    // in case of soft shutdown we skip this
    if (!moduleState()->preemptiveShutdown())
    {
        bool wasAborted = false;
        for (auto &oneJob : moduleState()->jobs())
        {
            if (oneJob == activeJob())
                stopCurrentJobAction();

            if (oneJob->getState() <= SchedulerJob::JOB_BUSY)
            {
                appendLogText(i18n("Job '%1' has not been processed upon scheduler stop, marking aborted.", oneJob->getName()));
                oneJob->setState(SchedulerJob::JOB_ABORTED);
                wasAborted = true;
            }
        }

        if (wasAborted)
            KSNotification::event(QLatin1String("SchedulerAborted"), i18n("Scheduler aborted."), KSNotification::Scheduler,
                                  KSNotification::Alert);
    }

    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_NOTHING).toLatin1().data());
    moduleState()->setupNextIteration(RUN_NOTHING);
    moduleState()->cancelGuidingTimer();

    moduleState()->setSchedulerState(SCHEDULER_IDLE);
    emit newStatus(moduleState()->schedulerState());
    moduleState()->setEkosState(EKOS_IDLE);
    moduleState()->setIndiState(INDI_IDLE);

    moduleState()->setParkWaitState(PARKWAIT_IDLE);

    // Only reset startup state to idle if the startup procedure was interrupted before it had the chance to complete.
    // Or if we're doing a soft shutdown
    if (moduleState()->startupState() != STARTUP_COMPLETE || moduleState()->preemptiveShutdown())
    {
        if (moduleState()->startupState() == STARTUP_SCRIPT)
        {
            process()->scriptProcess().disconnect();
            process()->scriptProcess().terminate();
        }

        moduleState()->setStartupState(STARTUP_IDLE);
    }
    // Reset startup state to unparking phase (dome -> mount -> cap)
    // We do not want to run the startup script again but unparking should be checked
    // whenever the scheduler is running again.
    else if (moduleState()->startupState() == STARTUP_COMPLETE)
    {
        if (unparkDomeCheck->isChecked())
            moduleState()->setStartupState(STARTUP_UNPARK_DOME);
        else if (unparkMountCheck->isChecked())
            moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
        else if (uncapCheck->isChecked())
            moduleState()->setStartupState(STARTUP_UNPARK_CAP);
    }

    moduleState()->setShutdownState(SHUTDOWN_IDLE);

    setCurrentJob(nullptr);
    moduleState()->resetFailureCounters();
    moduleState()->setAutofocusCompleted(false);

    startupB->setEnabled(true);
    shutdownB->setEnabled(true);

    // If soft shutdown, we return for now
    if (moduleState()->preemptiveShutdown())
    {
        sleepLabel->setToolTip(i18n("Scheduler is in shutdown until next job is ready"));
        sleepLabel->show();

        QDateTime const now = SchedulerModuleState::getLocalTime();
        int const nextObservationTime = now.secsTo(moduleState()->preemptiveShutdownWakeupTime());
        moduleState()->setupNextIteration(RUN_WAKEUP,
                                          std::lround(((nextObservationTime + 1) * 1000)
                                                  / KStarsData::Instance()->clock()->scale()));
        return;

    }

    // Clear target name in capture interface upon stopping
    if (process()->captureInterface().isNull() == false)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:setProperty", "targetName=\"\"");
        process()->captureInterface()->setProperty("targetName", QString());
    }

    if (process()->scriptProcess().state() == QProcess::Running)
        process()->scriptProcess().terminate();

    sleepLabel->hide();
    pi->stopAnimation();

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Start Scheduler"));
    pauseB->setEnabled(false);
    //startB->setText("Start Scheduler");

    queueLoadB->setEnabled(true);
    queueAppendB->setEnabled(true);
    addToQueueB->setEnabled(true);
    setJobManipulation(false, false);
    //mosaicB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
}

void Scheduler::execute()
{
    switch (moduleState()->schedulerState())
    {
        case SCHEDULER_IDLE:
            /* FIXME: Manage the non-validity of the startup script earlier, and make it a warning only when the scheduler starts */
            if (!moduleState()->startupScriptURL().isEmpty() && ! moduleState()->startupScriptURL().isValid())
            {
                appendLogText(i18n("Warning: startup script URL %1 is not valid.",
                                   moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile)));
                return;
            }

            /* FIXME: Manage the non-validity of the shutdown script earlier, and make it a warning only when the scheduler starts */
            if (!moduleState()->shutdownScriptURL().isEmpty() && !moduleState()->shutdownScriptURL().isValid())
            {
                appendLogText(i18n("Warning: shutdown script URL %1 is not valid.",
                                   moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile)));
                return;
            }

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is starting...";

            /* Update UI to reflect startup */
            pi->startAnimation();
            sleepLabel->hide();
            startB->setIcon(QIcon::fromTheme("media-playback-stop"));
            startB->setToolTip(i18n("Stop Scheduler"));
            pauseB->setEnabled(true);
            pauseB->setChecked(false);

            /* Disable edit-related buttons */
            queueLoadB->setEnabled(false);
            queueAppendB->setEnabled(false);
            addToQueueB->setEnabled(false);
            setJobManipulation(false, false);
            //mosaicB->setEnabled(false);
            evaluateOnlyB->setEnabled(false);
            startupB->setEnabled(false);
            shutdownB->setEnabled(false);

            moduleState()->setSchedulerState(SCHEDULER_RUNNING);
            emit newStatus(moduleState()->schedulerState());
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            moduleState()->setupNextIteration(RUN_SCHEDULER);

            appendLogText(i18n("Scheduler started."));
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler started.";
            break;

        case SCHEDULER_PAUSED:
            /* Update UI to reflect resume */
            startB->setIcon(QIcon::fromTheme("media-playback-stop"));
            startB->setToolTip(i18n("Stop Scheduler"));
            pauseB->setEnabled(true);
            pauseB->setCheckable(false);
            pauseB->setChecked(false);

            /* Edit-related buttons are still disabled */

            /* The end-user cannot update the schedule, don't re-evaluate jobs. Timer schedulerTimer is already running. */
            moduleState()->setSchedulerState(SCHEDULER_RUNNING);
            emit newStatus(moduleState()->schedulerState());
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            moduleState()->setupNextIteration(RUN_SCHEDULER);

            appendLogText(i18n("Scheduler resuming."));
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler resuming.";
            break;

        default:
            break;
    }
}

void Scheduler::pause()
{
    moduleState()->setSchedulerState(SCHEDULER_PAUSED);
    emit newStatus(moduleState()->schedulerState());
    appendLogText(i18n("Scheduler pause planned..."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Resume Scheduler"));
}

void Scheduler::setPaused()
{
    pauseB->setCheckable(true);
    pauseB->setChecked(true);
    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_NOTHING).toLatin1().data());
    moduleState()->setupNextIteration(RUN_NOTHING);
    appendLogText(i18n("Scheduler paused."));
}

void Scheduler::setCurrentJob(SchedulerJob *job)
{
    /* Reset job widgets */
    if (activeJob())
    {
        activeJob()->setStageLabel(nullptr);
    }

    /* Set current job */
    moduleState()->setActiveJob(job);

    /* Reassign job widgets, or reset to defaults */
    if (activeJob())
    {
        activeJob()->setStageLabel(jobStatus);
        queueTable->selectRow(moduleState()->jobs().indexOf(activeJob()));
    }
    else
    {
        jobStatus->setText(i18n("No job running"));
        //queueTable->clearSelection();
    }
}

void Scheduler::syncGreedyParams()
{
    m_GreedyScheduler->setParams(
        errorHandlingRestartImmediatelyButton->isChecked(),
        errorHandlingRestartQueueButton->isChecked(),
        errorHandlingRescheduleErrorsCB->isChecked(),
        errorHandlingDelaySB->value(),
        errorHandlingDelaySB->value());
}

void Scheduler::evaluateJobs(bool evaluateOnly)
{
    for (auto job : moduleState()->jobs())
        job->clearCache();

    /* Don't evaluate if list is empty */
    if (moduleState()->jobs().isEmpty())
        return;
    /* Start by refreshing the number of captures already present - unneeded if not remembering job progress */
    if (Options::rememberJobProgress())
        updateCompletedJobsCount();

    calculateDawnDusk();

    QList<SchedulerJob *> jobsToProcess;

    syncGreedyParams();
    jobsToProcess = m_GreedyScheduler->scheduleJobs(moduleState()->jobs(), SchedulerModuleState::getLocalTime(),
                    moduleState()->capturedFramesCount(),
                    this);
    processJobs(jobsToProcess, evaluateOnly);
    emit jobsUpdated(getJSONJobs());
}

void Scheduler::processJobs(QList<SchedulerJob *> sortedJobs, bool jobEvaluationOnly)
{
    /* Apply sorting to queue table, and mark it for saving if it changes */
    moduleState()->setDirty(reorderJobs(sortedJobs) || moduleState()->dirty());

    if (jobEvaluationOnly || moduleState()->schedulerState() != SCHEDULER_RUNNING)
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos finished evaluating jobs, no job selection required.";
        return;
    }

    /*
     * At this step, we finished evaluating jobs.
     * We select the first job that has to be run, per schedule.
     */
    auto finished_or_aborted = [](SchedulerJob const * const job)
    {
        SchedulerJob::JOBStatus const s = job->getState();
        return SchedulerJob::JOB_ERROR <= s || SchedulerJob::JOB_ABORTED == s;
    };

    /* This predicate matches jobs that are neither scheduled to run nor aborted */
    auto neither_scheduled_nor_aborted = [](SchedulerJob const * const job)
    {
        SchedulerJob::JOBStatus const s = job->getState();
        return SchedulerJob::JOB_SCHEDULED != s && SchedulerJob::JOB_ABORTED != s;
    };

    /* If there are no jobs left to run in the filtered list, stop evaluation */
    if (sortedJobs.isEmpty() || std::all_of(sortedJobs.begin(), sortedJobs.end(), neither_scheduled_nor_aborted))
    {
        appendLogText(i18n("No jobs left in the scheduler queue after evaluating."));
        setCurrentJob(nullptr);
        return;
    }
    /* If there are only aborted jobs that can run, reschedule those and let Scheduler restart one loop */
    else if (std::all_of(sortedJobs.begin(), sortedJobs.end(), finished_or_aborted) &&
             errorHandlingDontRestartButton->isChecked() == false)
    {
        appendLogText(i18n("Only aborted jobs left in the scheduler queue after evaluating, rescheduling those."));
        std::for_each(sortedJobs.begin(), sortedJobs.end(), [](SchedulerJob * job)
        {
            if (SchedulerJob::JOB_ABORTED == job->getState())
                job->setState(SchedulerJob::JOB_EVALUATION);
        });

        return;
    }

    // GreedyScheduler::scheduleJobs() must be called first.
    SchedulerJob *scheduledJob = m_GreedyScheduler->getScheduledJob();
    if (!scheduledJob)
    {
        appendLogText(i18n("No jobs scheduled."));
        setCurrentJob(nullptr);
        return;
    }
    setCurrentJob(scheduledJob);
}

void Scheduler::wakeUpScheduler()
{
    sleepLabel->hide();

    if (moduleState()->preemptiveShutdown())
    {
        moduleState()->disablePreemptiveShutdown();
        appendLogText(i18n("Scheduler is awake."));
        execute();
    }
    else
    {
        if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when ready..."));
        else
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when scheduler is resumed."));

        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
}

void Scheduler::calculateDawnDusk()
{
    SchedulerJob::calculateDawnDusk(QDateTime(), Dawn, Dusk);

    preDawnDateTime = Dawn.addSecs(-60.0 * abs(Options::preDawnTime()));
}

bool Scheduler::executeJob(SchedulerJob *job)
{
    // Some states have executeJob called after current job is cancelled - checkStatus does this
    if (job == nullptr)
        return false;

    // Don't execute the current job if it is already busy
    if (activeJob() == job && SchedulerJob::JOB_BUSY == activeJob()->getState())
        return false;

    setCurrentJob(job);
    int index = moduleState()->jobs().indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    // If we already started, we check when the next object is scheduled at.
    // If it is more than 30 minutes in the future, we park the mount if that is supported
    // and we unpark when it is due to start.
    //int const nextObservationTime = now.secsTo(getActiveJob()->getStartupTime());

    // If the time to wait is greater than the lead time (5 minutes by default)
    // then we sleep, otherwise we wait. It's the same thing, just different labels.
    if (shouldSchedulerSleep(activeJob()))
        return false;
    // If job schedule isn't now, wait - continuing to execute would cancel a parking attempt
    else if (0 < SchedulerModuleState::getLocalTime().secsTo(activeJob()->getStartupTime()))
        return false;

    // From this point job can be executed now

    if (job->getCompletionCondition() == FINISH_SEQUENCE && Options::rememberJobProgress())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s%s\n", __LINE__, "captureInterface:setProperty", "targetName=",
                   job->getName().toLatin1().data());
        process()->captureInterface()->setProperty("targetName", job->getName());
    }

    calculateDawnDusk();
    updateNightTime();

    // Reset autofocus so that focus step is applied properly when checked
    // When the focus step is not checked, the capture module will eventually run focus periodically
    moduleState()->setAutofocusCompleted(false);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Executing Job " << activeJob()->getName();

    activeJob()->setState(SchedulerJob::JOB_BUSY);
    emit jobsUpdated(getJSONJobs());

    KSNotification::event(QLatin1String("EkosSchedulerJobStart"),
                          i18n("Ekos job started (%1)", activeJob()->getName()), KSNotification::Scheduler);

    // No need to continue evaluating jobs as we already have one.
    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
    moduleState()->setupNextIteration(RUN_JOBCHECK);
    return true;
}

bool Scheduler::checkShutdownState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    if (moduleState()->shutdownState() == SHUTDOWN_IDLE)
    {
        KSNotification::event(QLatin1String("ObservatoryShutdown"), i18n("Observatory is in the shutdown process"),
                              KSNotification::Scheduler);

        qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting shutdown process...";

        //            weatherTimer.stop();
        //            weatherTimer.disconnect();
        weatherLabel->hide();

        setCurrentJob(nullptr);

        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SHUTDOWN).toLatin1().data());
        moduleState()->setupNextIteration(RUN_SHUTDOWN);

    }

    return process()->checkShutdownState();
}

bool Scheduler::checkStatus()
{
    for (auto job : moduleState()->jobs())
        job->updateJobCells();

    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
    {
        if (activeJob() == nullptr)
        {
            setPaused();
            return false;
        }
        switch (activeJob()->getState())
        {
            case  SchedulerJob::JOB_BUSY:
                // do nothing
                break;
            case  SchedulerJob::JOB_COMPLETE:
                // start finding next job before pausing
                break;
            default:
                // in all other cases pause
                setPaused();
                break;
        }
    }

    // #1 If no current job selected, let's check if we need to shutdown or evaluate jobs
    if (activeJob() == nullptr)
    {
        // #2.1 If shutdown is already complete or in error, we need to stop
        if (moduleState()->shutdownState() == SHUTDOWN_COMPLETE
                || moduleState()->shutdownState() == SHUTDOWN_ERROR)
        {
            return process()->completeShutdown();
        }

        // #2.2  Check if shutdown is in progress
        if (moduleState()->shutdownState() > SHUTDOWN_IDLE)
        {
            // If Ekos is not done stopping, try again later
            if (moduleState()->ekosState() == EKOS_STOPPING && process()->checkEkosState() == false)
                return false;

            checkShutdownState();
            return false;
        }

        // #2.3 Check if park wait procedure is in progress
        if (process()->checkParkWaitState() == false)
            return false;

        // #2.4 If not in shutdown state, evaluate the jobs
        evaluateJobs(false);

        // #2.5 check if all jobs have completed and repeat is set
        if (nullptr == activeJob() && checkRepeatSequence())
        {
            // Reset all jobs
            resetJobs();
            // Re-evaluate all jobs to check whether there is at least one that might be executed
            evaluateJobs(false);
            // if there is an executable job, restart;
            if (activeJob())
            {
                sequenceExecutionCounter++;
                appendLogText(i18n("Starting job sequence iteration #%1", sequenceExecutionCounter));
                return true;
            }
        }

        // #2.6 If there is no current job after evaluation, shutdown
        if (nullptr == activeJob())
        {
            checkShutdownState();
            return false;
        }
    }
    // JM 2018-12-07: Check if we need to sleep
    else if (shouldSchedulerSleep(activeJob()) == false)
    {
        // #3 Check if startup procedure has failed.
        if (moduleState()->startupState() == STARTUP_ERROR)
        {
            // Stop Scheduler
            stop();
            return true;
        }

        // #4 Check if startup procedure Phase #1 is complete (Startup script)
        if ((moduleState()->startupState() == STARTUP_IDLE
                && process()->checkStartupState() == false)
                || moduleState()->startupState() == STARTUP_SCRIPT)
            return false;

        // #5 Check if Ekos is started
        if (process()->checkEkosState() == false)
            return false;

        // #6 Check if INDI devices are connected.
        if (process()->checkINDIState() == false)
            return false;

        // #6.1 Check if park wait procedure is in progress - in the case we're waiting for a distant job
        if (process()->checkParkWaitState() == false)
            return false;

        // #7 Check if startup procedure Phase #2 is complete (Unparking phase)
        if (moduleState()->startupState() > STARTUP_SCRIPT
                && moduleState()->startupState() < STARTUP_ERROR
                && process()->checkStartupState() == false)
            return false;

        // #8 Check it it already completed (should only happen starting a paused job)
        //    Find the next job in this case, otherwise execute the current one
        if (activeJob() && activeJob()->getState() == SchedulerJob::JOB_COMPLETE)
            findNextJob();

        // N.B. We explicitly do not check for return result here because regardless of execution result
        // we do not have any pending tasks further down.
        executeJob(activeJob());
    }

    return true;
}

void Scheduler::checkJobStage()
{
    Q_ASSERT_X(activeJob(), __FUNCTION__, "Actual current job is required to check job stage");
    if (!activeJob())
        return;

    if (checkJobStageCounter == 0)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking job stage for" << activeJob()->getName() << "startup" <<
                                       activeJob()->getStartupCondition() << activeJob()->getStartupTime().toString(
                                           activeJob()->getDateTimeDisplayFormat()) <<
                                       "state" << activeJob()->getState();
        if (checkJobStageCounter++ == 30)
            checkJobStageCounter = 0;
    }


    syncGreedyParams();
    if (!m_GreedyScheduler->checkJob(moduleState()->jobs(), SchedulerModuleState::getLocalTime(), activeJob()))
    {
        activeJob()->setState(SchedulerJob::JOB_IDLE);
        stopCurrentJobAction();
        findNextJob();
        return;
    }
    checkJobStageEplogue();
}

void Scheduler::checkJobStageEplogue()
{
    if (!activeJob())
        return;

    // #5 Check system status to improve robustness
    // This handles external events such as disconnections or end-user manipulating INDI panel
    if (!checkStatus())
        return;

    // #5b Check the guiding timer, and possibly restart guiding.
    process()->processGuidingTimer();

    // #6 Check each stage is processing properly
    // FIXME: Vanishing property should trigger a call to its event callback
    if (!activeJob()) return;
    switch (activeJob()->getStage())
    {
        case SchedulerJob::STAGE_IDLE:
            // Job is just starting.
            emit jobStarted(activeJob()->getName());
            getNextAction();
            break;

        case SchedulerJob::STAGE_ALIGNING:
            // Let's make sure align module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > static_cast<int>(ALIGN_INACTIVITY_TIMEOUT))
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "alignInterface:property", "status");
                QVariant const status = process()->alignInterface()->property("status");
                TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
                Ekos::AlignState alignStatus = static_cast<Ekos::AlignState>(status.toInt());

                if (alignStatus == Ekos::ALIGN_IDLE)
                {
                    if (moduleState()->increaseAlignFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "Align module timed out. Restarting request...";
                        process()->startAstrometry();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' alignment procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else
                    moduleState()->startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_CAPTURING:
            // Let's make sure capture module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > static_cast<int>(CAPTURE_INACTIVITY_TIMEOUT))
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:property", "status");
                QVariant const status = process()->captureInterface()->property("status");
                TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
                Ekos::CaptureState captureStatus = static_cast<Ekos::CaptureState>(status.toInt());

                if (captureStatus == Ekos::CAPTURE_IDLE)
                {
                    if (moduleState()->increaseCaptureFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "capture module timed out. Restarting request...";
                        process()->startCapture();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' capture procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else moduleState()->startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_FOCUSING:
            // Let's make sure focus module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > static_cast<int>(FOCUS_INACTIVITY_TIMEOUT))
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "focusInterface:property", "status");
                QVariant const status = process()->focusInterface()->property("status");
                TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
                Ekos::FocusState focusStatus = static_cast<Ekos::FocusState>(status.toInt());

                if (focusStatus == Ekos::FOCUS_IDLE || focusStatus == Ekos::FOCUS_WAITING)
                {
                    if (moduleState()->increaseFocusFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus module timed out. Restarting request...";
                        process()->startFocusing();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else moduleState()->startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_GUIDING:
            // Let's make sure guide module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > GUIDE_INACTIVITY_TIMEOUT)
            {
                GuideState guideStatus = process()->getGuidingStatus();

                if (guideStatus == Ekos::GUIDE_IDLE || guideStatus == Ekos::GUIDE_CONNECTED || guideStatus == Ekos::GUIDE_DISCONNECTED)
                {
                    if (moduleState()->increaseGuideFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "guide module timed out. Restarting request...";
                        process()->startGuiding();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' guiding procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else moduleState()->startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_SLEWING:
        case SchedulerJob::STAGE_RESLEWING:
            // While slewing or re-slewing, check slew status can still be obtained
        {
            TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "status");
            QVariant const slewStatus = process()->mountInterface()->property("status");
            TEST_PRINT(stderr, "  @@@dbus received %d\n", !slewStatus.isValid() ? -1 : slewStatus.toInt());

            if (slewStatus.isValid())
            {
                // Send the slew status periodically to avoid the situation where the mount is already at location and does not send any event
                // FIXME: in that case, filter TRACKING events only?
                ISD::Mount::Status const status = static_cast<ISD::Mount::Status>(slewStatus.toInt());
                process()->setMountStatus(status);
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' lost connection to the mount, attempting to reconnect.", activeJob()->getName()));
                if (!process()->manageConnectionLoss())
                    activeJob()->setState(SchedulerJob::JOB_ERROR);
                return;
            }
        }
        break;

        case SchedulerJob::STAGE_SLEW_COMPLETE:
        case SchedulerJob::STAGE_RESLEWING_COMPLETE:
            // When done slewing or re-slewing and we use a dome, only shift to the next action when the dome is done moving
            if (moduleState()->domeReady())
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "isMoving");
                QVariant const isDomeMoving = process()->domeInterface()->property("isMoving");
                TEST_PRINT(stderr, "  @@@dbus received %s\n",
                           !isDomeMoving.isValid() ? "invalid" : (isDomeMoving.value<bool>() ? "T" : "F"));

                if (!isDomeMoving.isValid())
                {
                    appendLogText(i18n("Warning: job '%1' lost connection to the dome, attempting to reconnect.", activeJob()->getName()));
                    if (!process()->manageConnectionLoss())
                        activeJob()->setState(SchedulerJob::JOB_ERROR);
                    return;
                }

                if (!isDomeMoving.value<bool>())
                    getNextAction();
            }
            else getNextAction();
            break;

        default:
            break;
    }
}

void Scheduler::getNextAction()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Get next action...";

    switch (activeJob()->getStage())
    {
        case SchedulerJob::STAGE_IDLE:
            if (activeJob()->getLightFramesRequired())
            {
                if (activeJob()->getStepPipeline() & SchedulerJob::USE_TRACK)
                    process()->startSlew();
                else if (activeJob()->getStepPipeline() & SchedulerJob::USE_FOCUS && moduleState()->autofocusCompleted() == false)
                {
                    qCDebug(KSTARS_EKOS_SCHEDULER) << "process()->startFocusing on 3485";
                    process()->startFocusing();
                }
                else if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
                    process()->startAstrometry();
                else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                    if (process()->getGuidingStatus() == GUIDE_GUIDING)
                    {
                        appendLogText(i18n("Guiding already running, directly start capturing."));
                        process()->startCapture();
                    }
                    else
                        process()->startGuiding();
                else
                    process()->startCapture();
            }
            else
            {
                if (activeJob()->getStepPipeline())
                    appendLogText(
                        i18n("Job '%1' is proceeding directly to capture stage because only calibration frames are pending.",
                             activeJob()->getName()));
                process()->startCapture();
            }

            break;

        case SchedulerJob::STAGE_SLEW_COMPLETE:
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_FOCUS && moduleState()->autofocusCompleted() == false)
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "process()->startFocusing on 3514";
                process()->startFocusing();
            }
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
                process()->startAstrometry();
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                process()->startGuiding();
            else
                process()->startCapture();
            break;

        case SchedulerJob::STAGE_FOCUS_COMPLETE:
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
                process()->startAstrometry();
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                process()->startGuiding();
            else
                process()->startCapture();
            break;

        case SchedulerJob::STAGE_ALIGN_COMPLETE:
            activeJob()->setStage(SchedulerJob::STAGE_RESLEWING);
            break;

        case SchedulerJob::STAGE_RESLEWING_COMPLETE:
            // If we have in-sequence-focus in the sequence file then we perform post alignment focusing so that the focus
            // frame is ready for the capture module in-sequence-focus procedure.
            if ((activeJob()->getStepPipeline() & SchedulerJob::USE_FOCUS) && activeJob()->getInSequenceFocus())
                // Post alignment re-focusing
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "process()->startFocusing on 3544";
                process()->startFocusing();
            }
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                process()->startGuiding();
            else
                process()->startCapture();
            break;

        case SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE:
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                process()->startGuiding();
            else
                process()->startCapture();
            break;

        case SchedulerJob::STAGE_GUIDING_COMPLETE:
            process()->startCapture();
            break;

        default:
            break;
    }
}

void Scheduler::stopCurrentJobAction()
{
    if (nullptr != activeJob())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job '" << activeJob()->getName() << "' is stopping current action..." <<
                                       activeJob()->getStage();

        switch (activeJob()->getStage())
        {
            case SchedulerJob::STAGE_IDLE:
                break;

            case SchedulerJob::STAGE_SLEWING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "abort");
                process()->mountInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_FOCUSING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "focusInterface:call", "abort");
                process()->focusInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_ALIGNING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "alignInterface:call", "abort");
                process()->alignInterface()->call(QDBus::AutoDetect, "abort");
                break;

            // N.B. Need to use BlockWithGui as proposed by Wolfgang
            // to ensure capture is properly aborted before taking any further actions.
            case SchedulerJob::STAGE_CAPTURING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:call", "abort");
                process()->captureInterface()->call(QDBus::BlockWithGui, "abort");
                break;

            default:
                break;
        }

        /* Reset interrupted job stage */
        activeJob()->setStage(SchedulerJob::STAGE_IDLE);
    }

    /* Guiding being a parallel process, check to stop it */
    process()->stopGuiding();
}

void Scheduler::load(bool clearQueue, const QString &filename)
{
    QUrl fileURL;

    if (filename.isEmpty())
        fileURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Open Ekos Scheduler List"),
                                              dirPath,
                                              "Ekos Scheduler List (*.esl)");
    else fileURL.setUrl(filename);

    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    if (clearQueue)
        removeAllJobs();

    /* Run a job idle evaluation after a successful load */
    if (appendEkosScheduleList(fileURL.toLocalFile()))
        startJobEvaluation();
}

void Scheduler::removeAllJobs()
{
    if (jobUnderEdit >= 0)
        resetJobEdit();

    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);

    qDeleteAll(moduleState()->jobs());
    moduleState()->mutlableJobs().clear();
}

bool Scheduler::loadScheduler(const QString &fileURL)
{
    removeAllJobs();
    return appendEkosScheduleList(fileURL);
}

bool Scheduler::appendEkosScheduleList(const QString &fileURL)
{
    SchedulerState const old_state = moduleState()->schedulerState();
    moduleState()->setSchedulerState(SCHEDULER_LOADING);

    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        moduleState()->setSchedulerState(old_state);
        return false;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    XMLEle *ep   = nullptr;
    XMLEle *subEP = nullptr;
    char c;

    // We expect all data read from the XML to be in the C locale - QLocale::c()
    QLocale cLocale = QLocale::c();

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *tag = tagXMLEle(ep);
                if (!strcmp(tag, "Job"))
                    processJobInfo(ep);
                else if (!strcmp(tag, "Mosaic"))
                {
                    // If we have mosaic info, load it up.
                    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
                    tiles->fromXML(fileURL);
                }
                else if (!strcmp(tag, "Profile"))
                {
                    moduleState()->setCurrentProfile(pcdataXMLEle(ep));
                }
                else if (!strcmp(tag, "SchedulerAlgorithm"))
                {
                    setAlgorithm(static_cast<SchedulerAlgorithm>(cLocale.toInt(findXMLAttValu(ep, "value"))));
                }
                else if (!strcmp(tag, "ErrorHandlingStrategy"))
                {
                    Options::setErrorHandlingStrategy(static_cast<ErrorHandlingStrategy>(cLocale.toInt(findXMLAttValu(ep,
                                                      "value"))));

                    subEP = findXMLEle(ep, "delay");
                    if (subEP)
                    {
                        Options::setErrorHandlingStrategyDelay(cLocale.toInt(pcdataXMLEle(subEP)));
                    }
                    subEP = findXMLEle(ep, "RescheduleErrors");
                    Options::setRescheduleErrors(subEP != nullptr);
                }
                else if (!strcmp(tag, "StartupProcedure"))
                {
                    XMLEle *procedure;
                    Options::setSchedulerUnparkDome(false);
                    Options::setSchedulerUnparkMount(false);
                    Options::setSchedulerOpenDustCover(false);

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = pcdataXMLEle(procedure);

                        if (!strcmp(proc, "StartupScript"))
                        {
                            moduleState()->setStartupScriptURL(QUrl::fromUserInput(findXMLAttValu(procedure, "value")));
                        }
                        else if (!strcmp(proc, "UnparkDome"))
                            Options::setSchedulerUnparkDome(true);
                        else if (!strcmp(proc, "UnparkMount"))
                            Options::setSchedulerUnparkMount(true);
                        else if (!strcmp(proc, "UnparkCap"))
                            Options::setSchedulerOpenDustCover(true);
                    }
                }
                else if (!strcmp(tag, "ShutdownProcedure"))
                {
                    XMLEle *procedure;
                    Options::setSchedulerWarmCCD(false);
                    Options::setSchedulerParkDome(false);
                    Options::setSchedulerParkMount(false);
                    Options::setSchedulerCloseDustCover(false);

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = pcdataXMLEle(procedure);

                        if (!strcmp(proc, "ShutdownScript"))
                        {
                            moduleState()->setShutdownScriptURL(QUrl::fromUserInput(findXMLAttValu(procedure, "value")));
                        }
                        else if (!strcmp(proc, "WarmCCD"))
                            Options::setSchedulerWarmCCD(true);
                        else if (!strcmp(proc, "ParkDome"))
                            Options::setSchedulerParkDome(true);
                        else if (!strcmp(proc, "ParkMount"))
                            Options::setSchedulerParkMount(true);
                        else if (!strcmp(proc, "ParkCap"))
                            Options::setSchedulerCloseDustCover(true);
                    }
                }
            }
            delXMLEle(root);
            syncGUIToGeneralSettings();
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            moduleState()->setSchedulerState(old_state);
            return false;
        }
    }

    schedulerURL = QUrl::fromLocalFile(fileURL);
    //mosaicB->setEnabled(true);
    moduleState()->setDirty(false);
    delLilXML(xmlParser);
    // update save button tool tip
    queueSaveB->setToolTip("Save schedule to " + schedulerURL.fileName());


    moduleState()->setSchedulerState(old_state);
    return true;
}

bool Scheduler::processJobInfo(XMLEle *root)
{
    XMLEle *ep;
    XMLEle *subEP;

    altConstraintCheck->setChecked(false);
    moonSeparationCheck->setChecked(false);
    weatherCheck->setChecked(false);

    twilightCheck->blockSignals(true);
    twilightCheck->setChecked(false);
    twilightCheck->blockSignals(false);

    artificialHorizonCheck->blockSignals(true);
    artificialHorizonCheck->setChecked(false);
    artificialHorizonCheck->blockSignals(false);

    minAltitude->setValue(minAltitude->minimum());
    minMoonSeparation->setValue(minMoonSeparation->minimum());
    positionAngleSpin->setValue(0);

    // We expect all data read from the XML to be in the C locale - QLocale::c()
    QLocale cLocale = QLocale::c();
    fitsURL = QUrl();
    fitsEdit->clear();

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Name"))
            nameEdit->setText(pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Group"))
            groupEdit->setText(pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Coordinates"))
        {
            subEP = findXMLEle(ep, "J2000RA");
            if (subEP)
            {
                dms ra;
                ra.setH(cLocale.toDouble(pcdataXMLEle(subEP)));
                raBox->show(ra);
            }
            subEP = findXMLEle(ep, "J2000DE");
            if (subEP)
            {
                dms de;
                de.setD(cLocale.toDouble(pcdataXMLEle(subEP)));
                decBox->show(de);
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Sequence"))
        {
            sequenceEdit->setText(pcdataXMLEle(ep));
            sequenceURL = QUrl::fromUserInput(sequenceEdit->text());
        }
        else if (!strcmp(tagXMLEle(ep), "FITS"))
        {
            fitsEdit->setText(pcdataXMLEle(ep));
            fitsURL.setPath(fitsEdit->text());
        }
        else if (!strcmp(tagXMLEle(ep), "PositionAngle"))
        {
            positionAngleSpin->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "StartupCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("ASAP", pcdataXMLEle(subEP)))
                    asapConditionR->setChecked(true);
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    startupTimeConditionR->setChecked(true);
                    startupTimeEdit->setDateTime(QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate));
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Constraints"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("MinimumAltitude", pcdataXMLEle(subEP)))
                {
                    altConstraintCheck->setChecked(true);
                    minAltitude->setValue(cLocale.toDouble(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("MoonSeparation", pcdataXMLEle(subEP)))
                {
                    moonSeparationCheck->setChecked(true);
                    minMoonSeparation->setValue(cLocale.toDouble(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("EnforceWeather", pcdataXMLEle(subEP)))
                    weatherCheck->setChecked(true);
                else if (!strcmp("EnforceTwilight", pcdataXMLEle(subEP)))
                    twilightCheck->setChecked(true);
                else if (!strcmp("EnforceArtificialHorizon", pcdataXMLEle(subEP)))
                    artificialHorizonCheck->setChecked(true);
            }
        }
        else if (!strcmp(tagXMLEle(ep), "CompletionCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("Sequence", pcdataXMLEle(subEP)))
                    sequenceCompletionR->setChecked(true);
                else if (!strcmp("Repeat", pcdataXMLEle(subEP)))
                {
                    repeatCompletionR->setChecked(true);
                    repeatsSpin->setValue(cLocale.toInt(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("Loop", pcdataXMLEle(subEP)))
                    loopCompletionR->setChecked(true);
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    timeCompletionR->setChecked(true);
                    completionTimeEdit->setDateTime(QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate));
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Steps"))
        {
            XMLEle *module;
            trackStepCheck->setChecked(false);
            focusStepCheck->setChecked(false);
            alignStepCheck->setChecked(false);
            guideStepCheck->setChecked(false);

            for (module = nextXMLEle(ep, 1); module != nullptr; module = nextXMLEle(ep, 0))
            {
                const char *proc = pcdataXMLEle(module);

                if (!strcmp(proc, "Track"))
                    trackStepCheck->setChecked(true);
                else if (!strcmp(proc, "Focus"))
                    focusStepCheck->setChecked(true);
                else if (!strcmp(proc, "Align"))
                    alignStepCheck->setChecked(true);
                else if (!strcmp(proc, "Guide"))
                    guideStepCheck->setChecked(true);
            }
        }
    }

    addToQueueB->setEnabled(true);
    saveJob();

    return true;
}

void Scheduler::saveAs()
{
    schedulerURL.clear();
    save();
}

void Scheduler::save()
{
    QUrl backupCurrent = schedulerURL;

    if (schedulerURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || schedulerURL.toLocalFile().contains("/Temp"))
        schedulerURL.clear();

    // If no changes made, return.
    if (moduleState()->dirty() == false && !schedulerURL.isEmpty())
        return;

    if (schedulerURL.isEmpty())
    {
        schedulerURL =
            QFileDialog::getSaveFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Save Ekos Scheduler List"), dirPath,
                                        "Ekos Scheduler List (*.esl)");
        // if user presses cancel
        if (schedulerURL.isEmpty())
        {
            schedulerURL = backupCurrent;
            return;
        }

        dirPath = QUrl(schedulerURL.url(QUrl::RemoveFilename));

        if (schedulerURL.toLocalFile().contains('.') == 0)
            schedulerURL.setPath(schedulerURL.toLocalFile() + ".esl");
    }

    if (schedulerURL.isValid())
    {
        if ((saveScheduler(schedulerURL)) == false)
        {
            KSNotification::error(i18n("Failed to save scheduler list"), i18n("Save"));
            return;
        }

        // update save button tool tip
        queueSaveB->setToolTip("Save schedule to " + schedulerURL.fileName());
    }
    else
    {
        QString message = i18n("Invalid URL: %1", schedulerURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }
}

bool Scheduler::canCountCaptures(const SchedulerJob &job)
{
    QList<SequenceJob*> seqjobs;
    bool hasAutoFocus = false;
    SchedulerJob tempJob = job;
    if (loadSequenceQueue(tempJob.getSequenceFile().toLocalFile(), &tempJob, seqjobs, hasAutoFocus, nullptr) == false)
        return false;

    for (const SequenceJob *oneSeqJob : seqjobs)
    {
        if (oneSeqJob->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
            return false;
    }
    return true;
}

// FindNextJob (probably misnamed) deals with what to do when jobs end.
// For instance, if they complete their capture sequence, they may
// (a) be done, (b) be part of a repeat N times, or (c) be part of a loop forever.
// Similarly, if jobs are aborted they may (a) restart right away, (b) restart after a delay, (c) be ended.
void Scheduler::findNextJob()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
    {
        // everything finished, we can pause
        setPaused();
        return;
    }

    Q_ASSERT_X(activeJob()->getState() == SchedulerJob::JOB_ERROR ||
               activeJob()->getState() == SchedulerJob::JOB_ABORTED ||
               activeJob()->getState() == SchedulerJob::JOB_COMPLETE ||
               activeJob()->getState() == SchedulerJob::JOB_IDLE,
               __FUNCTION__, "Finding next job requires current to be in error, aborted, idle or complete");

    // Reset failed count
    moduleState()->resetAlignFailureCount();
    moduleState()->resetGuideFailureCount();
    moduleState()->resetFocusFailureCount();
    moduleState()->resetCaptureFailureCount();

    if (activeJob()->getState() == SchedulerJob::JOB_ERROR || activeJob()->getState() == SchedulerJob::JOB_ABORTED)
    {
        emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());
        moduleState()->resetCaptureBatch();
        // Stop Guiding if it was used
        process()->stopGuiding();

        if (activeJob()->getState() == SchedulerJob::JOB_ERROR)
            appendLogText(i18n("Job '%1' is terminated due to errors.", activeJob()->getName()));
        else
            appendLogText(i18n("Job '%1' is aborted.", activeJob()->getName()));

        // Always reset job stage
        activeJob()->setStage(SchedulerJob::STAGE_IDLE);

        // restart aborted jobs immediately, if error handling strategy is set to "restart immediately"
        if (errorHandlingRestartImmediatelyButton->isChecked() &&
                (activeJob()->getState() == SchedulerJob::JOB_ABORTED ||
                 (activeJob()->getState() == SchedulerJob::JOB_ERROR && errorHandlingRescheduleErrorsCB->isChecked())))
        {
            // reset the state so that it will be restarted
            activeJob()->setState(SchedulerJob::JOB_SCHEDULED);

            appendLogText(i18n("Waiting %1 seconds to restart job '%2'.", errorHandlingDelaySB->value(), activeJob()->getName()));

            // wait the given delay until the jobs will be evaluated again
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_WAKEUP).toLatin1().data());
            moduleState()->setupNextIteration(RUN_WAKEUP, std::lround((errorHandlingDelaySB->value() * 1000) /
                                              KStarsData::Instance()->clock()->scale()));
            sleepLabel->setToolTip(i18n("Scheduler waits for a retry."));
            sleepLabel->show();
            return;
        }

        // otherwise start re-evaluation
        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
    else if (activeJob()->getState() == SchedulerJob::JOB_IDLE)
    {
        emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());

        // job constraints no longer valid, start re-evaluation
        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
    // Job is complete, so check completion criteria to optimize processing
    // In any case, we're done whether the job completed successfully or not.
    else if (activeJob()->getCompletionCondition() == FINISH_SEQUENCE)
    {
        emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());

        /* If we remember job progress, mark the job idle as well as all its duplicates for re-evaluation */
        if (Options::rememberJobProgress())
        {
            foreach(SchedulerJob *a_job, moduleState()->jobs())
                if (a_job == activeJob() || a_job->isDuplicateOf(activeJob()))
                    a_job->setState(SchedulerJob::JOB_IDLE);
        }

        moduleState()->resetCaptureBatch();
        // Stop Guiding if it was used
        process()->stopGuiding();

        appendLogText(i18n("Job '%1' is complete.", activeJob()->getName()));

        // Always reset job stage
        activeJob()->setStage(SchedulerJob::STAGE_IDLE);

        // If saving remotely, then can't tell later that the job has been completed.
        // Set it complete now.
        if (!canCountCaptures(*activeJob()))
            activeJob()->setState(SchedulerJob::JOB_COMPLETE);

        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
    else if (activeJob()->getCompletionCondition() == FINISH_REPEAT &&
             (activeJob()->getRepeatsRemaining() <= 1))
    {
        /* If the job is about to repeat, decrease its repeat count and reset its start time */
        if (activeJob()->getRepeatsRemaining() > 0)
        {
            // If we can remember job progress, this is done in estimateJobTime()
            if (!Options::rememberJobProgress())
            {
                activeJob()->setRepeatsRemaining(activeJob()->getRepeatsRemaining() - 1);
                activeJob()->setCompletedIterations(activeJob()->getCompletedIterations() + 1);
            }
            activeJob()->setStartupTime(QDateTime());
        }

        /* Mark the job idle as well as all its duplicates for re-evaluation */
        foreach(SchedulerJob *a_job, moduleState()->jobs())
            if (a_job == activeJob() || a_job->isDuplicateOf(activeJob()))
                a_job->setState(SchedulerJob::JOB_IDLE);

        /* Re-evaluate all jobs, without selecting a new job */
        evaluateJobs(true);

        /* If current job is actually complete because of previous duplicates, prepare for next job */
        if (activeJob() == nullptr || activeJob()->getRepeatsRemaining() == 0)
        {
            stopCurrentJobAction();

            if (activeJob() != nullptr)
            {
                emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());
                appendLogText(i18np("Job '%1' is complete after #%2 batch.",
                                    "Job '%1' is complete after #%2 batches.",
                                    activeJob()->getName(), activeJob()->getRepeatsRequired()));
                if (!canCountCaptures(*activeJob()))
                    activeJob()->setState(SchedulerJob::JOB_COMPLETE);
                setCurrentJob(nullptr);
            }
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            moduleState()->setupNextIteration(RUN_SCHEDULER);
        }
        /* If job requires more work, continue current observation */
        else
        {
            /* FIXME: raise priority to allow other jobs to schedule in-between */
            if (executeJob(activeJob()) == false)
                return;

            /* JM 2020-08-23: If user opts to force realign instead of for each job then we force this FIRST */
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
            {
                process()->stopGuiding();
                activeJob()->setStage(SchedulerJob::STAGE_ALIGNING);
                process()->startAstrometry();
            }
            /* If we are guiding, continue capturing */
            else if ( (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE) )
            {
                activeJob()->setStage(SchedulerJob::STAGE_CAPTURING);
                process()->startCapture();
            }
            /* If we are not guiding, but using alignment, realign */
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
            {
                activeJob()->setStage(SchedulerJob::STAGE_ALIGNING);
                process()->startAstrometry();
            }
            /* Else if we are neither guiding nor using alignment, slew back to target */
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_TRACK)
            {
                activeJob()->setStage(SchedulerJob::STAGE_SLEWING);
                process()->startSlew();
            }
            /* Else just start capturing */
            else
            {
                activeJob()->setStage(SchedulerJob::STAGE_CAPTURING);
                process()->startCapture();
            }

            appendLogText(i18np("Job '%1' is repeating, #%2 batch remaining.",
                                "Job '%1' is repeating, #%2 batches remaining.",
                                activeJob()->getName(), activeJob()->getRepeatsRemaining()));
            /* getActiveJob() remains the same */
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
            moduleState()->setupNextIteration(RUN_JOBCHECK);
        }
    }
    else if ((activeJob()->getCompletionCondition() == FINISH_LOOP) ||
             (activeJob()->getCompletionCondition() == FINISH_REPEAT &&
              activeJob()->getRepeatsRemaining() > 0))
    {
        /* If the job is about to repeat, decrease its repeat count and reset its start time */
        if ((activeJob()->getCompletionCondition() == FINISH_REPEAT) &&
                (activeJob()->getRepeatsRemaining() > 1))
        {
            // If we can remember job progress, this is done in estimateJobTime()
            if (!Options::rememberJobProgress())
            {
                activeJob()->setRepeatsRemaining(activeJob()->getRepeatsRemaining() - 1);
                activeJob()->setCompletedIterations(activeJob()->getCompletedIterations() + 1);
            }
            activeJob()->setStartupTime(QDateTime());
        }

        if (executeJob(activeJob()) == false)
            return;

        if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
        {
            process()->stopGuiding();
            activeJob()->setStage(SchedulerJob::STAGE_ALIGNING);
            process()->startAstrometry();
        }
        else
        {
            activeJob()->setStage(SchedulerJob::STAGE_CAPTURING);
            process()->startCapture();
        }

        moduleState()->increaseCaptureBatch();

        if (activeJob()->getCompletionCondition() == FINISH_REPEAT )
            appendLogText(i18np("Job '%1' is repeating, #%2 batch remaining.",
                                "Job '%1' is repeating, #%2 batches remaining.",
                                activeJob()->getName(), activeJob()->getRepeatsRemaining()));
        else
            appendLogText(i18n("Job '%1' is repeating, looping indefinitely.", activeJob()->getName()));

        /* getActiveJob() remains the same */
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
        moduleState()->setupNextIteration(RUN_JOBCHECK);
    }
    else if (activeJob()->getCompletionCondition() == FINISH_AT)
    {
        if (SchedulerModuleState::getLocalTime().secsTo(activeJob()->getCompletionTime()) <= 0)
        {
            emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());

            /* Mark the job idle as well as all its duplicates for re-evaluation */
            foreach(SchedulerJob *a_job, moduleState()->jobs())
                if (a_job == activeJob() || a_job->isDuplicateOf(activeJob()))
                    a_job->setState(SchedulerJob::JOB_IDLE);
            stopCurrentJobAction();

            moduleState()->resetCaptureBatch();

            appendLogText(i18np("Job '%1' stopping, reached completion time with #%2 batch done.",
                                "Job '%1' stopping, reached completion time with #%2 batches done.",
                                activeJob()->getName(), moduleState()->captureBatch() + 1));

            // Always reset job stage
            activeJob()->setStage(SchedulerJob::STAGE_IDLE);

            setCurrentJob(nullptr);
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            moduleState()->setupNextIteration(RUN_SCHEDULER);
        }
        else
        {
            if (executeJob(activeJob()) == false)
                return;

            if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
            {
                process()->stopGuiding();
                activeJob()->setStage(SchedulerJob::STAGE_ALIGNING);
                process()->startAstrometry();
            }
            else
            {
                activeJob()->setStage(SchedulerJob::STAGE_CAPTURING);
                process()->startCapture();
            }

            moduleState()->increaseCaptureBatch();

            appendLogText(i18np("Job '%1' completed #%2 batch before completion time, restarted.",
                                "Job '%1' completed #%2 batches before completion time, restarted.",
                                activeJob()->getName(), moduleState()->captureBatch()));
            /* getActiveJob() remains the same */
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
            moduleState()->setupNextIteration(RUN_JOBCHECK);
        }
    }
    else
    {
        /* Unexpected situation, mitigate by resetting the job and restarting the scheduler timer */
        qCDebug(KSTARS_EKOS_SCHEDULER) << "BUGBUG! Job '" << activeJob()->getName() <<
                                       "' timer elapsed, but no action to be taken.";

        // Always reset job stage
        activeJob()->setStage(SchedulerJob::STAGE_IDLE);

        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
}

void Scheduler::setDirty()
{
    moduleState()->setDirty(true);

    if (sender() == startupProcedureButtonGroup || sender() == shutdownProcedureGroup)
        return;

    // update state
    if (sender() == startupScript)
        moduleState()->setStartupScriptURL(QUrl::fromUserInput(startupScript->text()));
    else if (sender() == shutdownScript)
        moduleState()->setShutdownScriptURL(QUrl::fromUserInput(shutdownScript->text()));

    if (0 <= jobUnderEdit && moduleState()->schedulerState() != SCHEDULER_RUNNING && 0 <= queueTable->currentRow())
    {
        // Now that jobs are sorted, reset jobs that are later than the edited one for re-evaluation
        for (int row = jobUnderEdit; row < moduleState()->jobs().size(); row++)
            moduleState()->jobs().at(row)->reset();

        saveJob();
    }

    // For object selection, all fields must be filled
    bool const nameSelectionOK = !raBox->isEmpty()  && !decBox->isEmpty() && !nameEdit->text().isEmpty();

    // For FITS selection, only the name and fits URL should be filled.
    bool const fitsSelectionOK = !nameEdit->text().isEmpty() && !fitsURL.isEmpty();

    // Sequence selection is required
    bool const seqSelectionOK = !sequenceEdit->text().isEmpty();

    // Finally, adding is allowed upon object/FITS and sequence selection
    bool const addingOK = (nameSelectionOK || fitsSelectionOK) && seqSelectionOK;

    addToQueueB->setEnabled(addingOK);
    //mosaicB->setEnabled(addingOK);
}

void Scheduler::updateLightFramesRequired(SchedulerJob *oneJob, const QList<SequenceJob*> &seqjobs,
        const SchedulerJob::CapturedFramesMap &framesCount)
{

    bool lightFramesRequired = false;
    QMap<QString, uint16_t> expected;
    switch (oneJob->getCompletionCondition())
    {
        case FINISH_SEQUENCE:
        case FINISH_REPEAT:
            // Step 1: determine expected frames
            calculateExpectedCapturesMap(seqjobs, expected);
            // Step 2: compare with already captured frames
            for (SequenceJob *oneSeqJob : seqjobs)
            {
                QString const signature = oneSeqJob->getSignature();
                /* If frame is LIGHT, how many do we have left? */
                if (oneSeqJob->getFrameType() == FRAME_LIGHT && expected[signature] * oneJob->getRepeatsRequired() > framesCount[signature])
                {
                    lightFramesRequired = true;
                    // exit the loop, one found is sufficient
                    break;
                }
            }
            break;
        default:
            // in all other cases it does not depend on the number of captured frames
            lightFramesRequired = true;
    }
    oneJob->setLightFramesRequired(lightFramesRequired);
}

uint16_t Scheduler::calculateExpectedCapturesMap(const QList<SequenceJob *> &seqJobs, QMap<QString, uint16_t> &expected)
{
    uint16_t capturesPerRepeat = 0;
    for (auto &seqJob : seqJobs)
    {
        capturesPerRepeat += seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt();
        QString signature = seqJob->getCoreProperty(SequenceJob::SJ_Signature).toString();
        expected[signature] = static_cast<uint16_t>(seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt()) + (expected.contains(
                                  signature) ? expected[signature] : 0);
    }
    return capturesPerRepeat;
}

// Explanation of inputs and outputs.
// expected: Input key/value pairs. Key = signature, Value = number of desired captures for that key.
// capturedFramesCount: Input. As above, but value is number captured so far.
// capture_map: Output a map telling capture how many captures are needed for each signature.
// schedJob: The scheduler job being examined.
// completedItertions:  Output the number iterations of the capture sequence the job has completed.
// Return value: The total number of frames captured so far (though not in excess of requirement).
uint16_t Scheduler::fillCapturedFramesMap(const QMap<QString, uint16_t> &expected,
        const SchedulerJob::CapturedFramesMap &capturedFramesCount,
        SchedulerJob &schedJob, SchedulerJob::CapturedFramesMap &capture_map,
        int &completedIterations)
{
    uint16_t totalCompletedCount = 0;

    // Figure out which repeat this is for the key with the least progress.
    int minIterationsCompleted = -1, currentIteration = 0;
    if (Options::rememberJobProgress())
    {
        completedIterations = 0;
        for (const QString &key : expected.keys())
        {
            const int iterationsCompleted = capturedFramesCount[key] / expected[key];
            if (minIterationsCompleted == -1 || iterationsCompleted < minIterationsCompleted)
                minIterationsCompleted = iterationsCompleted;
        }
        // If this condition is FINISH_REPEAT, and we've already completed enough iterations
        // Then set the currentIteratiion as 1 more than required. No need to go higher.
        if (schedJob.getCompletionCondition() == FINISH_REPEAT
                && minIterationsCompleted >= schedJob.getRepeatsRequired())
            currentIteration  = schedJob.getRepeatsRequired() + 1;
        else
            // Otherwise set it to one more than the number completed (i.e. the one it'll be working on).
            currentIteration = minIterationsCompleted + 1;
        completedIterations = std::max(0, currentIteration - 1);
    }
    else
        // If we are not remembering progress, we'll only know the iterations completed
        // by the current job's run.
        completedIterations = schedJob.getCompletedIterations();

    for (const QString &key : expected.keys())
    {
        if (Options::rememberJobProgress())
        {
            // If we're remembering progress, then figure out how many captures have not yet been captured.
            const int diff = expected[key] * currentIteration - capturedFramesCount[key];

            // Already captured more than required? Then don't capture any this round.
            if (diff <= 0)
                capture_map[key] = expected[key];
            // Need more captures than one cycle could capture? If so, capture the full amount.
            else if (diff >= expected[key])
                capture_map[key] = 0;
            // Otherwise we know that 0 < diff < expected[key]. Capture just the number needed.
            else
                capture_map[key] = expected[key] - diff;
        }
        else
            // If we are not remembering progress, then the capture module, which reads this
            // Will capture all requirements in the .esq file.
            capture_map[key] = 0;

        // collect all captured frames counts
        if (schedJob.getCompletionCondition() == FINISH_LOOP)
            totalCompletedCount += capturedFramesCount[key];
        else
            totalCompletedCount += std::min(capturedFramesCount[key],
                                            static_cast<uint16_t>(expected[key] * schedJob.getRepeatsRequired()));
    }
    return totalCompletedCount;
}

void Scheduler::updateCompletedJobsCount(bool forced)
{
    /* Use a temporary map in order to limit the number of file searches */
    SchedulerJob::CapturedFramesMap newFramesCount;

    /* FIXME: Capture storage cache is refreshed too often, feature requires rework. */

    /* Check if one job is idle or requires evaluation - if so, force refresh */
    forced |= std::any_of(moduleState()->jobs().begin(),
                          moduleState()->jobs().end(), [](SchedulerJob * oneJob) -> bool
    {
        SchedulerJob::JOBStatus const state = oneJob->getState();
        return state == SchedulerJob::JOB_IDLE || state == SchedulerJob::JOB_EVALUATION;});

    /* If update is forced, clear the frame map */
    if (forced)
        moduleState()->capturedFramesCount().clear();

    /* Enumerate SchedulerJobs to count captures that are already stored */
    for (SchedulerJob *oneJob : moduleState()->jobs())
    {
        QList<SequenceJob*> seqjobs;
        bool hasAutoFocus = false;

        //oneJob->setLightFramesRequired(false);
        /* Look into the sequence requirements, bypass if invalid */
        if (loadSequenceQueue(oneJob->getSequenceFile().toLocalFile(), oneJob, seqjobs, hasAutoFocus,
                              this) == false)
        {
            appendLogText(i18n("Warning: job '%1' has inaccessible sequence '%2', marking invalid.", oneJob->getName(),
                               oneJob->getSequenceFile().toLocalFile()));
            oneJob->setState(SchedulerJob::JOB_INVALID);
            continue;
        }

        /* Enumerate the SchedulerJob's SequenceJobs to count captures stored for each */
        for (SequenceJob *oneSeqJob : seqjobs)
        {
            /* Only consider captures stored on client (Ekos) side */
            /* FIXME: ask the remote for the file count */
            if (oneSeqJob->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
                continue;

            /* FIXME: this signature path is incoherent when there is no filter wheel on the setup - bugfix should be elsewhere though */
            QString const signature = oneSeqJob->getSignature();

            /* If signature was processed during this run, keep it */
            if (newFramesCount.constEnd() != newFramesCount.constFind(signature))
                continue;

            /* If signature was processed during an earlier run, use the earlier count */
            QMap<QString, uint16_t>::const_iterator const earlierRunIterator = moduleState()->capturedFramesCount().constFind(
                        signature);
            if (moduleState()->capturedFramesCount().constEnd() != earlierRunIterator)
            {
                newFramesCount[signature] = earlierRunIterator.value();
                continue;
            }

            /* Else recount captures already stored */
            newFramesCount[signature] = getCompletedFiles(signature);
        }

        // determine whether we need to continue capturing, depending on captured frames
        updateLightFramesRequired(oneJob, seqjobs, newFramesCount);
    }

    moduleState()->setCapturedFramesCount(newFramesCount);

    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Frame map summary:";
        QMap<QString, uint16_t>::const_iterator it = moduleState()->capturedFramesCount().constBegin();
        for (; it != moduleState()->capturedFramesCount().constEnd(); it++)
            qCDebug(KSTARS_EKOS_SCHEDULER) << " " << it.key() << ':' << it.value();
    }
}

bool Scheduler::estimateJobTime(SchedulerJob *schedJob, const QMap<QString, uint16_t> &capturedFramesCount,
                                Scheduler *scheduler)
{
    static SchedulerJob *jobWarned = nullptr;

    // Load the sequence job associated with the argument scheduler job.
    QList<SequenceJob *> seqJobs;
    bool hasAutoFocus = false;
    if (loadSequenceQueue(schedJob->getSequenceFile().toLocalFile(), schedJob, seqJobs, hasAutoFocus,
                          scheduler) == false)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) <<
                                         QString("Warning: Failed estimating the duration of job '%1', its sequence file is invalid.").arg(
                                             schedJob->getSequenceFile().toLocalFile());
        return false;
    }

    // FIXME: setting in-sequence focus should be done in XML processing.
    schedJob->setInSequenceFocus(hasAutoFocus);

    // Stop spam of log on re-evaluation. If we display the warning once, then that's it.
    if (schedJob != jobWarned && hasAutoFocus && !(schedJob->getStepPipeline() & SchedulerJob::USE_FOCUS))
    {
        if (scheduler != nullptr) scheduler->appendLogText(
                i18n("Warning: Job '%1' has its focus step disabled, periodic and/or HFR procedures currently set in its sequence will not occur.",
                     schedJob->getName()));
        jobWarned = schedJob;
    }

    /* This is the map of captured frames for this scheduler job, keyed per storage signature.
     * It will be forwarded to the Capture module in order to capture only what frames are required.
     * If option "Remember Job Progress" is disabled, this map will be empty, and the Capture module will process all requested captures unconditionally.
     */
    SchedulerJob::CapturedFramesMap capture_map;
    bool const rememberJobProgress = Options::rememberJobProgress();

    double totalImagingTime  = 0;
    double imagingTimePerRepeat = 0, imagingTimeLeftThisRepeat = 0;

    // Determine number of captures in the scheduler job
    QMap<QString, uint16_t> expected;
    uint16_t allCapturesPerRepeat = calculateExpectedCapturesMap(seqJobs, expected);

    // fill the captured frames map
    int completedIterations;
    uint16_t totalCompletedCount = fillCapturedFramesMap(expected, capturedFramesCount, *schedJob, capture_map,
                                   completedIterations);
    schedJob->setCompletedIterations(completedIterations);
    // Loop through sequence jobs to calculate the number of required frames and estimate duration.
    foreach (SequenceJob *seqJob, seqJobs)
    {
        // FIXME: find a way to actually display the filter name.
        QString seqName = i18n("Job '%1' %2x%3\" %4", schedJob->getName(), seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt(),
                               seqJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                               seqJob->getCoreProperty(SequenceJob::SJ_Filter).toString());

        if (seqJob->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) <<
                                          QString("%1 duration cannot be estimated time since the sequence saves the files remotely.").arg(seqName);
            schedJob->setEstimatedTime(-2);
            qDeleteAll(seqJobs);
            return true;
        }

        // Note that looping jobs will have zero repeats required.
        QString const signature      = seqJob->getSignature();
        QString const signature_path = QFileInfo(signature).path();
        int captures_required        = seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt() * schedJob->getRepeatsRequired();
        int captures_completed       = capturedFramesCount[signature];
        const int capturesRequiredPerRepeat = std::max(1, seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
        int capturesLeftThisRepeat   = std::max(0, capturesRequiredPerRepeat - (captures_completed % capturesRequiredPerRepeat));
        if (captures_completed >= (1 + completedIterations) * capturesRequiredPerRepeat)
        {
            // Something else is causing this iteration to be incomplete. Nothing left to do for this seqJob.
            capturesLeftThisRepeat = 0;
        }

        if (rememberJobProgress && schedJob->getCompletionCondition() != FINISH_LOOP)
        {
            /* Enumerate sequence jobs associated to this scheduler job, and assign them a completed count.
             *
             * The objective of this block is to fill the storage map of the scheduler job with completed counts for each capture storage.
             *
             * Sequence jobs capture to a storage folder, and are given a count of captures to store at that location.
             * The tricky part is to make sure the repeat count of the scheduler job is properly transferred to each sequence job.
             *
             * For instance, a scheduler job repeated three times must execute the full list of sequence jobs three times, thus
             * has to tell each sequence job it misses all captures, three times. It cannot tell the sequence job three captures are
             * missing, first because that's not how the sequence job is designed (completed count, not required count), and second
             * because this would make the single sequence job repeat three times, instead of repeating the full list of sequence
             * jobs three times.
             *
             * The consolidated storage map will be assigned to each sequence job based on their signature when the scheduler job executes them.
             *
             * For instance, consider a RGBL sequence of single captures. The map will store completed captures for R, G, B and L storages.
             * If R and G have 1 file each, and B and L have no files, map[storage(R)] = map[storage(G)] = 1 and map[storage(B)] = map[storage(L)] = 0.
             * When that scheduler job executes, only B and L captures will be processed.
             *
             * In the case of a RGBLRGB sequence of single captures, the second R, G and B map items will count one less capture than what is really in storage.
             * If R and G have 1 file each, and B and L have no files, map[storage(R1)] = map[storage(B1)] = 1, and all others will be 0.
             * When that scheduler job executes, B1, L, R2, G2 and B2 will be processed.
             *
             * This doesn't handle the case of duplicated scheduler jobs, that is, scheduler jobs with the same storage for capture sets.
             * Those scheduler jobs will all change state to completion at the same moment as they all target the same storage.
             * This is why it is important to manage the repeat count of the scheduler job, as stated earlier.
             */

            captures_required = expected[seqJob->getSignature()] * schedJob->getRepeatsRequired();

            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 sees %2 captures in output folder '%3'.").arg(seqName).arg(
                                              captures_completed).arg(QFileInfo(signature).path());

            // Enumerate sequence jobs to check how many captures are completed overall in the same storage as the current one
            foreach (SequenceJob *prevSeqJob, seqJobs)
            {
                // Enumerate seqJobs up to the current one
                if (seqJob == prevSeqJob)
                    break;

                // If the previous sequence signature matches the current, skip counting to take duplicates into account
                if (!signature.compare(prevSeqJob->getSignature()))
                    captures_required = 0;

                // And break if no captures remain, this job does not need to be executed
                if (captures_required == 0)
                    break;
            }

            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("%1 has completed %2/%3 of its required captures in output folder '%4'.").arg(
                                               seqName).arg(captures_completed).arg(captures_required).arg(signature_path);

        }
        // Else rely on the captures done during this session
        else if (0 < allCapturesPerRepeat)
        {
            captures_completed = schedJob->getCompletedCount() / allCapturesPerRepeat * seqJob->getCoreProperty(
                                     SequenceJob::SJ_Count).toInt();
        }
        else
        {
            captures_completed = 0;
        }

        // Check if we still need any light frames. Because light frames changes the flow of the observatory startup
        // Without light frames, there is no need to do focusing, alignment, guiding...etc
        // We check if the frame type is LIGHT and if either the number of captures_completed frames is less than required
        // OR if the completion condition is set to LOOP so it is never complete due to looping.
        // Note that looping jobs will have zero repeats required.
        // FIXME: As it is implemented now, FINISH_LOOP may loop over a capture-complete, therefore inoperant, scheduler job.
        bool const areJobCapturesComplete = (0 == captures_required || captures_completed >= captures_required);
        if (seqJob->getFrameType() == FRAME_LIGHT)
        {
            if(areJobCapturesComplete)
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 completed its sequence of %2 light frames.").arg(seqName).arg(
                                                  captures_required);
            }
        }
        else
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 captures calibration frames.").arg(seqName);
        }

        /* If captures are not complete, we have imaging time left */
        if (!areJobCapturesComplete || schedJob->getCompletionCondition() == FINISH_LOOP)
        {
            unsigned int const captures_to_go = captures_required - captures_completed;
            const double secsPerCapture = (seqJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() +
                                           (seqJob->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000.0));
            totalImagingTime += fabs(secsPerCapture * captures_to_go);
            imagingTimePerRepeat += fabs(secsPerCapture * seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
            imagingTimeLeftThisRepeat += fabs(secsPerCapture * capturesLeftThisRepeat);
            /* If we have light frames to process, add focus/dithering delay */
            if (seqJob->getFrameType() == FRAME_LIGHT)
            {
                // If inSequenceFocus is true
                if (hasAutoFocus)
                {
                    // Wild guess, 10s of autofocus for each capture required. Can vary a lot, but this is just a completion estimate.
                    constexpr int afSecsPerCapture = 10;
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a focus procedure.").arg(seqName);
                    totalImagingTime += captures_to_go * afSecsPerCapture;
                    imagingTimePerRepeat += capturesRequiredPerRepeat * afSecsPerCapture;
                    imagingTimeLeftThisRepeat += capturesLeftThisRepeat * afSecsPerCapture;
                }
                // If we're dithering after each exposure, that's another 10-20 seconds
                if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE && Options::ditherEnabled())
                {
                    constexpr int ditherSecs = 15;
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a dither procedure.").arg(seqName);
                    totalImagingTime += (captures_to_go * ditherSecs) / Options::ditherFrames();
                    imagingTimePerRepeat += (capturesRequiredPerRepeat * ditherSecs) / Options::ditherFrames();
                    imagingTimeLeftThisRepeat += (capturesLeftThisRepeat * ditherSecs) / Options::ditherFrames();
                }
            }
        }
    }

    schedJob->setCapturedFramesMap(capture_map);
    schedJob->setSequenceCount(allCapturesPerRepeat * schedJob->getRepeatsRequired());

    // only in case we remember the job progress, we change the completion count
    if (rememberJobProgress)
        schedJob->setCompletedCount(totalCompletedCount);

    qDeleteAll(seqJobs);

    schedJob->setEstimatedTimePerRepeat(imagingTimePerRepeat);
    schedJob->setEstimatedTimeLeftThisRepeat(imagingTimeLeftThisRepeat);
    if (schedJob->getLightFramesRequired())
        schedJob->setEstimatedStartupTime(timeHeuristics(schedJob));

    // FIXME: Move those ifs away to the caller in order to avoid estimating in those situations!

    // We can't estimate times that do not finish when sequence is done
    if (schedJob->getCompletionCondition() == FINISH_LOOP)
    {
        // We can't know estimated time if it is looping indefinitely
        schedJob->setEstimatedTime(-2);
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                       QString("Job '%1' is configured to loop until Scheduler is stopped manually, has undefined imaging time.")
                                       .arg(schedJob->getName());
    }
    // If we know startup and finish times, we can estimate time right away
    else if (schedJob->getStartupCondition() == START_AT &&
             schedJob->getCompletionCondition() == FINISH_AT)
    {
        // FIXME: SchedulerJob is probably doing this already
        qint64 const diff = schedJob->getStartupTime().secsTo(schedJob->getCompletionTime());
        schedJob->setEstimatedTime(diff);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a startup time and fixed completion time, will run for %2.")
                                       .arg(schedJob->getName())
                                       .arg(dms(diff * 15.0 / 3600.0f).toHMSString());
    }
    // If we know finish time only, we can roughly estimate the time considering the job starts now
    else if (schedJob->getStartupCondition() != START_AT &&
             schedJob->getCompletionCondition() == FINISH_AT)
    {
        qint64 const diff = SchedulerModuleState::getLocalTime().secsTo(schedJob->getCompletionTime());
        schedJob->setEstimatedTime(diff);
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                       QString("Job '%1' has no startup time but fixed completion time, will run for %2 if started now.")
                                       .arg(schedJob->getName())
                                       .arg(dms(diff * 15.0 / 3600.0f).toHMSString());
    }
    // Rely on the estimated imaging time to determine whether this job is complete or not - this makes the estimated time null
    else if (totalImagingTime <= 0)
    {
        schedJob->setEstimatedTime(0);
        schedJob->setEstimatedTimePerRepeat(1);
        schedJob->setEstimatedTimeLeftThisRepeat(0);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' will not run, complete with %2/%3 captures.")
                                       .arg(schedJob->getName()).arg(schedJob->getCompletedCount()).arg(schedJob->getSequenceCount());
    }
    // Else consolidate with step durations
    else
    {
        if (schedJob->getLightFramesRequired())
        {
            totalImagingTime += timeHeuristics(schedJob);
            schedJob->setEstimatedStartupTime(timeHeuristics(schedJob));
        }
        dms const estimatedTime(totalImagingTime * 15.0 / 3600.0);
        schedJob->setEstimatedTime(std::ceil(totalImagingTime));

        qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' estimated to take %2 to complete.").arg(schedJob->getName(),
                                      estimatedTime.toHMSString());
    }

    return true;
}

int Scheduler::timeHeuristics(const SchedulerJob *schedJob)
{
    double imagingTime = 0;
    /* FIXME: estimation should base on actual measure of each step, eventually with preliminary data as what it used now */
    // Are we doing tracking? It takes about 30 seconds
    if (schedJob->getStepPipeline() & SchedulerJob::USE_TRACK)
        imagingTime += 30;
    // Are we doing initial focusing? That can take about 2 minutes
    if (schedJob->getStepPipeline() & SchedulerJob::USE_FOCUS)
        imagingTime += 120;
    // Are we doing astrometry? That can take about 60 seconds
    if (schedJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
    {
        imagingTime += 60;
    }
    // Are we doing guiding?
    if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
    {
        // Looping, finding guide star, settling takes 15 sec
        imagingTime += 15;

        // Add guiding settle time from dither setting (used by phd2::guide())
        imagingTime += Options::ditherSettle();
        // Add guiding settle time from ekos sccheduler setting
        imagingTime += Options::guidingSettle();

        // If calibration always cleared
        // then calibration process can take about 2 mins
        if(Options::resetGuideCalibration())
            imagingTime += 120;
    }
    return imagingTime;
}

void Scheduler::startJobEvaluation()
{
    // Reset all jobs
    resetJobs();

    // reset the iterations counter
    sequenceExecutionCounter = 1;

    // And evaluate all pending jobs per the conditions set in each
    evaluateJobs(true);

    updateTable();
}

void Ekos::Scheduler::resetJobs()
{
    setCurrentJob(nullptr);

    // Reset ALL scheduler jobs to IDLE and force-reset their completed count - no effect when progress is kept
    for (SchedulerJob * job : moduleState()->jobs())
    {
        job->reset();
        job->setCompletedCount(0);
    }

    // Unconditionally update the capture storage
    updateCompletedJobsCount(true);
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

        evaluateJobs(true);
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
    errorHandlingDelaySB->setEnabled(strategy != ERROR_DONT_RESTART);

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
        appendLogText(i18n("Warning: The Classic scheduler algorithm has been retired. Switching you to the Greedy algorithm."));
        algIndex = ALGORITHM_GREEDY;
    }
    Options::setSchedulerAlgorithm(algIndex);

    groupLabel->setDisabled(false);
    groupEdit->setDisabled(false);
    queueTable->model()->setHeaderData(START_TIME_COLUMN, Qt::Horizontal, tr("Next Start"));
    queueTable->model()->setHeaderData(END_TIME_COLUMN, Qt::Horizontal, tr("Next End"));
    queueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

XMLEle * Scheduler::getSequenceJobRoot(const QString &filename) const
{
    QFile sFile;
    sFile.setFileName(filename);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        KSNotification::sorry(i18n("Unable to open file %1", sFile.fileName()),
                              i18n("Could Not Open File"));
        return nullptr;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
            break;
    }

    delLilXML(xmlParser);
    sFile.close();
    return root;
}

bool Scheduler::createJobSequence(XMLEle *root, const QString &prefix, const QString &outputDir)
{
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Job"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp(tagXMLEle(subEP), "Prefix"))
                {
                    XMLEle *rawPrefix = findXMLEle(subEP, "RawPrefix");
                    if (rawPrefix)
                    {
                        editXMLEle(rawPrefix, prefix.toLatin1().constData());
                    }
                }
                else if (!strcmp(tagXMLEle(subEP), "FITSDirectory"))
                {
                    editXMLEle(subEP, outputDir.toLatin1().constData());
                }
            }
        }
    }

    QDir().mkpath(outputDir);

    QString filename = QString("%1/%2.esq").arg(outputDir, prefix);
    FILE *outputFile = fopen(filename.toLatin1().constData(), "w");

    if (outputFile == nullptr)
    {
        QString message = i18n("Unable to write to file %1", filename);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    fprintf(outputFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    prXMLEle(outputFile, root, 0);

    fclose(outputFile);

    return true;
}

void Scheduler::resetAllJobs()
{
    if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
        return;

    // Reset capture count of all jobs before re-evaluating
    foreach (SchedulerJob *job, moduleState()->jobs())
        job->setCompletedCount(0);

    // Evaluate all jobs, this refreshes storage and resets job states
    startJobEvaluation();
}

void Scheduler::checkTwilightWarning(bool enabled)
{
    if (enabled)
        return;

    if (KMessageBox::warningContinueCancel(
                nullptr,
                i18n("Turning off astronomial twilight check may cause the observatory "
                     "to run during daylight. This can cause irreversible damage to your equipment!"),
                i18n("Astronomial Twilight Warning"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                "astronomical_twilight_warning") == KMessageBox::Cancel)
    {
        twilightCheck->setChecked(true);
    }
}


void Scheduler::updateProfiles()
{
    schedulerProfileCombo->blockSignals(true);
    schedulerProfileCombo->clear();
    schedulerProfileCombo->addItems(moduleState()->profiles());
    schedulerProfileCombo->setCurrentText(moduleState()->currentProfile());
    schedulerProfileCombo->blockSignals(false);
}

bool Scheduler::loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs,
                                  bool &hasAutoFocus, Scheduler *scheduler)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        if (scheduler != nullptr) scheduler->appendLogText(i18n("Unable to open sequence queue file '%1'", fileURL));
        return false;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    XMLEle *ep   = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "Autofocus"))
                    hasAutoFocus = (!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                else if (!strcmp(tagXMLEle(ep), "Job"))
                {
                    SequenceJob *thisJob = processJobInfo(ep, schedJob);
                    jobs.append(thisJob);
                    if (jobs.count() == 1)
                    {
                        auto &firstJob = jobs.first();
                        if (FRAME_LIGHT == firstJob->getFrameType() && nullptr != schedJob)
                        {
                            schedJob->setInitialFilter(firstJob->getCoreProperty(SequenceJob::SJ_Filter).toString());
                        }

                    }
                }
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            if (scheduler != nullptr) scheduler->appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            qDeleteAll(jobs);
            return false;
        }
    }

    return true;
}

SequenceJob * Scheduler::processJobInfo(XMLEle *root, SchedulerJob *schedJob)
{
    SequenceJob *job = new SequenceJob(root, schedJob->getName());
    if (FRAME_LIGHT == job->getFrameType() && nullptr != schedJob)
        schedJob->setLightFramesRequired(true);

    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.processJobInfo(job);

    return job;
}

int Scheduler::getCompletedFiles(const QString &path)
{
    int seqFileCount = 0;
#ifdef Q_OS_WIN
    // Splitting directory and baseName in QFileInfo does not distinguish regular expression backslash from directory separator on Windows.
    // So do not use QFileInfo for the code that separates directory and basename for Windows.
    // Conditions for calling this function:
    // - Directory separators must always be "/".
    // - Directory separators must not contain backslash.
    QString sig_dir;
    QString sig_file;
    int index = path.lastIndexOf('/');
    if (0 <= index)
    {
        // found '/'. path has both dir and filename
        sig_dir = path.left(index);
        sig_file = path.mid(index + 1);
    } // not found '/'. path has only filename
    else
    {
        sig_file = path;
    }
    // remove extension
    index = sig_file.lastIndexOf('.');
    if (0 <= index)
    {
        // found '.', then remove extension
        sig_file = sig_file.left(index);
    }
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler::getCompletedFiles path:" << path << " sig_dir:" << sig_dir << " sig_file:" <<
                                   sig_file;
#else
    QFileInfo const path_info(path);
    QString const sig_dir(path_info.dir().path());
    QString const sig_file(path_info.completeBaseName());
#endif
    QRegularExpression re(sig_file);

    QDirIterator it(sig_dir, QDir::Files);

    /* FIXME: this counts all files with prefix in the storage location, not just captures. DSS analysis files are counted in, for instance. */
    while (it.hasNext())
    {
        QString const fileName = QFileInfo(it.next()).completeBaseName();

        QRegularExpressionMatch match = re.match(fileName);
        if (match.hasMatch())
            seqFileCount++;
    }

    return seqFileCount;
}

void Scheduler::setINDICommunicationStatus(Ekos::CommunicationStatus status)
{
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %d\n", __LINE__, "ekosInterface:indiStatusChanged", status);
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler INDI status is" << status;

    moduleState()->setIndiCommunicationStatus(status);
}

void Scheduler::setEkosCommunicationStatus(Ekos::CommunicationStatus status)
{
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %d\n", __LINE__, "ekosInterface:ekosStatusChanged", status);
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler Ekos status is" << status;

    moduleState()->setEkosCommunicationStatus(status);
}

void Scheduler::simClockScaleChanged(float newScale)
{
    if (currentlySleeping())
    {
        QTime const remainingTimeMs = QTime::fromMSecsSinceStartOfDay(std::lround(static_cast<double>
                                      (moduleState()->iterationTimer().remainingTime())
                                      * KStarsData::Instance()->clock()->scale()
                                      / newScale));
        appendLogText(i18n("Sleeping for %1 on simulation clock update until next observation job is ready...",
                           remainingTimeMs.toString("hh:mm:ss")));
        moduleState()->iterationTimer().stop();
        moduleState()->iterationTimer().start(remainingTimeMs.msecsSinceStartOfDay());
    }
}

void Scheduler::simClockTimeChanged()
{
    calculateDawnDusk();
    updateNightTime();

    // If the Scheduler is not running, reset all jobs and re-evaluate from a new current start point
    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
    {
        startJobEvaluation();
    }
}

void Scheduler::registerNewDevice(const QString &name, int interface)
{
    Q_UNUSED(name)

    if (interface & INDI::BaseDevice::DOME_INTERFACE)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(INDI::BaseDevice::DOME_INTERFACE);
        QDBusReply<QStringList> paths = process()->indiInterface()->callWithArgumentList(QDBus::AutoDetect, "getDevicesPaths",
                                        dbusargs);
        if (paths.error().type() == QDBusError::NoError)
        {
            // Select last device in case a restarted caused multiple instances in the tree
            setDomePathString(paths.value().last());
            delete process()->domeInterface();
            process()->setDomeInterface(new QDBusInterface(kstarsInterfaceString, domePathString, domeInterfaceString,
                                        QDBusConnection::sessionBus(), this));
            connect(process()->domeInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
            checkInterfaceReady(process()->domeInterface());
        }
    }

    if (interface & INDI::BaseDevice::WEATHER_INTERFACE)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(INDI::BaseDevice::WEATHER_INTERFACE);
        QDBusReply<QStringList> paths = process()->indiInterface()->callWithArgumentList(QDBus::AutoDetect, "getDevicesPaths",
                                        dbusargs);
        if (paths.error().type() == QDBusError::NoError)
        {
            // Select last device in case a restarted caused multiple instances in the tree
            setWeatherPathString(paths.value().last());
            delete process()->weatherInterface();
            process()->setWeatherInterface(new QDBusInterface(kstarsInterfaceString, weatherPathString, weatherInterfaceString,
                                           QDBusConnection::sessionBus(), this));
            connect(process()->weatherInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
            connect(process()->weatherInterface(), SIGNAL(newStatus(ISD::Weather::Status)), this,
                    SLOT(setWeatherStatus(ISD::Weather::Status)));
            checkInterfaceReady(process()->weatherInterface());
        }
    }

    if (interface & INDI::BaseDevice::DUSTCAP_INTERFACE)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(INDI::BaseDevice::DUSTCAP_INTERFACE);
        QDBusReply<QStringList> paths = process()->indiInterface()->callWithArgumentList(QDBus::AutoDetect, "getDevicesPaths",
                                        dbusargs);
        if (paths.error().type() == QDBusError::NoError)
        {
            // Select last device in case a restarted caused multiple instances in the tree
            setDustCapPathString(paths.value().last());
            delete process()->capInterface();
            process()->setCapInterface(new QDBusInterface(kstarsInterfaceString, dustCapPathString, dustCapInterfaceString,
                                       QDBusConnection::sessionBus(), this));
            connect(process()->capInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
            checkInterfaceReady(process()->capInterface());
        }
    }
}

void Scheduler::registerNewModule(const QString &name)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Registering new Module (" << name << ")";

    if (name == "Focus")
    {
        delete process()->focusInterface();
        process()->setFocusInterface(new QDBusInterface(kstarsInterfaceString, focusPathString, focusInterfaceString,
                                     QDBusConnection::sessionBus(), this));
        connect(process()->focusInterface(), SIGNAL(newStatus(Ekos::FocusState)), this,
                SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
    }
    else if (name == "Capture")
    {
        delete process()->captureInterface();
        process()->setCaptureInterface(new QDBusInterface(kstarsInterfaceString, capturePathString, captureInterfaceString,
                                       QDBusConnection::sessionBus(), this));

        connect(process()->captureInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(process()->captureInterface(), SIGNAL(newStatus(Ekos::CaptureState)), this,
                SLOT(setCaptureStatus(Ekos::CaptureState)),
                Qt::UniqueConnection);
        connect(process()->captureInterface(), SIGNAL(captureComplete(QVariantMap)), this, SLOT(setCaptureComplete(QVariantMap)),
                Qt::UniqueConnection);
        checkInterfaceReady(process()->captureInterface());
    }
    else if (name == "Mount")
    {
        delete process()->mountInterface();
        process()->setMountInterface(new QDBusInterface(kstarsInterfaceString, mountPathString, mountInterfaceString,
                                     QDBusConnection::sessionBus(), this));

        connect(process()->mountInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(process()->mountInterface(), SIGNAL(newStatus(ISD::Mount::Status)), process(),
                SLOT(process()->setMountStatus(ISD::Mount::Status)),
                Qt::UniqueConnection);

        checkInterfaceReady(process()->mountInterface());
    }
    else if (name == "Align")
    {
        delete process()->alignInterface();
        process()->setAlignInterface(new QDBusInterface(kstarsInterfaceString, alignPathString, alignInterfaceString,
                                     QDBusConnection::sessionBus(), this));
        connect(process()->alignInterface(), SIGNAL(newStatus(Ekos::AlignState)), this, SLOT(setAlignStatus(Ekos::AlignState)),
                Qt::UniqueConnection);
    }
    else if (name == "Guide")
    {
        delete process()->guideInterface();
        process()->setGuideInterface(new QDBusInterface(kstarsInterfaceString, guidePathString, guideInterfaceString,
                                     QDBusConnection::sessionBus(), this));
        connect(process()->guideInterface(), SIGNAL(newStatus(Ekos::GuideState)), this,
                SLOT(setGuideStatus(Ekos::GuideState)), Qt::UniqueConnection);
    }
}

void Scheduler::syncProperties()
{
    QDBusInterface *iface = qobject_cast<QDBusInterface*>(sender());

    if (iface == process()->mountInterface())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "canPark");
        QVariant canMountPark = process()->mountInterface()->property("canPark");
        TEST_PRINT(stderr, "  @@@dbus received %s\n", !canMountPark.isValid() ? "invalid" : (canMountPark.toBool() ? "T" : "F"));

        unparkMountCheck->setEnabled(canMountPark.toBool());
        parkMountCheck->setEnabled(canMountPark.toBool());
        moduleState()->setMountReady(true);
    }
    else if (iface == process()->capInterface())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "dustCapInterface:property", "canPark");
        QVariant canCapPark = process()->capInterface()->property("canPark");
        TEST_PRINT(stderr, "  @@@dbus received %s\n", !canCapPark.isValid() ? "invalid" : (canCapPark.toBool() ? "T" : "F"));

        if (canCapPark.isValid())
        {
            capCheck->setEnabled(canCapPark.toBool());
            uncapCheck->setEnabled(canCapPark.toBool());
            moduleState()->setCapReady(true);
        }
        else
        {
            capCheck->setEnabled(false);
            uncapCheck->setEnabled(false);
        }
    }
    else if (iface == process()->domeInterface())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "canPark");
        QVariant canDomePark = process()->domeInterface()->property("canPark");
        TEST_PRINT(stderr, "  @@@dbus received %s\n", !canDomePark.isValid() ? "invalid" : (canDomePark.toBool() ? "T" : "F"));

        if (canDomePark.isValid())
        {
            parkDomeCheck->setEnabled(canDomePark.toBool());
            unparkDomeCheck->setEnabled(canDomePark.toBool());
            moduleState()->setDomeReady(true);
        }
        else
        {
            parkDomeCheck->setEnabled(false);
            unparkDomeCheck->setEnabled(false);
        }
    }
    else if (iface == process()->weatherInterface())
    {
        QVariant status = process()->weatherInterface()->property("status");
        if (status.isValid())
        {
            weatherCheck->setEnabled(true);
            setWeatherStatus(static_cast<ISD::Weather::Status>(status.toInt()));
        }
        else
            weatherCheck->setEnabled(false);
    }
    else if (iface == process()->captureInterface())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:property", "coolerControl");
        QVariant hasCoolerControl = process()->captureInterface()->property("coolerControl");
        TEST_PRINT(stderr, "  @@@dbus received %s\n",
                   !hasCoolerControl.isValid() ? "invalid" : (hasCoolerControl.toBool() ? "T" : "F"));
        warmCCDCheck->setEnabled(hasCoolerControl.toBool());
        moduleState()->setCaptureReady(true);
    }
}

void Scheduler::checkInterfaceReady(QDBusInterface *iface)
{
    if (iface == process()->mountInterface())
    {
        QVariant canMountPark = process()->mountInterface()->property("canPark");
        if (canMountPark.isValid())
        {
            unparkMountCheck->setEnabled(canMountPark.toBool());
            parkMountCheck->setEnabled(canMountPark.toBool());
            moduleState()->setMountReady(true);
        }
    }
    else if (iface == process()->capInterface())
    {
        QVariant canCapPark = process()->capInterface()->property("canPark");
        if (canCapPark.isValid())
        {
            capCheck->setEnabled(canCapPark.toBool());
            uncapCheck->setEnabled(canCapPark.toBool());
            moduleState()->setCapReady(true);
        }
        else
        {
            capCheck->setEnabled(false);
            uncapCheck->setEnabled(false);
        }
    }
    else if (iface == process()->weatherInterface())
    {
        QVariant status = process()->weatherInterface()->property("status");
        if (status.isValid())
        {
            weatherCheck->setEnabled(true);
            setWeatherStatus(static_cast<ISD::Weather::Status>(status.toInt()));
        }
        else
            weatherCheck->setEnabled(false);
    }
    else if (iface == process()->domeInterface())
    {
        QVariant canDomePark = process()->domeInterface()->property("canPark");
        if (canDomePark.isValid())
        {
            unparkDomeCheck->setEnabled(canDomePark.toBool());
            parkDomeCheck->setEnabled(canDomePark.toBool());
            moduleState()->setDomeReady(true);
        }
    }
    else if (iface == process()->captureInterface())
    {
        QVariant hasCoolerControl = process()->captureInterface()->property("coolerControl");
        if (hasCoolerControl.isValid())
        {
            warmCCDCheck->setEnabled(hasCoolerControl.toBool());
            moduleState()->setCaptureReady(true);
        }
    }
}

void Scheduler::setAlignStatus(AlignState status)
{
    process()->setAlignStatus(status);
}

void Scheduler::setGuideStatus(GuideState status)
{
    process()->setGuideStatus(status);
}

void Scheduler::setCaptureStatus(Ekos::CaptureState status)
{
    TEST_PRINT(stderr, "sch%d @@@setCaptureStatus(%d) %s\n", __LINE__, static_cast<int>(status),
               (activeJob() == nullptr) ? "IGNORED" : "OK");
    if (activeJob() == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Capture State" << Ekos::getCaptureStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == activeJob()->getState())
    {
        QDateTime const now = SchedulerModuleState::getLocalTime();
        if (now < activeJob()->getStartupTime())
            return;
    }

    if (activeJob()->getStage() == SchedulerJob::STAGE_CAPTURING)
    {
        if (status == Ekos::CAPTURE_PROGRESS && (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN))
        {
            // JM 2021.09.20
            // Re-set target coords in align module
            // When capture starts, alignment module automatically rests target coords to mount coords.
            // However, we want to keep align module target synced with the scheduler target and not
            // the mount coord
            const SkyPoint targetCoords = activeJob()->getTargetCoords();
            QList<QVariant> targetArgs;
            targetArgs << targetCoords.ra0().Hours() << targetCoords.dec0().Degrees();
            process()->alignInterface()->callWithArgumentList(QDBus::AutoDetect, "setTargetCoords", targetArgs);
        }
        else if (status == Ekos::CAPTURE_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' failed to capture target.", activeJob()->getName()));

            if (moduleState()->increaseCaptureFailureCount())
            {
                // If capture failed due to guiding error, let's try to restart that
                if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                {
                    // Check if it is guiding related.
                    Ekos::GuideState gStatus = process()->getGuidingStatus();
                    if (gStatus == Ekos::GUIDE_ABORTED ||
                            gStatus == Ekos::GUIDE_CALIBRATION_ERROR ||
                            gStatus == GUIDE_DITHERING_ERROR)
                    {
                        appendLogText(i18n("Job '%1' is capturing, is restarting its guiding procedure (attempt #%2 of %3).",
                                           activeJob()->getName(),
                                           moduleState()->captureFailureCount(), moduleState()->maxFailureAttempts()));
                        process()->startGuiding(true);
                        return;
                    }
                }

                /* FIXME: it's not clear whether it is actually possible to continue capturing when capture fails this way */
                appendLogText(i18n("Warning: job '%1' failed its capture procedure, restarting capture.", activeJob()->getName()));
                process()->startCapture(true);
            }
            else
            {
                /* FIXME: it's not clear whether this situation can be recovered at all */
                appendLogText(i18n("Warning: job '%1' failed its capture procedure, marking aborted.", activeJob()->getName()));
                activeJob()->setState(SchedulerJob::JOB_ABORTED);

                findNextJob();
            }
        }
        else if (status == Ekos::CAPTURE_COMPLETE)
        {
            KSNotification::event(QLatin1String("EkosScheduledImagingFinished"),
                                  i18n("Ekos job (%1) - Capture finished", activeJob()->getName()), KSNotification::Scheduler);

            activeJob()->setState(SchedulerJob::JOB_COMPLETE);
            findNextJob();
        }
        else if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
        {
            // We received a new image, but we don't know precisely where so update the storage map and re-estimate job times.
            // FIXME: rework this once capture storage is reworked
            if (Options::rememberJobProgress())
            {
                updateCompletedJobsCount(true);

                for (const auto &job : moduleState()->jobs())
                    estimateJobTime(job, moduleState()->capturedFramesCount(), this);
            }
            // Else if we don't remember the progress on jobs, increase the completed count for the current job only - no cross-checks
            else
                activeJob()->setCompletedCount(activeJob()->getCompletedCount() + 1);

            moduleState()->resetCaptureFailureCount();
        }
    }
}

void Scheduler::setFocusStatus(FocusState status)
{
    process()->setFocusStatus(status);
}

void Scheduler::setMountStatus(ISD::Mount::Status status)
{
    process()->setMountStatus(status);
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

    if (newStatus != moduleState()->weatherStatus())
    {
        moduleState()->setWeatherStatus(newStatus);

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

        appendLogText(statusString);

        emit weatherChanged(moduleState()->weatherStatus());
    }

    // Shutdown scheduler if it was started and not already in shutdown
    // and if weather checkbox is checked.
    if (weatherCheck->isChecked() && moduleState()->weatherStatus() == ISD::Weather::WEATHER_ALERT
            && moduleState()->schedulerState() != Ekos::SCHEDULER_IDLE
            && moduleState()->schedulerState() != Ekos::SCHEDULER_SHUTDOWN)
    {
        appendLogText(i18n("Starting shutdown procedure due to severe weather."));
        if (activeJob())
        {
            activeJob()->setState(SchedulerJob::JOB_ABORTED);
            stopCurrentJobAction();
        }
        checkShutdownState();
    }
}

bool Scheduler::shouldSchedulerSleep(SchedulerJob *job)
{
    Q_ASSERT_X(nullptr != job, __FUNCTION__,
               "There must be a valid current job for Scheduler to test sleep requirement");

    if (job->getLightFramesRequired() == false)
        return false;

    QDateTime const now = SchedulerModuleState::getLocalTime();
    int const nextObservationTime = now.secsTo(job->getStartupTime());

    // If start up procedure is complete and the user selected pre-emptive shutdown, let us check if the next observation time exceed
    // the pre-emptive shutdown time in hours (default 2). If it exceeds that, we perform complete shutdown until next job is ready
    if (moduleState()->startupState() == STARTUP_COMPLETE &&
            Options::preemptiveShutdown() &&
            nextObservationTime > (Options::preemptiveShutdownTime() * 3600))
    {
        appendLogText(i18n(
                          "Job '%1' scheduled for execution at %2. "
                          "Observatory scheduled for shutdown until next job is ready.",
                          job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
        moduleState()->enablePreemptiveShutdown(job->getStartupTime());
        weatherCheck->setEnabled(false);
        weatherLabel->hide();
        checkShutdownState();
        return true;
    }
    // Otherwise, sleep until job is ready
    /* FIXME: if not parking, stop tracking maybe? this would prevent crashes or scheduler stops from leaving the mount to track and bump the pier */
    // If start up procedure is already complete, and we didn't issue any parking commands before and parking is checked and enabled
    // Then we park the mount until next job is ready. But only if the job uses TRACK as its first step, otherwise we cannot get into position again.
    // This is also only performed if next job is due more than the default lead time (5 minutes).
    // If job is due sooner than that is not worth parking and we simply go into sleep or wait modes.
    else if (nextObservationTime > Options::leadTime() * 60 &&
             moduleState()->startupState() == STARTUP_COMPLETE &&
             moduleState()->parkWaitState() == PARKWAIT_IDLE &&
             (job->getStepPipeline() & SchedulerJob::USE_TRACK) &&
             parkMountCheck->isEnabled() &&
             parkMountCheck->isChecked())
    {
        appendLogText(i18n(
                          "Job '%1' scheduled for execution at %2. "
                          "Parking the mount until the job is ready.",
                          job->getName(), job->getStartupTime().toString()));

        moduleState()->setParkWaitState(PARKWAIT_PARK);

        return false;
    }
    else if (nextObservationTime > Options::leadTime() * 60)
    {
        appendLogText(i18n("Sleeping until observation job %1 is ready at %2...", job->getName(),
                           now.addSecs(nextObservationTime + 1).toString()));
        sleepLabel->setToolTip(i18n("Scheduler is in sleep mode"));
        sleepLabel->show();

        // Warn the user if the next job is really far away - 60/5 = 12 times the lead time
        if (nextObservationTime > Options::leadTime() * 60 * 12 && !Options::preemptiveShutdown())
        {
            dms delay(static_cast<double>(nextObservationTime * 15.0 / 3600.0));
            appendLogText(i18n(
                              "Warning: Job '%1' is %2 away from now, you may want to enable Preemptive Shutdown.",
                              job->getName(), delay.toHMSString()));
        }

        /* FIXME: stop tracking now */

        // Wake up when job is due.
        // FIXME: Implement waking up periodically before job is due for weather check.
        // int const nextWakeup = nextObservationTime < 60 ? nextObservationTime : 60;
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_WAKEUP).toLatin1().data());
        moduleState()->setupNextIteration(RUN_WAKEUP,
                                          std::lround(((nextObservationTime + 1) * 1000) / KStarsData::Instance()->clock()->scale()));

        return true;
    }

    return false;
}

void Scheduler::setCaptureComplete(const QVariantMap &metadata)
{
    if (activeJob() &&
            activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN &&
            metadata["type"].toInt() == FRAME_LIGHT &&
            Options::alignCheckFrequency() > 0 &&
            ++m_SolverIteration >= Options::alignCheckFrequency())
    {
        m_SolverIteration = 0;

        auto filename = metadata["filename"].toString();
        auto exposure = metadata["exposure"].toDouble();

        constexpr double minSolverSeconds = 5.0;
        double solverTimeout = std::max(exposure - 2, minSolverSeconds);
        if (solverTimeout >= minSolverSeconds)
        {
            auto profiles = getDefaultAlignOptionsProfiles();
            auto parameters = profiles.at(Options::solveOptionsProfile());
            // Double search radius
            parameters.search_radius = parameters.search_radius * 2;
            m_Solver.reset(new SolverUtils(parameters, solverTimeout),  &QObject::deleteLater);
            connect(m_Solver.get(), &SolverUtils::done, this, &Ekos::Scheduler::solverDone, Qt::UniqueConnection);
            //connect(m_Solver.get(), &SolverUtils::newLog, this, &Ekos::Scheduler::appendLogText, Qt::UniqueConnection);

            auto width = metadata["width"].toUInt();
            auto height = metadata["height"].toUInt();

            auto lowScale = Options::astrometryImageScaleLow();
            auto highScale = Options::astrometryImageScaleHigh();

            // solver utils uses arcsecs per pixel only
            if (Options::astrometryImageScaleUnits() == SSolver::DEG_WIDTH)
            {
                lowScale = (lowScale * 3600) / std::max(width, height);
                highScale = (highScale * 3600) / std::min(width, height);
            }
            else if (Options::astrometryImageScaleUnits() == SSolver::ARCMIN_WIDTH)
            {
                lowScale = (lowScale * 60) / std::max(width, height);
                highScale = (highScale * 60) / std::min(width, height);
            }

            m_Solver->useScale(Options::astrometryUseImageScale(), lowScale, highScale);
            m_Solver->usePosition(Options::astrometryUsePosition(), activeJob()->getTargetCoords().ra().Degrees(),
                                  activeJob()->getTargetCoords().dec().Degrees());
            m_Solver->setHealpix(moduleState()->indexToUse(), moduleState()->healpixToUse());
            m_Solver->runSolver(filename);
        }
    }
}

void Scheduler::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &Ekos::Scheduler::solverDone);

    if (!activeJob())
        return;

    QString healpixString = "";
    if (moduleState()->indexToUse() != -1 || moduleState()->healpixToUse() != -1)
        healpixString = QString("Healpix %1 Index %2").arg(moduleState()->healpixToUse()).arg(moduleState()->indexToUse());

    if (timedOut || !success)
    {
        // Don't use the previous index and healpix next time we solve.
        moduleState()->setIndexToUse(-1);
        moduleState()->setHealpixToUse(-1);
    }
    else
    {
        int index, healpix;
        // Get the index and healpix from the successful solve.
        m_Solver->getSolutionHealpix(&index, &healpix);
        moduleState()->setIndexToUse(index);
        moduleState()->setHealpixToUse(healpix);
    }

    if (timedOut)
        appendLogText(i18n("Solver timed out: %1s %2", QString("%L1").arg(elapsedSeconds, 0, 'f', 1), healpixString));
    else if (!success)
        appendLogText(i18n("Solver failed: %1s %2", QString("%L1").arg(elapsedSeconds, 0, 'f', 1), healpixString));
    else
    {
        const double ra = solution.ra;
        const double dec = solution.dec;

        const auto target = activeJob()->getTargetCoords();

        SkyPoint alignCoord;
        alignCoord.setRA0(ra / 15.0);
        alignCoord.setDec0(dec);
        alignCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        alignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        const double diffRa = (alignCoord.ra().deltaAngle(target.ra())).Degrees() * 3600;
        const double diffDec = (alignCoord.dec().deltaAngle(target.dec())).Degrees() * 3600;

        // This is an approximation, probably ok for small angles.
        const double diffTotal = hypot(diffRa, diffDec);

        // Note--the RA output is in DMS. This is because we're looking at differences in arcseconds
        // and HMS coordinates are misleading (one HMS second is really 6 arc-seconds).
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                       QString("Target Distance: %1\" Target (RA: %2 DE: %3) Current (RA: %4 DE: %5) %6 solved in %7s")
                                       .arg(QString("%L1").arg(diffTotal, 0, 'f', 0),
                                            target.ra().toDMSString(),
                                            target.dec().toDMSString(),
                                            alignCoord.ra().toDMSString(),
                                            alignCoord.dec().toDMSString(),
                                            healpixString,
                                            QString("%L1").arg(elapsedSeconds, 0, 'f', 2));
        emit targetDistance(diffTotal);

        // If we exceed align check threshold, we abort and re-align.
        if (diffTotal / 60 > Options::alignCheckThreshold())
        {
            appendLogText(i18n("Captured frame is %1 arcminutes away from target, re-aligning...", QString::number(diffTotal / 60.0,
                               'f', 1)));
            stopCurrentJobAction();
            process()->startAstrometry();
        }
    }
}

QJsonArray Scheduler::getJSONJobs()
{
    QJsonArray jobArray;

    for (const auto &oneJob : moduleState()->jobs())
        jobArray.append(oneJob->toJson());

    return jobArray;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setPrimarySettings(const QJsonObject &settings)
{
    syncControl(settings, "target", nameEdit);
    syncControl(settings, "group", groupEdit);
    syncControl(settings, "ra", raBox);
    syncControl(settings, "dec", decBox);
    syncControl(settings, "pa", positionAngleSpin);
    if (settings.contains("sequence"))
    {
        syncControl(settings, "sequence", sequenceEdit);
        setSequence(settings["sequence"].toString());
    }
    syncControl(settings, "fits", fitsEdit);
    syncControl(settings, "profile", schedulerProfileCombo);
    syncControl(settings, "track", trackStepCheck);
    syncControl(settings, "focus", focusStepCheck);
    syncControl(settings, "align", alignStepCheck);
    syncControl(settings, "guide", guideStepCheck);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setJobStartupConditions(const QJsonObject &settings)
{
    syncControl(settings, "asapCheck", asapConditionR);
    syncControl(settings, "startupTimeCheck", startupTimeConditionR);
    syncControl(settings, "scheduledStartTime", startupTimeEdit);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setJobConstraints(const QJsonObject &settings)
{
    syncControl(settings, "alt", altConstraintCheck);
    syncControl(settings, "moon", moonSeparationCheck);
    syncControl(settings, "weather", weatherCheck);
    syncControl(settings, "scheduledStartTime", startupTimeEdit);
    syncControl(settings, "twilight", twilightCheck);
    syncControl(settings, "artifHorizon", artificialHorizonCheck);
    syncControl(settings, "altValue", minAltitude);
    syncControl(settings, "moonValue", minMoonSeparation);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setJobCompletionConditions(const QJsonObject &settings)
{
    syncControl(settings, "sequenceCheck", sequenceCompletionR);
    syncControl(settings, "repeatCheck", repeatCompletionR);
    syncControl(settings, "loopCheck", loopCompletionR);
    syncControl(settings, "timeCheck", timeCompletionR);
    syncControl(settings, "repeatRuns", repeatsSpin);
    syncControl(settings, "scheduledStartTime", completionTimeEdit);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setObservatoryStartupProcedure(const QJsonObject &settings)
{
    syncControl(settings, "unparkDome", unparkDomeCheck);
    syncControl(settings, "unparkMount", unparkMountCheck);
    syncControl(settings, "uncap", uncapCheck);
    syncControl(settings, "script", startupScript);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setAbortedJobManagementSettings(const QJsonObject &settings)
{
    syncControl(settings, "rescheduleError", errorHandlingRescheduleErrorsCB);
    syncControl(settings, "rescheduleErrorTime", errorHandlingDelaySB);
    syncControl(settings, "noneCheck", errorHandlingDontRestartButton);
    syncControl(settings, "queueCheck", errorHandlingRestartQueueButton);
    syncControl(settings, "immediateCheck", errorHandlingRestartImmediatelyButton);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setObservatoryShutdownProcedure(const QJsonObject &settings)
{
    syncControl(settings, "warmCCD", warmCCDCheck);
    syncControl(settings, "cap", capCheck);
    syncControl(settings, "parkMount", parkMountCheck);
    syncControl(settings, "parkDome", parkDomeCheck);
    syncControl(settings, "script", shutdownScript);
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Scheduler::syncControl(const QJsonObject &settings, const QString &key, QWidget * widget)
{
    if (settings.contains(key) == false)
        return false;

    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QLineEdit *pLE = nullptr;
    QRadioButton *pRB = nullptr;
    QDateTimeEdit *pDT = nullptr;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(pSB->value());
        if (value != pSB->value())
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(pDSB->value());
        if (value != pDSB->value())
        {
            pDSB->setValue(value);
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool(pCB->isChecked());
        if (value != pCB->isChecked())
        {
            pCB->setChecked(value);
            return true;
        }
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString(pComboBox->currentText());
        if (value != pComboBox->currentText())
        {
            pComboBox->setCurrentText(value);
            return true;
        }
    }
    else if ((pLE = qobject_cast<QLineEdit *>(widget)))
    {
        const QString value = settings[key].toString(pLE->text());
        if (value != pLE->text())
        {
            pLE->setText(value);
            return true;
        }
    }
    else if ((pRB = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool(pRB->isChecked());
        if (value != pRB->isChecked())
        {
            pRB->setChecked(value);
            return true;
        }
    }
    else if ((pDT = qobject_cast<QDateTimeEdit *>(widget)))
    {
        // TODO Need 64bit integer.
        const auto value = settings[key].toDouble();
        if (std::abs(value - pDT->dateTime().currentSecsSinceEpoch()) > 0)
        {
            pDT->setDateTime(QDateTime::fromSecsSinceEpoch(value));
            return true;
        }
    }

    return false;
};

QJsonObject Scheduler::getSchedulerSettings()
{
    QJsonObject jobStartupSettings =
    {
        {"asap", asapConditionR->isChecked()},
        {"startupTimeConditionR", startupTimeConditionR->isChecked()},
        {"startupTimeEdit", startupTimeEdit->text()},
    };

    QJsonObject jobConstraintSettings =
    {
        {"altConstraintCheck", altConstraintCheck->isChecked()},
        {"minAltitude", minAltitude->value()},
        {"moonSeparationCheck", moonSeparationCheck->isChecked()},
        {"minMoonSeparation", minMoonSeparation->value()},
        {"weatherCheck", weatherCheck->isChecked()},
        {"twilightCheck", twilightCheck->isChecked()},
        {"nightTime", nightTime->text()},
        {"artificialHorizonCheck", artificialHorizonCheck->isChecked()}
    };

    QJsonObject jobCompletionSettings =
    {
        {"sequenceCompletionR", sequenceCompletionR->isChecked()},
        {"repeatCompletionR", repeatCompletionR->isChecked()},
        {"repeatsSpin", repeatsSpin->value()},
        {"loopCompletionR", loopCompletionR->isChecked()},
        {"timeCompletionR", timeCompletionR->isChecked()}
    };

    QJsonObject observatoryStartupSettings =
    {
        {"unparkDomeCheck", unparkDomeCheck->isChecked()},
        {"unparkMountCheck", unparkMountCheck->isChecked()},
        {"uncapCheck", uncapCheck->isChecked()},
        {"startupScript", startupScript->text()}
    };
    QJsonObject abortJobSettings =
    {
        {"none", errorHandlingDontRestartButton->isChecked() },
        {"queue", errorHandlingRestartQueueButton->isChecked()},
        {"immediate", errorHandlingRestartImmediatelyButton->isChecked()},
        {"rescheduleCheck", errorHandlingRescheduleErrorsCB->isChecked()},
        {"errorHandlingDelaySB", errorHandlingDelaySB->value()}

    };
    QJsonObject shutdownSettings =
    {
        {"warmCCDCheck", warmCCDCheck->isChecked()},
        {"capCheck", capCheck->isChecked()},
        {"parkMountCheck", parkMountCheck->isChecked()},
        {"parkDomeCheck", parkDomeCheck->isChecked()},
        {"shutdownScript", shutdownScript->text()}
    };

    QJsonObject schedulerSettings =
    {
        {"algorithm", ALGORITHM_GREEDY},
        {"state", moduleState()->schedulerState()},
        {"trackStepCheck", trackStepCheck->isChecked()},
        {"focusStepCheck", focusStepCheck->isChecked()},
        {"alignStepCheck", alignStepCheck->isChecked()},
        {"guideStepCheck", guideStepCheck->isChecked()},
        {"pa", positionAngleSpin->value()},
        {"sequence", sequenceEdit->text()},
        {"fits", fitsEdit->text()},
        {"profile", profile()},
        {"jobStartup", jobStartupSettings},
        {"jobConstraint", jobConstraintSettings},
        {"jobCompletion", jobCompletionSettings},
        {"observatoryStartup", observatoryStartupSettings},
        {"abortJob", abortJobSettings},
        {"shutdownProcedure", shutdownSettings}
    };
    return schedulerSettings;

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
            appendLogText(i18n("Manual startup procedure completed successfully."));
            break;
        case STARTUP_ERROR:
            startupB->setIcon(QIcon::fromTheme("media-playback-start"));
            appendLogText(i18n("Manual startup procedure terminated due to errors."));
            break;
        default:
            // in all other cases startup is running
            startupB->setIcon(QIcon::fromTheme("media-playback-stop"));
            break;
    }
}
void Scheduler::shutdownStateChanged(ShutdownState state)
{
    jobStatus->setText(shutdownStateString(state));
    if (state == SHUTDOWN_COMPLETE || state == SHUTDOWN_IDLE
            || state == SHUTDOWN_ERROR)
        shutdownB->setIcon(QIcon::fromTheme("media-playback-start"));
    else
        shutdownB->setIcon(QIcon::fromTheme("media-playback-stop"));
}
void Scheduler::ekosStateChanged(EkosState state)
{
    jobStatus->setText(ekosStateString(state));
}
void Scheduler::indiStateChanged(INDIState state)
{
    jobStatus->setText(indiStateString(state));
}
void Scheduler::parkWaitStateChanged(ParkWaitState state)
{
    jobStatus->setText(parkWaitStateString(state));
}

SchedulerJob *Scheduler::activeJob()
{
    return moduleState()->activeJob();
}

Ekos::SchedulerState Scheduler::status()
{
    return moduleState()->schedulerState();
}

bool Scheduler::saveScheduler(const QUrl &fileURL)
{
    return process()->saveScheduler(fileURL);
}
}
