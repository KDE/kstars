/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project:
    SPDX-FileCopyrightText: 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scheduler.h"

#include "ksalmanac.h"
#include "ksnotification.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "mosaic.h"
#include "Options.h"
#include "scheduleradaptor.h"
#include "schedulerjob.h"
#include "skymapcomposite.h"
#include "auxiliary/QProgressIndicator.h"
#include "dialogs/finddialog.h"
#include "ekos/manager.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/capture/placeholderpath.h"
#include "skyobjects/starobject.h"

#include <KNotifications/KNotification>
#include <KConfigDialog>

#include <fitsio.h>
#include <ekos_scheduler_debug.h>
#include <indicom.h>

#define BAD_SCORE                -1000
#define MAX_FAILURE_ATTEMPTS      5
#define RESTART_GUIDING_DELAY_MS  5000

#define DEFAULT_CULMINATION_TIME    -60
#define DEFAULT_MIN_ALTITUDE        15
#define DEFAULT_MIN_MOON_SEPARATION 0

// This is a temporary debugging printout introduced while gaining experience developing
// the unit tests in test_ekos_scheduler_ops.cpp.
// All these printouts should be eventually removed.
#define TEST_PRINT if (false) fprintf

namespace Ekos
{

// Functions to make human-readable debug messages for the various enums.

QString timerStr(Scheduler::SchedulerTimerState state)
{
    switch (state)
    {
        case Scheduler::RUN_WAKEUP:
            return QString("RUN_WAKEUP");
        case Scheduler::RUN_SCHEDULER:
            return QString("RUN_SCHEDULER");
        case Scheduler::RUN_JOBCHECK:
            return QString("RUN_JOBCHECK");
        case Scheduler::RUN_SHUTDOWN:
            return QString("RUN_SHUTDOWN");
        case Scheduler::RUN_NOTHING:
            return QString("RUN_NOTHING");
    }
    return QString("????");
}

QString jobStateString(SchedulerJob::JOBStatus state)
{
    switch(state)
    {
        case SchedulerJob::JOB_IDLE:
            return "JOB_IDLE";
        case SchedulerJob::JOB_EVALUATION:
            return "JOB_EVALUATION";
        case SchedulerJob::JOB_SCHEDULED:
            return "JOB_SCHEDULED";
        case SchedulerJob::JOB_BUSY:
            return "JOB_BUSY";
        case SchedulerJob::JOB_ERROR:
            return "JOB_ERROR";
        case SchedulerJob::JOB_ABORTED:
            return "JOB_ABORTED";
        case SchedulerJob::JOB_INVALID:
            return "JOB_INVALID";
        case SchedulerJob::JOB_COMPLETE:
            return "JOB_COMPLETE";
    }
    return QString("????");
}

QString jobStageString(SchedulerJob::JOBStage state)
{
    switch(state)
    {
        case SchedulerJob::STAGE_IDLE:
            return "STAGE_IDLE";
        case SchedulerJob::STAGE_SLEWING:
            return "STAGE_SLEWING";
        case SchedulerJob::STAGE_SLEW_COMPLETE:
            return "STAGE_SLEW_COMPLETE";
        case SchedulerJob::STAGE_FOCUSING:
            return "STAGE_FOCUSING";
        case SchedulerJob::STAGE_FOCUS_COMPLETE:
            return "STAGE_FOCUS_COMPLETE";
        case SchedulerJob::STAGE_ALIGNING:
            return "STAGE_ALIGNING";
        case SchedulerJob::STAGE_ALIGN_COMPLETE:
            return "STAGE_ALIGN_COMPLETE";
        case SchedulerJob::STAGE_RESLEWING:
            return "STAGE_RESLEWING";
        case SchedulerJob::STAGE_RESLEWING_COMPLETE:
            return "STAGE_RESLEWING_COMPLETE";
        case SchedulerJob::STAGE_POSTALIGN_FOCUSING:
            return "STAGE_POSTALIGN_FOCUSING";
        case SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE:
            return "STAGE_POSTALIGN_FOCUSING_COMPLETE";
        case SchedulerJob::STAGE_GUIDING:
            return "STAGE_GUIDING";
        case SchedulerJob::STAGE_GUIDING_COMPLETE:
            return "STAGE_GUIDING_COMPLETE";
        case SchedulerJob::STAGE_CAPTURING:
            return "STAGE_CAPTURING";
        case SchedulerJob::STAGE_COMPLETE:
            return "STAGE_COMPLETE";
    }
    return QString("????");
}

QString ekosStateString(Scheduler::EkosState state)
{
    switch(state)
    {
        case Scheduler::EKOS_IDLE:
            return "EKOS_IDLE";
        case Scheduler::EKOS_STARTING:
            return "EKOS_STARTING";
        case Scheduler::EKOS_STOPPING:
            return "EKOS_STOPPING";
        case Scheduler::EKOS_READY:
            return "EKOS_READY";
    }
    return QString("????");
}

QString indiStateString(Scheduler::INDIState state)
{
    switch(state)
    {
        case Scheduler::INDI_IDLE:
            return "INDI_IDLE";
        case Scheduler::INDI_PROPERTY_CHECK:
            return "INDI_PROPERTY_CHECK";
        case Scheduler::INDI_CONNECTING:
            return "INDI_CONNECTING";
        case Scheduler::INDI_DISCONNECTING:
            return "INDI_DISCONNECTING";
        case Scheduler::INDI_READY:
            return "INDI_READY";
    }
    return QString("????");
}

QString startupStateString(Scheduler::StartupState state)
{
    switch(state)
    {
        case Scheduler::STARTUP_IDLE:
            return "STARTUP_IDLE";
        case Scheduler::STARTUP_SCRIPT:
            return "STARTUP_SCRIPT";
        case Scheduler::STARTUP_UNPARK_DOME:
            return "STARTUP_UNPARK_DOME";
        case Scheduler::STARTUP_UNPARKING_DOME:
            return "STARTUP_UNPARKING_DOME";
        case Scheduler::STARTUP_UNPARK_MOUNT:
            return "STARTUP_UNPARK_MOUNT";
        case Scheduler::STARTUP_UNPARKING_MOUNT:
            return "STARTUP_UNPARKING_MOUNT";
        case Scheduler::STARTUP_UNPARK_CAP:
            return "STARTUP_UNPARK_CAP";
        case Scheduler::STARTUP_UNPARKING_CAP:
            return "STARTUP_UNPARKING_CAP";
        case Scheduler::STARTUP_ERROR:
            return "STARTUP_ERROR";
        case Scheduler::STARTUP_COMPLETE:
            return "STARTUP_COMPLETE";
    }
    return QString("????");
}

QString shutdownStateString(Scheduler::ShutdownState state)
{
    switch(state)
    {
        case Scheduler::SHUTDOWN_IDLE:
            return "SHUTDOWN_IDLE";
        case Scheduler::SHUTDOWN_PARK_CAP:
            return "SHUTDOWN_PARK_CAP";
        case Scheduler::SHUTDOWN_PARKING_CAP:
            return "SHUTDOWN_PARKING_CAP";
        case Scheduler::SHUTDOWN_PARK_MOUNT:
            return "SHUTDOWN_PARK_MOUNT";
        case Scheduler::SHUTDOWN_PARKING_MOUNT:
            return "SHUTDOWN_PARKING_MOUNT";
        case Scheduler::SHUTDOWN_PARK_DOME:
            return "SHUTDOWN_PARK_DOME";
        case Scheduler::SHUTDOWN_PARKING_DOME:
            return "SHUTDOWN_PARKING_DOME";
        case Scheduler::SHUTDOWN_SCRIPT:
            return "SHUTDOWN_SCRIPT";
        case Scheduler::SHUTDOWN_SCRIPT_RUNNING:
            return "SHUTDOWN_SCRIPT_RUNNING";
        case Scheduler::SHUTDOWN_ERROR:
            return "SHUTDOWN_ERROR";
        case Scheduler::SHUTDOWN_COMPLETE:
            return "SHUTDOWN_COMPLETE";
    }
    return QString("????");
}

QString parkWaitStateString(Scheduler::ParkWaitStatus state)
{
    switch(state)
    {
        case Scheduler::PARKWAIT_IDLE:
            return "PARKWAIT_IDLE";
        case Scheduler::PARKWAIT_PARK:
            return "PARKWAIT_PARK";
        case Scheduler::PARKWAIT_PARKING:
            return "PARKWAIT_PARKING";
        case Scheduler::PARKWAIT_PARKED:
            return "PARKWAIT_PARKED";
        case Scheduler::PARKWAIT_UNPARK:
            return "PARKWAIT_UNPARK";
        case Scheduler::PARKWAIT_UNPARKING:
            return "PARKWAIT_UNPARKING";
        case Scheduler::PARKWAIT_UNPARKED:
            return "PARKWAIT_UNPARKED";
        case Scheduler::PARKWAIT_ERROR:
            return "PARKWAIT_ERROR";
    }
    return QString("????");
}

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
               .arg(timerStr(timerState))
               .arg(schedulerStateString(state))
               .arg((timerState == Scheduler::RUN_JOBCHECK && currentJob != nullptr) ?
                    QString("(%1 %2)").arg(jobStateString(currentJob->getState()))
                    .arg(jobStageString(currentJob->getStage())) : "")
               .arg(ekosStateString(ekosState))
               .arg(indiStateString(indiState))
               .arg(startupStateString(startupState))
               .arg(shutdownStateString(shutdownState))
               .arg(parkWaitStateString(parkWaitState)).toLatin1().data());
}

QDateTime Scheduler::Dawn, Scheduler::Dusk, Scheduler::preDawnDateTime;

// Allows for unit testing of static Scheduler methods,
// as can't call KStarsData::Instance() during unit testing.
KStarsDateTime *Scheduler::storedLocalTime = nullptr;
KStarsDateTime Scheduler::getLocalTime()
{
    if (hasLocalTime())
        return *storedLocalTime;
    return KStarsData::Instance()->geo()->UTtoLT(KStarsData::Instance()->clock()->utc());
}

// This is the initial conditions that need to be set before starting.
void Scheduler::init()
{
    // This is needed to get wakeupScheduler() to call start() and startup,
    // instead of assuming it is already initialized (if preemptiveShutdown was not set).
    // The time itself is not used.
    enablePreemptiveShutdown(getLocalTime());

    iterationSetup = false;
    setupNextIteration(RUN_WAKEUP, 10);
}

// Setup the main loop and run.
void Scheduler::run()
{
    init();
    iterate();
}

// This is the main scheduler loop.
// Run an iteration, get the sleep time, sleep for that interval, and repeat.
void Scheduler::iterate()
{
    const int msSleep = runSchedulerIteration();
    if (msSleep < 0)
        return;

    connect(&iterationTimer, &QTimer::timeout, this, &Scheduler::iterate, Qt::UniqueConnection);
    iterationTimer.setSingleShot(true);
    iterationTimer.start(msSleep);
}

bool Scheduler::currentlySleeping()
{
    return iterationTimer.isActive() && timerState == RUN_WAKEUP;
}

int Scheduler::runSchedulerIteration()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (startMSecs == 0)
        startMSecs = now;

    printStates(QString("\nrunScheduler Iteration %1 @ %2")
                .arg(++schedulerIteration)
                .arg((now - startMSecs) / 1000.0, 1, 'f', 3));

    SchedulerTimerState keepTimerState = timerState;

    // TODO: At some point we should require that timerState and timerInterval
    // be explicitly set in all iterations. Not there yet, would require too much
    // refactoring of the scheduler. When we get there, we'd exectute the following here:
    // timerState = RUN_NOTHING;    // don't like this comment, it should always set a state and interval!
    // timerInterval = -1;
    iterationSetup = false;
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
            timerInterval = -1;
            break;
    }
    if (!iterationSetup)
    {
        // See the above TODO.
        // Since iterations aren't yet always set up, we repeat the current
        // iteration type if one wasn't set up in the current iteration.
        // qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler iteration never set up.";
        TEST_PRINT(stderr, "Scheduler iteration never set up--repeating %s %d...\n",
                   timerStr(timerState).toLatin1().data(), timerInterval);
    }
    printStates(QString("End iteration, sleep %1: ").arg(timerInterval));
    return timerInterval;
}
void Scheduler::setupNextIteration(SchedulerTimerState nextState)
{
    setupNextIteration(nextState, m_UpdatePeriodMs);
}

void Scheduler::setupNextIteration(SchedulerTimerState nextState, int milliseconds)
{
    if (iterationSetup)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER)
                << QString("Multiple setupNextIteration calls: current %1 %2, previous %3 %4")
                .arg(nextState).arg(milliseconds).arg(timerState).arg(timerInterval);
        TEST_PRINT(stderr, "Multiple setupNextIteration calls: current %s %d, previous %s %d.\n",
                   timerStr(nextState).toLatin1().data(), milliseconds,
                   timerStr(timerState).toLatin1().data(), timerInterval);
    }
    timerState = nextState;
    timerInterval = milliseconds;
    iterationSetup = true;
}

Scheduler::Scheduler()
{
    // Use the default path and interface when running the scheduler.
    setupScheduler(ekosPathString, ekosInterfaceString);
}

Scheduler::Scheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr)
{
    // During testing, when mocking ekos, use a special purpose path and interface.
    setupScheduler(ekosPathStr, ekosInterfaceStr);
}

void Scheduler::setupScheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr)
{
    setupUi(this);

    qRegisterMetaType<Ekos::SchedulerState>("Ekos::SchedulerState");
    qDBusRegisterMetaType<Ekos::SchedulerState>();

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    // Get current KStars time and set seconds to zero
    QDateTime currentDateTime = getLocalTime();
    QTime currentTime         = currentDateTime.time();
    currentTime.setHMS(currentTime.hour(), currentTime.minute(), 0);
    currentDateTime.setTime(currentTime);

    // Set initial time for startup and completion times
    startupTimeEdit->setDateTime(currentDateTime);
    completionTimeEdit->setDateTime(currentDateTime);

    // Set up DBus interfaces
    new SchedulerAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject("/KStars/Ekos/Scheduler");
    if (!QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler", this))
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Scheduler failed to register with dbus");
    ekosInterface = new QDBusInterface("org.kde.kstars", ekosPathStr, ekosInterfaceStr,
                                       QDBusConnection::sessionBus(), this);

    // Example of connecting DBus signals
    //connect(ekosInterface, SIGNAL(indiStatusChanged(Ekos::CommunicationStatus)), this, SLOT(setINDICommunicationStatus(Ekos::CommunicationStatus)));
    //connect(ekosInterface, SIGNAL(ekosStatusChanged(Ekos::CommunicationStatus)), this, SLOT(setEkosCommunicationStatus(Ekos::CommunicationStatus)));
    //connect(ekosInterface, SIGNAL(newModule(QString)), this, SLOT(registerNewModule(QString)));
    QDBusConnection::sessionBus().connect("org.kde.kstars", ekosPathStr, ekosInterfaceStr, "newModule", this,
                                          SLOT(registerNewModule(QString)));
    QDBusConnection::sessionBus().connect("org.kde.kstars", ekosPathStr, ekosInterfaceStr, "indiStatusChanged",
                                          this, SLOT(setINDICommunicationStatus(Ekos::CommunicationStatus)));
    QDBusConnection::sessionBus().connect("org.kde.kstars", ekosPathStr, ekosInterfaceStr, "ekosStatusChanged",
                                          this, SLOT(setEkosCommunicationStatus(Ekos::CommunicationStatus)));

    sleepLabel->setPixmap(
        QIcon::fromTheme("chronometer").pixmap(QSize(32, 32)));
    sleepLabel->hide();

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi, 0, nullptr);

    geo = KStarsData::Instance()->geo();

    raBox->setDegType(false); //RA box should be HMS-style

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
    queueUpB->setToolTip(i18n("Move selected job one line up in the list.\n"
                              "Order only affect observation jobs that are scheduled to start at the same time.\n"
                              "Not available if option \"Sort jobs by Altitude and Priority\" is set."));
    queueUpB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueDownB->setIcon(QIcon::fromTheme("go-down"));
    queueDownB->setToolTip(i18n("Move selected job one line down in the list.\n"
                                "Order only affect observation jobs that are scheduled to start at the same time.\n"
                                "Not available if option \"Sort jobs by Altitude and Priority\" is set."));
    queueDownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    evaluateOnlyB->setIcon(QIcon::fromTheme("system-reboot"));
    evaluateOnlyB->setToolTip(i18n("Reset state and force reevaluation of all observation jobs."));
    evaluateOnlyB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sortJobsB->setIcon(QIcon::fromTheme("transform-move-vertical"));
    sortJobsB->setToolTip(
        i18n("Reset state and sort observation jobs per altitude and movement in sky, using the start time of the first job.\n"
             "This action sorts setting targets before rising targets, and may help scheduling when starting your observation.\n"
             "Option \"Sort Jobs by Altitude and Priority\" keeps the job list sorted this way, but with current time as reference.\n"
             "Note the algorithm first calculates all altitudes using the same time, then evaluates jobs."));
    sortJobsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    mosaicB->setIcon(QIcon::fromTheme("zoom-draw"));
    mosaicB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

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

    connect(startupB, &QPushButton::clicked, this, &Scheduler::runStartupProcedure);
    connect(shutdownB, &QPushButton::clicked, this, &Scheduler::runShutdownProcedure);

    connect(selectObjectB, &QPushButton::clicked, this, &Scheduler::selectObject);
    connect(selectFITSB, &QPushButton::clicked, this, &Scheduler::selectFITS);
    connect(loadSequenceB, &QPushButton::clicked, this, &Scheduler::selectSequence);
    connect(selectStartupScriptB, &QPushButton::clicked, this, &Scheduler::selectStartupScript);
    connect(selectShutdownScriptB, &QPushButton::clicked, this, &Scheduler::selectShutdownScript);

    connect(mosaicB, &QPushButton::clicked, this, &Scheduler::startMosaicTool);
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

    // Connect geographical location - when it is available
    //connect(KStarsData::Instance()..., &LocationDialog::locationChanged..., this, &Scheduler::simClockTimeChanged);

    // restore default values for error handling strategy
    setErrorHandlingStrategy(static_cast<ErrorHandlingStrategy>(Options::errorHandlingStrategy()));
    errorHandlingRescheduleErrorsCB->setChecked(Options::rescheduleErrors());
    errorHandlingDelaySB->setValue(Options::errorHandlingStrategyDelay());

    // save new default values for error handling strategy

    connect(errorHandlingRescheduleErrorsCB, &QPushButton::clicked, [](bool checked)
    {
        Options::setRescheduleErrors(checked);
    });
    connect(errorHandlingButtonGroup, static_cast<void (QButtonGroup::*)(QAbstractButton *)>
            (&QButtonGroup::buttonClicked), [this](QAbstractButton * button)
    {
        Q_UNUSED(button)
        Options::setErrorHandlingStrategy(getErrorHandlingStrategy());
    });
    connect(errorHandlingDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [](int value)
    {
        Options::setErrorHandlingStrategyDelay(value);
    });

    connect(copySkyCenterB, &QPushButton::clicked, this, [this]()
    {
        SkyPoint center = SkyMap::Instance()->getCenterPoint();
        //center.deprecess(KStarsData::Instance()->updateNum());
        center.catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        raBox->setDMS(center.ra0().toHMSString());
        decBox->setDMS(center.dec0().toDMSString());
    });

    connect(KConfigDialog::exists("settings"), &KConfigDialog::settingsChanged, this, &Scheduler::applyConfig);

    calculateDawnDusk();
    updateNightTime();

    loadProfiles();

    watchJobChanges(true);
}

QString Scheduler::getCurrentJobName()
{
    return (currentJob != nullptr ? currentJob->getName() : "");
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
        schedulerProfileCombo
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
        culminationOffset,
        repeatsSpin,
        prioritySpin,
        errorHandlingDelaySB
    };

    QDoubleSpinBox * const dspinBoxes[] =
    {
        minMoonSeparation,
        minAltitude
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
            connect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, [this](int, bool)
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
            disconnect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, nullptr);
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
                            getLocalTime().toString("yyyy-MM-ddThh:mm:ss"), text));

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

    if (SCHEDULER_RUNNING != state)
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
        raBox->showInHours(object->ra0());
        decBox->showInDegrees(object->dec0());

        addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
        mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);

        setDirty();
    }
}

void Scheduler::selectFITS()
{
    fitsURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Select FITS Image"), dirPath,
                                          "FITS (*.fits *.fit)");
    if (fitsURL.isEmpty())
        return;

    dirPath = QUrl(fitsURL.url(QUrl::RemoveFilename));

    fitsEdit->setText(fitsURL.toLocalFile());

    if (nameEdit->text().isEmpty())
        nameEdit->setText(fitsURL.fileName());

    addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
    mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);

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

    raBox->setDMS(raDMS.toHMSString());
    decBox->setDMS(deDMS.toDMSString());

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
        mosaicB->setEnabled(true);
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
    startupScriptURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Select Startup Script"),
                       dirPath,
                       i18n("Script (*)"));
    if (startupScriptURL.isEmpty())
        return;

    dirPath = QUrl(startupScriptURL.url(QUrl::RemoveFilename));

    mDirty = true;
    startupScript->setText(startupScriptURL.toLocalFile());
}

void Scheduler::selectShutdownScript()
{
    shutdownScriptURL = QFileDialog::getOpenFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Select Shutdown Script"),
                        dirPath,
                        i18n("Script (*)"));
    if (shutdownScriptURL.isEmpty())
        return;

    dirPath = QUrl(shutdownScriptURL.url(QUrl::RemoveFilename));

    mDirty = true;
    shutdownScript->setText(shutdownScriptURL.toLocalFile());
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
}

void Scheduler::setupJob(
    SchedulerJob &job, const QString &name, int priority, const dms &ra,
    const dms &dec, double djd, double rotation, const QUrl &sequenceUrl, const QUrl &fitsUrl,
    SchedulerJob::StartupCondition startup, const QDateTime &startupTime,
    int16_t startupOffset,
    SchedulerJob::CompletionCondition completion,
    const QDateTime &completionTime, int completionRepeats,
    double minimumAltitude, double minimumMoonSeparation,
    bool enforceWeather, bool enforceTwilight, bool enforceArtificialHorizon,
    bool track, bool focus, bool align, bool guide)
{
    /* Configure or reconfigure the observation job */

    job.setName(name);
    job.setPriority(priority);
    // djd should be ut.djd
    job.setTargetCoords(ra, dec, djd);
    job.setRotation(rotation);

    /* Consider sequence file is new, and clear captured frames map */
    job.setCapturedFramesMap(SchedulerJob::CapturedFramesMap());
    job.setSequenceFile(sequenceUrl);
    job.setFITSFile(fitsUrl);
    // #1 Startup conditions

    job.setStartupCondition(startup);
    if (startup == SchedulerJob::START_CULMINATION)
    {
        job.setCulminationOffset(startupOffset);
    }
    else if (startup == SchedulerJob::START_AT)
    {
        job.setStartupTime(startupTime);
    }
    /* Store the original startup condition */
    job.setFileStartupCondition(job.getStartupCondition());
    job.setFileStartupTime(job.getStartupTime());

    // #2 Constraints

    job.setMinAltitude(minimumAltitude);
    job.setMinMoonSeparation(minimumMoonSeparation);

    // Check enforce weather constraints
    job.setEnforceWeather(enforceWeather);
    // twilight constraints
    job.setEnforceTwilight(enforceTwilight);
    job.setEnforceArtificialHorizon(enforceArtificialHorizon);

    job.setCompletionCondition(completion);
    if (completion == SchedulerJob::FINISH_AT)
        job.setCompletionTime(completionTime);
    else if (completion == SchedulerJob::FINISH_REPEAT)
    {
        job.setRepeatsRequired(completionRepeats);
        job.setRepeatsRemaining(completionRepeats);
    }
    // Job steps
    job.setStepPipeline(SchedulerJob::USE_NONE);
    if (track)
        job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_TRACK));
    if (focus)
        job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_FOCUS));
    if (align)
        job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_ALIGN));
    if (guide)
        job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_GUIDE));

    /* Store the original startup condition */
    job.setFileStartupCondition(job.getStartupCondition());
    job.setFileStartupTime(job.getStartupTime());

    /* Reset job state to evaluate the changes */
    job.reset();
}

void Scheduler::saveJob()
{
    if (state == SCHEDULER_RUNNING)
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
    dms /*const*/ ra(raBox->createDms(false, &raOk)); //false means expressed in hours
    dms /*const*/ dec(decBox->createDms(true, &decOk));

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
        job = jobs.at(currentRow);
    }
    else
    {
        /* Instantiate a new job, insert it in the job list and add a row in the table for it just after the row currently selected. */
        job = new SchedulerJob();
        jobs.insert(currentRow, job);
        queueTable->insertRow(currentRow);
    }

    /* Configure or reconfigure the observation job */

    job->setDateTimeDisplayFormat(startupTimeEdit->displayFormat());
    fitsURL = QUrl::fromLocalFile(fitsEdit->text());

    // Get several job values depending on the state of the UI.

    SchedulerJob::StartupCondition startCondition = SchedulerJob::START_AT;
    if (asapConditionR->isChecked())
        startCondition = SchedulerJob::START_ASAP;
    else if (culminationConditionR->isChecked())
        startCondition = SchedulerJob::START_CULMINATION;

    SchedulerJob::CompletionCondition stopCondition = SchedulerJob::FINISH_AT;
    if (sequenceCompletionR->isChecked())
        stopCondition = SchedulerJob::FINISH_SEQUENCE;
    else if (repeatCompletionR->isChecked())
        stopCondition = SchedulerJob::FINISH_REPEAT;
    else if (loopCompletionR->isChecked())
        stopCondition = SchedulerJob::FINISH_LOOP;

    double altConstraint = SchedulerJob::UNDEFINED_ALTITUDE;
    if (altConstraintCheck->isChecked())
        altConstraint = minAltitude->value();

    double moonConstraint = -1;
    if (moonSeparationCheck->isChecked())
        moonConstraint = minMoonSeparation->value();

    // The reason for this kitchen-sink function is to separate the UI from the
    // job setup, to allow for testing.
    setupJob(
        *job, nameEdit->text(), prioritySpin->value(), ra, dec,
        KStarsData::Instance()->ut().djd(),
        rotationSpin->value(), sequenceURL, fitsURL,

        startCondition, startupTimeEdit->dateTime(), culminationOffset->value(),
        stopCondition, completionTimeEdit->dateTime(), repeatsSpin->value(),

        altConstraint,
        moonConstraint,
        weatherCheck->isChecked(),
        twilightCheck->isChecked(),
        artificialHorizonCheck->isChecked(),

        trackStepCheck->isChecked(),
        focusStepCheck->isChecked(),
        alignStepCheck->isChecked(),
        guideStepCheck->isChecked()
    );


    /* Verifications */
    /* FIXME: perhaps use a method more visible to the end-user */
    if (SchedulerJob::START_AT == job->getFileStartupCondition())
    {
        /* Warn if appending a job which startup time doesn't allow proper score */
        if (calculateJobScore(job, Dawn, Dusk, job->getStartupTime()) < 0)
            appendLogText(
                i18n("Warning: job '%1' has startup time %2 resulting in a negative score, and will be marked invalid when processed.",
                     job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));

    }

    // Warn user if a duplicated job is in the list - same target, same sequence
    // FIXME: Those duplicated jobs are not necessarily processed in the order they appear in the list!
    foreach (SchedulerJob *a_job, jobs)
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

                if (a_job->getStartupTime() == a_job->getStartupTime() && a_job->getPriority() == job->getPriority())
                    appendLogText(i18n("Warning: job '%1' at row %2 might require a specific startup time or a different priority, "
                                       "as currently they will start in order of insertion in the table",
                                       job->getName(), currentRow));
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

        QTableWidgetItem *scoreValue = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_SCORE), scoreValue);
        scoreValue->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        scoreValue->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

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

        QTableWidgetItem *estimatedTimeCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_DURATION), estimatedTimeCell);
        estimatedTimeCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        estimatedTimeCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        QTableWidgetItem *leadTimeCell = new QTableWidgetItem();
        queueTable->setItem(currentRow, static_cast<int>(SCHEDCOL_LEADTIME), leadTimeCell);
        leadTimeCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        leadTimeCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }

    setJobStatusCells(currentRow);

    /* We just added or saved a job, so we have a job in the list - enable relevant buttons */
    queueSaveAsB->setEnabled(true);
    queueSaveB->setEnabled(true);
    startB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
    setJobManipulation(!Options::sortSchedulerJobs(), true);

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 was saved.").arg(job->getName()).arg(currentRow + 1);

    watchJobChanges(true);

    if (SCHEDULER_LOADING != state)
    {
        evaluateJobs(true);
    }
}

void Scheduler::syncGUIToJob(SchedulerJob *job)
{
    nameEdit->setText(job->getName());

    prioritySpin->setValue(job->getPriority());

    raBox->showInHours(job->getTargetCoords().ra0());
    decBox->showInDegrees(job->getTargetCoords().dec0());

    // fitsURL/sequenceURL are not part of UI, but the UI serves as model, so keep them here for now
    fitsURL = job->getFITSFile().isEmpty() ? QUrl() : job->getFITSFile();
    sequenceURL = job->getSequenceFile();
    fitsEdit->setText(fitsURL.toLocalFile());
    sequenceEdit->setText(sequenceURL.toLocalFile());

    rotationSpin->setValue(job->getRotation());

    trackStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_TRACK);
    focusStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_FOCUS);
    alignStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_ALIGN);
    guideStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_GUIDE);

    switch (job->getFileStartupCondition())
    {
        case SchedulerJob::START_ASAP:
            asapConditionR->setChecked(true);
            culminationOffset->setValue(DEFAULT_CULMINATION_TIME);
            break;

        case SchedulerJob::START_CULMINATION:
            culminationConditionR->setChecked(true);
            culminationOffset->setValue(job->getCulminationOffset());
            break;

        case SchedulerJob::START_AT:
            startupTimeConditionR->setChecked(true);
            startupTimeEdit->setDateTime(job->getStartupTime());
            culminationOffset->setValue(DEFAULT_CULMINATION_TIME);
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
        case SchedulerJob::FINISH_SEQUENCE:
            sequenceCompletionR->setChecked(true);
            break;

        case SchedulerJob::FINISH_REPEAT:
            repeatCompletionR->setChecked(true);
            repeatsSpin->setValue(job->getRepeatsRequired());
            break;

        case SchedulerJob::FINISH_LOOP:
            loopCompletionR->setChecked(true);
            break;

        case SchedulerJob::FINISH_AT:
            timeCompletionR->setChecked(true);
            completionTimeEdit->setDateTime(job->getCompletionTime());
            break;
    }

    updateNightTime(job);

    setJobManipulation(!Options::sortSchedulerJobs(), true);
}

void Scheduler::updateNightTime(SchedulerJob const *job)
{
    if (job == nullptr)
    {
        int const currentRow = queueTable->currentRow();
        if (0 < currentRow)
            job = jobs.at(currentRow);
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

    if (state == SCHEDULER_RUNNING)
    {
        appendLogText(i18n("Warning: you cannot add or modify a job while the scheduler is running."));
        return;
    }

    SchedulerJob * const job = jobs.at(i.row());

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
    if (state != SCHEDULER_IDLE)
        return;

    if (current.row() < 0 || (current.row() + 1) > jobs.size())
        return;

    SchedulerJob * const job = jobs.at(current.row());

    if (job != nullptr)
    {
        resetJobEdit();
        syncGUIToJob(job);
    }
    else nightTime->setText("-");
}

void Scheduler::clickQueueTable(QModelIndex index)
{
    setJobManipulation(!Options::sortSchedulerJobs() && index.isValid(), index.isValid());
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
    bool can_edit = (state == SCHEDULER_IDLE);

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
    foreach (SchedulerJob* job, jobs)
        if (!reordered_sublist.contains(job))
            reordered_sublist.append(job);

    if (jobs != reordered_sublist)
    {
        /* Remember job currently selected */
        int const selectedRow = queueTable->currentRow();
        SchedulerJob * const selectedJob = 0 <= selectedRow ? jobs.at(selectedRow) : nullptr;

        /* Reassign list */
        jobs = reordered_sublist;

        /* Reassign status cells for all jobs, and reset them */
        for (int row = 0; row < jobs.size(); row++)
            setJobStatusCells(row);

        /* Reselect previously selected job */
        if (nullptr != selectedJob)
            queueTable->selectRow(jobs.indexOf(selectedJob));

        return true;
    }
    else return false;
}

void Scheduler::moveJobUp()
{
    /* No move if jobs are sorted automatically */
    if (Options::sortSchedulerJobs())
        return;

    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int const destinationRow = currentRow - 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow < 0)
        return;

    /* Swap jobs in the list */
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
    jobs.swapItemsAt(currentRow, destinationRow);
#else
    jobs.swap(currentRow, destinationRow);
#endif

    /* Reassign status cells */
    setJobStatusCells(currentRow);
    setJobStatusCells(destinationRow);

    /* Move selection to destination row */
    queueTable->selectRow(destinationRow);
    setJobManipulation(!Options::sortSchedulerJobs(), true);

    /* Jobs are now sorted, so reset all later jobs */
    for (int row = destinationRow; row < jobs.size(); row++)
        jobs.at(row)->reset();

    /* Make list modified and evaluate jobs */
    mDirty = true;
    evaluateJobs(true);
}

void Scheduler::moveJobDown()
{
    /* No move if jobs are sorted automatically */
    if (Options::sortSchedulerJobs())
        return;

    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int const destinationRow = currentRow + 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow == rowCount)
        return;

    /* Swap jobs in the list */
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
    jobs.swapItemsAt(currentRow, destinationRow);
#else
    jobs.swap(currentRow, destinationRow);
#endif

    /* Reassign status cells */
    setJobStatusCells(currentRow);
    setJobStatusCells(destinationRow);

    /* Move selection to destination row */
    queueTable->selectRow(destinationRow);
    setJobManipulation(!Options::sortSchedulerJobs(), true);

    /* Jobs are now sorted, so reset all later jobs */
    for (int row = currentRow; row < jobs.size(); row++)
        jobs.at(row)->reset();

    /* Make list modified and evaluate jobs */
    mDirty = true;
    evaluateJobs(true);
}

void Scheduler::setJobStatusCells(int row)
{
    if (row < 0 || jobs.size() <= row)
        return;

    SchedulerJob * const job = jobs.at(row);

    job->setNameCell(queueTable->item(row, static_cast<int>(SCHEDCOL_NAME)));
    job->setStatusCell(queueTable->item(row, static_cast<int>(SCHEDCOL_STATUS)));
    job->setCaptureCountCell(queueTable->item(row, static_cast<int>(SCHEDCOL_CAPTURES)));
    job->setScoreCell(queueTable->item(row, static_cast<int>(SCHEDCOL_SCORE)));
    job->setAltitudeCell(queueTable->item(row, static_cast<int>(SCHEDCOL_ALTITUDE)));
    job->setStartupCell(queueTable->item(row, static_cast<int>(SCHEDCOL_STARTTIME)));
    job->setCompletionCell(queueTable->item(row, static_cast<int>(SCHEDCOL_ENDTIME)));
    job->setEstimatedTimeCell(queueTable->item(row, static_cast<int>(SCHEDCOL_DURATION)));
    job->setLeadTimeCell(queueTable->item(row, static_cast<int>(SCHEDCOL_LEADTIME)));
    job->updateJobCells();
}

void Scheduler::resetJobEdit()
{
    if (jobUnderEdit < 0)
        return;

    SchedulerJob * const job = jobs.at(jobUnderEdit);
    Q_ASSERT_X(job != nullptr, __FUNCTION__, "Edited job must be valid");

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is not longer edited.").arg(job->getName()).arg(
                                       jobUnderEdit + 1);

    jobUnderEdit = -1;

    watchJobChanges(false);

    /* Revert apply button to add */
    setJobAddApply(true);

    /* Refresh state of job manipulation buttons */
    setJobManipulation(!Options::sortSchedulerJobs(), true);

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
    SchedulerJob * const job = jobs.at(currentRow);
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
    jobs.removeOne(job);
    delete (job);

    mDirty = true;
    evaluateJobs(true);
}

void Scheduler::toggleScheduler()
{
    if (state == SCHEDULER_RUNNING)
    {
        disablePreemptiveShutdown();
        stop();
    }
    else
        run();
}

void Scheduler::stop()
{
    if (state != SCHEDULER_RUNNING)
        return;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is stopping...";

    // Stop running job and abort all others
    // in case of soft shutdown we skip this
    if (!preemptiveShutdown())
    {
        bool wasAborted = false;
        foreach (SchedulerJob *job, jobs)
        {
            if (job == currentJob)
                stopCurrentJobAction();

            if (job->getState() <= SchedulerJob::JOB_BUSY)
            {
                appendLogText(i18n("Job '%1' has not been processed upon scheduler stop, marking aborted.", job->getName()));
                job->setState(SchedulerJob::JOB_ABORTED);
                wasAborted = true;
            }
        }

        if (wasAborted)
            KNotification::event(QLatin1String("SchedulerAborted"), i18n("Scheduler aborted."));
    }

    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_NOTHING).toLatin1().data());
    setupNextIteration(RUN_NOTHING);
    cancelGuidingTimer();

    state     = SCHEDULER_IDLE;
    emit newStatus(state);
    ekosState = EKOS_IDLE;
    indiState = INDI_IDLE;

    parkWaitState = PARKWAIT_IDLE;

    // Only reset startup state to idle if the startup procedure was interrupted before it had the chance to complete.
    // Or if we're doing a soft shutdown
    if (startupState != STARTUP_COMPLETE || preemptiveShutdown())
    {
        if (startupState == STARTUP_SCRIPT)
        {
            scriptProcess.disconnect();
            scriptProcess.terminate();
        }

        startupState = STARTUP_IDLE;
    }
    // Reset startup state to unparking phase (dome -> mount -> cap)
    // We do not want to run the startup script again but unparking should be checked
    // whenever the scheduler is running again.
    else if (startupState == STARTUP_COMPLETE)
    {
        if (unparkDomeCheck->isChecked())
            startupState = STARTUP_UNPARK_DOME;
        else if (unparkMountCheck->isChecked())
            startupState = STARTUP_UNPARK_MOUNT;
        else if (uncapCheck->isChecked())
            startupState = STARTUP_UNPARK_CAP;
    }

    shutdownState = SHUTDOWN_IDLE;

    setCurrentJob(nullptr);
    captureBatch            = 0;
    indiConnectFailureCount = 0;
    ekosConnectFailureCount = 0;
    focusFailureCount       = 0;
    guideFailureCount       = 0;
    alignFailureCount       = 0;
    captureFailureCount     = 0;
    loadAndSlewProgress     = false;
    autofocusCompleted      = false;

    startupB->setEnabled(true);
    shutdownB->setEnabled(true);

    // If soft shutdown, we return for now
    if (preemptiveShutdown())
    {
        sleepLabel->setToolTip(i18n("Scheduler is in shutdown until next job is ready"));
        sleepLabel->show();

        QDateTime const now = getLocalTime();
        int const nextObservationTime = now.secsTo(getPreemptiveShutdownWakeupTime());
        setupNextIteration(RUN_WAKEUP,
                           std::lround(((nextObservationTime + 1) * 1000)
                                       / KStarsData::Instance()->clock()->scale()));
        return;

    }

    // Clear target name in capture interface upon stopping
    if (captureInterface.isNull() == false)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:setProperty", "targetName=\"\"");
        captureInterface->setProperty("targetName", QString());
    }

    if (scriptProcess.state() == QProcess::Running)
        scriptProcess.terminate();

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
    mosaicB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
}

void Scheduler::start()
{
    switch (state)
    {
        case SCHEDULER_IDLE:
            /* FIXME: Manage the non-validity of the startup script earlier, and make it a warning only when the scheduler starts */
            startupScriptURL = QUrl::fromUserInput(startupScript->text());
            if (!startupScript->text().isEmpty() && !startupScriptURL.isValid())
            {
                appendLogText(i18n("Warning: startup script URL %1 is not valid.", startupScript->text()));
                return;
            }

            /* FIXME: Manage the non-validity of the shutdown script earlier, and make it a warning only when the scheduler starts */
            shutdownScriptURL = QUrl::fromUserInput(shutdownScript->text());
            if (!shutdownScript->text().isEmpty() && !shutdownScriptURL.isValid())
            {
                appendLogText(i18n("Warning: shutdown script URL %1 is not valid.", shutdownScript->text()));
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
            mosaicB->setEnabled(false);
            evaluateOnlyB->setEnabled(false);
            startupB->setEnabled(false);
            shutdownB->setEnabled(false);

            state = SCHEDULER_RUNNING;
            emit newStatus(state);
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            setupNextIteration(RUN_SCHEDULER);

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
            state = SCHEDULER_RUNNING;
            emit newStatus(state);
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            setupNextIteration(RUN_SCHEDULER);

            appendLogText(i18n("Scheduler resuming."));
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler resuming.";
            break;

        default:
            break;
    }
}

void Scheduler::pause()
{
    state = SCHEDULER_PAUSED;
    emit newStatus(state);
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
    setupNextIteration(RUN_NOTHING);
    appendLogText(i18n("Scheduler paused."));
}

void Scheduler::setCurrentJob(SchedulerJob *job)
{
    /* Reset job widgets */
    if (currentJob)
    {
        currentJob->setStageLabel(nullptr);
    }

    /* Set current job */
    currentJob = job;

    /* Reassign job widgets, or reset to defaults */
    if (currentJob)
    {
        currentJob->setStageLabel(jobStatus);
        queueTable->selectRow(jobs.indexOf(currentJob));
    }
    else
    {
        jobStatus->setText(i18n("No job running"));
        //queueTable->clearSelection();
    }
}

void Scheduler::evaluateJobs(bool evaluateOnly)
{
    /* Don't evaluate if list is empty */
    if (jobs.isEmpty())
        return;
    /* Start by refreshing the number of captures already present - unneeded if not remembering job progress */
    if (Options::rememberJobProgress())
        updateCompletedJobsCount();

    calculateDawnDusk();
    bool possiblyDelay = false;
    QList<SchedulerJob *> sortedJobs = evaluateJobs(jobs, state, m_CapturedFramesCount, Dawn, Dusk,
                                       errorHandlingRescheduleErrorsCB->isChecked(),
                                       errorHandlingDontRestartButton->isChecked() == false, &possiblyDelay, this);
    if (sortedJobs.empty())
    {
        setCurrentJob(nullptr);
        return;
    }
    if (possiblyDelay && errorHandlingRestartAfterAllButton->isChecked())
    {
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_WAKEUP).toLatin1().data());
        setupNextIteration(RUN_WAKEUP, std::lround((errorHandlingDelaySB->value() * 1000) /
                           KStarsData::Instance()->clock()->scale()));
        // but before we restart them, we wait for the given delay.
        appendLogText(i18n("All jobs aborted. Waiting %1 seconds to re-schedule.", errorHandlingDelaySB->value()));

        sleepLabel->setToolTip(i18n("Scheduler waits for a retry."));
        sleepLabel->show();
        // we continue to determine which job should be running, when the delay is over
    }

    processJobs(sortedJobs, evaluateOnly);
}

QList<SchedulerJob *> Scheduler::evaluateJobs( QList<SchedulerJob *> &jobs, SchedulerState state,
        const QMap<QString, uint16_t> &capturedFramesCount, QDateTime const &Dawn, QDateTime const &Dusk,
        bool rescheduleErrors, bool restartJobs, bool *possiblyDelay, Scheduler *scheduler)
{

    /* FIXME: it is possible to evaluate jobs while KStars has a time offset, so warn the user about this */
    QDateTime const now = getLocalTime();

    /* First, filter out non-schedulable jobs */
    /* FIXME: jobs in state JOB_ERROR should not be in the list, reorder states */
    QList<SchedulerJob *> sortedJobs = jobs;

    /* Then enumerate SchedulerJobs to consolidate imaging time */
    foreach (SchedulerJob *job, sortedJobs)
    {
        /* Let aborted jobs be rescheduled later instead of forgetting them */
        switch (job->getState())
        {
            case SchedulerJob::JOB_SCHEDULED:
                /* If job is scheduled, keep it for evaluation against others */
                break;

            case SchedulerJob::JOB_INVALID:
            case SchedulerJob::JOB_COMPLETE:
                /* If job is invalid or complete, bypass evaluation */
                continue;

            case SchedulerJob::JOB_BUSY:
                /* If job is busy, edge case, bypass evaluation */
                continue;

            case SchedulerJob::JOB_ERROR:
            case SchedulerJob::JOB_ABORTED:
                /* If job is in error or aborted and we're running, keep its evaluation until there is nothing else to do */
                if (state == SCHEDULER_RUNNING)
                    continue;
            /* Fall through */
            case SchedulerJob::JOB_IDLE:
            case SchedulerJob::JOB_EVALUATION:
            default:
                /* If job is idle, re-evaluate completely */
                job->setEstimatedTime(-1);
                break;
        }

        switch (job->getCompletionCondition())
        {
            case SchedulerJob::FINISH_AT:
                /* If planned finishing time has passed, the job is set to IDLE waiting for a next chance to run */
                if (job->getCompletionTime().isValid() && job->getCompletionTime() < now)
                {
                    job->setState(SchedulerJob::JOB_IDLE);
                    continue;
                }
                break;

            case SchedulerJob::FINISH_REPEAT:
                // In case of a repeating jobs, let's make sure we have more runs left to go
                // If we don't, re-estimate imaging time for the scheduler job before concluding
                if (job->getRepeatsRemaining() == 0)
                {
                    if (scheduler != nullptr) scheduler->appendLogText(i18n("Job '%1' has no more batches remaining.", job->getName()));
                    if (Options::rememberJobProgress())
                    {
                        job->setEstimatedTime(-1);
                    }
                    else
                    {
                        job->setState(SchedulerJob::JOB_COMPLETE);
                        job->setEstimatedTime(0);
                        continue;
                    }
                }
                break;

            default:
                break;
        }

        // -1 = Job is not estimated yet
        // -2 = Job is estimated but time is unknown
        // > 0  Job is estimated and time is known
        if (job->getEstimatedTime() == -1)
        {
            if (estimateJobTime(job, capturedFramesCount, scheduler) == false)
            {
                job->setState(SchedulerJob::JOB_INVALID);
                continue;
            }
        }

        if (job->getEstimatedTime() == 0)
        {
            job->setRepeatsRemaining(0);
            job->setState(SchedulerJob::JOB_COMPLETE);
            continue;
        }

        // In any other case, evaluate
        job->setState(SchedulerJob::JOB_EVALUATION);
    }

    /*
     * At this step, we prepare scheduling of jobs.
     * We filter out jobs that won't run now, and make sure jobs are not all starting at the same time.
     */

    /* This predicate matches jobs not being evaluated and not aborted */
    auto neither_evaluated_nor_aborted = [](SchedulerJob const * const job)
    {
        SchedulerJob::JOBStatus const s = job->getState();
        return SchedulerJob::JOB_EVALUATION != s && SchedulerJob::JOB_ABORTED != s;
    };

    /* This predicate matches jobs neither being evaluated nor aborted nor in error state */
    auto neither_evaluated_nor_aborted_nor_error = [](SchedulerJob const * const job)
    {
        SchedulerJob::JOBStatus const s = job->getState();
        return SchedulerJob::JOB_EVALUATION != s && SchedulerJob::JOB_ABORTED != s && SchedulerJob::JOB_ERROR != s;
    };

    /* This predicate matches jobs that aborted, or completed for whatever reason */
    auto finished_or_aborted = [](SchedulerJob const * const job)
    {
        SchedulerJob::JOBStatus const s = job->getState();
        return SchedulerJob::JOB_ERROR <= s || SchedulerJob::JOB_ABORTED == s;
    };

    bool nea  = std::all_of(sortedJobs.begin(), sortedJobs.end(), neither_evaluated_nor_aborted);
    bool neae = std::all_of(sortedJobs.begin(), sortedJobs.end(), neither_evaluated_nor_aborted_nor_error);

    /* If there are no jobs left to run in the filtered list, stop evaluation */
    if (sortedJobs.isEmpty() || (!rescheduleErrors && nea)
            || (rescheduleErrors && neae))
    {
        if (scheduler != nullptr) scheduler->appendLogText(i18n("No jobs left in the scheduler queue."));
        QList<SchedulerJob *> noJobs;
        return noJobs;
    }

    /* If there are only aborted jobs that can run, reschedule those */
    if (std::all_of(sortedJobs.begin(), sortedJobs.end(), finished_or_aborted) && restartJobs)
    {
        if (scheduler != nullptr) scheduler->appendLogText(i18n("Only %1 jobs left in the scheduler queue, rescheduling those.",
                    rescheduleErrors ? "aborted or error" : "aborted"));

        // set aborted and error jobs to evaluation state
        for (int index = 0; index < sortedJobs.size(); index++)
        {
            SchedulerJob * const job = sortedJobs.at(index);
            if (SchedulerJob::JOB_ABORTED == job->getState() ||
                    (rescheduleErrors && SchedulerJob::JOB_ERROR == job->getState()))
                job->setState(SchedulerJob::JOB_EVALUATION);
        }
        *possiblyDelay = true;
    }

    /* If option says so, reorder by altitude and priority before sequencing */
    /* FIXME: refactor so all sorts are using the same predicates */
    /* FIXME: use std::stable_sort as qStableSort is deprecated */
    /* FIXME: dissociate altitude and priority, it's difficult to choose which predicate to use first */
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Option to sort jobs based on priority and altitude is" << Options::sortSchedulerJobs();
    if (Options::sortSchedulerJobs())
    {
        using namespace std::placeholders;
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(),
                         std::bind(SchedulerJob::decreasingAltitudeOrder, _1, _2, getLocalTime()));
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::increasingPriorityOrder);
    }

    /* The first reordered job has no lead time - this could also be the delay from now to startup */
    sortedJobs.first()->setLeadTime(0);

    /* The objective of the following block is to make sure jobs are sequential in the list filtered previously.
     *
     * The algorithm manages overlap between jobs by stating that scheduled jobs that start sooner are non-movable.
     * If the completion time of the previous job overlaps the current job, we offset the startup of the current job.
     * Jobs that have no valid startup time when evaluated (ASAP jobs) are assigned an immediate startup time.
     * The lead time from the Options registry is used as a buffer between jobs.
     *
     * Note about the situation where the current job overlaps the next job, and the next job is not movable:
     * - If we mark the current job invalid, it will not be processed at all. Dropping is not satisfactory.
     * - If we move the current job after the fixed job, we need to restart evaluation with a new list, and risk an
     *   infinite loop eventually. This means swapping schedules, and is incompatible with altitude/priority sort.
     * - If we mark the current job aborted, it will be re-evaluated each time a job is complete to see if it can fit.
     *   Although puzzling for the end-user, this solution is dynamic: the aborted job might or might not be scheduled
     *   at the planned time slot. But as the end-user did not enforce the start time, this is acceptable. Moreover, the
     *   schedule will be altered by external events during the execution.
     *
     * Here are the constraints that have an effect on the job being examined, and indirectly on all subsequent jobs:
     * - Twilight constraint moves jobs to the next dark sky interval.
     * - Altitude constraint, currently linked with Moon separation, moves jobs to the next acceptable altitude time.
     * - Culmination constraint moves jobs to the next transit time, with arbitrary offset.
     * - Fixed startup time moves jobs to a fixed time, essentially making them non-movable, or invalid if in the past.
     *
     * Here are the constraints that have an effect on jobs following the job being examined:
     * - Repeats requirement increases the duration of the current job, pushing subsequent jobs.
     * - Looping requirement causes subsequent jobs to become invalid (until dynamic priority is implemented).
     * - Fixed completion makes subsequent jobs start after that boundary time.
     *
     * However, we need a way to inform the end-user about failed schedules clearly in the UI.
     * The message to get through is that if jobs are not sorted by altitude/priority, the aborted or invalid jobs
     * should be modified or manually moved to a better position. If jobs are sorted automatically, aborted jobs will
     * be processed when possible, probably not at the expected moment.
     */

    // Make sure no two jobs have the same scheduled time or overlap with other jobs
    for (int index = 0; index < sortedJobs.size(); index++)
    {
        SchedulerJob * const currentJob = sortedJobs.at(index);

        // Bypass jobs that are not marked for evaluation - we did not remove them to preserve schedule order
        if (SchedulerJob::JOB_EVALUATION != currentJob->getState())
            continue;

        // At this point, a job with no valid start date is a problem, so consider invalid startup time is now
        if (!currentJob->getStartupTime().isValid())
            currentJob->setStartupTime(now);

        // Locate the previous scheduled job, so that a full schedule plan may be actually consolidated
        SchedulerJob const * previousJob = nullptr;
        for (int i = index - 1; 0 <= i; i--)
        {
            SchedulerJob const * const a_job = sortedJobs.at(i);

            if (SchedulerJob::JOB_SCHEDULED == a_job->getState())
            {
                previousJob = a_job;
                break;
            }
        }

        Q_ASSERT_X(nullptr == previousJob
                   || previousJob != currentJob, __FUNCTION__,
                   "Previous job considered for schedule is either undefined or not equal to current.");

        // Locate the next job - nothing special required except end of list check
        SchedulerJob const * const nextJob = index + 1 < sortedJobs.size() ? sortedJobs.at(index + 1) : nullptr;

        Q_ASSERT_X(nullptr == nextJob
                   || nextJob != currentJob, __FUNCTION__, "Next job considered for schedule is either undefined or not equal to current.");

        // We're attempting to schedule the job 10 times before making it invalid
        for (int attempt = 1; attempt < 11; attempt++)
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                           QString("Schedule attempt #%1 for %2-second job '%3' on row #%4 starting at %5, completing at %6.")
                                           .arg(attempt)
                                           .arg(static_cast<int>(currentJob->getEstimatedTime()))
                                           .arg(currentJob->getName())
                                           .arg(index + 1)
                                           .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()))
                                           .arg(currentJob->getCompletionTime().toString(currentJob->getDateTimeDisplayFormat()));


            // ----- #1 Should we reject the current job because of its fixed startup time?
            //
            // A job with fixed startup time must be processed at the time of startup, and may be late up to leadTime.
            // When such a job repeats, its startup time is reinitialized to prevent abort - see completion algorithm.
            // If such a job requires night time, minimum altitude or Moon separation, the consolidated startup time is checked for errors.
            // If all restrictions are complied with, we bypass the rest of the verifications as the job cannot be moved.

            if (SchedulerJob::START_AT == currentJob->getFileStartupCondition())
            {
                // Check whether the current job is too far in the past to be processed - if job is repeating, its startup time is already now
                if (currentJob->getStartupTime().addSecs(static_cast <int> (ceil(Options::leadTime() * 60))) < now)
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);


                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' has fixed startup time %2 set in the past, marking invalid.",
                                 currentJob->getName(), currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat())));

                    break;
                }
                // Check whether the current job has a positive dark sky score at the time of startup
                else if (true == currentJob->getEnforceTwilight() && !currentJob->runsDuringAstronomicalNightTime())
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' has a fixed start time incompatible with its twilight restriction, marking invalid.",
                                 currentJob->getName()));

                    break;
                }
                // Check whether the current job has a positive altitude score at the time of startup
                else if (currentJob->hasAltitudeConstraint() && currentJob->getAltitudeScore(currentJob->getStartupTime()) < 0)
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' has a fixed start time incompatible with its altitude restriction, marking invalid.",
                                 currentJob->getName()));

                    break;
                }
                // Check whether the current job has a positive Moon separation score at the time of startup
                else if (0 < currentJob->getMinMoonSeparation() && currentJob->getMoonSeparationScore(currentJob->getStartupTime()) < 0)
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' has a fixed start time incompatible with its Moon separation restriction, marking invalid.",
                                 currentJob->getName()));

                    break;
                }

                // Check whether a previous job overlaps the current job
                if (nullptr != previousJob && previousJob->getCompletionTime().isValid())
                {
                    // Calculate time we should be at after finishing the previous job
                    QDateTime const previousCompletionTime = previousJob->getCompletionTime().addSecs(static_cast <int> (ceil(
                                Options::leadTime() * 60.0)));

                    // Make this job invalid if startup time is not achievable because a START_AT job is non-movable
                    if (currentJob->getStartupTime() < previousCompletionTime)
                    {
                        currentJob->setState(SchedulerJob::JOB_INVALID);

                        if (scheduler != nullptr) scheduler->appendLogText(
                                i18n("Warning: job '%1' has fixed startup time %2 unachievable due to the completion time of its previous sibling, marking invalid.",
                                     currentJob->getName(), currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat())));

                        break;
                    }

                    currentJob->setLeadTime(previousJob->getCompletionTime().secsTo(currentJob->getStartupTime()));
                }

                // This job is non-movable, we're done
                currentJob->setScore(calculateJobScore(currentJob, Dawn, Dusk, now));
                currentJob->setState(SchedulerJob::JOB_SCHEDULED);
                qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                               QString("Job '%1' is scheduled to start at %2, in compliance with fixed startup time requirement.")
                                               .arg(currentJob->getName())
                                               .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()));

                break;
            }

            // ----- #2 Should we delay the current job because it overlaps the previous job?
            //
            // The previous job is considered non-movable, and its completion, plus lead time, is the origin for the current job.
            // If no previous job exists, or if all prior jobs in the list are rejected, there is no overlap.
            // If there is a previous job, the current job is simply delayed to avoid an eventual overlap.
            // IF there is a previous job but it never finishes, the current job is rejected.
            // This scheduling obviously relies on imaging time estimation: because errors stack up, future startup times are less and less reliable.

            if (nullptr != previousJob)
            {
                if (previousJob->getCompletionTime().isValid())
                {
                    // Calculate time we should be at after finishing the previous job
                    QDateTime const previousCompletionTime = previousJob->getCompletionTime().addSecs(static_cast <int> (ceil(
                                Options::leadTime() * 60.0)));

                    // Delay the current job to completion of its previous sibling if needed - this updates the completion time automatically
                    if (currentJob->getStartupTime() < previousCompletionTime)
                    {
                        currentJob->setStartupTime(previousCompletionTime);

                        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                                       QString("Job '%1' is scheduled to start at %2, %3 seconds after %4, in compliance with previous job completion requirement.")
                                                       .arg(currentJob->getName())
                                                       .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()))
                                                       .arg(previousJob->getCompletionTime().secsTo(currentJob->getStartupTime()))
                                                       .arg(previousJob->getCompletionTime().toString(previousJob->getDateTimeDisplayFormat()));

                        // If the job is repeating or looping, re-estimate imaging duration - error case may be a bug
                        if (SchedulerJob::FINISH_SEQUENCE != currentJob->getCompletionCondition())
                            if (false == estimateJobTime(currentJob, capturedFramesCount, scheduler))
                                currentJob->setState(SchedulerJob::JOB_INVALID);

                        continue;
                    }
                }
                else
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: Job '%1' cannot start because its previous sibling has no completion time, marking invalid.",
                                 currentJob->getName()));

                    break;
                }

                currentJob->setLeadTime(previousJob->getCompletionTime().secsTo(currentJob->getStartupTime()));

                // Lead time can be zero, so completion may equal startup
                Q_ASSERT_X(previousJob->getCompletionTime() <= currentJob->getStartupTime(), __FUNCTION__,
                           "Previous and current jobs do not overlap.");
            }


            // ----- #3 Should we delay the current job because it overlaps daylight?
            //
            // Pre-dawn time rules whether a job may be started before dawn, or delayed to next night.
            // Note that the case of START_AT jobs is considered earlier in the algorithm, thus may be omitted here.
            // In addition to be hardcoded currently, the imaging duration is not reliable enough to start a short job during pre-dawn.
            // However, completion time during daylight only causes a warning, as this case will be processed as the job runs.

            if (currentJob->getEnforceTwilight())
            {
                // During that check, we don't verify the current job can actually complete before dawn.
                // If the job is interrupted while running, it will be aborted and rescheduled at a later time.

                // If the job does not run during the astronomical night time, delay it to the next dusk
                // This function takes care of Ekos offsets, dawn/dusk and pre-dawn
                if (!currentJob->runsDuringAstronomicalNightTime())
                {
                    // Delay job to next dusk - we will check other requirements later on
                    currentJob->setStartupTime(currentJob->getDuskAstronomicalTwilight());

                    qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                                   QString("Job '%1' is scheduled to start at %2, in compliance with night time requirement.")
                                                   .arg(currentJob->getName())
                                                   .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()));

                    continue;
                }

                // Check if the completion date overlaps the next dawn, and issue a warning if so
                if (currentJob->getDawnAstronomicalTwilight() < currentJob->getCompletionTime())
                {
                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' execution overlaps daylight, it will be interrupted at dawn and rescheduled on next night time.",
                                 currentJob->getName()));
                }
            }


            // ----- #4 Should we delay the current job because of its target culmination?
            //
            // Culmination uses the transit time, and fixes the startup time of the job to a particular offset around this transit time.
            // This restriction may be used to start a job at the least air mass, or after a meridian flip.
            // Culmination is scheduled before altitude restriction because it is normally more restrictive for the resulting startup time.
            // It may happen that a target cannot rise enough to comply with the altitude restriction, but a culmination time is always valid.

            if (SchedulerJob::START_CULMINATION == currentJob->getFileStartupCondition())
            {
                // Consolidate the culmination time, with offset, of the current job
                QDateTime const nextCulminationTime = currentJob->calculateCulmination(currentJob->getStartupTime());

                if (nextCulminationTime.isValid()) // Guaranteed
                {
                    if (currentJob->getStartupTime() < nextCulminationTime)
                    {
                        currentJob->setStartupTime(nextCulminationTime);

                        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                                       QString("Job '%1' is scheduled to start at %2, in compliance with culmination requirements.")
                                                       .arg(currentJob->getName())
                                                       .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()));

                        continue;
                    }
                }
                else
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' requires culmination offset of %2 minutes, not achievable, marking invalid.",
                                 currentJob->getName(),
                                 QString("%L1").arg(currentJob->getCulminationOffset())));

                    break;
                }

                // Don't test altitude here, because we will push the job during the next check step
                // Q_ASSERT_X(0 <= getAltitudeScore(currentJob, currentJob->getStartupTime()), __FUNCTION__, "Consolidated altitude time results in a positive altitude score.");
            }


            // ----- #5 Should we delay the current job because its altitude is incorrect?
            //
            // Altitude time ensures the job is assigned a startup time when its target is high enough.
            // As other restrictions, the altitude is only considered for startup time, completion time is managed while the job is running.
            // Because a target setting down is a problem for the schedule, a cutoff altitude is added in the case the job target is past the meridian at startup time.
            // FIXME: though arguable, Moon separation is also considered in that restriction check - move it to a separate case.

            if (currentJob->hasAltitudeConstraint())
            {
                // Consolidate a new altitude time from the startup time of the current job
                QDateTime const nextAltitudeTime = currentJob->calculateAltitudeTime(currentJob->getStartupTime());

                if (nextAltitudeTime.isValid())
                {
                    if (currentJob->getStartupTime() < nextAltitudeTime)
                    {
                        currentJob->setStartupTime(nextAltitudeTime);

                        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                                       QString("Job '%1' is scheduled to start at %2, in compliance with altitude and Moon separation requirements.")
                                                       .arg(currentJob->getName())
                                                       .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()));

                        continue;
                    }
                }
                else
                {
                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' requires minimum altitude %2 and Moon separation %3, not achievable, marking invalid.",
                                 currentJob->getName(),
                                 QString("%L1").arg(static_cast<double>(currentJob->getMinAltitude()), 0, 'f', 2),
                                 0.0 < currentJob->getMinMoonSeparation() ?
                                 QString("%L1").arg(static_cast<double>(currentJob->getMinMoonSeparation()), 0, 'f', 2) :
                                 QString("-")));

                    break;
                }

                Q_ASSERT_X(0 <= currentJob->getAltitudeScore(currentJob->getStartupTime()), __FUNCTION__,
                           "Consolidated altitude time results in a positive altitude score.");
            }


            // ----- #6 Should we reject the current job because it overlaps the next job and that next job is not movable?
            //
            // If we have a blocker next to the current job, we compare the completion time of the current job and the startup time of this next job, taking lead time into account.
            // This verification obviously relies on the imaging time to be reliable, but there's not much we can do at this stage of the implementation.

            if (nullptr != nextJob && SchedulerJob::START_AT == nextJob->getFileStartupCondition())
            {
                // In the current implementation, it is not possible to abort a running job when the next job is supposed to start.
                // Movable jobs after this one will be delayed, but non-movable jobs are considered blockers.

                // Calculate time we have between the end of the current job and the next job
                double const timeToNext = static_cast<double> (currentJob->getCompletionTime().secsTo(nextJob->getStartupTime()));

                // If that time is overlapping the next job, abort the current job
                if (timeToNext < Options::leadTime() * 60)
                {
                    currentJob->setState(SchedulerJob::JOB_ABORTED);

                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Warning: job '%1' is constrained by the start time of the next job, and cannot finish in time, marking aborted.",
                                 currentJob->getName()));

                    break;
                }

                Q_ASSERT_X(currentJob->getCompletionTime().addSecs(Options::leadTime() * 60) < nextJob->getStartupTime(), __FUNCTION__,
                           "No overlap ");
            }


            // ----- #7 Should we reject the current job because it exceeded its fixed completion time?
            //
            // This verification simply checks that because of previous jobs, the startup time of the current job doesn't exceed its fixed completion time.
            // Its main objective is to catch wrong dates in the FINISH_AT configuration.

            if (SchedulerJob::FINISH_AT == currentJob->getCompletionCondition())
            {
                if (currentJob->getCompletionTime() < currentJob->getStartupTime())
                {
                    if (scheduler != nullptr) scheduler->appendLogText(
                            i18n("Job '%1' completion time (%2) could not be achieved before start up time (%3)",
                                 currentJob->getName(),
                                 currentJob->getCompletionTime().toString(currentJob->getDateTimeDisplayFormat()),
                                 currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat())));

                    currentJob->setState(SchedulerJob::JOB_INVALID);

                    break;
                }
            }


            // ----- #8 Should we reject the current job because of weather?
            //
            // That verification is left for runtime
            //
            // if (false == isWeatherOK(currentJob))
            //{
            //    currentJob->setState(SchedulerJob::JOB_ABORTED);
            //
            //    appendLogText(i18n("Job '%1' cannot run now because of bad weather, marking aborted.", currentJob->getName()));
            //}


            // ----- #9 Update score for current time and mark evaluating jobs as scheduled

            currentJob->setScore(calculateJobScore(currentJob, Dawn, Dusk, now));
            currentJob->setState(SchedulerJob::JOB_SCHEDULED);

            qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                           QString("Job '%1' on row #%2 passed all checks after %3 attempts, will proceed at %4 for approximately %5 seconds, marking scheduled")
                                           .arg(currentJob->getName())
                                           .arg(index + 1)
                                           .arg(attempt)
                                           .arg(currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()))
                                           .arg(currentJob->getEstimatedTime());

            break;
        }

        // Check if job was successfully scheduled, else reject it
        if (SchedulerJob::JOB_EVALUATION == currentJob->getState())
        {
            currentJob->setState(SchedulerJob::JOB_INVALID);

            //appendLogText(i18n("Warning: job '%1' on row #%2 could not be scheduled during evaluation and is marked invalid, please review your plan.",
            //            currentJob->getName(),
            //            index + 1));

        }
    }
    return sortedJobs;
}

void Scheduler::processJobs(QList<SchedulerJob *> sortedJobs, bool jobEvaluationOnly)
{
    /* Apply sorting to queue table, and mark it for saving if it changes */
    mDirty = reorderJobs(sortedJobs) | mDirty;

    if (jobEvaluationOnly || state != SCHEDULER_RUNNING)
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

    /* The job to run is the first scheduled, locate it in the list */
    QList<SchedulerJob*>::iterator job_to_execute_iterator = std::find_if(sortedJobs.begin(),
            sortedJobs.end(), [](SchedulerJob * const job)
    {
        return SchedulerJob::JOB_SCHEDULED == job->getState();
    });

    /* If there is no scheduled job anymore (because the restriction loop made them invalid, for instance), bail out */
    if (sortedJobs.end() == job_to_execute_iterator)
    {
        appendLogText(i18n("No jobs left in the scheduler queue after schedule cleanup."));
        setCurrentJob(nullptr);
        return;
    }

    /* FIXME: it is possible to evaluate jobs while KStars has a time offset, so warn the user about this */
    QDateTime const now = getLocalTime();

    /* Check if job can be processed right now */
    SchedulerJob * const job_to_execute = *job_to_execute_iterator;
    if (job_to_execute->getFileStartupCondition() == SchedulerJob::START_ASAP)
        if( 0 <= calculateJobScore(job_to_execute, Dawn, Dusk, now))
            job_to_execute->setStartupTime(now);

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' is selected for next observation with priority #%2 and score %3.")
                                   .arg(job_to_execute->getName())
                                   .arg(job_to_execute->getPriority())
                                   .arg(job_to_execute->getScore());

    // Set the current job, and let the status timer execute it when ready
    setCurrentJob(job_to_execute);
}

void Scheduler::wakeUpScheduler()
{
    sleepLabel->hide();

    if (preemptiveShutdown())
    {
        disablePreemptiveShutdown();
        appendLogText(i18n("Scheduler is awake."));
        start();
    }
    else
    {
        if (state == SCHEDULER_RUNNING)
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when ready..."));
        else
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when scheduler is resumed."));

        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        setupNextIteration(RUN_SCHEDULER);
    }
}

int16_t Scheduler::getWeatherScore() const
{
    if (weatherCheck->isEnabled() == false || weatherCheck->isChecked() == false)
        return 0;

    if (weatherStatus == ISD::Weather::WEATHER_WARNING)
        return BAD_SCORE / 2;
    else if (weatherStatus == ISD::Weather::WEATHER_ALERT)
        return BAD_SCORE;

    return 0;
}

int16_t Scheduler::getDarkSkyScore(QDateTime const &dawn, QDateTime const &dusk, QDateTime const &when)
{
    double const secsPerDay = 24.0 * 3600.0;

    // Dark sky score is calculated based on distance to today's next dawn and dusk.
    // Option "Pre-dawn Time" avoids executing a job when dawn is approaching, and is a value in minutes.
    // - If observation is between option "Pre-dawn Time" and dawn, score is BAD_SCORE/50.
    // - If observation is before next dawn, which arrives first, score is fraction of the day from beginning of observation to dawn time, as percentage.
    // - If observation is before next dusk, which arrives first, score is BAD_SCORE.
    //
    // If observation time is invalid, the score is calculated for the current day time.
    // Note exact dusk time is considered valid in terms of night time, and will return a positive, albeit null, score.

    // FIXME: Current algorithm uses the dawn and dusk of today, instead of the day of the observation.

    // If both dawn and dusk are in the past, (incorrectly) readjust the dawn and dusk to the next day
    // This was OK for next-day calculations, but Scheduler should now drop dark sky scores and rely on SchedulerJob dawn and dusk
    QDateTime const now = when.isValid() ? when : getLocalTime();
    int const earlyDawnSecs = now.secsTo(dawn.addDays(dawn < now ? dawn.daysTo(now) + 1 : 0).addSecs(
            -60.0 * Options::preDawnTime()));
    int const dawnSecs = now.secsTo(dawn.addDays(dawn < now ? dawn.daysTo(now) + 1 : 0));
    int const duskSecs = now.secsTo(dusk.addDays(dawn < now ? dusk.daysTo(now) + 1 : 0));
    int const obsSecs = now.secsTo(when);

    Q_ASSERT_X(dawnSecs >= 0, __FUNCTION__, "Scheduler computes the next dawn after the considered event.");
    Q_ASSERT_X(duskSecs >= 0, __FUNCTION__, "Scheduler computes the next dusk after the considered event.");

    // If dawn is future and the next event is dusk, it is day time
    if (obsSecs < duskSecs && duskSecs <= dawnSecs)
        return BAD_SCORE;

    // If dawn is past and the next event is dusk, it is still day time
    if (dawnSecs <= obsSecs && obsSecs < duskSecs)
        return BAD_SCORE;

    // If early dawn is past and the next event is dawn, it could be OK but nope
    if (earlyDawnSecs <= obsSecs && obsSecs < dawnSecs && dawnSecs <= duskSecs)
        return BAD_SCORE / 50;

    // If the next event is early dawn, then it is night time
    if (obsSecs < earlyDawnSecs && earlyDawnSecs <= dawnSecs && earlyDawnSecs <= duskSecs)
        return static_cast <int16_t> ((100 * (earlyDawnSecs - obsSecs)) / secsPerDay);

    return BAD_SCORE;
}

int16_t Scheduler::calculateJobScore(SchedulerJob const *job, QDateTime const &dawn, QDateTime const &dusk,
                                     QDateTime const &when)
{
    if (nullptr == job)
        return BAD_SCORE;

    /* Only consolidate the score if light frames are required, calibration frames can run whenever needed */
    if (!job->getLightFramesRequired())
        return 1000;

    int16_t total = 0;

    /* As soon as one score is negative, it's a no-go and other scores are unneeded */

    if (job->getEnforceTwilight())
    {
        int16_t const darkSkyScore = getDarkSkyScore(dawn, dusk, when);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' dark sky score is %2 at %3")
                                       .arg(job->getName())
                                       .arg(QString::asprintf("%+d", darkSkyScore))
                                       .arg(when.toString(job->getDateTimeDisplayFormat()));

        total += darkSkyScore;
    }

    /* We still enforce altitude if the job is neither required to track nor guide, because this is too confusing for the end-user.
     * If we bypass calculation here, it must also be bypassed when checking job constraints in checkJobStage.
     */
    if (0 <= total /*&& ((job->getStepPipeline() & SchedulerJob::USE_TRACK) || (job->getStepPipeline() & SchedulerJob::USE_GUIDE))*/)
    {
        int16_t const altitudeScore = job->getAltitudeScore(when);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' altitude score is %2 at %3")
                                       .arg(job->getName())
                                       .arg(QString::asprintf("%+d", altitudeScore))
                                       .arg(when.toString(job->getDateTimeDisplayFormat()));

        total += altitudeScore;
    }

    if (0 <= total)
    {
        int16_t const moonSeparationScore = job->getMoonSeparationScore(when);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' Moon separation score is %2 at %3")
                                       .arg(job->getName())
                                       .arg(QString::asprintf("%+d", moonSeparationScore))
                                       .arg(when.toString(job->getDateTimeDisplayFormat()));

        total += moonSeparationScore;
    }

    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a total score of %2 at %3.")
                                  .arg(job->getName())
                                  .arg(QString::asprintf("%+d", total))
                                  .arg(when.toString(job->getDateTimeDisplayFormat()));

    return total;
}

void Scheduler::calculateDawnDusk()
{
    SchedulerJob::calculateDawnDusk(QDateTime(), Dawn, Dusk);

    preDawnDateTime = Dawn.addSecs(-60.0 * abs(Options::preDawnTime()));
}

void Scheduler::executeJob(SchedulerJob *job)
{
    // Some states have executeJob called after current job is cancelled - checkStatus does this
    if (job == nullptr)
        return;

    // Don't execute the current job if it is already busy
    if (currentJob == job && SchedulerJob::JOB_BUSY == currentJob->getState())
        return;

    setCurrentJob(job);
    int index = jobs.indexOf(job);
    if (index >= 0)
        queueTable->selectRow(index);

    // If we already started, we check when the next object is scheduled at.
    // If it is more than 30 minutes in the future, we park the mount if that is supported
    // and we unpark when it is due to start.
    //int const nextObservationTime = now.secsTo(currentJob->getStartupTime());

    // If the time to wait is greater than the lead time (5 minutes by default)
    // then we sleep, otherwise we wait. It's the same thing, just different labels.
    if (shouldSchedulerSleep(currentJob))
        return;
    // If job schedule isn't now, wait - continuing to execute would cancel a parking attempt
    else if (0 < getLocalTime().secsTo(currentJob->getStartupTime()))
        return;

    // From this point job can be executed now

    if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE && Options::rememberJobProgress())
    {
        QString sanitized = job->getName();
        sanitized = sanitized.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                    // Remove any two or more __
                    .replace( QRegularExpression("_{2,}"), "_")
                    // Remove any _ at the end
                    .replace( QRegularExpression("_$"), "");
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s%s\n", __LINE__, "captureInterface:setProperty", "targetName=",
                   sanitized.toLatin1().data());
        captureInterface->setProperty("targetName", sanitized);
    }

    calculateDawnDusk();
    updateNightTime();

    // Reset autofocus so that focus step is applied properly when checked
    // When the focus step is not checked, the capture module will eventually run focus periodically
    autofocusCompleted = false;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Executing Job " << currentJob->getName();

    currentJob->setState(SchedulerJob::JOB_BUSY);

    KNotification::event(QLatin1String("EkosSchedulerJobStart"),
                         i18n("Ekos job started (%1)", currentJob->getName()));

    // No need to continue evaluating jobs as we already have one.
    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
    setupNextIteration(RUN_JOBCHECK);
}

bool Scheduler::checkEkosState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    switch (ekosState)
    {
        case EKOS_IDLE:
        {
            if (m_EkosCommunicationStatus == Ekos::Success)
            {
                ekosState = EKOS_READY;
                return true;
            }
            else
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "start");
                ekosInterface->call(QDBus::AutoDetect, "start");
                ekosState = EKOS_STARTING;
                startCurrentOperationTimer();

                qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos communication status is" << m_EkosCommunicationStatus << "Starting Ekos...";

                return false;
            }
        }

        case EKOS_STARTING:
        {
            if (m_EkosCommunicationStatus == Ekos::Success)
            {
                appendLogText(i18n("Ekos started."));
                ekosConnectFailureCount = 0;
                ekosState = EKOS_READY;
                return true;
            }
            else if (m_EkosCommunicationStatus == Ekos::Error)
            {
                if (ekosConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Starting Ekos failed. Retrying..."));
                    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "start");
                    ekosInterface->call(QDBus::AutoDetect, "start");
                    return false;
                }

                appendLogText(i18n("Starting Ekos failed."));
                stop();
                return false;
            }
            else if (m_EkosCommunicationStatus == Ekos::Idle)
                return false;
            // If a minute passed, give up
            else if (getCurrentOperationMsec() > (60 * 1000))
            {
                if (ekosConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Starting Ekos timed out. Retrying..."));
                    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "stop");
                    ekosInterface->call(QDBus::AutoDetect, "stop");
                    QTimer::singleShot(1000, this, [&]()
                    {
                        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "start");
                        ekosInterface->call(QDBus::AutoDetect, "start");
                        startCurrentOperationTimer();
                    });
                    return false;
                }

                appendLogText(i18n("Starting Ekos timed out."));
                stop();
                return false;
            }
        }
        break;

        case EKOS_STOPPING:
        {
            if (m_EkosCommunicationStatus == Ekos::Idle)
            {
                appendLogText(i18n("Ekos stopped."));
                ekosState = EKOS_IDLE;
                return true;
            }
        }
        break;

        case EKOS_READY:
            return true;
    }
    return false;
}

bool Scheduler::isINDIConnected()
{
    return (m_INDICommunicationStatus == Ekos::Success);
}

bool Scheduler::checkINDIState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    //qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI State" << indiState;

    switch (indiState)
    {
        case INDI_IDLE:
        {
            if (m_INDICommunicationStatus == Ekos::Success)
            {
                indiState = INDI_PROPERTY_CHECK;
                indiConnectFailureCount = 0;
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI Properties...";
            }
            else
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Connecting INDI devices...";
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "connectDevices");
                ekosInterface->call(QDBus::AutoDetect, "connectDevices");
                indiState = INDI_CONNECTING;

                startCurrentOperationTimer();
            }
        }
        break;

        case INDI_CONNECTING:
        {
            if (m_INDICommunicationStatus == Ekos::Success)
            {
                appendLogText(i18n("INDI devices connected."));
                indiState = INDI_PROPERTY_CHECK;
            }
            else if (m_INDICommunicationStatus == Ekos::Error)
            {
                if (indiConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("One or more INDI devices failed to connect. Retrying..."));
                    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "connectDevices");
                    ekosInterface->call(QDBus::AutoDetect, "connectDevices");
                }
                else
                {
                    appendLogText(i18n("One or more INDI devices failed to connect. Check INDI control panel for details."));
                    stop();
                }
            }
            // If 30 seconds passed, we retry
            else if (getCurrentOperationMsec() > (30 * 1000))
            {
                if (indiConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("One or more INDI devices timed out. Retrying..."));
                    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "ekosInterface", "connectDevices");
                    ekosInterface->call(QDBus::AutoDetect, "connectDevices");
                    startCurrentOperationTimer();
                }
                else
                {
                    appendLogText(i18n("One or more INDI devices timed out. Check INDI control panel for details."));
                    stop();
                }
            }
        }
        break;

        case INDI_DISCONNECTING:
        {
            if (m_INDICommunicationStatus == Ekos::Idle)
            {
                appendLogText(i18n("INDI devices disconnected."));
                indiState = INDI_IDLE;
                return true;
            }
        }
        break;

        case INDI_PROPERTY_CHECK:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI properties.";
            // If dome unparking is required then we wait for dome interface
            if (unparkDomeCheck->isChecked() && m_DomeReady == false)
            {
                if (getCurrentOperationMsec() > (30 * 1000))
                {
                    startCurrentOperationTimer();
                    appendLogText(i18n("Warning: dome device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome unpark required but dome is not yet ready.";
                return false;
            }

            // If mount unparking is required then we wait for mount interface
            if (unparkMountCheck->isChecked() && m_MountReady == false)
            {
                if (getCurrentOperationMsec() > (30 * 1000))
                {
                    startCurrentOperationTimer();
                    appendLogText(i18n("Warning: mount device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount unpark required but mount is not yet ready.";
                return false;
            }

            // If cap unparking is required then we wait for cap interface
            if (uncapCheck->isChecked() && m_CapReady == false)
            {
                if (getCurrentOperationMsec() > (30 * 1000))
                {
                    startCurrentOperationTimer();
                    appendLogText(i18n("Warning: cap device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap unpark required but cap is not yet ready.";
                return false;
            }

            // capture interface is required at all times to proceed.
            if (captureInterface.isNull())
                return false;

            if (m_CaptureReady == false)
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:property", "coolerControl");
                QVariant hasCoolerControl = captureInterface->property("coolerControl");
                TEST_PRINT(stderr, "  @@@dbus received %s\n",
                           !hasCoolerControl.isValid() ? "invalid" : (hasCoolerControl.toBool() ? "T" : "F"));
                if (hasCoolerControl.isValid())
                {
                    warmCCDCheck->setEnabled(hasCoolerControl.toBool());
                    m_CaptureReady = true;
                }
                else
                    qCWarning(KSTARS_EKOS_SCHEDULER) << "Capture module is not ready yet...";
            }

            indiState = INDI_READY;
            indiConnectFailureCount = 0;
            return true;
        }

        case INDI_READY:
            return true;
    }

    return false;
}

bool Scheduler::checkStartupState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Checking Startup State (%1)...").arg(startupState);

    switch (startupState)
    {
        case STARTUP_IDLE:
        {
            KNotification::event(QLatin1String("ObservatoryStartup"), i18n("Observatory is in the startup process"));

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Startup Idle. Starting startup process...";

            // If Ekos is already started, we skip the script and move on to dome unpark step
            // unless we do not have light frames, then we skip all
            //QDBusReply<int> isEkosStarted;
            //isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            //if (isEkosStarted.value() == Ekos::Success)
            if (m_EkosCommunicationStatus == Ekos::Success)
            {
                if (startupScriptURL.isEmpty() == false)
                    appendLogText(i18n("Ekos is already started, skipping startup script..."));

                if (currentJob->getLightFramesRequired())
                    startupState = STARTUP_UNPARK_DOME;
                else
                    startupState = STARTUP_COMPLETE;
                return true;
            }

            if (schedulerProfileCombo->currentText() != i18n("Default"))
            {
                QList<QVariant> profile;
                profile.append(schedulerProfileCombo->currentText());
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "ekosInterface:callWithArgs", "setProfile");
                ekosInterface->callWithArgumentList(QDBus::AutoDetect, "setProfile", profile);
            }

            if (startupScriptURL.isEmpty() == false)
            {
                startupState = STARTUP_SCRIPT;
                executeScript(startupScriptURL.toString(QUrl::PreferLocalFile));
                return false;
            }

            startupState = STARTUP_UNPARK_DOME;
            return false;
        }

        case STARTUP_SCRIPT:
            return false;

        case STARTUP_UNPARK_DOME:
            // If there is no job in case of manual startup procedure,
            // or if the job requires light frames, let's proceed with
            // unparking the dome, otherwise startup process is complete.
            if (currentJob == nullptr || currentJob->getLightFramesRequired())
            {
                if (unparkDomeCheck->isEnabled() && unparkDomeCheck->isChecked())
                    unParkDome();
                else
                    startupState = STARTUP_UNPARK_MOUNT;
            }
            else
            {
                startupState = STARTUP_COMPLETE;
                return true;
            }

            break;

        case STARTUP_UNPARKING_DOME:
            checkDomeParkingStatus();
            break;

        case STARTUP_UNPARK_MOUNT:
            if (unparkMountCheck->isEnabled() && unparkMountCheck->isChecked())
                unParkMount();
            else
                startupState = STARTUP_UNPARK_CAP;
            break;

        case STARTUP_UNPARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case STARTUP_UNPARK_CAP:
            if (uncapCheck->isEnabled() && uncapCheck->isChecked())
                unParkCap();
            else
                startupState = STARTUP_COMPLETE;
            break;

        case STARTUP_UNPARKING_CAP:
            checkCapParkingStatus();
            break;

        case STARTUP_COMPLETE:
            return true;

        case STARTUP_ERROR:
            stop();
            return true;
    }

    return false;
}

bool Scheduler::checkShutdownState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking shutdown state...";

    switch (shutdownState)
    {
        case SHUTDOWN_IDLE:
            KNotification::event(QLatin1String("ObservatoryShutdown"), i18n("Observatory is in the shutdown process"));

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting shutdown process...";

            //            weatherTimer.stop();
            //            weatherTimer.disconnect();
            weatherLabel->hide();

            setCurrentJob(nullptr);

            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SHUTDOWN).toLatin1().data());
            setupNextIteration(RUN_SHUTDOWN);

            if (warmCCDCheck->isEnabled() && warmCCDCheck->isChecked())
            {
                appendLogText(i18n("Warming up CCD..."));

                // Turn it off
                //QVariant arg(false);
                //captureInterface->call(QDBus::AutoDetect, "setCoolerControl", arg);
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s%s\n", __LINE__, "captureInterface:setProperty", "coolerControl=", "false");
                captureInterface->setProperty("coolerControl", false);
            }

            // The following steps require a connection to the INDI server
            if (isINDIConnected())
            {
                if (capCheck->isEnabled() && capCheck->isChecked())
                {
                    shutdownState = SHUTDOWN_PARK_CAP;
                    return false;
                }

                if (parkMountCheck->isEnabled() && parkMountCheck->isChecked())
                {
                    shutdownState = SHUTDOWN_PARK_MOUNT;
                    return false;
                }

                if (parkDomeCheck->isEnabled() && parkDomeCheck->isChecked())
                {
                    shutdownState = SHUTDOWN_PARK_DOME;
                    return false;
                }
            }
            else appendLogText(i18n("Warning: Bypassing parking procedures, no INDI connection."));

            if (shutdownScriptURL.isEmpty() == false)
            {
                shutdownState = SHUTDOWN_SCRIPT;
                return false;
            }

            shutdownState = SHUTDOWN_COMPLETE;
            return true;

        case SHUTDOWN_PARK_CAP:
            if (!isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                shutdownState = SHUTDOWN_SCRIPT;
            }
            else if (capCheck->isEnabled() && capCheck->isChecked())
                parkCap();
            else
                shutdownState = SHUTDOWN_PARK_MOUNT;
            break;

        case SHUTDOWN_PARKING_CAP:
            checkCapParkingStatus();
            break;

        case SHUTDOWN_PARK_MOUNT:
            if (!isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                shutdownState = SHUTDOWN_SCRIPT;
            }
            else if (parkMountCheck->isEnabled() && parkMountCheck->isChecked())
                parkMount();
            else
                shutdownState = SHUTDOWN_PARK_DOME;
            break;

        case SHUTDOWN_PARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case SHUTDOWN_PARK_DOME:
            if (!isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                shutdownState = SHUTDOWN_SCRIPT;
            }
            else if (parkDomeCheck->isEnabled() && parkDomeCheck->isChecked())
                parkDome();
            else
                shutdownState = SHUTDOWN_SCRIPT;
            break;

        case SHUTDOWN_PARKING_DOME:
            checkDomeParkingStatus();
            break;

        case SHUTDOWN_SCRIPT:
            if (shutdownScriptURL.isEmpty() == false)
            {
                // Need to stop Ekos now before executing script if it happens to stop INDI
                if (ekosState != EKOS_IDLE && Options::shutdownScriptTerminatesINDI())
                {
                    stopEkos();
                    return false;
                }

                shutdownState = SHUTDOWN_SCRIPT_RUNNING;
                executeScript(shutdownScriptURL.toString(QUrl::PreferLocalFile));
            }
            else
                shutdownState = SHUTDOWN_COMPLETE;
            break;

        case SHUTDOWN_SCRIPT_RUNNING:
            return false;

        case SHUTDOWN_COMPLETE:
            return completeShutdown();

        case SHUTDOWN_ERROR:
            stop();
            return true;
    }

    return false;
}

bool Scheduler::checkParkWaitState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    if (parkWaitState == PARKWAIT_IDLE)
        return true;

    // qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking Park Wait State...";

    switch (parkWaitState)
    {
        case PARKWAIT_PARK:
            parkMount();
            break;

        case PARKWAIT_PARKING:
            checkMountParkingStatus();
            break;

        case PARKWAIT_UNPARK:
            unParkMount();
            break;

        case PARKWAIT_UNPARKING:
            checkMountParkingStatus();
            break;

        case PARKWAIT_IDLE:
        case PARKWAIT_PARKED:
        case PARKWAIT_UNPARKED:
            return true;

        case PARKWAIT_ERROR:
            appendLogText(i18n("park/unpark wait procedure failed, aborting..."));
            stop();
            return true;

    }

    return false;
}

void Scheduler::executeScript(const QString &filename)
{
    appendLogText(i18n("Executing script %1...", filename));

    connect(&scriptProcess, &QProcess::readyReadStandardOutput, this, &Scheduler::readProcessOutput);

    connect(&scriptProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus)
    {
        checkProcessExit(exitCode);
    });

    scriptProcess.start(filename);
}

void Scheduler::readProcessOutput()
{
    appendLogText(scriptProcess.readAllStandardOutput().simplified());
}

void Scheduler::checkProcessExit(int exitCode)
{
    scriptProcess.disconnect();

    if (exitCode == 0)
    {
        if (startupState == STARTUP_SCRIPT)
            startupState = STARTUP_UNPARK_DOME;
        else if (shutdownState == SHUTDOWN_SCRIPT_RUNNING)
            shutdownState = SHUTDOWN_COMPLETE;

        return;
    }

    if (startupState == STARTUP_SCRIPT)
    {
        appendLogText(i18n("Startup script failed, aborting..."));
        startupState = STARTUP_ERROR;
    }
    else if (shutdownState == SHUTDOWN_SCRIPT_RUNNING)
    {
        appendLogText(i18n("Shutdown script failed, aborting..."));
        shutdownState = SHUTDOWN_ERROR;
    }
}

bool Scheduler::completeShutdown()
{
    // If INDI is not done disconnecting, try again later
    if (indiState == INDI_DISCONNECTING && checkINDIState() == false)
        return false;

    // Disconnect INDI if required first
    if (indiState != INDI_IDLE && Options::stopEkosAfterShutdown())
    {
        disconnectINDI();
        return false;
    }

    // If Ekos is not done stopping, try again later
    if (ekosState == EKOS_STOPPING && checkEkosState() == false)
        return false;

    // Stop Ekos if required.
    if (ekosState != EKOS_IDLE && Options::stopEkosAfterShutdown())
    {
        stopEkos();
        return false;
    }

    if (shutdownState == SHUTDOWN_COMPLETE)
        appendLogText(i18n("Shutdown complete."));
    else
        appendLogText(i18n("Shutdown procedure failed, aborting..."));

    // Stop Scheduler
    stop();

    return true;
}

bool Scheduler::checkStatus()
{
    for (auto job : jobs)
        job->updateJobCells();

    if (state == SCHEDULER_PAUSED)
    {
        if (currentJob == nullptr)
        {
            setPaused();
            return false;
        }
        switch (currentJob->getState())
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
    if (currentJob == nullptr)
    {
        // #2.1 If shutdown is already complete or in error, we need to stop
        if (shutdownState == SHUTDOWN_COMPLETE || shutdownState == SHUTDOWN_ERROR)
        {
            return completeShutdown();
        }

        // #2.2  Check if shutdown is in progress
        if (shutdownState > SHUTDOWN_IDLE)
        {
            // If Ekos is not done stopping, try again later
            if (ekosState == EKOS_STOPPING && checkEkosState() == false)
                return false;

            checkShutdownState();
            return false;
        }

        // #2.3 Check if park wait procedure is in progress
        if (checkParkWaitState() == false)
            return false;

        // #2.4 If not in shutdown state, evaluate the jobs
        evaluateJobs(false);

        // #2.5 If there is no current job after evaluation, shutdown
        if (nullptr == currentJob)
        {
            checkShutdownState();
            return false;
        }
    }
    // JM 2018-12-07: Check if we need to sleep
    else if (shouldSchedulerSleep(currentJob) == false)
    {
        // #3 Check if startup procedure has failed.
        if (startupState == STARTUP_ERROR)
        {
            // Stop Scheduler
            stop();
            return true;
        }

        // #4 Check if startup procedure Phase #1 is complete (Startup script)
        if ((startupState == STARTUP_IDLE && checkStartupState() == false) || startupState == STARTUP_SCRIPT)
            return false;

        // #5 Check if Ekos is started
        if (checkEkosState() == false)
            return false;

        // #6 Check if INDI devices are connected.
        if (checkINDIState() == false)
            return false;

        // #6.1 Check if park wait procedure is in progress - in the case we're waiting for a distant job
        if (checkParkWaitState() == false)
            return false;

        // #7 Check if startup procedure Phase #2 is complete (Unparking phase)
        if (startupState > STARTUP_SCRIPT && startupState < STARTUP_ERROR && checkStartupState() == false)
            return false;

        // #8 Check it it already completed (should only happen starting a paused job)
        //    Find the next job in this case, otherwise execute the current one
        if (currentJob->getState() == SchedulerJob::JOB_COMPLETE)
            findNextJob();
        else
            executeJob(currentJob);
    }

    return true;
}

void Scheduler::checkJobStage()
{
    Q_ASSERT_X(currentJob, __FUNCTION__, "Actual current job is required to check job stage");
    if (!currentJob)
        return;

    if (checkJobStageCounter == 0)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking job stage for" << currentJob->getName() << "startup" <<
                                       currentJob->getStartupCondition() << currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()) <<
                                       "state" << currentJob->getState();
        if (checkJobStageCounter++ == 30)
            checkJobStageCounter = 0;
    }

    QDateTime const now = getLocalTime();

    /* Refresh the score of the current job */
    /* currentJob->setScore(calculateJobScore(currentJob, now)); */

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
        if (now < currentJob->getStartupTime())
            return;

    // #1 Check if we need to stop at some point
    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT &&
            currentJob->getState() == SchedulerJob::JOB_BUSY)
    {
        // If the job reached it COMPLETION time, we stop it.
        if (now.secsTo(currentJob->getCompletionTime()) <= 0)
        {
            appendLogText(i18n("Job '%1' reached completion time %2, stopping.", currentJob->getName(),
                               currentJob->getCompletionTime().toString(currentJob->getDateTimeDisplayFormat())));
            currentJob->setState(SchedulerJob::JOB_COMPLETE);
            stopCurrentJobAction();
            findNextJob();
            return;
        }
    }

    // #2 Check if altitude restriction still holds true
    if (currentJob->hasAltitudeConstraint())
    {
        SkyPoint p = currentJob->getTargetCoords();

        p.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

        /* FIXME: find a way to use altitude cutoff here, because the job can be scheduled when evaluating, then aborted when running */
        const double altitudeConstraint = currentJob->getMinAltitudeConstraint(p.az().Degrees());
        if (p.alt().Degrees() < altitudeConstraint)
        {
            // Only terminate job due to altitude limitation if mount is NOT parked.
            if (isMountParked() == false)
            {
                appendLogText(i18n("Job '%1' current altitude (%2 degrees) crossed minimum constraint altitude (%3 degrees), "
                                   "marking idle.", currentJob->getName(),
                                   QString("%L1").arg(p.alt().Degrees(), 0, 'f', minAltitude->decimals()),
                                   QString("%L1").arg(altitudeConstraint, 0, 'f', minAltitude->decimals())));

                currentJob->setState(SchedulerJob::JOB_IDLE);
                stopCurrentJobAction();
                findNextJob();
                return;
            }
        }
    }

    // #3 Check if moon separation is still valid
    if (currentJob->getMinMoonSeparation() > 0)
    {
        SkyPoint p = currentJob->getTargetCoords();
        p.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

        double moonSeparation = currentJob->getCurrentMoonSeparation();

        if (moonSeparation < currentJob->getMinMoonSeparation())
        {
            // Only terminate job due to moon separation limitation if mount is NOT parked.
            if (isMountParked() == false)
            {
                appendLogText(i18n("Job '%2' current moon separation (%1 degrees) is lower than minimum constraint (%3 "
                                   "degrees), marking idle.",
                                   moonSeparation, currentJob->getName(), currentJob->getMinMoonSeparation()));

                currentJob->setState(SchedulerJob::JOB_IDLE);
                stopCurrentJobAction();
                findNextJob();
                return;
            }
        }
    }

    // #4 Check if we're not at dawn - dawn is still next event before dusk, and early dawn is past
    if (currentJob->getEnforceTwilight() && ((Dawn < Dusk && preDawnDateTime < now) || (Dusk < Dawn)))
    {
        // If either mount or dome are not parked, we shutdown if we approach dawn
        if (isMountParked() == false || (parkDomeCheck->isEnabled() && isDomeParked() == false))
        {
            // Minute is a DOUBLE value, do not use i18np
            appendLogText(i18n(
                              "Job '%3' is now approaching astronomical twilight rise limit at %1 (%2 minutes safety margin), marking idle.",
                              preDawnDateTime.toString(), abs(Options::preDawnTime()), currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_IDLE);
            stopCurrentJobAction();
            findNextJob();
            return;
        }
    }

    // #5 Check system status to improve robustness
    // This handles external events such as disconnections or end-user manipulating INDI panel
    if (!checkStatus())
        return;

    // #5b Check the guiding timer, and possibly restart guiding.
    processGuidingTimer();

    // #6 Check each stage is processing properly
    // FIXME: Vanishing property should trigger a call to its event callback
    switch (currentJob->getStage())
    {
        case SchedulerJob::STAGE_IDLE:
            getNextAction();
            break;

        case SchedulerJob::STAGE_ALIGNING:
            // Let's make sure align module does not become unresponsive
            if (getCurrentOperationMsec() > static_cast<int>(ALIGN_INACTIVITY_TIMEOUT))
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "alignInterface:property", "status");
                QVariant const status = alignInterface->property("status");
                TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
                Ekos::AlignState alignStatus = static_cast<Ekos::AlignState>(status.toInt());

                if (alignStatus == Ekos::ALIGN_IDLE)
                {
                    if (alignFailureCount++ < MAX_FAILURE_ATTEMPTS)
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "Align module timed out. Restarting request...";
                        startAstrometry();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' alignment procedure failed, marking aborted.", currentJob->getName()));
                        currentJob->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else
                    startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_CAPTURING:
            // Let's make sure capture module does not become unresponsive
            if (getCurrentOperationMsec() > static_cast<int>(CAPTURE_INACTIVITY_TIMEOUT))
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:property", "status");
                QVariant const status = captureInterface->property("status");
                TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
                Ekos::CaptureState captureStatus = static_cast<Ekos::CaptureState>(status.toInt());

                if (captureStatus == Ekos::CAPTURE_IDLE)
                {
                    if (captureFailureCount++ < MAX_FAILURE_ATTEMPTS)
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "capture module timed out. Restarting request...";
                        startCapture();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' capture procedure failed, marking aborted.", currentJob->getName()));
                        currentJob->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_FOCUSING:
            // Let's make sure focus module does not become unresponsive
            if (getCurrentOperationMsec() > static_cast<int>(FOCUS_INACTIVITY_TIMEOUT))
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "focusInterface:property", "status");
                QVariant const status = focusInterface->property("status");
                TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
                Ekos::FocusState focusStatus = static_cast<Ekos::FocusState>(status.toInt());

                if (focusStatus == Ekos::FOCUS_IDLE || focusStatus == Ekos::FOCUS_WAITING)
                {
                    if (focusFailureCount++ < MAX_FAILURE_ATTEMPTS)
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus module timed out. Restarting request...";
                        startFocusing();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking aborted.", currentJob->getName()));
                        currentJob->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_GUIDING:
            // Let's make sure guide module does not become unresponsive
            if (getCurrentOperationMsec() > GUIDE_INACTIVITY_TIMEOUT)
            {
                GuideState guideStatus = getGuidingStatus();

                if (guideStatus == Ekos::GUIDE_IDLE || guideStatus == Ekos::GUIDE_CONNECTED || guideStatus == Ekos::GUIDE_DISCONNECTED)
                {
                    if (guideFailureCount++ < MAX_FAILURE_ATTEMPTS)
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "guide module timed out. Restarting request...";
                        startGuiding();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' guiding procedure failed, marking aborted.", currentJob->getName()));
                        currentJob->setState(SchedulerJob::JOB_ABORTED);
                        findNextJob();
                    }
                }
                else startCurrentOperationTimer();
            }
            break;

        case SchedulerJob::STAGE_SLEWING:
        case SchedulerJob::STAGE_RESLEWING:
            // While slewing or re-slewing, check slew status can still be obtained
        {
            TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "status");
            QVariant const slewStatus = mountInterface->property("status");
            TEST_PRINT(stderr, "  @@@dbus received %d\n", !slewStatus.isValid() ? -1 : slewStatus.toInt());

            if (slewStatus.isValid())
            {
                // Send the slew status periodically to avoid the situation where the mount is already at location and does not send any event
                // FIXME: in that case, filter TRACKING events only?
                ISD::Telescope::Status const status = static_cast<ISD::Telescope::Status>(slewStatus.toInt());
                setMountStatus(status);
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' lost connection to the mount, attempting to reconnect.", currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
                return;
            }
        }
        break;

        case SchedulerJob::STAGE_SLEW_COMPLETE:
        case SchedulerJob::STAGE_RESLEWING_COMPLETE:
            // When done slewing or re-slewing and we use a dome, only shift to the next action when the dome is done moving
            if (m_DomeReady)
            {
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "isMoving");
                QVariant const isDomeMoving = domeInterface->property("isMoving");
                TEST_PRINT(stderr, "  @@@dbus received %s\n",
                           !isDomeMoving.isValid() ? "invalid" : (isDomeMoving.value<bool>() ? "T" : "F"));

                if (!isDomeMoving.isValid())
                {
                    appendLogText(i18n("Warning: job '%1' lost connection to the dome, attempting to reconnect.", currentJob->getName()));
                    if (!manageConnectionLoss())
                        currentJob->setState(SchedulerJob::JOB_ERROR);
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

    switch (currentJob->getStage())
    {
        case SchedulerJob::STAGE_IDLE:
            if (currentJob->getLightFramesRequired())
            {
                if (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK)
                    startSlew();
                else if (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS && autofocusCompleted == false)
                {
                    qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 3485";
                    startFocusing();
                }
                else if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                    startAstrometry();
                else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                    if (getGuidingStatus() == GUIDE_GUIDING)
                    {
                        appendLogText(i18n("Guiding already running, directly start capturing."));
                        startCapture();
                    }
                    else
                        startGuiding();
                else
                    startCapture();
            }
            else
            {
                if (currentJob->getStepPipeline())
                    appendLogText(
                        i18n("Job '%1' is proceeding directly to capture stage because only calibration frames are pending.",
                             currentJob->getName()));
                startCapture();
            }

            break;

        case SchedulerJob::STAGE_SLEW_COMPLETE:
            if (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS && autofocusCompleted == false)
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 3514";
                startFocusing();
            }
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                startAstrometry();
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_FOCUS_COMPLETE:
            if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                startAstrometry();
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_ALIGN_COMPLETE:
            currentJob->setStage(SchedulerJob::STAGE_RESLEWING);
            break;

        case SchedulerJob::STAGE_RESLEWING_COMPLETE:
            // If we have in-sequence-focus in the sequence file then we perform post alignment focusing so that the focus
            // frame is ready for the capture module in-sequence-focus procedure.
            if ((currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS) && currentJob->getInSequenceFocus())
                // Post alignment re-focusing
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 3544";
                startFocusing();
            }
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE:
            if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_GUIDING_COMPLETE:
            startCapture();
            break;

        default:
            break;
    }
}

void Scheduler::stopCurrentJobAction()
{
    if (nullptr != currentJob)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job '" << currentJob->getName() << "' is stopping current action..." <<
                                       currentJob->getStage();

        switch (currentJob->getStage())
        {
            case SchedulerJob::STAGE_IDLE:
                break;

            case SchedulerJob::STAGE_SLEWING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "abort");
                mountInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_FOCUSING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "focusInterface:call", "abort");
                focusInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_ALIGNING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "alignInterface:call", "abort");
                alignInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_CAPTURING:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:call", "abort");
                captureInterface->call(QDBus::AutoDetect, "abort");
                break;

            default:
                break;
        }

        /* Reset interrupted job stage */
        currentJob->setStage(SchedulerJob::STAGE_IDLE);
    }

    /* Guiding being a parallel process, check to stop it */
    stopGuiding();
}

bool Scheduler::manageConnectionLoss()
{
    if (SCHEDULER_RUNNING != state)
        return false;

    // Don't manage loss if Ekos is actually down in the state machine
    switch (ekosState)
    {
        case EKOS_IDLE:
        case EKOS_STOPPING:
            return false;

        default:
            break;
    }

    // Don't manage loss if INDI is actually down in the state machine
    switch (indiState)
    {
        case INDI_IDLE:
        case INDI_DISCONNECTING:
            return false;

        default:
            break;
    }

    // If Ekos is assumed to be up, check its state
    //QDBusReply<int> const isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
    if (m_EkosCommunicationStatus == Ekos::Success)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Ekos is currently connected, checking INDI before mitigating connection loss.");

        // If INDI is assumed to be up, check its state
        if (isINDIConnected())
        {
            // If both Ekos and INDI are assumed up, and are actually up, no mitigation needed, this is a DBus interface error
            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("INDI is currently connected, no connection loss mitigation needed.");
            return false;
        }
    }

    // Stop actions of the current job
    stopCurrentJobAction();

    // Acknowledge INDI and Ekos disconnections
    disconnectINDI();
    stopEkos();

    // Let the Scheduler attempt to connect INDI again
    return true;
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

    qDeleteAll(jobs);
    jobs.clear();
}

bool Scheduler::loadScheduler(const QString &fileURL)
{
    removeAllJobs();
    return appendEkosScheduleList(fileURL);
}

bool Scheduler::appendEkosScheduleList(const QString &fileURL)
{
    SchedulerState const old_state = state;
    state = SCHEDULER_LOADING;

    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        state = old_state;
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
                else if (!strcmp(tag, "Profile"))
                {
                    schedulerProfileCombo->setCurrentText(pcdataXMLEle(ep));
                }
                else if (!strcmp(tag, "ErrorHandlingStrategy"))
                {
                    setErrorHandlingStrategy(static_cast<ErrorHandlingStrategy>(cLocale.toInt(findXMLAttValu(ep, "value"))));

                    subEP = findXMLEle(ep, "delay");
                    if (subEP)
                    {
                        errorHandlingDelaySB->setValue(cLocale.toInt(pcdataXMLEle(subEP)));
                    }
                    subEP = findXMLEle(ep, "RescheduleErrors");
                    errorHandlingRescheduleErrorsCB->setChecked(subEP != nullptr);
                }
                else if (!strcmp(tag, "StartupProcedure"))
                {
                    XMLEle *procedure;
                    startupScript->clear();
                    unparkDomeCheck->setChecked(false);
                    unparkMountCheck->setChecked(false);
                    uncapCheck->setChecked(false);

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = pcdataXMLEle(procedure);

                        if (!strcmp(proc, "StartupScript"))
                        {
                            startupScript->setText(findXMLAttValu(procedure, "value"));
                            startupScriptURL = QUrl::fromUserInput(startupScript->text());
                        }
                        else if (!strcmp(proc, "UnparkDome"))
                            unparkDomeCheck->setChecked(true);
                        else if (!strcmp(proc, "UnparkMount"))
                            unparkMountCheck->setChecked(true);
                        else if (!strcmp(proc, "UnparkCap"))
                            uncapCheck->setChecked(true);
                    }
                }
                else if (!strcmp(tag, "ShutdownProcedure"))
                {
                    XMLEle *procedure;
                    shutdownScript->clear();
                    warmCCDCheck->setChecked(false);
                    parkDomeCheck->setChecked(false);
                    parkMountCheck->setChecked(false);
                    capCheck->setChecked(false);

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = pcdataXMLEle(procedure);

                        if (!strcmp(proc, "ShutdownScript"))
                        {
                            shutdownScript->setText(findXMLAttValu(procedure, "value"));
                            shutdownScriptURL = QUrl::fromUserInput(shutdownScript->text());
                        }
                        else if (!strcmp(proc, "ParkDome"))
                            parkDomeCheck->setChecked(true);
                        else if (!strcmp(proc, "ParkMount"))
                            parkMountCheck->setChecked(true);
                        else if (!strcmp(proc, "ParkCap"))
                            capCheck->setChecked(true);
                        else if (!strcmp(proc, "WarmCCD"))
                            warmCCDCheck->setChecked(true);
                    }
                }
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            state = old_state;
            return false;
        }
    }

    schedulerURL = QUrl::fromLocalFile(fileURL);
    mosaicB->setEnabled(true);
    mDirty = false;
    delLilXML(xmlParser);
    // update save button tool tip
    queueSaveB->setToolTip("Save schedule to " + schedulerURL.fileName());


    state = old_state;
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
    rotationSpin->setValue(0);

    // We expect all data read from the XML to be in the C locale - QLocale::c()
    QLocale cLocale = QLocale::c();

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Name"))
            nameEdit->setText(pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Priority"))
            prioritySpin->setValue(atoi(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Coordinates"))
        {
            subEP = findXMLEle(ep, "J2000RA");
            if (subEP)
            {
                dms ra;
                ra.setH(cLocale.toDouble(pcdataXMLEle(subEP)));
                raBox->showInHours(ra);
            }
            subEP = findXMLEle(ep, "J2000DE");
            if (subEP)
            {
                dms de;
                de.setD(cLocale.toDouble(pcdataXMLEle(subEP)));
                decBox->showInDegrees(de);
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
        else if (!strcmp(tagXMLEle(ep), "Rotation"))
        {
            rotationSpin->setValue(cLocale.toDouble(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "StartupCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("ASAP", pcdataXMLEle(subEP)))
                    asapConditionR->setChecked(true);
                else if (!strcmp("Culmination", pcdataXMLEle(subEP)))
                {
                    culminationConditionR->setChecked(true);
                    culminationOffset->setValue(cLocale.toDouble(findXMLAttValu(subEP, "value")));
                }
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
    if (mDirty == false && !schedulerURL.isEmpty())
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

        mDirty = false;
        // update save button tool tip
        queueSaveB->setToolTip("Save schedule to " + schedulerURL.fileName());
    }
    else
    {
        QString message = i18n("Invalid URL: %1", schedulerURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }
}

bool Scheduler::saveScheduler(const QUrl &fileURL)
{
    QFile file;
    file.setFileName(fileURL.toLocalFile());

    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    QTextStream outstream(&file);

    // We serialize sequence data to XML using the C locale
    QLocale cLocale = QLocale::c();

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<SchedulerList version='1.4'>" << endl;
    // ensure to escape special XML characters
    outstream << "<Profile>" << QString(entityXML(strdup(schedulerProfileCombo->currentText().toStdString().c_str()))) << "</Profile>" << endl;

    foreach (SchedulerJob *job, jobs)
    {
        outstream << "<Job>" << endl;

        // ensure to escape special XML characters
        outstream << "<Name>" << QString(entityXML(strdup(job->getName().toStdString().c_str()))) << "</Name>" << endl;
        outstream << "<Priority>" << job->getPriority() << "</Priority>" << endl;
        outstream << "<Coordinates>" << endl;
        outstream << "<J2000RA>" << cLocale.toString(job->getTargetCoords().ra0().Hours()) << "</J2000RA>" << endl;
        outstream << "<J2000DE>" << cLocale.toString(job->getTargetCoords().dec0().Degrees()) << "</J2000DE>" << endl;
        outstream << "</Coordinates>" << endl;

        if (job->getFITSFile().isValid() && job->getFITSFile().isEmpty() == false)
            outstream << "<FITS>" << job->getFITSFile().toLocalFile() << "</FITS>" << endl;
        else
            outstream << "<Rotation>" << job->getRotation() << "</Rotation>" << endl;

        outstream << "<Sequence>" << job->getSequenceFile().toLocalFile() << "</Sequence>" << endl;

        outstream << "<StartupCondition>" << endl;
        if (job->getFileStartupCondition() == SchedulerJob::START_ASAP)
            outstream << "<Condition>ASAP</Condition>" << endl;
        else if (job->getFileStartupCondition() == SchedulerJob::START_CULMINATION)
            outstream << "<Condition value='" << cLocale.toString(job->getCulminationOffset()) << "'>Culmination</Condition>" << endl;
        else if (job->getFileStartupCondition() == SchedulerJob::START_AT)
            outstream << "<Condition value='" << job->getFileStartupTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << endl;
        outstream << "</StartupCondition>" << endl;

        outstream << "<Constraints>" << endl;
        if (job->hasMinAltitude())
            outstream << "<Constraint value='" << cLocale.toString(job->getMinAltitude()) << "'>MinimumAltitude</Constraint>" << endl;
        if (job->getMinMoonSeparation() > 0)
            outstream << "<Constraint value='" << cLocale.toString(job->getMinMoonSeparation()) << "'>MoonSeparation</Constraint>"
                      << endl;
        if (job->getEnforceWeather())
            outstream << "<Constraint>EnforceWeather</Constraint>" << endl;
        if (job->getEnforceTwilight())
            outstream << "<Constraint>EnforceTwilight</Constraint>" << endl;
        if (job->getEnforceArtificialHorizon())
            outstream << "<Constraint>EnforceArtificialHorizon</Constraint>" << endl;
        outstream << "</Constraints>" << endl;

        outstream << "<CompletionCondition>" << endl;
        if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
            outstream << "<Condition>Sequence</Condition>" << endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
            outstream << "<Condition value='" << cLocale.toString(job->getRepeatsRequired()) << "'>Repeat</Condition>" << endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
            outstream << "<Condition>Loop</Condition>" << endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
            outstream << "<Condition value='" << job->getCompletionTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << endl;
        outstream << "</CompletionCondition>" << endl;

        outstream << "<Steps>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_TRACK)
            outstream << "<Step>Track</Step>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_FOCUS)
            outstream << "<Step>Focus</Step>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_ALIGN)
            outstream << "<Step>Align</Step>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_GUIDE)
            outstream << "<Step>Guide</Step>" << endl;
        outstream << "</Steps>" << endl;

        outstream << "</Job>" << endl;
    }

    outstream << "<ErrorHandlingStrategy value='" << getErrorHandlingStrategy() << "'>" << endl;
    if (errorHandlingRescheduleErrorsCB->isChecked())
        outstream << "<RescheduleErrors />" << endl;
    outstream << "<delay>" << errorHandlingDelaySB->value() << "</delay>" << endl;
    outstream << "</ErrorHandlingStrategy>" << endl;

    outstream << "<StartupProcedure>" << endl;
    if (startupScript->text().isEmpty() == false)
        outstream << "<Procedure value='" << startupScript->text() << "'>StartupScript</Procedure>" << endl;
    if (unparkDomeCheck->isChecked())
        outstream << "<Procedure>UnparkDome</Procedure>" << endl;
    if (unparkMountCheck->isChecked())
        outstream << "<Procedure>UnparkMount</Procedure>" << endl;
    if (uncapCheck->isChecked())
        outstream << "<Procedure>UnparkCap</Procedure>" << endl;
    outstream << "</StartupProcedure>" << endl;

    outstream << "<ShutdownProcedure>" << endl;
    if (warmCCDCheck->isChecked())
        outstream << "<Procedure>WarmCCD</Procedure>" << endl;
    if (capCheck->isChecked())
        outstream << "<Procedure>ParkCap</Procedure>" << endl;
    if (parkMountCheck->isChecked())
        outstream << "<Procedure>ParkMount</Procedure>" << endl;
    if (parkDomeCheck->isChecked())
        outstream << "<Procedure>ParkDome</Procedure>" << endl;
    if (shutdownScript->text().isEmpty() == false)
        outstream << "<Procedure value='" << shutdownScript->text() << "'>ShutdownScript</Procedure>" << endl;
    outstream << "</ShutdownProcedure>" << endl;

    outstream << "</SchedulerList>" << endl;

    appendLogText(i18n("Scheduler list saved to %1", fileURL.toLocalFile()));
    file.close();
    return true;
}

void Scheduler::startSlew()
{
    Q_ASSERT_X(nullptr != currentJob, __FUNCTION__, "Job starting slewing must be valid");

    // If the mount was parked by a pause or the end-user, unpark
    if (isMountParked())
    {
        parkWaitState = PARKWAIT_UNPARK;
        return;
    }

    if (Options::resetMountModelBeforeJob())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "resetModel");
        mountInterface->call(QDBus::AutoDetect, "resetModel");
    }

    SkyPoint target = currentJob->getTargetCoords();
    QList<QVariant> telescopeSlew;
    telescopeSlew.append(target.ra().Hours());
    telescopeSlew.append(target.dec().Degrees());

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s%s,%s\n", __LINE__, "mountInterface:callWithArgs", "slew: ",
               target.ra().toHMSString().toLatin1().data(), target.dec().toDMSString().toLatin1().data());
    QDBusReply<bool> const slewModeReply = mountInterface->callWithArgumentList(QDBus::AutoDetect, "slew", telescopeSlew);
    TEST_PRINT(stderr, "  @@@dbus received %s\n", slewModeReply.error().type() == QDBusError::NoError ? "no error" : "error");

    if (slewModeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' slew request received DBUS error: %2").arg(
                                              currentJob->getName(), QDBusError::errorString(slewModeReply.error().type()));
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
    }
    else
    {
        currentJob->setStage(SchedulerJob::STAGE_SLEWING);
        appendLogText(i18n("Job '%1' is slewing to target.", currentJob->getName()));
    }
}

void Scheduler::startFocusing()
{
    Q_ASSERT_X(nullptr != currentJob, __FUNCTION__, "Job starting focusing must be valid");

    // 2017-09-30 Jasem: We're skipping post align focusing now as it can be performed
    // when first focus request is made in capture module
    if (currentJob->getStage() == SchedulerJob::STAGE_RESLEWING_COMPLETE ||
            currentJob->getStage() == SchedulerJob::STAGE_POSTALIGN_FOCUSING)
    {
        // Clear the HFR limit value set in the capture module
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "captureInterface", "clearAutoFocusHFR");
        captureInterface->call(QDBus::AutoDetect, "clearAutoFocusHFR");
        // Reset Focus frame so that next frame take a full-resolution capture first.
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "focusInterface", "resetFrame");
        focusInterface->call(QDBus::AutoDetect, "resetFrame");
        currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE);
        getNextAction();
        return;
    }

    // Check if autofocus is supported
    QDBusReply<bool> focusModeReply;
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "focusInterface", "canAutoFocus");
    focusModeReply = focusInterface->call(QDBus::AutoDetect, "canAutoFocus");

    if (focusModeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' canAutoFocus request received DBUS error: %2").arg(
                                              currentJob->getName(), QDBusError::errorString(focusModeReply.error().type()));
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
        return;
    }

    if (focusModeReply.value() == false)
    {
        appendLogText(i18n("Warning: job '%1' is unable to proceed with autofocus, not supported.", currentJob->getName()));
        currentJob->setStepPipeline(
            static_cast<SchedulerJob::StepPipeline>(currentJob->getStepPipeline() & ~SchedulerJob::USE_FOCUS));
        currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);
        getNextAction();
        return;
    }

    // Clear the HFR limit value set in the capture module
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "captureInterface", "clearAutoFocusHFR");
    captureInterface->call(QDBus::AutoDetect, "clearAutoFocusHFR");

    QDBusMessage reply;

    // We always need to reset frame first
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "focusInterface", "resetFrame");
    if ((reply = focusInterface->call(QDBus::AutoDetect, "resetFrame")).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' resetFrame request received DBUS error: %2").arg(
                                              currentJob->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
        return;
    }

    // Set autostar if full field option is false
    if (Options::focusUseFullField() == false)
    {
        QList<QVariant> autoStar;
        autoStar.append(true);
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "focusInterface", "setAutoStarEnabled");
        if ((reply = focusInterface->callWithArgumentList(QDBus::AutoDetect, "setAutoStarEnabled", autoStar)).type() ==
                QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setAutoFocusStar request received DBUS error: %1").arg(
                                                  currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }
    }

    // Start auto-focus
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "focusInterface", "start");
    if ((reply = focusInterface->call(QDBus::AutoDetect, "start")).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' startFocus request received DBUS error: %2").arg(
                                              currentJob->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
        return;
    }

    /*if (currentJob->getStage() == SchedulerJob::STAGE_RESLEWING_COMPLETE ||
        currentJob->getStage() == SchedulerJob::STAGE_POSTALIGN_FOCUSING)
    {
        currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING);
        appendLogText(i18n("Post-alignment focusing for %1 ...", currentJob->getName()));
    }
    else
    {
        currentJob->setStage(SchedulerJob::STAGE_FOCUSING);
        appendLogText(i18n("Focusing %1 ...", currentJob->getName()));
    }*/

    currentJob->setStage(SchedulerJob::STAGE_FOCUSING);
    appendLogText(i18n("Job '%1' is focusing.", currentJob->getName()));
    startCurrentOperationTimer();
}

void Scheduler::findNextJob()
{
    if (state == SCHEDULER_PAUSED)
    {
        // everything finished, we can pause
        setPaused();
        return;
    }

    Q_ASSERT_X(currentJob->getState() == SchedulerJob::JOB_ERROR ||
               currentJob->getState() == SchedulerJob::JOB_ABORTED ||
               currentJob->getState() == SchedulerJob::JOB_COMPLETE ||
               currentJob->getState() == SchedulerJob::JOB_IDLE,
               __FUNCTION__, "Finding next job requires current to be in error, aborted, idle or complete");

    // Reset failed count
    alignFailureCount = guideFailureCount = focusFailureCount = captureFailureCount = 0;

    /* FIXME: Other debug logs in that function probably */
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Find next job...";

    if (currentJob->getState() == SchedulerJob::JOB_ERROR || currentJob->getState() == SchedulerJob::JOB_ABORTED)
    {
        captureBatch = 0;
        // Stop Guiding if it was used
        stopGuiding();

        if (currentJob->getState() == SchedulerJob::JOB_ERROR)
            appendLogText(i18n("Job '%1' is terminated due to errors.", currentJob->getName()));
        else
            appendLogText(i18n("Job '%1' is aborted.", currentJob->getName()));

        // Always reset job stage
        currentJob->setStage(SchedulerJob::STAGE_IDLE);

        // restart aborted jobs immediately, if error handling strategy is set to "restart immediately"
        if (errorHandlingRestartImmediatelyButton->isChecked() &&
                (currentJob->getState() == SchedulerJob::JOB_ABORTED ||
                 (currentJob->getState() == SchedulerJob::JOB_ERROR && errorHandlingRescheduleErrorsCB->isChecked())))
        {
            // reset the state so that it will be restarted
            currentJob->setState(SchedulerJob::JOB_SCHEDULED);

            appendLogText(i18n("Waiting %1 seconds to restart job '%2'.", errorHandlingDelaySB->value(), currentJob->getName()));

            // wait the given delay until the jobs will be evaluated again
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_WAKEUP).toLatin1().data());
            setupNextIteration(RUN_WAKEUP, std::lround((errorHandlingDelaySB->value() * 1000) /
                               KStarsData::Instance()->clock()->scale()));
            sleepLabel->setToolTip(i18n("Scheduler waits for a retry."));
            sleepLabel->show();
            return;
        }

        // otherwise start re-evaluation
        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        setupNextIteration(RUN_SCHEDULER);
    }
    else if (currentJob->getState() == SchedulerJob::JOB_IDLE)
    {
        // job constraints no longer valid, start re-evaluation
        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        setupNextIteration(RUN_SCHEDULER);
    }
    // Job is complete, so check completion criteria to optimize processing
    // In any case, we're done whether the job completed successfully or not.
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
    {
        /* If we remember job progress, mark the job idle as well as all its duplicates for re-evaluation */
        if (Options::rememberJobProgress())
        {
            foreach(SchedulerJob *a_job, jobs)
                if (a_job == currentJob || a_job->isDuplicateOf(currentJob))
                    a_job->setState(SchedulerJob::JOB_IDLE);
        }

        captureBatch = 0;
        // Stop Guiding if it was used
        stopGuiding();

        appendLogText(i18n("Job '%1' is complete.", currentJob->getName()));

        // Always reset job stage
        currentJob->setStage(SchedulerJob::STAGE_IDLE);

        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        setupNextIteration(RUN_SCHEDULER);
    }
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
    {
        /* If the job is about to repeat, decrease its repeat count and reset its start time */
        if (0 < currentJob->getRepeatsRemaining())
        {
            currentJob->setRepeatsRemaining(currentJob->getRepeatsRemaining() - 1);
            currentJob->setStartupTime(QDateTime());
        }

        /* Mark the job idle as well as all its duplicates for re-evaluation */
        foreach(SchedulerJob *a_job, jobs)
            if (a_job == currentJob || a_job->isDuplicateOf(currentJob))
                a_job->setState(SchedulerJob::JOB_IDLE);

        /* Re-evaluate all jobs, without selecting a new job */
        evaluateJobs(true);

        /* If current job is actually complete because of previous duplicates, prepare for next job */
        if (currentJob == nullptr || currentJob->getRepeatsRemaining() == 0)
        {
            stopCurrentJobAction();

            if (currentJob != nullptr)
            {
                appendLogText(i18np("Job '%1' is complete after #%2 batch.",
                                    "Job '%1' is complete after #%2 batches.",
                                    currentJob->getName(), currentJob->getRepeatsRequired()));
                setCurrentJob(nullptr);
            }
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            setupNextIteration(RUN_SCHEDULER);
        }
        /* If job requires more work, continue current observation */
        else
        {
            /* FIXME: raise priority to allow other jobs to schedule in-between */
            executeJob(currentJob);

            /* JM 2020-08-23: If user opts to force realign instead of for each job then we force this FIRST */
            if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
            {
                stopGuiding();
                currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
                startAstrometry();
            }
            /* If we are guiding, continue capturing */
            else if ( (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE) )
            {
                currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
                startCapture();
            }
            /* If we are not guiding, but using alignment, realign */
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
            {
                currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
                startAstrometry();
            }
            /* Else if we are neither guiding nor using alignment, slew back to target */
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK)
            {
                currentJob->setStage(SchedulerJob::STAGE_SLEWING);
                startSlew();
            }
            /* Else just start capturing */
            else
            {
                currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
                startCapture();
            }

            appendLogText(i18np("Job '%1' is repeating, #%2 batch remaining.",
                                "Job '%1' is repeating, #%2 batches remaining.",
                                currentJob->getName(), currentJob->getRepeatsRemaining()));
            /* currentJob remains the same */
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
            setupNextIteration(RUN_JOBCHECK);
        }
    }
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        executeJob(currentJob);

        if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
        {
            stopGuiding();
            currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
            startAstrometry();
        }
        else
        {
            currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
            startCapture();
        }

        captureBatch++;

        appendLogText(i18n("Job '%1' is repeating, looping indefinitely.", currentJob->getName()));
        /* currentJob remains the same */
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
        setupNextIteration(RUN_JOBCHECK);
    }
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        if (getLocalTime().secsTo(currentJob->getCompletionTime()) <= 0)
        {
            /* Mark the job idle as well as all its duplicates for re-evaluation */
            foreach(SchedulerJob *a_job, jobs)
                if (a_job == currentJob || a_job->isDuplicateOf(currentJob))
                    a_job->setState(SchedulerJob::JOB_IDLE);
            stopCurrentJobAction();

            captureBatch = 0;

            appendLogText(i18np("Job '%1' stopping, reached completion time with #%2 batch done.",
                                "Job '%1' stopping, reached completion time with #%2 batches done.",
                                currentJob->getName(), captureBatch + 1));

            // Always reset job stage
            currentJob->setStage(SchedulerJob::STAGE_IDLE);

            setCurrentJob(nullptr);
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
            setupNextIteration(RUN_SCHEDULER);
        }
        else
        {
            executeJob(currentJob);

            if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
            {
                stopGuiding();
                currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
                startAstrometry();
            }
            else
            {
                currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
                startCapture();
            }

            captureBatch++;

            appendLogText(i18np("Job '%1' completed #%2 batch before completion time, restarted.",
                                "Job '%1' completed #%2 batches before completion time, restarted.",
                                currentJob->getName(), captureBatch));
            /* currentJob remains the same */
            TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_JOBCHECK).toLatin1().data());
            setupNextIteration(RUN_JOBCHECK);
        }
    }
    else
    {
        /* Unexpected situation, mitigate by resetting the job and restarting the scheduler timer */
        qCDebug(KSTARS_EKOS_SCHEDULER) << "BUGBUG! Job '" << currentJob->getName() << "' timer elapsed, but no action to be taken.";

        // Always reset job stage
        currentJob->setStage(SchedulerJob::STAGE_IDLE);

        setCurrentJob(nullptr);
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
        setupNextIteration(RUN_SCHEDULER);
    }
}

void Scheduler::startAstrometry()
{
    Q_ASSERT_X(nullptr != currentJob, __FUNCTION__, "Job starting aligning must be valid");

    QDBusMessage reply;
    setSolverAction(Align::GOTO_SLEW);

    // Always turn update coords on
    //QVariant arg(true);
    //alignInterface->call(QDBus::AutoDetect, "setUpdateCoords", arg);

    // If FITS file is specified, then we use load and slew
    if (currentJob->getFITSFile().isEmpty() == false)
    {
        QList<QVariant> solveArgs;
        solveArgs.append(currentJob->getFITSFile().toString(QUrl::PreferLocalFile));

        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "alignInterface", "loadAndSlew");
        if ((reply = alignInterface->callWithArgumentList(QDBus::AutoDetect, "loadAndSlew", solveArgs)).type() ==
                QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' loadAndSlew request received DBUS error: %2").arg(
                                                  currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }

        loadAndSlewProgress = true;
        appendLogText(i18n("Job '%1' is plate solving %2.", currentJob->getName(), currentJob->getFITSFile().fileName()));
    }
    else
    {
        // JM 2020.08.20: Send J2000 TargetCoords to Align module so that we always resort back to the
        // target original targets even if we drifted away due to any reason like guiding calibration failures.
        const SkyPoint targetCoords = currentJob->getTargetCoords();
        QList<QVariant> targetArgs, rotationArgs;
        targetArgs << targetCoords.ra0().Hours() << targetCoords.dec0().Degrees();
        rotationArgs << currentJob->getRotation();

        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "alignInterface", "setTargetCoords");
        if ((reply = alignInterface->callWithArgumentList(QDBus::AutoDetect, "setTargetCoords",
                     targetArgs)).type() == QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setTargetCoords request received DBUS error: %2").arg(
                                                  currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }

        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "alignInterface", "setTargetRotation");
        if ((reply = alignInterface->callWithArgumentList(QDBus::AutoDetect, "setTargetRotation",
                     rotationArgs)).type() == QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setTargetRotation request received DBUS error: %2").arg(
                                                  currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }

        TEST_PRINT(stderr, "sch%d @@@dbus(%s): sending %s\n", __LINE__, "alignInterface", "captureAndSolve");
        if ((reply = alignInterface->call(QDBus::AutoDetect, "captureAndSolve")).type() == QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' captureAndSolve request received DBUS error: %2").arg(
                                                  currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }

        appendLogText(i18n("Job '%1' is capturing and plate solving.", currentJob->getName()));
    }

    /* FIXME: not supposed to modify the job */
    currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
    startCurrentOperationTimer();
}

void Scheduler::startGuiding(bool resetCalibration)
{
    Q_ASSERT_X(nullptr != currentJob, __FUNCTION__, "Job starting guiding must be valid");

    // avoid starting the guider twice
    if (resetCalibration == false && getGuidingStatus() == GUIDE_GUIDING)
    {
        appendLogText(i18n("Guiding already running for %1 ...", currentJob->getName()));
        currentJob->setStage(SchedulerJob::STAGE_GUIDING);
        startCurrentOperationTimer();
        return;
    }

    // Connect Guider
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "guideInterface:call", "connectGuider");
    guideInterface->call(QDBus::AutoDetect, "connectGuider");

    // Set Auto Star to true
    QVariant arg(true);
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "guideInterface:call", "setCalibrationAutoStar");
    guideInterface->call(QDBus::AutoDetect, "setCalibrationAutoStar", arg);

    // Only reset calibration on trouble
    // and if we are allowed to reset calibration (true by default)
    if (resetCalibration && Options::resetGuideCalibration())
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "guideInterface:call", "clearCalibration");
        guideInterface->call(QDBus::AutoDetect, "clearCalibration");
    }

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "guideInterface:call", "guide");
    guideInterface->call(QDBus::AutoDetect, "guide");

    currentJob->setStage(SchedulerJob::STAGE_GUIDING);

    appendLogText(i18n("Starting guiding procedure for %1 ...", currentJob->getName()));

    startCurrentOperationTimer();
}

void Scheduler::startCapture(bool restart)
{
    Q_ASSERT_X(nullptr != currentJob, __FUNCTION__, "Job starting capturing must be valid");

    // ensure that guiding is running before we start capturing
    if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE && getGuidingStatus() != GUIDE_GUIDING)
    {
        // guiding should run, but it doesn't. So start guiding first
        currentJob->setStage(SchedulerJob::STAGE_GUIDING);
        startGuiding();
        return;
    }

    QString sanitized = currentJob->getName();
    sanitized = sanitized.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                // Remove any two or more __
                .replace( QRegularExpression("_{2,}"), "_")
                // Remove any _ at the end
                .replace( QRegularExpression("_$"), "");
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s%s\n", __LINE__, "captureInterface:setProperty", "targetName=",
               sanitized.toLatin1().data());
    captureInterface->setProperty("targetName", sanitized);

    QString url = currentJob->getSequenceFile().toLocalFile();

    if (restart == false)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(url);
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:callWithArgs", "loadSequenceQueue");
        QDBusReply<bool> const captureReply = captureInterface->callWithArgumentList(QDBus::AutoDetect, "loadSequenceQueue",
                                              dbusargs);
        if (captureReply.error().type() != QDBusError::NoError)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' loadSequenceQueue request received DBUS error: %1").arg(currentJob->getName()).arg(
                                                  captureReply.error().message());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }
        // Check if loading sequence fails for whatever reason
        else if (captureReply.value() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' loadSequenceQueue request failed").arg(currentJob->getName());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }
    }


    switch (currentJob->getCompletionCondition())
    {
        case SchedulerJob::FINISH_LOOP:
        case SchedulerJob::FINISH_AT:
            // In these cases, we leave the captured frames map empty
            // to ensure, that the capture sequence is executed in any case.
            break;

        default:
            // Scheduler always sets captured frame map when starting a sequence - count may be different, robustness, dynamic priority

            // hand over the map of captured frames so that the capture
            // process knows about existing frames
            SchedulerJob::CapturedFramesMap fMap = currentJob->getCapturedFramesMap();

            for (auto &e : fMap.keys())
            {
                QList<QVariant> dbusargs;
                QDBusMessage reply;

                dbusargs.append(e);
                dbusargs.append(fMap.value(e));
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:callWithArgs", "setCapturedFramesMap");
                if ((reply = captureInterface->callWithArgumentList(QDBus::AutoDetect, "setCapturedFramesMap", dbusargs)).type() ==
                        QDBusMessage::ErrorMessage)
                {
                    qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                                      QString("Warning: job '%1' setCapturedFramesCount request received DBUS error: %1").arg(currentJob->getName()).arg(
                                                          reply.errorMessage());
                    if (!manageConnectionLoss())
                        currentJob->setState(SchedulerJob::JOB_ERROR);
                    return;
                }
            }
            break;
    }

    // Start capture process
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:call", "start");
    captureInterface->call(QDBus::AutoDetect, "start");

    currentJob->setStage(SchedulerJob::STAGE_CAPTURING);

    KNotification::event(QLatin1String("EkosScheduledImagingStart"),
                         i18n("Ekos job (%1) - Capture started", currentJob->getName()));

    if (captureBatch > 0)
        appendLogText(i18n("Job '%1' capture is in progress (batch #%2)...", currentJob->getName(), captureBatch + 1));
    else
        appendLogText(i18n("Job '%1' capture is in progress...", currentJob->getName()));

    startCurrentOperationTimer();
}

void Scheduler::stopGuiding()
{
    if (!guideInterface)
        return;

    // Tell guider to abort if the current job requires guiding - end-user may enable guiding manually before observation
    if (nullptr != currentJob && (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE))
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' is stopping guiding...").arg(currentJob->getName());
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "guideInterface:call", "abort");
        guideInterface->call(QDBus::AutoDetect, "abort");
        guideFailureCount = 0;
    }

    // In any case, stop the automatic guider restart
    if (isGuidingTimerActive())
        cancelGuidingTimer();
}

void Scheduler::setSolverAction(Align::GotoMode mode)
{
    QVariant gotoMode(static_cast<int>(mode));
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "alignInterface:call", "setSolverAction");
    alignInterface->call(QDBus::AutoDetect, "setSolverAction", gotoMode);
}

void Scheduler::disconnectINDI()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Disconnecting INDI...";
    indiState = INDI_DISCONNECTING;
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "ekosInterface:call", "disconnectDevices");
    ekosInterface->call(QDBus::AutoDetect, "disconnectDevices");
}

void Scheduler::stopEkos()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Stopping Ekos...";
    ekosState               = EKOS_STOPPING;
    ekosConnectFailureCount = 0;
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "ekosInterface:call", "stop");
    ekosInterface->call(QDBus::AutoDetect, "stop");
    m_MountReady = m_CapReady = m_CaptureReady = m_DomeReady = false;
}

void Scheduler::setDirty()
{
    mDirty = true;

    if (sender() == startupProcedureButtonGroup || sender() == shutdownProcedureGroup)
        return;

    if (0 <= jobUnderEdit && state != SCHEDULER_RUNNING && 0 <= queueTable->currentRow())
    {
        // Now that jobs are sorted, reset jobs that are later than the edited one for re-evaluation
        for (int row = jobUnderEdit; row < jobs.size(); row++)
            jobs.at(row)->reset();

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
    mosaicB->setEnabled(addingOK);
}

void Scheduler::updateCompletedJobsCount(bool forced)
{
    /* Use a temporary map in order to limit the number of file searches */
    SchedulerJob::CapturedFramesMap newFramesCount;

    /* FIXME: Capture storage cache is refreshed too often, feature requires rework. */

    /* Check if one job is idle or requires evaluation - if so, force refresh */
    forced |= std::any_of(jobs.begin(), jobs.end(), [](SchedulerJob * oneJob) -> bool
    {
        SchedulerJob::JOBStatus const state = oneJob->getState();
        return state == SchedulerJob::JOB_IDLE || state == SchedulerJob::JOB_EVALUATION;});

    /* If update is forced, clear the frame map */
    if (forced)
        m_CapturedFramesCount.clear();

    /* Enumerate SchedulerJobs to count captures that are already stored */
    for (SchedulerJob *oneJob : jobs)
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
            if (oneSeqJob->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
                continue;

            /* FIXME: this signature path is incoherent when there is no filter wheel on the setup - bugfix should be elsewhere though */
            QString const signature = oneSeqJob->getSignature();

            /* If signature was processed during this run, keep it */
            if (newFramesCount.constEnd() != newFramesCount.constFind(signature))
                continue;

            /* If signature was processed during an earlier run, use the earlier count */
            QMap<QString, uint16_t>::const_iterator const earlierRunIterator = m_CapturedFramesCount.constFind(signature);
            if (m_CapturedFramesCount.constEnd() != earlierRunIterator)
            {
                newFramesCount[signature] = earlierRunIterator.value();
                continue;
            }

            /* Else recount captures already stored */
            newFramesCount[signature] = getCompletedFiles(signature, oneSeqJob->getFullPrefix());
        }

        // determine whether we need to continue capturing, depending on captured frames
        bool lightFramesRequired = false;
        switch (oneJob->getCompletionCondition())
        {
            case SchedulerJob::FINISH_SEQUENCE:
            case SchedulerJob::FINISH_REPEAT:
                for (SequenceJob *oneSeqJob : seqjobs)
                {
                    QString const signature = oneSeqJob->getSignature();
                    /* If frame is LIGHT, how hany do we have left? */
                    if (oneSeqJob->getFrameType() == FRAME_LIGHT
                            && oneSeqJob->getCount()*oneJob->getRepeatsRequired() > newFramesCount[signature])
                        lightFramesRequired = true;
                }
                break;
            default:
                // in all other cases it does not depend on the number of captured frames
                lightFramesRequired = true;
        }


        oneJob->setLightFramesRequired(lightFramesRequired);
    }

    m_CapturedFramesCount = newFramesCount;

    //if (forced)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Frame map summary:";
        QMap<QString, uint16_t>::const_iterator it = m_CapturedFramesCount.constBegin();
        for (; it != m_CapturedFramesCount.constEnd(); it++)
            qCDebug(KSTARS_EKOS_SCHEDULER) << " " << it.key() << ':' << it.value();
    }
}

bool Scheduler::estimateJobTime(SchedulerJob *schedJob, const QMap<QString, uint16_t> &capturedFramesCount,
                                Scheduler *scheduler)
{
    static SchedulerJob *jobWarned = nullptr;

    /* updateCompletedJobsCount(); */

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

    int totalCompletedCount = 0;
    double totalImagingTime  = 0;

    // Determine number of captures in the scheduler job
    int capturesPerRepeat = 0;
    QMap<QString, uint16_t> expected;
    foreach (SequenceJob *seqJob, seqJobs)
    {
        capturesPerRepeat += seqJob->getCount();
        QString const signature = seqJob->getSignature();
        expected[signature] = seqJob->getCount() + (expected.contains(signature) ? expected[signature] : 0);
    }

    // fill the captured frames map
    for (QString key : expected.keys())
    {
        if (rememberJobProgress)
        {
            int diff = expected[key] * schedJob->getRepeatsRequired() - capturedFramesCount[key];
            // captured more than required?
            if (diff <= 0)
                capture_map[key] = expected[key];
            // need more frames than one cycle could capture?
            else if (diff >= expected[key])
                capture_map[key] = 0;
            // else we know that 0 < diff < expected[key]
            else
                capture_map[key] = expected[key] - diff;
        }
        else
            capture_map[key] = 0;

        // collect all captured frames counts
        if (schedJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
            totalCompletedCount += capturedFramesCount[key];
        else
            totalCompletedCount += std::min(capturedFramesCount[key],
                                            static_cast<uint16_t>(expected[key] * schedJob->getRepeatsRequired()));
    }

    // Loop through sequence jobs to calculate the number of required frames and estimate duration.
    foreach (SequenceJob *seqJob, seqJobs)
    {
        // FIXME: find a way to actually display the filter name.
        QString seqName = i18n("Job '%1' %2x%3\" %4", schedJob->getName(), seqJob->getCount(), seqJob->getExposure(),
                               seqJob->getFilterName());

        if (seqJob->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
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
        int captures_required        = seqJob->getCount() * schedJob->getRepeatsRequired();
        int captures_completed       = capturedFramesCount[signature];

        if (rememberJobProgress && schedJob->getCompletionCondition() != SchedulerJob::FINISH_LOOP)
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

            // we start with the total value
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
        else if (0 < capturesPerRepeat)
        {
            captures_completed = schedJob->getCompletedCount() / capturesPerRepeat * seqJob->getCount();
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
        if (!areJobCapturesComplete)
        {
            unsigned int const captures_to_go = captures_required - captures_completed;
            totalImagingTime += fabs((seqJob->getExposure() + seqJob->getDelay()) * captures_to_go);

            /* If we have light frames to process, add focus/dithering delay */
            if (seqJob->getFrameType() == FRAME_LIGHT)
            {
                // If inSequenceFocus is true
                if (hasAutoFocus)
                {
                    // Wild guess that each in sequence auto focus takes an average of 30 seconds. It can take any where from 2 seconds to 2+ minutes.
                    // FIXME: estimating one focus per capture is probably not realistic.
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a focus procedure.").arg(seqName);
                    totalImagingTime += captures_to_go * 30;
                }
                // If we're dithering after each exposure, that's another 10-20 seconds
                if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE && Options::ditherEnabled())
                {
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a dither procedure.").arg(seqName);
                    totalImagingTime += (captures_to_go * 15) / Options::ditherFrames();
                }
            }
        }
    }

    schedJob->setCapturedFramesMap(capture_map);
    schedJob->setSequenceCount(capturesPerRepeat * schedJob->getRepeatsRequired());

    // only in case we remember the job progress, we change the completion count
    if (rememberJobProgress)
        schedJob->setCompletedCount(totalCompletedCount);

    qDeleteAll(seqJobs);

    // FIXME: Move those ifs away to the caller in order to avoid estimating in those situations!

    // We can't estimate times that do not finish when sequence is done
    if (schedJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        // We can't know estimated time if it is looping indefinitely
        schedJob->setEstimatedTime(-2);

        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                       QString("Job '%1' is configured to loop until Scheduler is stopped manually, has undefined imaging time.")
                                       .arg(schedJob->getName());
    }
    // If we know startup and finish times, we can estimate time right away
    else if (schedJob->getStartupCondition() == SchedulerJob::START_AT &&
             schedJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        // FIXME: SchedulerJob is probably doing this already
        qint64 const diff = schedJob->getStartupTime().secsTo(schedJob->getCompletionTime());
        schedJob->setEstimatedTime(diff);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a startup time and fixed completion time, will run for %2.")
                                       .arg(schedJob->getName())
                                       .arg(dms(diff * 15.0 / 3600.0f).toHMSString());
    }
    // If we know finish time only, we can roughly estimate the time considering the job starts now
    else if (schedJob->getStartupCondition() != SchedulerJob::START_AT &&
             schedJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        qint64 const diff = getLocalTime().secsTo(schedJob->getCompletionTime());
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

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' will not run, complete with %2/%3 captures.")
                                       .arg(schedJob->getName()).arg(schedJob->getCompletedCount()).arg(schedJob->getSequenceCount());
    }
    // Else consolidate with step durations
    else
    {
        if (schedJob->getLightFramesRequired())
        {
            totalImagingTime += timeHeuristics(schedJob);
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

void Scheduler::parkMount()
{
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "parkStatus");
    QVariant parkingStatus = mountInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkWaitState = PARKWAIT_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (shutdownState == SHUTDOWN_PARK_MOUNT)
                shutdownState = SHUTDOWN_PARK_DOME;

            parkWaitState = PARKWAIT_PARKED;
            appendLogText(i18n("Mount already parked."));
            break;

        case ISD::PARK_UNPARKING:
        //case Mount::UNPARKING_BUSY:
        /* FIXME: Handle the situation where we request parking but an unparking procedure is running. */

        //        case Mount::PARKING_IDLE:
        //        case Mount::UNPARKING_OK:
        case ISD::PARK_ERROR:
        case ISD::PARK_UNKNOWN:
        case ISD::PARK_UNPARKED:
        {
            TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "park");
            QDBusReply<bool> const mountReply = mountInterface->call(QDBus::AutoDetect, "park");

            if (mountReply.error().type() != QDBusError::NoError)
            {
                qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount park request received DBUS error: %1").arg(
                                                      QDBusError::errorString(mountReply.error().type()));
                if (!manageConnectionLoss())
                    parkWaitState = PARKWAIT_ERROR;
            }
            else startCurrentOperationTimer();
        }

        // Fall through
        case ISD::PARK_PARKING:
            //case Mount::PARKING_BUSY:
            if (shutdownState == SHUTDOWN_PARK_MOUNT)
                shutdownState = SHUTDOWN_PARKING_MOUNT;

            parkWaitState = PARKWAIT_PARKING;
            appendLogText(i18n("Parking mount in progress..."));
            break;

            // All cases covered above so no need for default
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while parking mount.").arg(mountReply.value());
    }
}

void Scheduler::unParkMount()
{
    if (mountInterface.isNull())
        return;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "parkStatus");
    QVariant parkingStatus = mountInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkWaitState = PARKWAIT_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        //case Mount::UNPARKING_OK:
        case ISD::PARK_UNPARKED:
            if (startupState == STARTUP_UNPARK_MOUNT)
                startupState = STARTUP_UNPARK_CAP;

            parkWaitState = PARKWAIT_UNPARKED;
            appendLogText(i18n("Mount already unparked."));
            break;

        //case Mount::PARKING_BUSY:
        case ISD::PARK_PARKING:
        /* FIXME: Handle the situation where we request unparking but a parking procedure is running. */

        //        case Mount::PARKING_IDLE:
        //        case Mount::PARKING_OK:
        //        case Mount::PARKING_ERROR:
        case ISD::PARK_ERROR:
        case ISD::PARK_UNKNOWN:
        case ISD::PARK_PARKED:
        {
            TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "unpark");
            QDBusReply<bool> const mountReply = mountInterface->call(QDBus::AutoDetect, "unpark");

            if (mountReply.error().type() != QDBusError::NoError)
            {
                qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount unpark request received DBUS error: %1").arg(
                                                      QDBusError::errorString(mountReply.error().type()));
                if (!manageConnectionLoss())
                    parkWaitState = PARKWAIT_ERROR;
            }
            else startCurrentOperationTimer();
        }

        // Fall through
        //case Mount::UNPARKING_BUSY:
        case ISD::PARK_UNPARKING:
            if (startupState == STARTUP_UNPARK_MOUNT)
                startupState = STARTUP_UNPARKING_MOUNT;

            parkWaitState = PARKWAIT_UNPARKING;
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";
            break;

            // All cases covered above
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while unparking mount.").arg(mountReply.value());
    }
}

void Scheduler::checkMountParkingStatus()
{
    if (mountInterface.isNull())
        return;

    static int parkingFailureCount = 0;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "parkStatus");
    QVariant parkingStatus = mountInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkWaitState = PARKWAIT_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        //case Mount::PARKING_OK:
        case ISD::PARK_PARKED:
            // If we are starting up, we will unpark the mount in checkParkWaitState soon
            // If we are shutting down and mount is parked, proceed to next step
            if (shutdownState == SHUTDOWN_PARKING_MOUNT)
                shutdownState = SHUTDOWN_PARK_DOME;

            // Update parking engine state
            if (parkWaitState == PARKWAIT_PARKING)
                parkWaitState = PARKWAIT_PARKED;

            appendLogText(i18n("Mount parked."));
            parkingFailureCount = 0;
            break;

        //case Mount::UNPARKING_OK:
        case ISD::PARK_UNPARKED:
            // If we are starting up and mount is unparked, proceed to next step
            // If we are shutting down, we will park the mount in checkParkWaitState soon
            if (startupState == STARTUP_UNPARKING_MOUNT)
                startupState = STARTUP_UNPARK_CAP;

            // Update parking engine state
            if (parkWaitState == PARKWAIT_UNPARKING)
                parkWaitState = PARKWAIT_UNPARKED;

            appendLogText(i18n("Mount unparked."));
            parkingFailureCount = 0;
            break;

        // FIXME: Create an option for the parking/unparking timeout.

        //case Mount::UNPARKING_BUSY:
        case ISD::PARK_UNPARKING:
            if (getCurrentOperationMsec() > (60 * 1000))
            {
                if (++parkingFailureCount < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Warning: mount unpark operation timed out on attempt %1/%2. Restarting operation...",
                                       parkingFailureCount, MAX_FAILURE_ATTEMPTS));
                    unParkMount();
                }
                else
                {
                    appendLogText(i18n("Warning: mount unpark operation timed out on last attempt."));
                    parkWaitState = PARKWAIT_ERROR;
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";

            break;

        //case Mount::PARKING_BUSY:
        case ISD::PARK_PARKING:
            if (getCurrentOperationMsec() > (60 * 1000))
            {
                if (++parkingFailureCount < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Warning: mount park operation timed out on attempt %1/%2. Restarting operation...", parkingFailureCount,
                                       MAX_FAILURE_ATTEMPTS));
                    parkMount();
                }
                else
                {
                    appendLogText(i18n("Warning: mount park operation timed out on last attempt."));
                    parkWaitState = PARKWAIT_ERROR;
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Parking mount in progress...";

            break;

        //case Mount::PARKING_ERROR:
        case ISD::PARK_ERROR:
            if (startupState == STARTUP_UNPARKING_MOUNT)
            {
                appendLogText(i18n("Mount unparking error."));
                startupState = STARTUP_ERROR;
                parkingFailureCount = 0;
            }
            else if (shutdownState == SHUTDOWN_PARKING_MOUNT)
            {
                if (++parkingFailureCount < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Warning: mount park operation failed on attempt %1/%2. Restarting operation...", parkingFailureCount,
                                       MAX_FAILURE_ATTEMPTS));
                    parkMount();
                }
                else
                {
                    appendLogText(i18n("Mount parking error."));
                    shutdownState = SHUTDOWN_ERROR;
                    parkingFailureCount = 0;
                }

            }
            else if (parkWaitState == PARKWAIT_PARKING)
            {
                appendLogText(i18n("Mount parking error."));
                parkWaitState = PARKWAIT_ERROR;
                parkingFailureCount = 0;
            }
            else if (parkWaitState == PARKWAIT_UNPARKING)
            {
                appendLogText(i18n("Mount unparking error."));
                parkWaitState = PARKWAIT_ERROR;
                parkingFailureCount = 0;
            }
            break;

        //case Mount::PARKING_IDLE:
        // FIXME Does this work as intended? check!
        case ISD::PARK_UNKNOWN:
            // Last parking action did not result in an action, so proceed to next step
            if (shutdownState == SHUTDOWN_PARKING_MOUNT)
                shutdownState = SHUTDOWN_PARK_DOME;

            // Last unparking action did not result in an action, so proceed to next step
            if (startupState == STARTUP_UNPARKING_MOUNT)
                startupState = STARTUP_UNPARK_CAP;

            // Update parking engine state
            if (parkWaitState == PARKWAIT_PARKING)
                parkWaitState = PARKWAIT_PARKED;
            else if (parkWaitState == PARKWAIT_UNPARKING)
                parkWaitState = PARKWAIT_UNPARKED;

            parkingFailureCount = 0;
            break;

            // All cases covered above
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while checking progress.").arg(mountReply.value());
    }
}

bool Scheduler::isMountParked()
{
    if (mountInterface.isNull())
        return false;
    // First check if the mount is able to park - if it isn't, getParkingStatus will reply PARKING_ERROR and status won't be clear
    //QDBusReply<bool> const parkCapableReply = mountInterface->call(QDBus::AutoDetect, "canPark");
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "canPark");
    QVariant canPark = mountInterface->property("canPark");
    TEST_PRINT(stderr, "  @@@dbus received %s\n", !canPark.isValid() ? "invalid" : (canPark.toBool() ? "T" : "F"));

    if (canPark.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount canPark request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        manageConnectionLoss();
        return false;
    }
    else if (canPark.toBool() == true)
    {
        // If it is able to park, obtain its current status
        //QDBusReply<int> const mountReply  = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "parkStatus");
        QVariant parkingStatus = mountInterface->property("parkStatus");
        TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

        if (parkingStatus.isValid() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parking status property is invalid %1.").arg(
                                                  mountInterface->lastError().type());
            manageConnectionLoss();
            return false;
        }

        // Deduce state of mount - see getParkingStatus in mount.cpp
        switch (static_cast<ISD::ParkStatus>(parkingStatus.toInt()))
        {
            //            case Mount::PARKING_OK:     // INDI switch ok, and parked
            //            case Mount::PARKING_IDLE:   // INDI switch idle, and parked
            case ISD::PARK_PARKED:
                return true;

            //            case Mount::UNPARKING_OK:   // INDI switch idle or ok, and unparked
            //            case Mount::PARKING_ERROR:  // INDI switch error
            //            case Mount::PARKING_BUSY:   // INDI switch busy
            //            case Mount::UNPARKING_BUSY: // INDI switch busy
            default:
                return false;
        }
    }
    // If the mount is not able to park, consider it not parked
    return false;
}

void Scheduler::parkDome()
{
    if (domeInterface.isNull())
        return;

    //QDBusReply<int> const domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    //Dome::ParkingStatus status = static_cast<Dome::ParkingStatus>(domeReply.value());
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "parkStatus");
    QVariant parkingStatus = domeInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());
    if (status != ISD::PARK_PARKED)
    {
        shutdownState = SHUTDOWN_PARKING_DOME;
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:call", "park");
        domeInterface->call(QDBus::AutoDetect, "park");
        appendLogText(i18n("Parking dome..."));

        startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Dome already parked."));
        shutdownState = SHUTDOWN_SCRIPT;
    }
}

void Scheduler::unParkDome()
{
    if (domeInterface.isNull())
        return;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "parkStatus");
    QVariant parkingStatus = domeInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    if (static_cast<ISD::ParkStatus>(parkingStatus.toInt()) != ISD::PARK_UNPARKED)
    {
        startupState = STARTUP_UNPARKING_DOME;
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:call", "unpark");
        domeInterface->call(QDBus::AutoDetect, "unpark");
        appendLogText(i18n("Unparking dome..."));

        startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Dome already unparked."));
        startupState = STARTUP_UNPARK_MOUNT;
    }
}

void Scheduler::checkDomeParkingStatus()
{
    if (domeInterface.isNull())
        return;

    /* FIXME: move this elsewhere */
    static int parkingFailureCount = 0;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "parkStatus");
    QVariant parkingStatus = domeInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkWaitState = PARKWAIT_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (shutdownState == SHUTDOWN_PARKING_DOME)
            {
                appendLogText(i18n("Dome parked."));

                shutdownState = SHUTDOWN_SCRIPT;
            }
            parkingFailureCount = 0;
            break;

        case ISD::PARK_UNPARKED:
            if (startupState == STARTUP_UNPARKING_DOME)
            {
                startupState = STARTUP_UNPARK_MOUNT;
                appendLogText(i18n("Dome unparked."));
            }
            parkingFailureCount = 0;
            break;

        case ISD::PARK_PARKING:
        case ISD::PARK_UNPARKING:
            // TODO make the timeouts configurable by the user
            if (getCurrentOperationMsec() > (120 * 1000))
            {
                if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Operation timeout. Restarting operation..."));
                    if (status == ISD::PARK_PARKING)
                        parkDome();
                    else
                        unParkDome();
                    break;
                }
            }
            break;

        case ISD::PARK_ERROR:
            if (shutdownState == SHUTDOWN_PARKING_DOME)
            {
                if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Dome parking failed. Restarting operation..."));
                    parkDome();
                }
                else
                {
                    appendLogText(i18n("Dome parking error."));
                    shutdownState = SHUTDOWN_ERROR;
                    parkingFailureCount = 0;
                }
            }
            else if (startupState == STARTUP_UNPARKING_DOME)
            {
                if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Dome unparking failed. Restarting operation..."));
                    unParkDome();
                }
                else
                {
                    appendLogText(i18n("Dome unparking error."));
                    startupState = STARTUP_ERROR;
                    parkingFailureCount = 0;
                }
            }
            break;

        default:
            break;
    }
}

bool Scheduler::isDomeParked()
{
    if (domeInterface.isNull())
        return false;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "parkStatus");
    QVariant parkingStatus = domeInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    return status == ISD::PARK_PARKED;
}

void Scheduler::parkCap()
{
    if (capInterface.isNull())
        return;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "dustCapInterface:property", "parkStatus");
    QVariant parkingStatus = capInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    if (status != ISD::PARK_PARKED)
    {
        shutdownState = SHUTDOWN_PARKING_CAP;
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "distCapInterface:call", "park");
        capInterface->call(QDBus::AutoDetect, "park");
        appendLogText(i18n("Parking Cap..."));

        startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Cap already parked."));
        shutdownState = SHUTDOWN_PARK_MOUNT;
    }
}

void Scheduler::unParkCap()
{
    if (capInterface.isNull())
        return;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "dustCapInterface:property", "parkStatus");
    QVariant parkingStatus = capInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    if (status != ISD::PARK_UNPARKED)
    {
        startupState = STARTUP_UNPARKING_CAP;
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "dustCapInterface:call", "unpark");
        capInterface->call(QDBus::AutoDetect, "unpark");
        appendLogText(i18n("Unparking cap..."));

        startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Cap already unparked."));
        startupState = STARTUP_COMPLETE;
    }
}

void Scheduler::checkCapParkingStatus()
{
    if (capInterface.isNull())
        return;

    /* FIXME: move this elsewhere */
    static int parkingFailureCount = 0;

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "dustCapInterface:property", "parkStatus");
    QVariant parkingStatus = capInterface->property("parkStatus");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (shutdownState == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parked."));
                shutdownState = SHUTDOWN_PARK_MOUNT;
            }
            parkingFailureCount = 0;
            break;

        case ISD::PARK_UNPARKED:
            if (startupState == STARTUP_UNPARKING_CAP)
            {
                startupState = STARTUP_COMPLETE;
                appendLogText(i18n("Cap unparked."));
            }
            parkingFailureCount = 0;
            break;

        case ISD::PARK_PARKING:
        case ISD::PARK_UNPARKING:
            // TODO make the timeouts configurable by the user
            if (getCurrentOperationMsec() > (60 * 1000))
            {
                if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Operation timeout. Restarting operation..."));
                    if (status == ISD::PARK_PARKING)
                        parkCap();
                    else
                        unParkCap();
                    break;
                }
            }
            break;

        case ISD::PARK_ERROR:
            if (shutdownState == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parking error."));
                shutdownState = SHUTDOWN_ERROR;
            }
            else if (startupState == STARTUP_UNPARKING_CAP)
            {
                appendLogText(i18n("Cap unparking error."));
                startupState = STARTUP_ERROR;
            }
            parkingFailureCount = 0;
            break;

        default:
            break;
    }
}

void Scheduler::startJobEvaluation()
{
    // Reset current job
    setCurrentJob(nullptr);

    // Reset ALL scheduler jobs to IDLE and force-reset their completed count - no effect when progress is kept
    for (SchedulerJob * job : jobs)
    {
        job->reset();
        job->setCompletedCount(0);
    }

    // Unconditionally update the capture storage
    updateCompletedJobsCount(true);

    // And evaluate all pending jobs per the conditions set in each
    evaluateJobs(true);
}

void Scheduler::sortJobsPerAltitude()
{
    // We require a first job to sort, so bail out if list is empty
    if (jobs.isEmpty())
        return;

    // Don't reset current job
    // setCurrentJob(nullptr);

    // Don't reset scheduler jobs startup times before sorting - we need the first job startup time

    // Sort by startup time, using the first job time as reference for altitude calculations
    using namespace std::placeholders;
    QList<SchedulerJob*> sortedJobs = jobs;
    std::stable_sort(sortedJobs.begin() + 1, sortedJobs.end(),
                     std::bind(SchedulerJob::decreasingAltitudeOrder, _1, _2, jobs.first()->getStartupTime()));

    // If order changed, reset and re-evaluate
    if (reorderJobs(sortedJobs))
    {
        for (SchedulerJob * job : jobs)
            job->reset();

        evaluateJobs(true);
    }
}

void Scheduler::resumeCheckStatus()
{
    disconnect(this, &Scheduler::weatherChanged, this, &Scheduler::resumeCheckStatus);
    TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_SCHEDULER).toLatin1().data());
    setupNextIteration(RUN_SCHEDULER);
}

Scheduler::ErrorHandlingStrategy Scheduler::getErrorHandlingStrategy()
{
    // The UI holds the state
    if (errorHandlingRestartAfterAllButton->isChecked())
        return ERROR_RESTART_AFTER_TERMINATION;
    else if (errorHandlingRestartImmediatelyButton->isChecked())
        return ERROR_RESTART_IMMEDIATELY;
    else
        return ERROR_DONT_RESTART;

}

void Scheduler::setErrorHandlingStrategy(Scheduler::ErrorHandlingStrategy strategy)
{
    errorHandlingDelaySB->setEnabled(strategy != ERROR_DONT_RESTART);

    switch (strategy)
    {
        case ERROR_RESTART_AFTER_TERMINATION:
            errorHandlingRestartAfterAllButton->setChecked(true);
            break;
        case ERROR_RESTART_IMMEDIATELY:
            errorHandlingRestartImmediatelyButton->setChecked(true);
            break;
        default:
            errorHandlingDontRestartButton->setChecked(true);
            break;
    }
}



void Scheduler::startMosaicTool()
{
    bool raOk = false, decOk = false;
    dms ra(raBox->createDms(false, &raOk)); //false means expressed in hours
    dms dec(decBox->createDms(true, &decOk));

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

    SkyPoint center;
    center.setRA0(ra);
    center.setDec0(dec);

    Mosaic mosaicTool(nameEdit->text(), center, Ekos::Manager::Instance());

    if (mosaicTool.exec() == QDialog::Accepted)
    {
        // #1 Edit Sequence File ---> Not needed as of 2016-09-12 since Scheduler can send Target Name to Capture module it will append it to root dir
        // #1.1 Set prefix to Target-Part#
        // #1.2 Set directory to output/Target-Part#

        // #2 Save all sequence files in Jobs dir
        // #3 Set as current Sequence file
        // #4 Change Target name to Target-Part#
        // #5 Update J2000 coords
        // #6 Repeat and save Ekos Scheduler List in the output directory
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job accepted with # " << mosaicTool.getJobs().size() << " jobs and fits dir "
                                       << mosaicTool.getJobsDir();

        QString outputDir  = mosaicTool.getJobsDir();
        QString targetName = nameEdit->text().simplified();

        // Sanitize name
        targetName = targetName.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                     // Remove any two or more __
                     .replace( QRegularExpression("_{2,}"), "_")
                     // Remove any _ at the end
                     .replace( QRegularExpression("_$"), "");

        int batchCount     = 1;

        XMLEle *root = getSequenceJobRoot();
        if (root == nullptr)
            return;

        // Delete any prior jobs before saving
        if (!jobs.empty())
        {
            if (KMessageBox::questionYesNo(nullptr,
                                           i18n("Do you want to keep the existing jobs in the mosaic schedule?")) == KMessageBox::No)
            {
                qDeleteAll(jobs);
                jobs.clear();
                while (queueTable->rowCount() > 0)
                    queueTable->removeRow(0);
            }
        }

        // We do not want FITS image for mosaic job since each job has its own calculated center
        QString fitsFileBackup = fitsEdit->text();
        fitsEdit->clear();

        foreach (auto oneJob, mosaicTool.getJobs())
        {
            QString prefix = QString("%1-Part%2").arg(targetName).arg(batchCount++);

            prefix.replace(' ', '-');
            nameEdit->setText(prefix);

            if (createJobSequence(root, prefix, outputDir) == false)
                return;

            QString filename = QString("%1/%2.esq").arg(outputDir, prefix);
            sequenceEdit->setText(filename);
            sequenceURL = QUrl::fromLocalFile(filename);

            raBox->showInHours(oneJob.center.ra0());
            decBox->showInDegrees(oneJob.center.dec0());
            rotationSpin->setValue(oneJob.rotation);

            alignStepCheck->setChecked(oneJob.doAlign);
            focusStepCheck->setChecked(oneJob.doFocus);

            saveJob();
        }

        delXMLEle(root);

        QUrl mosaicURL = QUrl::fromLocalFile((QString("%1/%2_mosaic.esl").arg(outputDir, targetName)));

        if (saveScheduler(mosaicURL))
        {
            appendLogText(i18n("Mosaic file %1 saved successfully.", mosaicURL.toLocalFile()));
        }
        else
        {
            appendLogText(i18n("Error saving mosaic file %1. Please reload job.", mosaicURL.toLocalFile()));
        }

        fitsEdit->setText(fitsFileBackup);
    }
}

XMLEle * Scheduler::getSequenceJobRoot()
{
    QFile sFile;
    sFile.setFileName(sequenceURL.toLocalFile());

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
    QFile sFile;
    sFile.setFileName(sequenceURL.toLocalFile());

    if (!sFile.open(QIODevice::ReadOnly))
    {
        KSNotification::sorry(i18n("Unable to open sequence file %1", sFile.fileName()),
                              i18n("Could Not Open File"));
        return false;
    }

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
                    editXMLEle(subEP, QString("%1/%2").arg(outputDir, prefix).toLatin1().constData());
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
    if (state == SCHEDULER_RUNNING)
        return;

    // Reset capture count of all jobs before re-evaluating
    foreach (SchedulerJob *job, jobs)
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

void Scheduler::checkStartupProcedure()
{
    if (checkStartupState() == false)
        QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));
    else
    {
        if (startupState == STARTUP_COMPLETE)
            appendLogText(i18n("Manual startup procedure completed successfully."));
        else if (startupState == STARTUP_ERROR)
            appendLogText(i18n("Manual startup procedure terminated due to errors."));

        startupB->setIcon(
            QIcon::fromTheme("media-playback-start"));
    }
}

void Scheduler::runStartupProcedure()
{
    if (startupState == STARTUP_IDLE || startupState == STARTUP_ERROR || startupState == STARTUP_COMPLETE)
    {
        /* FIXME: Probably issue a warning only, in case the user wants to run the startup script alone */
        if (indiState == INDI_IDLE)
        {
            KSNotification::sorry(i18n("Cannot run startup procedure while INDI devices are not online."));
            return;
        }

        if (KMessageBox::questionYesNo(
                    nullptr, i18n("Are you sure you want to execute the startup procedure manually?")) == KMessageBox::Yes)
        {
            appendLogText(i18n("Warning: executing startup procedure manually..."));
            startupB->setIcon(
                QIcon::fromTheme("media-playback-stop"));
            startupState = STARTUP_IDLE;
            checkStartupState();
            QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));
        }
    }
    else
    {
        switch (startupState)
        {
            case STARTUP_IDLE:
                break;

            case STARTUP_SCRIPT:
                scriptProcess.terminate();
                break;

            case STARTUP_UNPARK_DOME:
                break;

            case STARTUP_UNPARKING_DOME:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:call", "abort");
                domeInterface->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_MOUNT:
                break;

            case STARTUP_UNPARKING_MOUNT:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "abort");
                mountInterface->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_CAP:
                break;

            case STARTUP_UNPARKING_CAP:
                break;

            case STARTUP_COMPLETE:
                break;

            case STARTUP_ERROR:
                break;
        }

        startupState = STARTUP_IDLE;

        appendLogText(i18n("Startup procedure terminated."));
    }
}

void Scheduler::checkShutdownProcedure()
{
    // If shutdown procedure is not finished yet, let's check again in 1 second.
    if (checkShutdownState() == false)
        QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));
    else
    {
        if (shutdownState == SHUTDOWN_COMPLETE)
        {
            appendLogText(i18n("Manual shutdown procedure completed successfully."));
            // Stop Ekos
            if (Options::stopEkosAfterShutdown())
                stopEkos();
        }
        else if (shutdownState == SHUTDOWN_ERROR)
            appendLogText(i18n("Manual shutdown procedure terminated due to errors."));

        shutdownState = SHUTDOWN_IDLE;
        shutdownB->setIcon(
            QIcon::fromTheme("media-playback-start"));
    }
}

void Scheduler::runShutdownProcedure()
{
    if (shutdownState == SHUTDOWN_IDLE || shutdownState == SHUTDOWN_ERROR || shutdownState == SHUTDOWN_COMPLETE)
    {
        if (KMessageBox::questionYesNo(
                    nullptr, i18n("Are you sure you want to execute the shutdown procedure manually?")) == KMessageBox::Yes)
        {
            appendLogText(i18n("Warning: executing shutdown procedure manually..."));
            shutdownB->setIcon(
                QIcon::fromTheme("media-playback-stop"));
            shutdownState = SHUTDOWN_IDLE;
            checkShutdownState();
            QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));
        }
    }
    else
    {
        switch (shutdownState)
        {
            case SHUTDOWN_IDLE:
                break;

            case SHUTDOWN_SCRIPT:
                break;

            case SHUTDOWN_SCRIPT_RUNNING:
                scriptProcess.terminate();
                break;

            case SHUTDOWN_PARK_DOME:
                break;

            case SHUTDOWN_PARKING_DOME:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:call", "abort");
                domeInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_MOUNT:
                break;

            case SHUTDOWN_PARKING_MOUNT:
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "abort");
                mountInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_CAP:
                break;

            case SHUTDOWN_PARKING_CAP:
                break;

            case SHUTDOWN_COMPLETE:
                break;

            case SHUTDOWN_ERROR:
                break;
        }

        shutdownState = SHUTDOWN_IDLE;

        appendLogText(i18n("Shutdown procedure terminated."));
    }
}

void Scheduler::loadProfiles()
{
    QString currentProfile = schedulerProfileCombo->currentText();

    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "ekosInterface:call", "getProfiles");
    QDBusReply<QStringList> profiles = ekosInterface->call(QDBus::AutoDetect, "getProfiles");

    if (profiles.error().type() == QDBusError::NoError)
    {
        schedulerProfileCombo->blockSignals(true);
        schedulerProfileCombo->clear();
        schedulerProfileCombo->addItem(i18n("Default"));
        schedulerProfileCombo->addItems(profiles);
        schedulerProfileCombo->setCurrentText(currentProfile);
        schedulerProfileCombo->blockSignals(false);
    }
}

bool Scheduler::loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs,
                                  bool &hasAutoFocus, Scheduler *scheduler)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        scheduler->appendLogText(i18n("Unable to open sequence queue file '%1'", fileURL));
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
                    jobs.append(processJobInfo(ep, schedJob));
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
    SequenceJob *job = new SequenceJob(root);
    if (FRAME_LIGHT == job->getFrameType() && nullptr != schedJob)
        schedJob->setLightFramesRequired(true);

    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.processJobInfo(job, schedJob->getName());

    return job;
}

int Scheduler::getCompletedFiles(const QString &path, const QString &seqPrefix)
{
    int seqFileCount = 0;
    QFileInfo const path_info(path);
    QString const sig_dir(path_info.dir().path());
    QString const sig_file(path_info.completeBaseName());

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Searching in path '%1', files '%2*' for prefix '%3'...").arg(sig_dir, sig_file,
                                   seqPrefix);
    QDirIterator it(sig_dir, QDir::Files);

    /* FIXME: this counts all files with prefix in the storage location, not just captures. DSS analysis files are counted in, for instance. */
    while (it.hasNext())
    {
        QString const fileName = QFileInfo(it.next()).completeBaseName();

        if (fileName.startsWith(seqPrefix))
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("> Found '%1'").arg(fileName);
            seqFileCount++;
        }
    }

    TEST_PRINT(stderr, "sch%d @@@getCompletedFiles(%s %s): %d\n", __LINE__, path.toLatin1().data(), seqPrefix.toLatin1().data(),
               seqFileCount);
    return seqFileCount;
}

void Scheduler::setINDICommunicationStatus(Ekos::CommunicationStatus status)
{
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %d\n", __LINE__, "ekosInterface:indiStatusChanged", status);
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler INDI status is" << status;

    m_INDICommunicationStatus = status;
}

void Scheduler::setEkosCommunicationStatus(Ekos::CommunicationStatus status)
{
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %d\n", __LINE__, "ekosInterface:ekosStatusChanged", status);
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler Ekos status is" << status;

    m_EkosCommunicationStatus = status;
}

void Scheduler::simClockScaleChanged(float newScale)
{
    if (currentlySleeping())
    {
        QTime const remainingTimeMs = QTime::fromMSecsSinceStartOfDay(std::lround(static_cast<double>
                                      (iterationTimer.remainingTime())
                                      * KStarsData::Instance()->clock()->scale()
                                      / newScale));
        appendLogText(i18n("Sleeping for %1 on simulation clock update until next observation job is ready...",
                           remainingTimeMs.toString("hh:mm:ss")));
        iterationTimer.stop();
        iterationTimer.start(remainingTimeMs.msecsSinceStartOfDay());
    }
}

void Scheduler::simClockTimeChanged()
{
    calculateDawnDusk();
    updateNightTime();

    // If the Scheduler is not running, reset all jobs and re-evaluate from a new current start point
    if (SCHEDULER_RUNNING != state)
    {
        startJobEvaluation();
    }
}

void Scheduler::registerNewModule(const QString &name)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Registering new Module (" << name << ")";

    if (name == "Focus")
    {
        delete focusInterface;
        focusInterface   = new QDBusInterface("org.kde.kstars", focusPathString, focusInterfaceString,
                                              QDBusConnection::sessionBus(), this);
        connect(focusInterface, SIGNAL(newStatus(Ekos::FocusState)), this, SLOT(setFocusStatus(Ekos::FocusState)),
                Qt::UniqueConnection);
    }
    else if (name == "Capture")
    {
        delete captureInterface;
        captureInterface = new QDBusInterface("org.kde.kstars", capturePathString, captureInterfaceString,
                                              QDBusConnection::sessionBus(), this);

        connect(captureInterface, SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(captureInterface, SIGNAL(newStatus(Ekos::CaptureState)), this, SLOT(setCaptureStatus(Ekos::CaptureState)),
                Qt::UniqueConnection);
        checkInterfaceReady(captureInterface);
    }
    else if (name == "Mount")
    {
        delete mountInterface;
        mountInterface   = new QDBusInterface("org.kde.kstars", mountPathString, mountInterfaceString,
                                              QDBusConnection::sessionBus(), this);

        connect(mountInterface, SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(mountInterface, SIGNAL(newStatus(ISD::Telescope::Status)), this, SLOT(setMountStatus(ISD::Telescope::Status)),
                Qt::UniqueConnection);

        checkInterfaceReady(mountInterface);
    }
    else if (name == "Align")
    {
        delete alignInterface;
        alignInterface   = new QDBusInterface("org.kde.kstars", alignPathString, alignInterfaceString,
                                              QDBusConnection::sessionBus(), this);
        connect(alignInterface, SIGNAL(newStatus(Ekos::AlignState)), this, SLOT(setAlignStatus(Ekos::AlignState)),
                Qt::UniqueConnection);
    }
    else if (name == "Guide")
    {
        delete guideInterface;
        guideInterface   = new QDBusInterface("org.kde.kstars", guidePathString, guideInterfaceString,
                                              QDBusConnection::sessionBus(), this);
        connect(guideInterface, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(setGuideStatus(Ekos::GuideState)),
                Qt::UniqueConnection);
    }
    else if (name == "Dome")
    {
        delete domeInterface;
        domeInterface    = new QDBusInterface("org.kde.kstars", domePathString, domeInterfaceString,
                                              QDBusConnection::sessionBus(), this);

        connect(domeInterface, SIGNAL(ready()), this, SLOT(syncProperties()));
        checkInterfaceReady(domeInterface);
    }
    else if (name == "Weather")
    {
        delete weatherInterface;
        weatherInterface = new QDBusInterface("org.kde.kstars", weatherPathString, weatherInterfaceString,
                                              QDBusConnection::sessionBus(), this);

        connect(weatherInterface, SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(weatherInterface, SIGNAL(newStatus(ISD::Weather::Status)), this, SLOT(setWeatherStatus(ISD::Weather::Status)));
        checkInterfaceReady(weatherInterface);
    }
    else if (name == "DustCap")
    {
        delete capInterface;
        capInterface = new QDBusInterface("org.kde.kstars", dustCapPathString, dustCapInterfaceString,
                                          QDBusConnection::sessionBus(), this);

        connect(capInterface, SIGNAL(ready()), this, SLOT(syncProperties()), Qt::UniqueConnection);
        checkInterfaceReady(capInterface);
    }
}

void Scheduler::syncProperties()
{
    QDBusInterface *iface = qobject_cast<QDBusInterface*>(sender());

    if (iface == mountInterface)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:property", "canPark");
        QVariant canMountPark = mountInterface->property("canPark");
        TEST_PRINT(stderr, "  @@@dbus received %s\n", !canMountPark.isValid() ? "invalid" : (canMountPark.toBool() ? "T" : "F"));

        unparkMountCheck->setEnabled(canMountPark.toBool());
        parkMountCheck->setEnabled(canMountPark.toBool());
        m_MountReady = true;
    }
    else if (iface == capInterface)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "dustCapInterface:property", "canPark");
        QVariant canCapPark = capInterface->property("canPark");
        TEST_PRINT(stderr, "  @@@dbus received %s\n", !canCapPark.isValid() ? "invalid" : (canCapPark.toBool() ? "T" : "F"));

        if (canCapPark.isValid())
        {
            capCheck->setEnabled(canCapPark.toBool());
            uncapCheck->setEnabled(canCapPark.toBool());
            m_CapReady = true;
        }
        else
        {
            capCheck->setEnabled(false);
            uncapCheck->setEnabled(false);
        }
    }
    else if (iface == weatherInterface)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "weatherInterface:property", "updatePeriod");
        QVariant updatePeriod = weatherInterface->property("updatePeriod");
        TEST_PRINT(stderr, "  @@@dbus received %d\n", !updatePeriod.isValid() ? -1 : updatePeriod.toInt());

        if (updatePeriod.isValid())
        {
            weatherCheck->setEnabled(true);

            TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "weatherInterface:property", "status");
            QVariant status = weatherInterface->property("status");
            TEST_PRINT(stderr, "  @@@dbus received %d\n", !status.isValid() ? -1 : status.toInt());
            setWeatherStatus(static_cast<ISD::Weather::Status>(status.toInt()));

            //            if (updatePeriod.toInt() > 0)
            //            {
            //                weatherTimer.setInterval(updatePeriod.toInt() * 1000);
            //                connect(&weatherTimer, &QTimer::timeout, this, &Scheduler::checkWeather, Qt::UniqueConnection);
            //                weatherTimer.start();

            //                // Check weather initially
            //                checkWeather();
            //            }
        }
        else
            weatherCheck->setEnabled(true);
    }
    else if (iface == domeInterface)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "domeInterface:property", "canPark");
        QVariant canDomePark = domeInterface->property("canPark");
        TEST_PRINT(stderr, "  @@@dbus received %s\n", !canDomePark.isValid() ? "invalid" : (canDomePark.toBool() ? "T" : "F"));
        unparkDomeCheck->setEnabled(canDomePark.toBool());
        parkDomeCheck->setEnabled(canDomePark.toBool());
        m_DomeReady = true;
    }
    else if (iface == captureInterface)
    {
        TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "captureInterface:property", "coolerControl");
        QVariant hasCoolerControl = captureInterface->property("coolerControl");
        TEST_PRINT(stderr, "  @@@dbus received %s\n",
                   !hasCoolerControl.isValid() ? "invalid" : (hasCoolerControl.toBool() ? "T" : "F"));
        warmCCDCheck->setEnabled(hasCoolerControl.toBool());
        m_CaptureReady = true;
    }
}

void Scheduler::checkInterfaceReady(QDBusInterface *iface)
{
    if (iface == mountInterface)
    {
        QVariant canMountPark = mountInterface->property("canPark");
        if (canMountPark.isValid())
        {
            unparkMountCheck->setEnabled(canMountPark.toBool());
            parkMountCheck->setEnabled(canMountPark.toBool());
            m_MountReady = true;
        }
    }
    else if (iface == capInterface)
    {
        QVariant canCapPark = capInterface->property("canPark");
        if (canCapPark.isValid())
        {
            capCheck->setEnabled(canCapPark.toBool());
            uncapCheck->setEnabled(canCapPark.toBool());
            m_CapReady = true;
        }
        else
        {
            capCheck->setEnabled(false);
            uncapCheck->setEnabled(false);
        }
    }
    else if (iface == weatherInterface)
    {
        QVariant updatePeriod = weatherInterface->property("updatePeriod");
        if (updatePeriod.isValid())
        {
            weatherCheck->setEnabled(true);
            QVariant status = weatherInterface->property("status");
            setWeatherStatus(static_cast<ISD::Weather::Status>(status.toInt()));
        }
        else
            weatherCheck->setEnabled(true);
    }
    else if (iface == domeInterface)
    {
        QVariant canDomePark = domeInterface->property("canPark");
        if (canDomePark.isValid())
        {
            unparkDomeCheck->setEnabled(canDomePark.toBool());
            parkDomeCheck->setEnabled(canDomePark.toBool());
            m_DomeReady = true;
        }
    }
    else if (iface == captureInterface)
    {
        QVariant hasCoolerControl = captureInterface->property("coolerControl");
        if (hasCoolerControl.isValid())
        {
            warmCCDCheck->setEnabled(hasCoolerControl.toBool());
            m_CaptureReady = true;
        }
    }
}

void Scheduler::setAlignStatus(Ekos::AlignState status)
{
    TEST_PRINT(stderr, "sch%d @@@setAlignStatus(%d)%s\n", __LINE__, static_cast<int>(status), (state == SCHEDULER_PAUSED
               || currentJob == nullptr) ? "IGNORED" : "OK");

    if (state == SCHEDULER_PAUSED || currentJob == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Align State" << Ekos::getAlignStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
    {
        QDateTime const now = getLocalTime();
        if (now < currentJob->getStartupTime())
            return;
    }

    if (currentJob->getStage() == SchedulerJob::STAGE_ALIGNING)
    {
        // Is solver complete?
        if (status == Ekos::ALIGN_COMPLETE)
        {
            appendLogText(i18n("Job '%1' alignment is complete.", currentJob->getName()));
            alignFailureCount = 0;

            currentJob->setStage(SchedulerJob::STAGE_ALIGN_COMPLETE);
            getNextAction();
        }
        else if (status == Ekos::ALIGN_FAILED || status == Ekos::ALIGN_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' alignment failed.", currentJob->getName()));

            if (alignFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                if (Options::resetMountModelOnAlignFail() && MAX_FAILURE_ATTEMPTS - 1 < alignFailureCount)
                {
                    appendLogText(i18n("Warning: job '%1' forcing mount model reset after failing alignment #%2.", currentJob->getName(),
                                       alignFailureCount));
                    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "mountInterface:call", "resetModel");
                    mountInterface->call(QDBus::AutoDetect, "resetModel");
                }
                appendLogText(i18n("Restarting %1 alignment procedure...", currentJob->getName()));
                startAstrometry();
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' alignment procedure failed, marking aborted.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ABORTED);

                findNextJob();
            }
        }
    }
}

void Scheduler::setGuideStatus(Ekos::GuideState status)
{
    TEST_PRINT(stderr, "sch%d @@@setGuideStatus(%d)%s\n", __LINE__, static_cast<int>(status), (state == SCHEDULER_PAUSED
               || currentJob == nullptr) ? "IGNORED" : "OK");
    if (state == SCHEDULER_PAUSED || currentJob == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Guide State" << Ekos::getGuideStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
    {
        QDateTime const now = getLocalTime();
        if (now < currentJob->getStartupTime())
            return;
    }

    if (currentJob->getStage() == SchedulerJob::STAGE_GUIDING)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Calibration & Guide stage...";

        // If calibration stage complete?
        if (status == Ekos::GUIDE_GUIDING)
        {
            appendLogText(i18n("Job '%1' guiding is in progress.", currentJob->getName()));
            guideFailureCount = 0;
            // if guiding recovered while we are waiting, abort the restart
            cancelGuidingTimer();

            currentJob->setStage(SchedulerJob::STAGE_GUIDING_COMPLETE);
            getNextAction();
        }
        else if (status == Ekos::GUIDE_CALIBRATION_ERROR ||
                 status == Ekos::GUIDE_ABORTED)
        {
            if (status == Ekos::GUIDE_ABORTED)
                appendLogText(i18n("Warning: job '%1' guiding failed.", currentJob->getName()));
            else
                appendLogText(i18n("Warning: job '%1' calibration failed.", currentJob->getName()));

            // if the timer for restarting the guiding is already running, we do nothing and
            // wait for the action triggered by the timer. This way we avoid that a small guiding problem
            // abort the scheduler job

            if (isGuidingTimerActive())
                return;

            if (guideFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                if (status == Ekos::GUIDE_CALIBRATION_ERROR &&
                        Options::realignAfterCalibrationFailure())
                {
                    appendLogText(i18n("Restarting %1 alignment procedure...", currentJob->getName()));
                    startAstrometry();
                }
                else
                {
                    appendLogText(i18n("Job '%1' is guiding, guiding procedure will be restarted in %2 seconds.", currentJob->getName(),
                                       (RESTART_GUIDING_DELAY_MS * guideFailureCount) / 1000));
                    startGuidingTimer(RESTART_GUIDING_DELAY_MS * guideFailureCount);
                }
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' guiding procedure failed, marking aborted.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ABORTED);

                findNextJob();
            }
        }
    }
}

GuideState Scheduler::getGuidingStatus()
{
    TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "guideInterface:property", "status");
    QVariant guideStatus = guideInterface->property("status");
    TEST_PRINT(stderr, "  @@@dbus received %d\n", !guideStatus.isValid() ? -1 : guideStatus.toInt());
    Ekos::GuideState gStatus = static_cast<Ekos::GuideState>(guideStatus.toInt());

    return gStatus;
}

void Scheduler::setCaptureStatus(Ekos::CaptureState status)
{
    TEST_PRINT(stderr, "sch%d @@@setCaptureStatus(%d) %s\n", __LINE__, static_cast<int>(status),
               (currentJob == nullptr) ? "IGNORED" : "OK");
    if (currentJob == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Capture State" << Ekos::getCaptureStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
    {
        QDateTime const now = getLocalTime();
        if (now < currentJob->getStartupTime())
            return;
    }

    if (currentJob->getStage() == SchedulerJob::STAGE_CAPTURING)
    {
        if (status == Ekos::CAPTURE_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' failed to capture target.", currentJob->getName()));

            if (captureFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                // If capture failed due to guiding error, let's try to restart that
                if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                {
                    // Check if it is guiding related.
                    Ekos::GuideState gStatus = getGuidingStatus();
                    if (gStatus == Ekos::GUIDE_ABORTED ||
                            gStatus == Ekos::GUIDE_CALIBRATION_ERROR ||
                            gStatus == GUIDE_DITHERING_ERROR)
                    {
                        appendLogText(i18n("Job '%1' is capturing, is restarting its guiding procedure (attempt #%2 of %3).", currentJob->getName(),
                                           captureFailureCount, MAX_FAILURE_ATTEMPTS));
                        startGuiding(true);
                        return;
                    }
                }

                /* FIXME: it's not clear whether it is actually possible to continue capturing when capture fails this way */
                appendLogText(i18n("Warning: job '%1' failed its capture procedure, restarting capture.", currentJob->getName()));
                startCapture(true);
            }
            else
            {
                /* FIXME: it's not clear whether this situation can be recovered at all */
                appendLogText(i18n("Warning: job '%1' failed its capture procedure, marking aborted.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ABORTED);

                findNextJob();
            }
        }
        else if (status == Ekos::CAPTURE_COMPLETE)
        {
            KNotification::event(QLatin1String("EkosScheduledImagingFinished"),
                                 i18n("Ekos job (%1) - Capture finished", currentJob->getName()));

            currentJob->setState(SchedulerJob::JOB_COMPLETE);
            findNextJob();
        }
        else if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
        {
            // We received a new image, but we don't know precisely where so update the storage map and re-estimate job times.
            // FIXME: rework this once capture storage is reworked
            if (Options::rememberJobProgress())
            {
                updateCompletedJobsCount(true);

                for (SchedulerJob * job : jobs)
                    estimateJobTime(job, m_CapturedFramesCount, this);
            }
            // Else if we don't remember the progress on jobs, increase the completed count for the current job only - no cross-checks
            else currentJob->setCompletedCount(currentJob->getCompletedCount() + 1);

            captureFailureCount = 0;
        }
    }
}

void Scheduler::setFocusStatus(Ekos::FocusState status)
{
    TEST_PRINT(stderr, "sch%d @@@setFocusStatus(%d)%s\n", __LINE__, static_cast<int>(status), (state == SCHEDULER_PAUSED
               || currentJob == nullptr) ? "IGNORED" : "OK");
    if (state == SCHEDULER_PAUSED || currentJob == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus State" << Ekos::getFocusStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
    {
        QDateTime const now = getLocalTime();
        if (now < currentJob->getStartupTime())
            return;
    }

    if (currentJob->getStage() == SchedulerJob::STAGE_FOCUSING)
    {
        // Is focus complete?
        if (status == Ekos::FOCUS_COMPLETE)
        {
            appendLogText(i18n("Job '%1' focusing is complete.", currentJob->getName()));

            autofocusCompleted = true;

            currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);

            getNextAction();
        }
        else if (status == Ekos::FOCUS_FAILED || status == Ekos::FOCUS_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' focusing failed.", currentJob->getName()));

            if (focusFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                appendLogText(i18n("Job '%1' is restarting its focusing procedure.", currentJob->getName()));
                // Reset frame to original size.
                TEST_PRINT(stderr, "sch%d @@@dbus(%s): %s\n", __LINE__, "focusInterface:call", "resetFrame");
                focusInterface->call(QDBus::AutoDetect, "resetFrame");
                // Restart focusing
                qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 6883";
                startFocusing();
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking aborted.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ABORTED);

                findNextJob();
            }
        }
    }
}

void Scheduler::setMountStatus(ISD::Telescope::Status status)
{
    TEST_PRINT(stderr, "sch%d @@@setMountStatus(%d)%s\n", __LINE__, static_cast<int>(status), (state == SCHEDULER_PAUSED
               || currentJob == nullptr) ? "IGNORED" : "OK");
    if (state == SCHEDULER_PAUSED || currentJob == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount State changed to" << status;

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
        if (static_cast<QDateTime const>(getLocalTime()) < currentJob->getStartupTime())
            return;

    switch (currentJob->getStage())
    {
        case SchedulerJob::STAGE_SLEWING:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Slewing stage...";

            if (status == ISD::Telescope::MOUNT_TRACKING)
            {
                appendLogText(i18n("Job '%1' slew is complete.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_SLEW_COMPLETE);
                /* getNextAction is deferred to checkJobStage for dome support */
            }
            else if (status == ISD::Telescope::MOUNT_ERROR)
            {
                appendLogText(i18n("Warning: job '%1' slew failed, marking terminated due to errors.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ERROR);
                findNextJob();
            }
            else if (status == ISD::Telescope::MOUNT_IDLE)
            {
                appendLogText(i18n("Warning: job '%1' found not slewing, restarting.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_IDLE);
                getNextAction();
            }
        }
        break;

        case SchedulerJob::STAGE_RESLEWING:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Re-slewing stage...";

            if (status == ISD::Telescope::MOUNT_TRACKING)
            {
                appendLogText(i18n("Job '%1' repositioning is complete.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_RESLEWING_COMPLETE);
                /* getNextAction is deferred to checkJobStage for dome support */
            }
            else if (status == ISD::Telescope::MOUNT_ERROR)
            {
                appendLogText(i18n("Warning: job '%1' repositioning failed, marking terminated due to errors.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ERROR);
                findNextJob();
            }
            else if (status == ISD::Telescope::MOUNT_IDLE)
            {
                appendLogText(i18n("Warning: job '%1' found not repositioning, restarting.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_IDLE);
                getNextAction();
            }
        }
        break;

        default:
            break;
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

    if (newStatus != weatherStatus)
    {
        weatherStatus = newStatus;

        qCDebug(KSTARS_EKOS_SCHEDULER) << statusString;

        if (weatherStatus == ISD::Weather::WEATHER_OK)
            weatherLabel->setPixmap(
                QIcon::fromTheme("security-high")
                .pixmap(QSize(32, 32)));
        else if (weatherStatus == ISD::Weather::WEATHER_WARNING)
        {
            weatherLabel->setPixmap(
                QIcon::fromTheme("security-medium")
                .pixmap(QSize(32, 32)));
            KNotification::event(QLatin1String("WeatherWarning"), i18n("Weather conditions in warning zone"));
        }
        else if (weatherStatus == ISD::Weather::WEATHER_ALERT)
        {
            weatherLabel->setPixmap(
                QIcon::fromTheme("security-low")
                .pixmap(QSize(32, 32)));
            KNotification::event(QLatin1String("WeatherAlert"),
                                 i18n("Weather conditions are critical. Observatory shutdown is imminent"));
        }
        else
            weatherLabel->setPixmap(QIcon::fromTheme("chronometer")
                                    .pixmap(QSize(32, 32)));

        weatherLabel->show();
        weatherLabel->setToolTip(statusString);

        appendLogText(statusString);

        emit weatherChanged(weatherStatus);
    }

    // Shutdown scheduler if it was started and not already in shutdown
    // and if weather checkbox is checked.
    if (weatherCheck->isChecked() && weatherStatus == ISD::Weather::WEATHER_ALERT && state != Ekos::SCHEDULER_IDLE
            && state != Ekos::SCHEDULER_SHUTDOWN)
    {
        appendLogText(i18n("Starting shutdown procedure due to severe weather."));
        if (currentJob)
        {
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            stopCurrentJobAction();
        }
        checkShutdownState();
    }
}

bool Scheduler::shouldSchedulerSleep(SchedulerJob *currentJob)
{
    Q_ASSERT_X(nullptr != currentJob, __FUNCTION__,
               "There must be a valid current job for Scheduler to test sleep requirement");

    if (currentJob->getLightFramesRequired() == false)
        return false;

    QDateTime const now = getLocalTime();
    int const nextObservationTime = now.secsTo(currentJob->getStartupTime());

    // If start up procedure is complete and the user selected pre-emptive shutdown, let us check if the next observation time exceed
    // the pre-emptive shutdown time in hours (default 2). If it exceeds that, we perform complete shutdown until next job is ready
    if (startupState == STARTUP_COMPLETE &&
            Options::preemptiveShutdown() &&
            nextObservationTime > (Options::preemptiveShutdownTime() * 3600))
    {
        appendLogText(i18n(
                          "Job '%1' scheduled for execution at %2. "
                          "Observatory scheduled for shutdown until next job is ready.",
                          currentJob->getName(), currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat())));
        enablePreemptiveShutdown(currentJob->getStartupTime());
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
             startupState == STARTUP_COMPLETE &&
             parkWaitState == PARKWAIT_IDLE &&
             (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK) &&
             parkMountCheck->isEnabled() &&
             parkMountCheck->isChecked())
    {
        appendLogText(i18n(
                          "Job '%1' scheduled for execution at %2. "
                          "Parking the mount until the job is ready.",
                          currentJob->getName(), currentJob->getStartupTime().toString()));

        parkWaitState = PARKWAIT_PARK;

        return false;
    }
    else if (nextObservationTime > Options::leadTime() * 60)
    {
        appendLogText(i18n("Sleeping until observation job %1 is ready at %2...", currentJob->getName(),
                           now.addSecs(nextObservationTime + 1).toString()));
        sleepLabel->setToolTip(i18n("Scheduler is in sleep mode"));
        sleepLabel->show();

        // Warn the user if the next job is really far away - 60/5 = 12 times the lead time
        if (nextObservationTime > Options::leadTime() * 60 * 12 && !Options::preemptiveShutdown())
        {
            dms delay(static_cast<double>(nextObservationTime * 15.0 / 3600.0));
            appendLogText(i18n(
                              "Warning: Job '%1' is %2 away from now, you may want to enable Preemptive Shutdown.",
                              currentJob->getName(), delay.toHMSString()));
        }

        /* FIXME: stop tracking now */

        // Wake up when job is due.
        // FIXME: Implement waking up periodically before job is due for weather check.
        // int const nextWakeup = nextObservationTime < 60 ? nextObservationTime : 60;
        TEST_PRINT(stderr, "%d Setting %s\n", __LINE__, timerStr(RUN_WAKEUP).toLatin1().data());
        setupNextIteration(RUN_WAKEUP, std::lround(((nextObservationTime + 1) * 1000) / KStarsData::Instance()->clock()->scale()));

        return true;
    }

    return false;
}

// TODO. It would be better to make this a class and give each operation its own timer.
// TODO. These should be disabled once no longer relevant.
// These are implement with a KStarsDateTime instead of a QTimer type class
// so that the simulated clock can be used.
void Scheduler::startCurrentOperationTimer()
{
    currentOperationTimeStarted = true;
    currentOperationTime = KStarsData::Instance()->ut();
}

// Returns milliseconds since startCurrentOperationTImer() was called.
qint64 Scheduler::getCurrentOperationMsec()
{
    if (!currentOperationTimeStarted) return 0;
    return currentOperationTime.msecsTo(KStarsData::Instance()->ut());
}

// Operations on the guiding timer, which can restart guiding after failure.
void Scheduler::startGuidingTimer(int milliseconds)
{
    restartGuidingInterval = milliseconds;
    restartGuidingTime = KStarsData::Instance()->ut();
}

void Scheduler::cancelGuidingTimer()
{
    restartGuidingInterval = -1;
    restartGuidingTime = KStarsDateTime();
}

bool Scheduler::isGuidingTimerActive()
{
    return (restartGuidingInterval > 0 &&
            restartGuidingTime.msecsTo(KStarsData::Instance()->ut()) >= 0);
}

void Scheduler::processGuidingTimer()
{
    if ((restartGuidingInterval > 0) &&
            (restartGuidingTime.msecsTo(KStarsData::Instance()->ut()) > restartGuidingInterval))
    {
        cancelGuidingTimer();
        startGuiding(true);
    }
}

void Scheduler::enablePreemptiveShutdown(const QDateTime &wakeupTime)
{
    preemptiveShutdownWakeupTime = wakeupTime;
}

void Scheduler::disablePreemptiveShutdown()
{
    preemptiveShutdownWakeupTime = QDateTime();
}

QDateTime Scheduler::getPreemptiveShutdownWakeupTime()
{
    return preemptiveShutdownWakeupTime;
}

bool Scheduler::preemptiveShutdown()
{
    return preemptiveShutdownWakeupTime.isValid();
}

}
