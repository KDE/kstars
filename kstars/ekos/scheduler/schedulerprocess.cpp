/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "schedulerprocess.h"
#include "schedulermodulestate.h"
#include "scheduleradaptor.h"
#include "greedyscheduler.h"
#include "schedulerutils.h"
#include "schedulerjob.h"
#include "schedulersafetymonitor.h"
#include "taskqueue/queue/queuemanager.h"
#include "taskqueue/queue/queueexecutor.h"
#include "taskqueue/queue/queueitem.h"
#include "ekos/capture/sequencejob.h"
#include "Options.h"
#include "ksmessagebox.h"
#include "ksnotification.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "indi/indistd.h"
#include "indi/indimount.h"
#include "ekos/analyze/analyze.h"
#include "skymapcomposite.h"
#include "mosaiccomponent.h"
#include "mosaictiles.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include <ekos_scheduler_debug.h>

#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusInterface>

#define RESTART_GUIDING_DELAY_MS  5000

// This is a temporary debugging printout introduced while gaining experience developing
// the unit tests in test_ekos_scheduler_ops.cpp.
// All these printouts should be eventually removed.

namespace Ekos
{

SchedulerProcess::SchedulerProcess(QSharedPointer<SchedulerModuleState> state, const QString &ekosPathStr,
                                   const QString &ekosInterfaceStr, QueueManager *queueManager, QueueExecutor *queueExecutor)
    : QObject(KStars::Instance()), m_queueManager(queueManager), m_queueExecutor(queueExecutor)
{
    setObjectName("SchedulerProcess");
    m_moduleState = state;
    m_GreedyScheduler = new GreedyScheduler();

    // Connect queue executor signals if available
    if (m_queueExecutor)
    {
        connect(m_queueExecutor, &QueueExecutor::completed, this, &SchedulerProcess::queueExecutionCompleted);
        connect(m_queueExecutor, &QueueExecutor::itemFailed, this, &SchedulerProcess::queueItemFailed);
    }
    connect(KConfigDialog::exists("settings"), &KConfigDialog::settingsChanged, this, &SchedulerProcess::applyConfig);

    // Connect simulation clock scale
    connect(KStarsData::Instance()->clock(), &SimClock::scaleChanged, this, &SchedulerProcess::simClockScaleChanged);
    connect(KStarsData::Instance()->clock(), &SimClock::timeChanged, this, &SchedulerProcess::simClockTimeChanged);

    // connection to state machine events
    connect(moduleState().data(), &SchedulerModuleState::schedulerStateChanged, this, &SchedulerProcess::newStatus);
    connect(moduleState().data(), &SchedulerModuleState::newLog, this, &SchedulerProcess::appendLogText);

    // Set up DBus interfaces
    new SchedulerAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(schedulerProcessPathString);
    if (!QDBusConnection::sessionBus().registerObject(schedulerProcessPathString, this))
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("SchedulerProcess failed to register with dbus");

    setEkosInterface(new QDBusInterface(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr,
                                        QDBusConnection::sessionBus(), this));
    setIndiInterface(new QDBusInterface(kstarsInterfaceString, INDIPathString, INDIInterfaceString,
                                        QDBusConnection::sessionBus(), this));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "indiStatusChanged",
                                          this, SLOT(setINDICommunicationStatus(Ekos::CommunicationStatus)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "ekosStatusChanged",
                                          this, SLOT(setEkosCommunicationStatus(Ekos::CommunicationStatus)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "newModule", this,
                                          SLOT(registerNewModule(QString)));
    QDBusConnection::sessionBus().connect(kstarsInterfaceString, ekosPathStr, ekosInterfaceStr, "newDevice", this,
                                          SLOT(registerNewDevice(QString, int)));

    m_WeatherShutdownTimer.setSingleShot(true);
    connect(&m_WeatherShutdownTimer, &QTimer::timeout, this, &SchedulerProcess::startShutdownDueToWeather);

    // Initialize standalone safety monitor monitoring (before equipment profile starts)
    if (Options::schedulerWeather())
    {
        QString connectionString = Options::schedulerSafetyMonitorConnectionString();
        if (!connectionString.isEmpty())
        {
            m_StandaloneSafetyMonitor = new SchedulerSafetyMonitor(this);
            connect(m_StandaloneSafetyMonitor, &SchedulerSafetyMonitor::newSafetyStatus, this, &SchedulerProcess::setSafetyStatus);
            connect(m_StandaloneSafetyMonitor, &SchedulerSafetyMonitor::newLog, this, &SchedulerProcess::appendLogText);
            m_StandaloneSafetyMonitor->initStandaloneSafetyMonitor(connectionString);
        }
    }
}

SchedulerProcess::~SchedulerProcess()
{
    // Clean up standalone safety monitor monitoring
    if (m_StandaloneSafetyMonitor)
    {
        delete m_StandaloneSafetyMonitor;
        m_StandaloneSafetyMonitor = nullptr;
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << "SchedulerProcess destructor: Standalone safety monitor cleanup complete";
}

SchedulerState SchedulerProcess::status()
{
    return moduleState()->schedulerState();
}

void SchedulerProcess::execute()
{
    switch (moduleState()->schedulerState())
    {
        case SCHEDULER_IDLE:
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is starting...";

            moduleState()->setSchedulerState(SCHEDULER_RUNNING);
            moduleState()->setupNextIteration(RUN_SCHEDULER);

            appendLogText(i18n("Scheduler started."));
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler started.";
            break;

        case SCHEDULER_PAUSED:
            moduleState()->setSchedulerState(SCHEDULER_RUNNING);
            moduleState()->setupNextIteration(RUN_SCHEDULER);

            appendLogText(i18n("Scheduler resuming."));
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler resuming.";
            break;

        default:
            break;
    }

}

// FindNextJob (probably misnamed) deals with what to do when jobs end.
// For instance, if they complete their capture sequence, they may
// (a) be done, (b) be part of a repeat N times, or (c) be part of a loop forever.
// Similarly, if jobs are aborted they may (a) restart right away, (b) restart after a delay, (c) be ended.
void SchedulerProcess::findNextJob()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
    {
        // everything finished, we can pause
        setPaused();
        return;
    }

    Q_ASSERT_X(activeJob()->getState() == SCHEDJOB_ERROR ||
               activeJob()->getState() == SCHEDJOB_ABORTED ||
               activeJob()->getState() == SCHEDJOB_COMPLETE ||
               activeJob()->getState() == SCHEDJOB_IDLE,
               __FUNCTION__, "Finding next job requires current to be in error, aborted, idle or complete");

    // Reset failed count
    moduleState()->resetAlignFailureCount();
    moduleState()->resetGuideFailureCount();
    moduleState()->resetFocusFailureCount();
    moduleState()->resetCaptureFailureCount();

    if (activeJob()->getState() == SCHEDJOB_ERROR || activeJob()->getState() == SCHEDJOB_ABORTED)
    {
        emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());
        moduleState()->resetCaptureBatch();
        // Stop Guiding if it was used
        stopGuiding();

        if (activeJob()->getState() == SCHEDJOB_ERROR)
            appendLogText(i18n("Job '%1' is terminated due to errors.", activeJob()->getName()));
        else
            appendLogText(i18n("Job '%1' is aborted.", activeJob()->getName()));

        // Always reset job stage
        moduleState()->updateJobStage(SCHEDSTAGE_IDLE);

        // Restart aborted jobs immediately, if error handling strategy is set to "restart immediately"
        // but only if the job could resume.
        bool canResume = false;
        if (Options::errorHandlingStrategy() == ERROR_RESTART_IMMEDIATELY &&
                (activeJob()->getState() == SCHEDJOB_ABORTED ||
                 (activeJob()->getState() == SCHEDJOB_ERROR && Options::rescheduleErrors())))
        {
            const auto oldState = activeJob()->getState();
            activeJob()->setState(SCHEDJOB_SCHEDULED);
            // Need to add a few seconds, since greedy scheduler doesn't like bumping jobs that just changed state.
            canResume = getGreedyScheduler()->checkJob(moduleState()->jobs(), SchedulerModuleState::getLocalTime().addSecs(30),
                        activeJob());
            activeJob()->setState(oldState);
        }
        if (canResume)
        {
            // reset the state so that it will be restarted
            activeJob()->setState(SCHEDJOB_SCHEDULED);

            appendLogText(i18n("Waiting %1 seconds to restart job '%2'.", Options::errorHandlingStrategyDelay(),
                               activeJob()->getName()));

            // wait the given delay until the jobs will be evaluated again
            moduleState()->setupNextIteration(RUN_WAKEUP, std::lround((Options::errorHandlingStrategyDelay() * 1000) /
                                              KStarsData::Instance()->clock()->scale()));
            emit changeSleepLabel(i18n("Scheduler waits for a retry."));
            return;
        }

        // otherwise start re-evaluation
        moduleState()->setActiveJob(nullptr);
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
    else if (activeJob()->getState() == SCHEDJOB_IDLE)
    {
        emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());

        // job constraints no longer valid, start re-evaluation
        moduleState()->setActiveJob(nullptr);
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
                    a_job->setState(SCHEDJOB_IDLE);
        }

        moduleState()->resetCaptureBatch();
        // Stop Guiding if it was used
        stopGuiding();

        appendLogText(i18n("Job '%1' is complete.", activeJob()->getName()));

        // Always reset job stage
        moduleState()->updateJobStage(SCHEDSTAGE_IDLE);

        // If saving remotely, then can't tell later that the job has been completed.
        // Set it complete now.
        if (!canCountCaptures(*activeJob()))
            activeJob()->setState(SCHEDJOB_COMPLETE);

        moduleState()->setActiveJob(nullptr);
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
                a_job->setState(SCHEDJOB_IDLE);

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
                    activeJob()->setState(SCHEDJOB_COMPLETE);
                moduleState()->setActiveJob(nullptr);
            }
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
                stopGuiding();
                moduleState()->updateJobStage(SCHEDSTAGE_ALIGNING);
                startAstrometry();
            }
            /* If we are guiding, continue capturing */
            else if ( (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE) )
            {
                moduleState()->updateJobStage(SCHEDSTAGE_CAPTURING);
                startCapture();
            }
            /* If we are not guiding, but using alignment, realign */
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
            {
                moduleState()->updateJobStage(SCHEDSTAGE_ALIGNING);
                startAstrometry();
            }
            /* Else if we are neither guiding nor using alignment, slew back to target */
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_TRACK)
            {
                moduleState()->updateJobStage(SCHEDSTAGE_SLEWING);
                startSlew();
            }
            /* Else just start capturing */
            else
            {
                moduleState()->updateJobStage(SCHEDSTAGE_CAPTURING);
                startCapture();
            }

            appendLogText(i18np("Job '%1' is repeating, #%2 batch remaining.",
                                "Job '%1' is repeating, #%2 batches remaining.",
                                activeJob()->getName(), activeJob()->getRepeatsRemaining()));
            /* getActiveJob() remains the same */
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
            stopGuiding();
            moduleState()->updateJobStage(SCHEDSTAGE_ALIGNING);
            startAstrometry();
        }
        else
        {
            moduleState()->updateJobStage(SCHEDSTAGE_CAPTURING);
            startCapture();
        }

        moduleState()->increaseCaptureBatch();

        if (activeJob()->getCompletionCondition() == FINISH_REPEAT )
            appendLogText(i18np("Job '%1' is repeating, #%2 batch remaining.",
                                "Job '%1' is repeating, #%2 batches remaining.",
                                activeJob()->getName(), activeJob()->getRepeatsRemaining()));
        else
            appendLogText(i18n("Job '%1' is repeating, looping indefinitely.", activeJob()->getName()));

        /* getActiveJob() remains the same */
        moduleState()->setupNextIteration(RUN_JOBCHECK);
    }
    else if (activeJob()->getCompletionCondition() == FINISH_AT)
    {
        if (SchedulerModuleState::getLocalTime().secsTo(activeJob()->getFinishAtTime()) <= 0)
        {
            emit jobEnded(activeJob()->getName(), activeJob()->getStopReason());

            /* Mark the job idle as well as all its duplicates for re-evaluation */
            foreach(SchedulerJob *a_job, moduleState()->jobs())
                if (a_job == activeJob() || a_job->isDuplicateOf(activeJob()))
                    a_job->setState(SCHEDJOB_IDLE);
            stopCurrentJobAction();

            moduleState()->resetCaptureBatch();

            appendLogText(i18np("Job '%1' stopping, reached completion time with #%2 batch done.",
                                "Job '%1' stopping, reached completion time with #%2 batches done.",
                                activeJob()->getName(), moduleState()->captureBatch() + 1));

            // Always reset job stage
            moduleState()->updateJobStage(SCHEDSTAGE_IDLE);

            moduleState()->setActiveJob(nullptr);
            moduleState()->setupNextIteration(RUN_SCHEDULER);
        }
        else
        {
            if (executeJob(activeJob()) == false)
                return;

            if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN && Options::forceAlignmentBeforeJob())
            {
                stopGuiding();
                moduleState()->updateJobStage(SCHEDSTAGE_ALIGNING);
                startAstrometry();
            }
            else
            {
                moduleState()->updateJobStage(SCHEDSTAGE_CAPTURING);
                startCapture();
            }

            moduleState()->increaseCaptureBatch();

            appendLogText(i18np("Job '%1' completed #%2 batch before completion time, restarted.",
                                "Job '%1' completed #%2 batches before completion time, restarted.",
                                activeJob()->getName(), moduleState()->captureBatch()));
            /* getActiveJob() remains the same */
            moduleState()->setupNextIteration(RUN_JOBCHECK);
        }
    }
    else
    {
        /* Unexpected situation, mitigate by resetting the job and restarting the scheduler timer */
        qCDebug(KSTARS_EKOS_SCHEDULER) << "BUGBUG! Job '" << activeJob()->getName() <<
                                          "' timer elapsed, but no action to be taken.";

        // Always reset job stage
        moduleState()->updateJobStage(SCHEDSTAGE_IDLE);

        moduleState()->setActiveJob(nullptr);
        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
}

void Ekos::SchedulerProcess::stopCapturing(QString train, bool followersOnly)
{
    if (train == "" && followersOnly)
    {
        for (auto key : m_activeJobs.keys())
        {
            // abort capturing of all jobs except for the lead job
            SchedulerJob *job = m_activeJobs[key];
            if (! job->isLead())
            {
                QList<QVariant> dbusargs;
                dbusargs.append(job->getOpticalTrain());
                captureInterface()->callWithArgumentList(QDBus::BlockWithGui, "abort", dbusargs);
                job->setState(SCHEDJOB_ABORTED);
            }
        }
    }
    else
    {
        QList<QVariant> dbusargs;
        dbusargs.append(train);
        captureInterface()->callWithArgumentList(QDBus::BlockWithGui, "abort", dbusargs);

        // set all relevant jobs to aborted
        for (auto job : m_activeJobs.values())
            if (train == "" || job->getOpticalTrain() == train)
                job->setState(SCHEDJOB_ABORTED);
    }
}

void SchedulerProcess::stopCurrentJobAction()
{
    if (nullptr != activeJob())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job '" << activeJob()->getName() << "' is stopping current action..." <<
                                       activeJob()->getStage();

        switch (activeJob()->getStage())
        {
            case SCHEDSTAGE_IDLE:
                break;

            case SCHEDSTAGE_SLEWING:
                mountInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SCHEDSTAGE_FOCUSING:
                focusInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SCHEDSTAGE_ALIGNING:
                alignInterface()->call(QDBus::AutoDetect, "abort");
                break;

            // N.B. Need to use BlockWithGui as proposed by Wolfgang
            // to ensure capture is properly aborted before taking any further actions.
            case SCHEDSTAGE_CAPTURING:
                stopCapturing();
                break;

            default:
                break;
        }

        /* Reset interrupted job stage */
        moduleState()->updateJobStage(SCHEDSTAGE_IDLE);
    }

    /* Guiding being a parallel process, check to stop it */
    stopGuiding();
}

void SchedulerProcess::wakeUpScheduler()
{
    if (moduleState()->preemptiveShutdown())
    {
        // If waking up from weather grace period, check if weather has improved
        if (moduleState()->weatherGracePeriodActive())
        {
            ISD::Weather::Status weatherStatus = moduleState()->weatherStatus();

            // Check if weather has improved
            if (weatherStatus == ISD::Weather::WEATHER_OK)
            {
                // Weather improved during grace period, proceed with recovery
                appendLogText(i18n("Weather has improved during grace period. Resuming operations."));
                moduleState()->setWeatherGracePeriodActive(false);
            }
            else
            {
                // Grace period has expired and weather is still bad - complete shutdown
                appendLogText(i18n("Weather grace period has expired with safety status still %1. Proceeding to complete shutdown.",
                                   (weatherStatus == ISD::Weather::WEATHER_WARNING) ? i18n("Warning") : i18n("Alert")));

                if (activeJob())
                    activeJob()->setState(SCHEDJOB_IDLE);

                // If we're going into FULL shutdown mode
                // we need to be able to begin startup later from IDLE
                // since we're not going to *stop* the scheduler officially.
                moduleState()->setStartupState(STARTUP_IDLE);
                // No need for grace period anymore
                moduleState()->setWeatherGracePeriodActive(false);
                // No need to preemptive shutdown
                moduleState()->disablePreemptiveShutdown();
                // Clear job
                moduleState()->setActiveJob(nullptr);
                // Set flag to indicate we're entering weather monitoring mode
                moduleState()->setWeatherShutdownMonitoring(true);
                // Proceed to remaining shutdown stages
                checkShutdownState();
                return;
            }
        }

        moduleState()->disablePreemptiveShutdown();
        appendLogText(i18n("Scheduler is awake."));

        // If we're recovering from weather alert soft shutdown, execute post-startup queue
        // to reverse the parking/shutdown actions (symmetric with pre-shutdown queue execution)
        if (moduleState()->weatherGracePeriodActive() &&
                Options::schedulerStartupEnabled() &&
                m_queueManager &&
                !moduleState()->postStartupScriptURL().isEmpty())
        {
            QString queueFile = moduleState()->postStartupScriptURL().toLocalFile();
            if (m_queueManager->loadQueue(queueFile))
            {
                if (m_queueManager->count() > 0)
                {
                    appendLogText(i18n("Executing post-startup tasks from %1 after safety recovery...", queueFile));
                    // Set a temporary state to track post-weather-recovery startup
                    moduleState()->setStartupState(STARTUP_POST_DEVICES_RUNNING);
                    m_queueExecutor->start();
                    return; // Don't call execute() yet, wait for queue completion
                }
            }
            else
            {
                appendLogText(i18n("Warning: Failed to load post-startup queue from %1 after safety recovery", queueFile));
            }
        }

        execute();
    }
    else
    {
        if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when ready..."));
        else
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when scheduler is resumed."));

        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
}

void SchedulerProcess::start()
{
    // New scheduler session shouldn't inherit ABORT or ERROR states from the last one.
    foreach (auto j, moduleState()->jobs())
    {
        j->setState(SCHEDJOB_IDLE);
        emit updateJobTable(j);
    }
    moduleState()->init();
    iterate();
}

void SchedulerProcess::stop()
{
    // do nothing if the scheduler is not running
    if (moduleState()->schedulerState() != SCHEDULER_RUNNING)
        return;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is stopping...";

    // Stop running job and abort all others
    // in case of soft shutdown we skip this
    if (!moduleState()->preemptiveShutdown())
    {
        for (auto &oneJob : moduleState()->jobs())
        {
            if (oneJob == activeJob())
                stopCurrentJobAction();

            if (oneJob->getState() <= SCHEDJOB_BUSY)
            {
                appendLogText(i18n("Job '%1' has not been processed upon scheduler stop, marking aborted.", oneJob->getName()));
                oneJob->setState(SCHEDJOB_ABORTED);
            }
        }
    }

    moduleState()->setupNextIteration(RUN_NOTHING);
    moduleState()->cancelGuidingTimer();
    moduleState()->tickleTimer().stop();

    moduleState()->setSchedulerState(SCHEDULER_IDLE);
    moduleState()->setParkWaitState(PARKWAIT_IDLE);
    moduleState()->setEkosState(EKOS_IDLE);
    moduleState()->setIndiState(INDI_IDLE);

    // Handle startup state reset based on shutdown type
    // For preemptive shutdown (weather/sleep), maintain POST_DEVICES to skip pre-startup on wakeup
    // For normal shutdown, reset to IDLE only if startup was complete to allow full sequence on restart
    if (!moduleState()->preemptiveShutdown())
    {
        // Normal shutdown: if startup completed, reset to IDLE so next run does full startup
        // This ensures post-startup queue (unparking) runs on every scheduler restart
        if (moduleState()->startupState() == STARTUP_COMPLETE)
            moduleState()->setStartupState(STARTUP_IDLE);
    }
    // For preemptive shutdown, startup state is managed by wakeUpScheduler() and weather recovery logic

    moduleState()->setShutdownState(SHUTDOWN_IDLE);

    moduleState()->setActiveJob(nullptr);
    moduleState()->resetFailureCounters();
    moduleState()->resetAutofocusCompleted();

    // Clear weather shutdown monitoring flag when stopping
    moduleState()->setWeatherShutdownMonitoring(false);

    // If soft shutdown, we return for now
    if (moduleState()->preemptiveShutdown())
    {
        QDateTime const now = SchedulerModuleState::getLocalTime();
        int const nextObservationTime = now.secsTo(moduleState()->preemptiveShutdownWakeupTime());
        moduleState()->setupNextIteration(RUN_WAKEUP,
                                          std::lround(((nextObservationTime + 1) * 1000)
                                                  / KStarsData::Instance()->clock()->scale()));
        // report success
        emit schedulerStopped();
        return;
    }

    // Clear target name in capture interface upon stopping
    if (captureInterface().isNull() == false)
        captureInterface()->setProperty("targetName", QString());

    // scriptProcess is no longer used for shutdown.
    // The queue manager handles script execution.

    // report success
    emit schedulerStopped();
}

void SchedulerProcess::removeAllJobs()
{
    emit clearJobTable();

    qDeleteAll(moduleState()->jobs());
    moduleState()->mutlableJobs().clear();
    moduleState()->setCurrentPosition(-1);

}

bool SchedulerProcess::loadScheduler(const QString &fileURL)
{
    removeAllJobs();
    return appendEkosScheduleList(fileURL);
}

void SchedulerProcess::setSequence(const QString &sequenceFileURL)
{
    emit changeCurrentSequence(sequenceFileURL);
}

void SchedulerProcess::resetAllJobs()
{
    if (moduleState()->schedulerState() == SCHEDULER_RUNNING)
        return;

    // Reset capture count of all jobs before re-evaluating
    foreach (SchedulerJob *job, moduleState()->jobs())
        job->setCompletedCount(0);

    // Evaluate all jobs, this refreshes storage and resets job states
    startJobEvaluation();
}

bool SchedulerProcess::shouldSchedulerSleep(SchedulerJob * job)
{
    Q_ASSERT_X(nullptr != job, __FUNCTION__,
               "There must be a valid current job for Scheduler to test sleep requirement");

    if (job->getLightFramesRequired() == false)
        return false;

    QDateTime const now = SchedulerModuleState::getLocalTime();
    int const nextObservationTime = now.secsTo(job->getStartupTime());

    // It is possible that the nextObservationTime is far away, but the reason is that
    // the user has edited the jobs, and now the active job is not the next thing scheduled.
    if (getGreedyScheduler()->getScheduledJob() != job)
        return false;

    // Check weather status before starting the job, if we're not already in preemptive shutdown
    if (Options::schedulerWeather())
    {
        ISD::Weather::Status weatherStatus = moduleState()->weatherStatus();
        // Do not evaluate job that are in the process of getting aborted or aborted or complete already
        if (m_WeatherShutdownTimer.isActive() == false && job->getState() < SCHEDJOB_ERROR &&
                (weatherStatus == ISD::Weather::WEATHER_WARNING || weatherStatus == ISD::Weather::WEATHER_ALERT))
        {
            // If we're already in grace period, just indicate we should sleep
            // The wakeup timer has already been scheduled by startShutdownDueToWeather()
            if (moduleState()->weatherGracePeriodActive())
            {
                appendLogText(i18n("Job '%1' cannot start, weather grace period is active.",
                                   job->getName()));
                return true;
            }

            startShutdownDueToWeather();
            return true;
        }
    }
    else
        moduleState()->setWeatherGracePeriodActive(false);

    // If start up procedure is complete and the user selected pre-emptive shutdown, let us check if the next observation time exceed
    // the pre-emptive shutdown time in hours (default 2). If it exceeds that, we perform complete shutdown until next job is ready
    if (moduleState()->startupState() == STARTUP_COMPLETE &&
            Options::preemptiveShutdown() &&
            nextObservationTime > (Options::preemptiveShutdownTime() * 3600))
    {
        appendLogText(i18n(
                          "Job '%1' scheduled for execution at %2. "
                          "Observatory scheduled for shutdown until next job is ready.",
                          job->getName(), job->getStartupTime().toString()));
        moduleState()->enablePreemptiveShutdown(job->getStartupTime());
        checkShutdownState();
        emit schedulerSleeping(true, false);
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
             (job->getStepPipeline() & SchedulerJob::USE_TRACK))
    {
        appendLogText(i18n(
                          "Job '%1' scheduled for execution at %2. "
                          "Parking the mount until the job is ready.",
                          job->getName(), job->getStartupTime().toString()));

        // Use queue manager to park mount programmatically
        if (m_queueManager && m_queueManager->addMountParkTask())
        {
            // Park task added successfully, let queue manager handle it
            return false;
        }
        else
        {
            // Failed to add park task, continue without parking
            appendLogText(i18n("Warning: Failed to add mount park task."));
            return false;
        }
    }
    else if (nextObservationTime > Options::leadTime() * 60)
    {
        auto log = i18n("Sleeping until observation job %1 is ready at %2", job->getName(),
                        now.addSecs(nextObservationTime + 1).toString());
        appendLogText(log);
        KSNotification::event(QLatin1String("SchedulerSleeping"), log, KSNotification::Scheduler,
                              KSNotification::Info);

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
        moduleState()->setupNextIteration(RUN_WAKEUP,
                                          std::lround(((nextObservationTime + 1) * 1000) / KStarsData::Instance()->clock()->scale()));

        emit schedulerSleeping(false, true);
        return true;
    }

    return false;
}

void SchedulerProcess::startSlew()
{
    Q_ASSERT_X(nullptr != activeJob(), __FUNCTION__, "Job starting slewing must be valid");

    // If the mount was parked by a pause or the end-user, unpark using queue manager
    if (isMountParked())
    {
        if (m_queueManager && m_queueManager->addMountUnparkTask())
        {
            appendLogText(i18n("Mount is parked, adding unpark task before slewing."));
            return;
        }
        else
        {
            appendLogText(i18n("Warning: Failed to add mount unpark task, attempting to slew anyway."));
        }
    }

    if (Options::resetMountModelBeforeJob())
    {
        mountInterface()->call(QDBus::AutoDetect, "resetModel");
    }

    SkyPoint target = activeJob()->getTargetCoords();
    QList<QVariant> telescopeSlew;
    telescopeSlew.append(target.ra().Hours());
    telescopeSlew.append(target.dec().Degrees());

    QDBusReply<bool> const slewModeReply = mountInterface()->callWithArgumentList(QDBus::AutoDetect, "slew",
                                           telescopeSlew);

    if (slewModeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' slew request received DBUS error: %2").arg(
                                              activeJob()->getName(), QDBusError::errorString(slewModeReply.error().type()));
        if (!manageConnectionLoss())
            activeJob()->setState(SCHEDJOB_ERROR);
    }
    else
    {
        moduleState()->updateJobStage(SCHEDSTAGE_SLEWING);
        appendLogText(i18n("Job '%1' is slewing to target.", activeJob()->getName()));
    }
}

void SchedulerProcess::startFocusing()
{
    Q_ASSERT_X(nullptr != activeJob(), __FUNCTION__, "Job starting focusing must be valid");
    // 2017-09-30 Jasem: We're skipping post align focusing now as it can be performed
    // when first focus request is made in capture module
    if (activeJob()->getStage() == SCHEDSTAGE_RESLEWING_COMPLETE ||
            activeJob()->getStage() == SCHEDSTAGE_POSTALIGN_FOCUSING)
    {
        // Clear the HFR limit value set in the capture module
        captureInterface()->call(QDBus::AutoDetect, "clearAutoFocusHFR");
        // Reset Focus frame so that next frame take a full-resolution capture first.
        focusInterface()->call(QDBus::AutoDetect, "resetFrame");
        moduleState()->updateJobStage(SCHEDSTAGE_POSTALIGN_FOCUSING_COMPLETE);
        getNextAction();
        return;
    }

    if (activeJob()->getOpticalTrain() != "")
        m_activeJobs.insert(activeJob()->getOpticalTrain(), activeJob());
    else
    {
        QVariant opticalTrain = captureInterface()->property("opticalTrain");

        if (opticalTrain.isValid() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' opticalTrain request failed.").arg(activeJob()->getName());
            if (!manageConnectionLoss())
            {
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }
        // use the default optical train for the active job
        m_activeJobs.insert(opticalTrain.toString(), activeJob());
        activeJob()->setOpticalTrain(opticalTrain.toString());
    }

    // start focusing of the lead job
    startFocusing(activeJob());
    // start focusing of all follower jobds
    foreach (auto follower, activeJob()->followerJobs())
    {
        m_activeJobs.insert(follower->getOpticalTrain(), follower);
        startFocusing(follower);
    }
}


void SchedulerProcess::startFocusing(SchedulerJob *job)
{

    // Check if autofocus is supported
    QDBusReply<bool> boolReply;
    QList<QVariant> dBusArgs;
    dBusArgs.append(job->getOpticalTrain());
    boolReply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "canAutoFocus", dBusArgs);

    if (boolReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' canAutoFocus request received DBUS error: %2").arg(
                                              job->getName(), QDBusError::errorString(boolReply.error().type()));
        if (!manageConnectionLoss())
        {
            job->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }

    if (boolReply.value() == false)
    {
        appendLogText(i18n("Warning: job '%1' is unable to proceed with autofocus, not supported.", job->getName()));
        job->setStepPipeline(
            static_cast<SchedulerJob::StepPipeline>(job->getStepPipeline() & ~SchedulerJob::USE_FOCUS));
        moduleState()->setAutofocusCompleted(job->getOpticalTrain(), true);
        if (moduleState()->autofocusCompleted())
        {
            moduleState()->updateJobStage(SCHEDSTAGE_FOCUS_COMPLETE);
            getNextAction();
            return;
        }
    }

    QDBusMessage reply;

    // Clear the HFR limit value set in the capture module
    if ((reply = captureInterface()->callWithArgumentList(QDBus::AutoDetect, "clearAutoFocusHFR",
                 dBusArgs)).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' clearAutoFocusHFR request received DBUS error: %2").arg(
                                              job->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
        {
            job->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }

    // We always need to reset frame first
    if ((reply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "resetFrame",
                 dBusArgs)).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' resetFrame request received DBUS error: %2").arg(
                                              job->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
        {
            job->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }


    // If we have a LIGHT filter set, let's set it.
    if (!job->getInitialFilter().isEmpty())
    {
        dBusArgs.clear();
        dBusArgs.append(job->getInitialFilter());
        dBusArgs.append(job->getOpticalTrain());
        if ((reply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "setFilter",
                     dBusArgs)).type() == QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setFilter request received DBUS error: %1").arg(
                                                  job->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
            {
                job->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }
    }

    dBusArgs.clear();
    dBusArgs.append(job->getOpticalTrain());
    boolReply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "useFullField", dBusArgs);

    if (boolReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' useFullField request received DBUS error: %2").arg(
                                              job->getName(), QDBusError::errorString(boolReply.error().type()));
        if (!manageConnectionLoss())
        {
            job->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }

    if (boolReply.value() == false)
    {
        // Set autostar if full field option is false
        dBusArgs.clear();
        dBusArgs.append(true);
        dBusArgs.append(job->getOpticalTrain());
        if ((reply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "setAutoStarEnabled", dBusArgs)).type() ==
                QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setAutoFocusStar request received DBUS error: %1").arg(
                                                  job->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
            {
                job->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }
    }

    // Start auto-focus
    dBusArgs.clear();
    dBusArgs.append(job->getOpticalTrain());
    if ((reply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "start",
                 dBusArgs)).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' startFocus request received DBUS error: %2").arg(
                                              job->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
        {
            job->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }

    moduleState()->updateJobStage(SCHEDSTAGE_FOCUSING);
    appendLogText(i18n("Job '%1' is focusing.", job->getName()));
    moduleState()->startCurrentOperationTimer();
}

void SchedulerProcess::startAstrometry()
{
    Q_ASSERT_X(nullptr != activeJob(), __FUNCTION__, "Job starting aligning must be valid");

    QDBusMessage reply;
    setSolverAction(Align::GOTO_SLEW);

    // Always turn update coords on
    //QVariant arg(true);
    //alignInterface->call(QDBus::AutoDetect, "setUpdateCoords", arg);

    // Reset the solver speedup (using the last successful index file and healpix for the
    // pointing check) when re-aligning.
    moduleState()->setIndexToUse(-1);
    moduleState()->setHealpixToUse(-1);

    // If FITS file is specified, then we use load and slew
    if (activeJob()->getFITSFile().isEmpty() == false)
    {
        auto path = activeJob()->getFITSFile().toString(QUrl::PreferLocalFile);
        // check if the file exists
        if (QFile::exists(path) == false)
        {
            appendLogText(i18n("Warning: job '%1' target FITS file does not exist.", activeJob()->getName()));
            activeJob()->setState(SCHEDJOB_ERROR);
            findNextJob();
            return;
        }

        QList<QVariant> solveArgs;
        solveArgs.append(path);

        if ((reply = alignInterface()->callWithArgumentList(QDBus::AutoDetect, "loadAndSlew", solveArgs)).type() ==
                QDBusMessage::ErrorMessage)
        {
            appendLogText(i18n("Warning: job '%1' loadAndSlew request received DBUS error: %2",
                               activeJob()->getName(), reply.errorMessage()));
            if (!manageConnectionLoss())
            {
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }
        else if (reply.arguments().first().toBool() == false)
        {
            appendLogText(i18n("Warning: job '%1' loadAndSlew request failed.", activeJob()->getName()));
            activeJob()->setState(SCHEDJOB_ABORTED);
            findNextJob();
            return;
        }

        appendLogText(i18n("Job '%1' is plate solving %2.", activeJob()->getName(), activeJob()->getFITSFile().fileName()));
    }
    else
    {
        // JM 2020.08.20: Send J2000 TargetCoords to Align module so that we always resort back to the
        // target original targets even if we drifted away due to any reason like guiding calibration failures.
        const SkyPoint targetCoords = activeJob()->getTargetCoords();
        QList<QVariant> targetArgs, rotationArgs;
        targetArgs << targetCoords.ra0().Hours() << targetCoords.dec0().Degrees();
        rotationArgs << activeJob()->getPositionAngle();

        if ((reply = alignInterface()->callWithArgumentList(QDBus::AutoDetect, "setTargetCoords",
                     targetArgs)).type() == QDBusMessage::ErrorMessage)
        {
            appendLogText(i18n("Warning: job '%1' setTargetCoords request received DBUS error: %2",
                               activeJob()->getName(), reply.errorMessage()));
            if (!manageConnectionLoss())
            {
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }

        // Only send if it has valid value.
        if (activeJob()->getPositionAngle() >= -180)
        {
            if ((reply = alignInterface()->callWithArgumentList(QDBus::AutoDetect, "setTargetPositionAngle",
                         rotationArgs)).type() == QDBusMessage::ErrorMessage)
            {
                appendLogText(i18n("Warning: job '%1' setTargetPositionAngle request received DBUS error: %2").arg(
                                  activeJob()->getName(), reply.errorMessage()));
                if (!manageConnectionLoss())
                {
                    activeJob()->setState(SCHEDJOB_ERROR);
                    findNextJob();
                }
                return;
            }
        }

        if ((reply = alignInterface()->call(QDBus::AutoDetect, "captureAndSolve")).type() == QDBusMessage::ErrorMessage)
        {
            appendLogText(i18n("Warning: job '%1' captureAndSolve request received DBUS error: %2").arg(
                              activeJob()->getName(), reply.errorMessage()));
            if (!manageConnectionLoss())
            {
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }
        else if (reply.arguments().first().toBool() == false)
        {
            appendLogText(i18n("Warning: job '%1' captureAndSolve request failed.", activeJob()->getName()));
            activeJob()->setState(SCHEDJOB_ABORTED);
            findNextJob();
            return;
        }

        appendLogText(i18n("Job '%1' is capturing and plate solving.", activeJob()->getName()));
    }

    /* FIXME: not supposed to modify the job */
    moduleState()->updateJobStage(SCHEDSTAGE_ALIGNING);
    moduleState()->startCurrentOperationTimer();
}

void SchedulerProcess::startGuiding(bool resetCalibration)
{
    Q_ASSERT_X(nullptr != activeJob(), __FUNCTION__, "Job starting guiding must be valid");

    // avoid starting the guider twice
    if (resetCalibration == false && getGuidingStatus() == GUIDE_GUIDING)
    {
        moduleState()->updateJobStage(SCHEDSTAGE_GUIDING_COMPLETE);
        appendLogText(i18n("Guiding already running for %1, starting next scheduler action...", activeJob()->getName()));
        getNextAction();
        moduleState()->startCurrentOperationTimer();
        return;
    }

    // Connect Guider
    guideInterface()->call(QDBus::AutoDetect, "connectGuider");

    // Set Auto Star to true
    QVariant arg(true);
    guideInterface()->call(QDBus::AutoDetect, "setAutoStarEnabled", arg);

    // Only reset calibration on trouble
    // and if we are allowed to reset calibration (true by default)
    if (resetCalibration && Options::resetGuideCalibration())
    {
        guideInterface()->call(QDBus::AutoDetect, "clearCalibration");
    }

    guideInterface()->call(QDBus::AutoDetect, "guide");

    moduleState()->updateJobStage(SCHEDSTAGE_GUIDING);

    appendLogText(i18n("Starting guiding procedure for %1 ...", activeJob()->getName()));

    moduleState()->startCurrentOperationTimer();
}

void SchedulerProcess::stopGuiding()
{
    if (!guideInterface())
        return;

    // Tell guider to abort if the current job requires guiding - end-user may enable guiding manually before observation
    if (nullptr != activeJob() && (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE))
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' is stopping guiding...").arg(activeJob()->getName());
        guideInterface()->call(QDBus::AutoDetect, "abort");
        moduleState()->resetGuideFailureCount();
        // abort all follower jobs
        stopCapturing("", true);
    }

    // In any case, stop the automatic guider restart
    if (moduleState()->isGuidingTimerActive())
        moduleState()->cancelGuidingTimer();
}

void SchedulerProcess::processGuidingTimer()
{
    if ((moduleState()->restartGuidingInterval() > 0) &&
            (moduleState()->restartGuidingTime().msecsTo(KStarsData::Instance()->ut()) > moduleState()->restartGuidingInterval()))
    {
        moduleState()->cancelGuidingTimer();
        startGuiding(true);
    }
}

void SchedulerProcess::startCapture(bool restart)
{
    Q_ASSERT_X(nullptr != activeJob(), __FUNCTION__, "Job starting capturing must be valid");

    // ensure that guiding is running before we start capturing
    if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE && getGuidingStatus() != GUIDE_GUIDING)
    {
        // guiding should run, but it doesn't. So start guiding first
        moduleState()->updateJobStage(SCHEDSTAGE_GUIDING);
        startGuiding();
        return;
    }

    startSingleCapture(activeJob(), restart);
    for (auto follower : activeJob()->followerJobs())
    {
        // start follower jobs that scheduled or that were already capturing, but stopped
        if (follower->getState() == SCHEDJOB_SCHEDULED || (follower->getStage() == SCHEDSTAGE_CAPTURING && follower->isStopped()))
        {
            follower->setState(SCHEDJOB_BUSY);
            follower->setStage(SCHEDSTAGE_CAPTURING);
            startSingleCapture(follower, restart);
        }
    }

    moduleState()->updateJobStage(SCHEDSTAGE_CAPTURING);

    KSNotification::event(QLatin1String("EkosScheduledImagingStart"),
                          i18n("Ekos job (%1) - Capture started", activeJob()->getName()), KSNotification::Scheduler);

    if (moduleState()->captureBatch() > 0)
        appendLogText(i18n("Job '%1' capture is in progress (batch #%2)...", activeJob()->getName(),
                           moduleState()->captureBatch() + 1));
    else
        appendLogText(i18n("Job '%1' capture is in progress...", activeJob()->getName()));

    moduleState()->startCurrentOperationTimer();
}

void SchedulerProcess::startSingleCapture(SchedulerJob *job, bool restart)
{
    captureInterface()->setProperty("targetName", job->getName());

    QString url = job->getSequenceFile().toLocalFile();
    QVariant train(job->getOpticalTrain());

    // Refresh the job's captured frames map from the module state before restarting
    if (restart == true)
    {
        SchedulerUtils::estimateJobTime(job, moduleState()->capturedFramesCount(), this);
    }

    if (restart == false)
    {
        QList<QVariant> dbusargs;
        QVariant isLead(job->isLead());
        // override targets from sequence queue file
        QVariant targetName(job->getName());
        dbusargs.append(url);
        dbusargs.append(train);
        dbusargs.append(isLead);
        dbusargs.append(targetName);
        QDBusReply<bool> const captureReply = captureInterface()->callWithArgumentList(QDBus::AutoDetect,
                                              "loadSequenceQueue",
                                              dbusargs);
        if (captureReply.error().type() != QDBusError::NoError)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' loadSequenceQueue request received DBUS error: %1").arg(job->getName()).arg(
                                                  captureReply.error().message());
            if (!manageConnectionLoss())
                job->setState(SCHEDJOB_ERROR);
            return;
        }
        // Check if loading sequence fails for whatever reason
        else if (captureReply.value() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' loadSequenceQueue request failed").arg(job->getName());
            if (!manageConnectionLoss())
                job->setState(SCHEDJOB_ERROR);
            return;
        }
    }

    const CapturedFramesMap fMap = job->getCapturedFramesMap();

    for (auto &e : fMap.keys())
    {
        QList<QVariant> dbusargs;
        QDBusMessage reply;
        dbusargs.append(e);
        dbusargs.append(fMap.value(e));
        dbusargs.append(train);

        if ((reply = captureInterface()->callWithArgumentList(QDBus::Block, "setCapturedFramesMap",
                     dbusargs)).type() ==
                QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' setCapturedFramesCount request received DBUS error: %1").arg(job->getName()).arg(
                                                  reply.errorMessage());
            if (!manageConnectionLoss())
                job->setState(SCHEDJOB_ERROR);
            return;
        }
    }

    // Start capture process
    QList<QVariant> dbusargs;
    dbusargs.append(train);

    QDBusReply<QString> const startReply = captureInterface()->callWithArgumentList(QDBus::AutoDetect, "start",
                                           dbusargs);

    if (startReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                          QString("Warning: job '%1' start request received DBUS error: %1").arg(job->getName()).arg(
                                              startReply.error().message());
        if (!manageConnectionLoss())
            job->setState(SCHEDJOB_ERROR);
        return;
    }

    QString trainName = startReply.value();
    m_activeJobs[trainName] = job;
    // set the
}

void SchedulerProcess::setSolverAction(Align::GotoMode mode)
{
    QVariant gotoMode(static_cast<int>(mode));
    alignInterface()->call(QDBus::AutoDetect, "setSolverAction", gotoMode);
}

void SchedulerProcess::loadProfiles()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Loading profiles";
    QDBusReply<QStringList> profiles = ekosInterface()->call(QDBus::AutoDetect, "getProfiles");

    if (profiles.error().type() == QDBusError::NoError)
        moduleState()->updateProfiles(profiles);
}


bool SchedulerProcess::checkEkosState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    switch (moduleState()->ekosState())
    {
        case EKOS_IDLE:
        {
            if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
            {
                moduleState()->setEkosState(EKOS_READY);
                return true;
            }
            else
            {
                ekosInterface()->call(QDBus::AutoDetect, "start");
                moduleState()->setEkosState(EKOS_STARTING);
                moduleState()->startCurrentOperationTimer();

                qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos communication status is" << moduleState()->ekosCommunicationStatus() <<
                                                 "Starting Ekos...";

                return false;
            }
        }

        case EKOS_STARTING:
        {
            if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
            {
                appendLogText(i18n("Ekos started."));
                moduleState()->resetEkosConnectFailureCount();
                moduleState()->setEkosState(EKOS_READY);
                return true;
            }
            else if (moduleState()->ekosCommunicationStatus() == Ekos::Error)
            {
                if (moduleState()->increaseEkosConnectFailureCount())
                {
                    appendLogText(i18n("Starting Ekos failed. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "start");
                    return false;
                }

                appendLogText(i18n("Starting Ekos failed."));
                stop();
                return false;
            }
            else if (moduleState()->ekosCommunicationStatus() == Ekos::Idle)
                return false;
            // If a minute passed, give up
            else if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseEkosConnectFailureCount())
                {
                    appendLogText(i18n("Starting Ekos timed out. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "stop");
                    QTimer::singleShot(1000, this, [&]()
                    {
                        ekosInterface()->call(QDBus::AutoDetect, "start");
                        moduleState()->startCurrentOperationTimer();
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
            if (moduleState()->ekosCommunicationStatus() == Ekos::Idle)
            {
                appendLogText(i18n("Ekos stopped."));
                moduleState()->setEkosState(EKOS_IDLE);
                return true;
            }
        }
        break;

        case EKOS_READY:
            return true;
    }
    return false;
}

bool SchedulerProcess::checkINDIState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    switch (moduleState()->indiState())
    {
        case INDI_IDLE:
        {
            if (moduleState()->indiCommunicationStatus() == Ekos::Success)
            {
                moduleState()->setIndiState(INDI_PROPERTY_CHECK);
                moduleState()->resetIndiConnectFailureCount();
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI Properties...";
            }
            else
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Connecting INDI devices...";
                ekosInterface()->call(QDBus::AutoDetect, "connectDevices");
                moduleState()->setIndiState(INDI_CONNECTING);

                moduleState()->startCurrentOperationTimer();
            }
        }
        break;

        case INDI_CONNECTING:
        {
            if (moduleState()->indiCommunicationStatus() == Ekos::Success)
            {
                appendLogText(i18n("INDI devices connected."));
                moduleState()->setIndiState(INDI_PROPERTY_CHECK);
            }
            else if (moduleState()->indiCommunicationStatus() == Ekos::Error)
            {
                if (moduleState()->increaseIndiConnectFailureCount() <= moduleState()->maxFailureAttempts())
                {
                    appendLogText(i18n("One or more INDI devices failed to connect. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "connectDevices");
                }
                else
                {
                    appendLogText(i18n("One or more INDI devices failed to connect. Check INDI control panel for details."));
                    stop();
                }
            }
            // If 30 seconds passed, we retry
            else if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
            {
                if (moduleState()->increaseIndiConnectFailureCount() <= moduleState()->maxFailureAttempts())
                {
                    appendLogText(i18n("One or more INDI devices timed out. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "connectDevices");
                    moduleState()->startCurrentOperationTimer();
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
            if (moduleState()->indiCommunicationStatus() == Ekos::Idle)
            {
                appendLogText(i18n("INDI devices disconnected."));
                moduleState()->setIndiState(INDI_IDLE);
                return true;
            }
        }
        break;

        case INDI_PROPERTY_CHECK:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI properties.";
            // Unparking procedures are now handled by the startup queue, so these explicit checks are removed.
            // The queue manager will handle device readiness.

            // capture interface is required at all times to proceed.
            if (captureInterface().isNull())
                return false;

            if (moduleState()->captureReady() == false)
            {
                QVariant hasCoolerControl = captureInterface()->property("coolerControl");
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Cooler control" << (!hasCoolerControl.isValid() ? "invalid" :
                                               (hasCoolerControl.toBool() ? "True" : "Faklse"));
                if (hasCoolerControl.isValid())
                    moduleState()->setCaptureReady(true);
                else
                    qCWarning(KSTARS_EKOS_SCHEDULER) << "Capture module is not ready yet...";
            }

            moduleState()->setIndiState(INDI_READY);
            moduleState()->resetIndiConnectFailureCount();

            // If we were waiting for INDI during startup, transition to post-devices phase
            if (moduleState()->startupState() == STARTUP_PRE_DEVICES)
            {
                appendLogText(i18n("INDI devices ready. Proceeding to post-startup phase..."));
                moduleState()->setStartupState(STARTUP_POST_DEVICES);
            }

            return true;
        }

        case INDI_READY:
            return true;
    }

    return false;
}

bool SchedulerProcess::completeShutdown()
{
    // Clear the queue BEFORE disconnecting devices to prevent crashes
    // This releases device pointers while devices are still valid
    if (m_queueManager && moduleState()->indiState() != INDI_IDLE)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Clearing queue manager before device disconnection...";
        m_queueManager->clear();
    }

    // If INDI is not done disconnecting, try again later
    if (moduleState()->indiState() == INDI_DISCONNECTING
            && checkINDIState() == false)
        return false;

    // If we are in weather grace period, never shutdown completely
    if (moduleState()->weatherGracePeriodActive() == false)
    {
        // Disconnect INDI if required first
        if (moduleState()->indiState() != INDI_IDLE && Options::stopEkosAfterShutdown())
        {
            disconnectINDI();
            return false;
        }

        // If Ekos is not done stopping, try again later
        if (moduleState()->ekosState() == EKOS_STOPPING && checkEkosState() == false)
            return false;

        // Stop Ekos if required.
        if (moduleState()->ekosState() != EKOS_IDLE && Options::stopEkosAfterShutdown())
        {
            stopEkos();
            return false;
        }

        // Check if we need to stop Ekos/INDI or if they're already stopped
        bool ekosIndiHandled = (moduleState()->ekosState() == EKOS_IDLE && moduleState()->indiState() == INDI_IDLE) ||
                               !Options::stopEkosAfterShutdown();

        if (ekosIndiHandled)
        {
            // Only run post-shutdown queue if we came from pre-shutdown (not if we're already done)
            if (moduleState()->shutdownState() != SHUTDOWN_POST_QUEUE_RUNNING &&
                    moduleState()->shutdownState() != SHUTDOWN_COMPLETE)
            {
                // Phase 2: Execute post-shutdown tasks (after Ekos/INDI stop)
                if (Options::schedulerShutdownEnabled() && m_queueManager && !moduleState()->postShutdownScriptURL().isEmpty())
                {
                    QString queueFile = moduleState()->postShutdownScriptURL().toLocalFile();
                    if (m_queueManager->loadQueue(queueFile))
                    {
                        if (m_queueManager->count() > 0)
                        {
                            appendLogText(i18n("Executing post-shutdown tasks from %1...", queueFile));
                            moduleState()->setShutdownState(SHUTDOWN_POST_QUEUE_RUNNING);
                            m_queueExecutor->start();
                            return false;
                        }
                        else
                        {
                            // Queue loaded but is empty - consider shutdown complete
                            appendLogText(i18n("Post-shutdown queue is empty, shutdown complete."));
                            moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
                        }
                    }
                    else
                    {
                        appendLogText(i18n("Failed to load post-shutdown queue from %1", queueFile));
                        moduleState()->setShutdownState(SHUTDOWN_ERROR);
                        return false;
                    }
                }
                else
                {
                    // No post-shutdown queue configured - shutdown is complete
                    moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
                }
            }
        }
    }

    if (moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
        appendLogText(i18n("Shutdown complete."));
    else
        appendLogText(i18n("Shutdown procedure failed, aborting..."));

    // If we're in weather shutdown monitoring mode, keep scheduler running
    if (moduleState()->weatherShutdownMonitoring())
    {
        appendLogText(i18n("Entering weather monitoring mode. Waiting for weather to improve..."));
        // Reset shutdown state to prevent repeated calls to completeShutdown()
        moduleState()->setShutdownState(SHUTDOWN_IDLE);
        moduleState()->setupNextIteration(RUN_WAKEUP, Options::schedulerWeatherGracePeriod() * 60000);
        emit schedulerSleeping(false, true);
        return true;
    }

    // Stop Scheduler
    stop();

    return true;
}

void SchedulerProcess::disconnectINDI()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Disconnecting INDI...";
    moduleState()->setIndiState(INDI_DISCONNECTING);
    ekosInterface()->call(QDBus::AutoDetect, "disconnectDevices");
}

void SchedulerProcess::stopEkos()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Stopping Ekos...";
    moduleState()->setEkosState(EKOS_STOPPING);
    moduleState()->resetEkosConnectFailureCount();
    ekosInterface()->call(QDBus::AutoDetect, "stop");
    moduleState()->setMountReady(false);
    moduleState()->setCaptureReady(false);
    moduleState()->setDomeReady(false);
    moduleState()->setCapReady(false);
}

bool SchedulerProcess::manageConnectionLoss()
{
    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
        return false;

    // Don't manage loss if Ekos is actually down in the state machine
    switch (moduleState()->ekosState())
    {
        case EKOS_IDLE:
        case EKOS_STOPPING:
            return false;

        default:
            break;
    }

    // Don't manage loss if INDI is actually down in the state machine
    switch (moduleState()->indiState())
    {
        case INDI_IDLE:
        case INDI_DISCONNECTING:
            return false;

        default:
            break;
    }

    // If Ekos is assumed to be up, check its state
    //QDBusReply<int> const isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
    if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Ekos is currently connected, checking INDI before mitigating connection loss.");

        // If INDI is assumed to be up, check its state
        if (moduleState()->isINDIConnected())
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

bool SchedulerProcess::checkStartupState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Checking Startup State (%1)...").arg(moduleState()->startupState());

    switch (moduleState()->startupState())
    {
        case STARTUP_IDLE:
        {
            KSNotification::event(QLatin1String("ObservatoryStartup"), i18n("Observatory is in the startup process"),
                                  KSNotification::Scheduler);

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Startup Idle. Starting startup process...";

            if (moduleState()->currentProfile() != i18n("Default"))
            {
                QList<QVariant> profile;
                profile.append(moduleState()->currentProfile());
                ekosInterface()->callWithArgumentList(QDBus::AutoDetect, "setProfile", profile);
            }

            // Phase 1: Execute pre-startup tasks (before Ekos/INDI start)
            if (Options::schedulerStartupEnabled() && m_queueManager && !moduleState()->preStartupScriptURL().isEmpty())
            {
                QString queueFile = moduleState()->preStartupScriptURL().toLocalFile();
                if (m_queueManager->loadQueue(queueFile))
                {
                    if (m_queueManager->count() > 0)
                    {
                        appendLogText(i18n("Executing pre-startup tasks from %1...", queueFile));
                        moduleState()->setStartupState(STARTUP_PRE_DEVICES_RUNNING);
                        m_queueExecutor->start();
                        return false;
                    }
                }
                else
                {
                    appendLogText(i18n("Failed to load pre-startup queue from %1", queueFile));
                    moduleState()->setStartupState(STARTUP_ERROR);
                    return false;
                }
            }
            // No pre-startup queue or empty, proceed directly to start Ekos/INDI
            appendLogText(i18n("No pre-startup tasks, pre-startup phase complete. Starting Ekos..."));
            moduleState()->setStartupState(STARTUP_PRE_DEVICES);
            return false;
        }

        case STARTUP_PRE_DEVICES_RUNNING:
            // Wait for pre-startup queue to complete (queueExecutionCompleted signal will transition to STARTUP_PRE_DEVICES)
            return false;

        case STARTUP_PRE_DEVICES:
            // Pre-device tasks complete (or skipped), now allow Ekos/INDI startup to proceed
            // Return true to let checkEkosState() and checkINDIState() take over
            return true;

        case STARTUP_POST_DEVICES:
        {
            // Phase 2: Execute post-startup tasks (after INDI devices ready)
            if (Options::schedulerStartupEnabled() && m_queueManager && !moduleState()->postStartupScriptURL().isEmpty())
            {
                QString queueFile = moduleState()->postStartupScriptURL().toLocalFile();
                if (m_queueManager->loadQueue(queueFile))
                {
                    if (m_queueManager->count() > 0)
                    {
                        appendLogText(i18n("Executing post-startup tasks from %1...", queueFile));
                        moduleState()->setStartupState(STARTUP_POST_DEVICES_RUNNING);
                        m_queueExecutor->start();
                        return false;
                    }
                }
                else
                {
                    appendLogText(i18n("Failed to load post-startup tasks from %1", queueFile));
                    moduleState()->setStartupState(STARTUP_ERROR);
                    return false;
                }
            }
            // No post-startup queue or empty, proceed to complete
            appendLogText(i18n("No post-startup tasks, startup complete."));
            moduleState()->setStartupState(STARTUP_COMPLETE);
            return true;
        }

        case STARTUP_POST_DEVICES_RUNNING:
            // Waiting for post-startup queue execution to complete via signals
            return false;

        case STARTUP_COMPLETE:
            return true;

        case STARTUP_ERROR:
            stop();
            return true;
    }

    return false;
}

bool SchedulerProcess::checkShutdownState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Checking Shutdown State (%1)...").arg(moduleState()->shutdownState());

    switch (moduleState()->shutdownState())
    {
        case SHUTDOWN_IDLE:
        {
            KSNotification::event(QLatin1String("ObservatoryShutdown"), i18n("Observatory is in the shutdown process"),
                                  KSNotification::Scheduler);

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Shutdown Idle. Starting shutdown process...";

            // Phase 1: Execute pre-shutdown tasks (before Ekos/INDI stop)
            if (Options::schedulerShutdownEnabled() && m_queueManager && !moduleState()->preShutdownScriptURL().isEmpty())
            {
                QString queueFile = moduleState()->preShutdownScriptURL().toLocalFile();
                if (m_queueManager->loadQueue(queueFile))
                {
                    if (m_queueManager->count() > 0)
                    {
                        appendLogText(i18n("Executing pre-shutdown tasks from %1...", queueFile));
                        moduleState()->setShutdownState(SHUTDOWN_PRE_QUEUE_RUNNING);
                        m_queueExecutor->start();
                        return false;
                    }
                }
                else
                {
                    appendLogText(i18n("Failed to load pre-shutdown queue from %1", queueFile));
                    moduleState()->setShutdownState(SHUTDOWN_ERROR);
                    return false;
                }
            }
            // No pre-shutdown queue or empty, proceed to stop Ekos/INDI (handled by completeShutdown)
            appendLogText(i18n("No pre-shutdown tasks, proceeding to stop Ekos/INDI."));
            return true;
        }

        case SHUTDOWN_PRE_QUEUE_RUNNING:
            // Waiting for pre-shutdown queue execution to complete via signals
            return false;

        case SHUTDOWN_STOPPING_EKOS:
            // Pre-shutdown tasks complete, now stop Ekos/INDI and execute post-shutdown tasks
            // This is handled by completeShutdown()
            return completeShutdown();

        case SHUTDOWN_POST_QUEUE_RUNNING:
            // Waiting for post-shutdown queue execution to complete via signals
            return false;

        case SHUTDOWN_COMPLETE:
            return true;

        case SHUTDOWN_ERROR:
            stop();
            return true;
    }

    return false;
}

bool SchedulerProcess::checkParkWaitState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    // Parking/unparking is now handled by the queue manager.
    // This function now only checks for error states.
    switch (moduleState()->parkWaitState())
    {
        case PARKWAIT_IDLE:
            return true;

        case PARKWAIT_ERROR:
            appendLogText(i18n("Park/unpark procedure failed, aborting..."));
            stop();
            return true;

        default:
            // Should not reach here with the simplified enum
            return true;
    }
}

void SchedulerProcess::setPaused()
{
    moduleState()->setupNextIteration(RUN_NOTHING);
    appendLogText(i18n("Scheduler paused."));
    emit schedulerPaused();
}

void SchedulerProcess::resetJobs()
{
    // Reset ALL scheduler jobs to IDLE and force-reset their completed count - no effect when progress is kept
    for (SchedulerJob * job : moduleState()->jobs())
    {
        job->reset();
        job->setCompletedCount(0);
    }

    // Unconditionally update the capture storage
    updateCompletedJobsCount(true);
}

void SchedulerProcess::selectActiveJob(const QList<SchedulerJob *> &jobs)
{
    auto finished_or_aborted = [](SchedulerJob const * const job)
    {
        SchedulerJobStatus const s = job->getState();
        return SCHEDJOB_ERROR <= s || SCHEDJOB_ABORTED == s;
    };

    /* This predicate matches jobs that are neither scheduled to run nor aborted */
    auto neither_scheduled_nor_aborted = [](SchedulerJob const * const job)
    {
        SchedulerJobStatus const s = job->getState();
        return SCHEDJOB_SCHEDULED != s && SCHEDJOB_ABORTED != s;
    };

    /* If there are no jobs left to run in the filtered list, stop evaluation */
    ErrorHandlingStrategy strategy = static_cast<ErrorHandlingStrategy>(Options::errorHandlingStrategy());
    qCDebug(KSTARS_EKOS_SCHEDULER) << "selectActiveJob: Evaluating jobs. Total jobs:" << jobs.count();
    for (const auto& job : jobs)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "  Job:" << job->getName() << "State:" << SchedulerJob::jobStatusString(
                                           job->getState()) << "Stage:" << SchedulerJob::jobStageString(job->getStage());
    }

    if (jobs.isEmpty() || std::all_of(jobs.begin(), jobs.end(), neither_scheduled_nor_aborted))
    {
        appendLogText(i18n("No jobs left in the scheduler queue after evaluating."));
        moduleState()->setActiveJob(nullptr);
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
        "selectActiveJob: No jobs left or all are neither scheduled nor aborted. Setting active job to nullptr.";
        return;
    }
    /* If there are only aborted jobs that can run, reschedule those and let Scheduler restart one loop */
    else if (std::all_of(jobs.begin(), jobs.end(), finished_or_aborted) &&
             strategy != ERROR_DONT_RESTART)
    {
        appendLogText(i18n("Only aborted jobs left in the scheduler queue after evaluating, rescheduling those."));
        std::for_each(jobs.begin(), jobs.end(), [](SchedulerJob * job)
        {
            if (SCHEDJOB_ABORTED == job->getState())
                job->setState(SCHEDJOB_EVALUATION);
        });

        return;
    }

    // GreedyScheduler::scheduleJobs() must be called first.
    SchedulerJob *scheduledJob = getGreedyScheduler()->getScheduledJob();
    if (!scheduledJob)
    {
        appendLogText(i18n("No jobs scheduled."));
        moduleState()->setActiveJob(nullptr);
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
        "selectActiveJob: GreedyScheduler returned no scheduled job. Setting active job to nullptr.";
        return;
    }
    qCDebug(KSTARS_EKOS_SCHEDULER) << "selectActiveJob: Scheduled job:" << scheduledJob->getName() << "State:" <<
                                   SchedulerJob::jobStatusString(scheduledJob->getState()) << "Stage:" << SchedulerJob::jobStageString(
                                       scheduledJob->getStage());

    if (activeJob() != nullptr && scheduledJob != activeJob())
    {
        // Changing lead, therefore abort all follower jobs that are still running
        for (auto job : m_activeJobs.values())
            if (!job->isLead() && job->getState() == SCHEDJOB_BUSY)
                stopCapturing(job->getOpticalTrain(), false);

        // clear the mapping camera name --> scheduler job
        m_activeJobs.clear();
    }
    moduleState()->setActiveJob(scheduledJob);

}

void SchedulerProcess::startJobEvaluation()
{
    // Reset all jobs
    // other states too?
    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
        resetJobs();

    // reset the iterations counter
    moduleState()->resetSequenceExecutionCounter();

    // And evaluate all pending jobs per the conditions set in each
    evaluateJobs(true);
}

void SchedulerProcess::evaluateJobs(bool evaluateOnly)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: Starting evaluation. evaluateOnly:" << evaluateOnly;
    for (auto job : moduleState()->jobs())
        job->clearCache();

    /* Don't evaluate if list is empty */
    if (moduleState()->jobs().isEmpty())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: Job list is empty. Skipping evaluation.";
        return;
    }
    /* Start by refreshing the number of captures already present - unneeded if not remembering job progress */
    if (Options::rememberJobProgress())
        updateCompletedJobsCount();

    moduleState()->calculateDawnDusk();

    qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: Before scheduling jobs with GreedyScheduler.";
    for (const auto& job : moduleState()->jobs())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "  Job:" << job->getName() << "State:" << SchedulerJob::jobStatusString(
                                           job->getState()) << "Stage:" << SchedulerJob::jobStageString(job->getStage());
    }

    getGreedyScheduler()->scheduleJobs(moduleState()->jobs(), SchedulerModuleState::getLocalTime(),
                                       moduleState()->capturedFramesCount(), this);

    qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: After scheduling jobs with GreedyScheduler.";
    for (const auto& job : moduleState()->jobs())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "  Job:" << job->getName() << "State:" << SchedulerJob::jobStatusString(
                                           job->getState()) << "Stage:" << SchedulerJob::jobStageString(job->getStage());
    }

    // schedule or job states might have been changed, update the table

    if (!evaluateOnly && moduleState()->schedulerState() == SCHEDULER_RUNNING)
    {
        // At this step, we finished evaluating jobs.
        // We select the first job that has to be run, per schedule.
        qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: Scheduler is running and not evaluateOnly. Selecting active job.";
        selectActiveJob(moduleState()->jobs());
    }
    else
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos finished evaluating jobs, no job selection required.";
        qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: evaluateOnly is" << evaluateOnly << "or scheduler is not running.";
    }

    emit jobsUpdated(moduleState()->getJSONJobs());
    qCDebug(KSTARS_EKOS_SCHEDULER) << "evaluateJobs: Finished evaluation.";
}

bool SchedulerProcess::checkStatus()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
    {
        if (activeJob() == nullptr)
        {
            setPaused();
            return false;
        }
        switch (activeJob()->getState())
        {
            case  SCHEDJOB_BUSY:
                // do nothing
                break;
            case  SCHEDJOB_COMPLETE:
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
            return completeShutdown();
        }

        // #2.2  Check if shutdown is in progress
        if (moduleState()->shutdownState() > SHUTDOWN_IDLE)
        {
            // If Ekos is not done stopping, try again later
            if (moduleState()->ekosState() == EKOS_STOPPING && checkEkosState() == false)
                return false;

            checkShutdownState();
            return false;
        }

        // #2.3 Check if park wait procedure is in progress
        if (checkParkWaitState() == false)
            return false;

        // #2.4 If not in shutdown state, evaluate the jobs
        evaluateJobs(false);

        // If we're in weather shutdown monitoring mode, keep sleeping (weather status change will wake us)
        if (moduleState()->weatherShutdownMonitoring())
        {
            moduleState()->setupNextIteration(RUN_WAKEUP, Options::schedulerWeatherGracePeriod() * 60000);
            emit schedulerSleeping(false, true);
            moduleState()->setActiveJob(nullptr);
            return false;
        }

        // #2.5 check if all jobs have completed and repeat is set
        if (nullptr == activeJob() && moduleState()->checkRepeatSequence())
        {
            // Reset all jobs
            resetJobs();
            // Re-evaluate all jobs to check whether there is at least one that might be executed
            evaluateJobs(false);
            // if there is an executable job, restart;
            if (activeJob())
            {
                moduleState()->increaseSequenceExecutionCounter();
                appendLogText(i18n("Starting job sequence iteration #%1", moduleState()->sequenceExecutionCounter()));
                return true;
            }
        }

        // #2.6 If there is no current job after evaluation, shutdown or stop
        if (nullptr == activeJob())
        {
            // Only execute shutdown procedures if the observatory has actually been started
            if (moduleState()->startupState() == STARTUP_COMPLETE)
            {
                checkShutdownState();
            }
            else
            {
                // Observatory was never started and there are no jobs, stop the scheduler
                appendLogText(i18n("No executable jobs found. Stopping scheduler."));
                stop();
            }
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

        // #4 Check if startup procedure is complete
        if (checkStartupState() == false)
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

        // #8 Check it it already completed (should only happen starting a paused job)
        //    Find the next job in this case, otherwise execute the current one
        if (activeJob() && activeJob()->getState() == SCHEDJOB_COMPLETE)
            findNextJob();

        // N.B. We explicitly do not check for return result here because regardless of execution result
        // we do not have any pending tasks further down.
        executeJob(activeJob());
        emit updateJobTable();
    }

    return true;
}

void SchedulerProcess::getNextAction()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Get next action...";

    switch (activeJob()->getStage())
    {
        case SCHEDSTAGE_IDLE:
            if (activeJob()->getLightFramesRequired())
            {
                if (activeJob()->getStepPipeline() & SchedulerJob::USE_TRACK)
                    startSlew();
                else if (activeJob()->getStepPipeline() & SchedulerJob::USE_FOCUS && moduleState()->autofocusCompleted() == false)
                {
                    qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 3485";
                    startFocusing();
                }
                else if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
                    startAstrometry();
                else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
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
                if (activeJob()->getStepPipeline())
                    appendLogText(
                        i18n("Job '%1' is proceeding directly to capture stage because only calibration frames are pending.",
                             activeJob()->getName()));
                startCapture();
            }

            break;

        case SCHEDSTAGE_SLEW_COMPLETE:
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_FOCUS && moduleState()->autofocusCompleted() == false)
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 3514";
                startFocusing();
            }
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
                startAstrometry();
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SCHEDSTAGE_FOCUS_COMPLETE:
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN)
                startAstrometry();
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SCHEDSTAGE_ALIGN_COMPLETE:
            moduleState()->updateJobStage(SCHEDSTAGE_RESLEWING);
            break;

        case SCHEDSTAGE_RESLEWING_COMPLETE:
            // If we have in-sequence-focus in the sequence file then we perform post alignment focusing so that the focus
            // frame is ready for the capture module in-sequence-focus procedure.
            if ((activeJob()->getStepPipeline() & SchedulerJob::USE_FOCUS) && activeJob()->getInSequenceFocus())
                // Post alignment re-focusing
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 3544";
                startFocusing();
            }
            else if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SCHEDSTAGE_POSTALIGN_FOCUSING_COMPLETE:
            if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SCHEDSTAGE_GUIDING_COMPLETE:
            startCapture();
            break;

        default:
            break;
    }
}

void SchedulerProcess::iterate()
{
    const int msSleep = runSchedulerIteration();
    if (msSleep < 0)
        return;

    auto &iterationTimer = moduleState()->iterationTimer();
    auto &tickleTimer = moduleState()->tickleTimer();

    connect(&iterationTimer, &QTimer::timeout, this, &SchedulerProcess::iterate, Qt::UniqueConnection);

    // Update the scheduler's altitude graph every hour, if the scheduler will be sleeping.
    constexpr int oneHour = 1000 * 3600;
    tickleTimer.stop();
    tickleTimer.disconnect();
    if (msSleep > oneHour)
    {
        connect(&tickleTimer, &QTimer::timeout, this, [this, oneHour]()
        {
            if (moduleState() && moduleState()->currentlySleeping())
            {
                moduleState()->tickleTimer().start(oneHour);
                emit updateJobTable(nullptr);
            }
        });
        tickleTimer.setSingleShot(true);
        tickleTimer.start(oneHour);
    }
    iterationTimer.setSingleShot(true);
    iterationTimer.start(msSleep);

}

int SchedulerProcess::runSchedulerIteration()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (moduleState()->startMSecs() == 0)
        moduleState()->setStartMSecs(now);

    //    printStates(QString("\nrunScheduler Iteration %1 @ %2")
    //                .arg(moduleState()->increaseSchedulerIteration())
    //                .arg((now - moduleState()->startMSecs()) / 1000.0, 1, 'f', 3));

    SchedulerTimerState keepTimerState = moduleState()->timerState();

    // TODO: At some point we should require that timerState and timerInterval
    // be explicitly set in all iterations. Not there yet, would require too much
    // refactoring of the scheduler. When we get there, we'd execute the following here:
    // timerState = RUN_NOTHING;    // don't like this comment, it should always set a state and interval!
    // timerInterval = -1;
    moduleState()->setIterationSetup(false);
    switch (keepTimerState)
    {
        case RUN_WAKEUP:
            changeSleepLabel("", false);
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
    }
    //    printStates(QString("End iteration, sleep %1: ").arg(moduleState()->timerInterval()));
    return moduleState()->timerInterval();
}

void SchedulerProcess::checkJobStage()
{
    Q_ASSERT_X(activeJob(), __FUNCTION__, "Actual current job is required to check job stage");
    if (!activeJob())
        return;

    if (checkJobStageCounter == 0)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking job stage for" << activeJob()->getName() << "startup" <<
                                       activeJob()->getStartupCondition() << activeJob()->getStartupTime().toString() << "state" << activeJob()->getState();
        if (checkJobStageCounter++ == 30)
            checkJobStageCounter = 0;
    }

    emit syncGreedyParams();
    if (!getGreedyScheduler()->checkJob(moduleState()->leadJobs(), SchedulerModuleState::getLocalTime(), activeJob()))
    {
        activeJob()->setState(SCHEDJOB_IDLE);
        stopCurrentJobAction();
        findNextJob();
        return;
    }
    checkJobStageEpilogue();
}

void SchedulerProcess::checkJobStageEpilogue()
{
    if (!activeJob())
        return;

    // Do not process if we are in soft shutdown mode
    if (moduleState()->weatherGracePeriodActive())
        return;

    // #5 Check system status to improve robustness
    // This handles external events such as disconnections or end-user manipulating INDI panel
    if (!checkStatus())
        return;

    // #5b Check the guiding timer, and possibly restart guiding.
    processGuidingTimer();

    // #6 Check each stage is processing properly
    // FIXME: Vanishing property should trigger a call to its event callback
    if (!activeJob()) return;
    switch (activeJob()->getStage())
    {
        case SCHEDSTAGE_IDLE:
            // Job is just starting.
            emit jobStarted(activeJob()->getName());
            getNextAction();
            break;

        case SCHEDSTAGE_ALIGNING:
            // Let's make sure align module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > static_cast<int>(ALIGN_INACTIVITY_TIMEOUT))
            {
                QVariant const status = alignInterface()->property("status");
                Ekos::AlignState alignStatus = static_cast<Ekos::AlignState>(status.toInt());

                if (alignStatus == Ekos::ALIGN_IDLE)
                {
                    if (moduleState()->increaseAlignFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "Align module timed out. Restarting request...";
                        startAstrometry();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' alignment procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SCHEDJOB_ABORTED);
                        findNextJob();
                    }
                }
                else
                    moduleState()->startCurrentOperationTimer();
            }
            break;

        case SCHEDSTAGE_CAPTURING:
            // Let's make sure capture module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > static_cast<int>(CAPTURE_INACTIVITY_TIMEOUT))
            {
                QVariant const status = captureInterface()->property("status");
                Ekos::CaptureState captureStatus = static_cast<Ekos::CaptureState>(status.toInt());

                if (captureStatus == Ekos::CAPTURE_IDLE)
                {
                    if (moduleState()->increaseCaptureFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "capture module timed out. Restarting request...";
                        startCapture();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' capture procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SCHEDJOB_ABORTED);
                        findNextJob();
                    }
                }
                else moduleState()->startCurrentOperationTimer();
            }
            break;

        case SCHEDSTAGE_FOCUSING:
            // Let's make sure focus module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > static_cast<int>(FOCUS_INACTIVITY_TIMEOUT))
            {
                bool success = true;
                foreach (const QString trainname, m_activeJobs.keys())
                {
                    QList<QVariant> dbusargs;
                    dbusargs.append(trainname);
                    QDBusReply<Ekos::FocusState> statusReply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "status", dbusargs);
                    if (statusReply.error().type() != QDBusError::NoError)
                    {
                        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' status request received DBUS error: %2").arg(
                                                              m_activeJobs[trainname]->getName(), QDBusError::errorString(statusReply.error().type()));
                        success = false;
                    }
                    if (success == false && !manageConnectionLoss())
                    {
                        activeJob()->setState(SCHEDJOB_ERROR);
                        findNextJob();
                        return;
                    }
                    Ekos::FocusState focusStatus = statusReply.value();
                    if (focusStatus == Ekos::FOCUS_IDLE || focusStatus == Ekos::FOCUS_WAITING)
                    {
                        if (moduleState()->increaseFocusFailureCount(trainname))
                        {
                            qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus module timed out. Restarting request...";
                            startFocusing(m_activeJobs[trainname]);
                        }
                        else
                            success = false;
                    }
                }

                if (success == false)
                {
                    appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking aborted.", activeJob()->getName()));
                    activeJob()->setState(SCHEDJOB_ABORTED);
                    findNextJob();
                }
            }
            else moduleState()->startCurrentOperationTimer();
            break;

        case SCHEDSTAGE_GUIDING:
            // Let's make sure guide module does not become unresponsive
            if (moduleState()->getCurrentOperationMsec() > GUIDE_INACTIVITY_TIMEOUT)
            {
                GuideState guideStatus = getGuidingStatus();

                if (guideStatus == Ekos::GUIDE_IDLE || guideStatus == Ekos::GUIDE_CONNECTED || guideStatus == Ekos::GUIDE_DISCONNECTED)
                {
                    if (moduleState()->increaseGuideFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "guide module timed out. Restarting request...";
                        startGuiding();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' guiding procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SCHEDJOB_ABORTED);
                        findNextJob();
                    }
                }
                else moduleState()->startCurrentOperationTimer();
            }
            break;

        case SCHEDSTAGE_SLEWING:
        case SCHEDSTAGE_RESLEWING:
            // While slewing or re-slewing, check slew status can still be obtained
        {
            QVariant const slewStatus = mountInterface()->property("status");

            if (slewStatus.isValid())
            {
                // Send the slew status periodically to avoid the situation where the mount is already at location and does not send any event
                // FIXME: in that case, filter TRACKING events only?
                ISD::Mount::Status const status = static_cast<ISD::Mount::Status>(slewStatus.toInt());
                setMountStatus(status);
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' lost connection to the mount, attempting to reconnect.", activeJob()->getName()));
                if (!manageConnectionLoss())
                    activeJob()->setState(SCHEDJOB_ERROR);
                return;
            }
        }
        break;

        case SCHEDSTAGE_SLEW_COMPLETE:
        case SCHEDSTAGE_RESLEWING_COMPLETE:
            // When done slewing or re-slewing and we use a dome, only shift to the next action when the dome is done moving
            if (moduleState()->domeReady())
            {
                QVariant const isDomeMoving = domeInterface()->property("isMoving");

                if (!isDomeMoving.isValid())
                {
                    appendLogText(i18n("Warning: job '%1' lost connection to the dome, attempting to reconnect.", activeJob()->getName()));
                    if (!manageConnectionLoss())
                        activeJob()->setState(SCHEDJOB_ERROR);
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

void SchedulerProcess::applyConfig()
{
    moduleState()->calculateDawnDusk();

    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
    {
        evaluateJobs(true);
    }
}

bool SchedulerProcess::executeJob(SchedulerJob * job)
{
    if (job == nullptr)
        return false;

    // Don't execute the current job if it is already busy
    if (activeJob() == job && SCHEDJOB_BUSY == activeJob()->getState())
        return false;

    moduleState()->setActiveJob(job);

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
        captureInterface()->setProperty("targetName", job->getName());

    moduleState()->calculateDawnDusk();

    // Reset autofocus so that focus step is applied properly when checked
    // When the focus step is not checked, the capture module will eventually run focus periodically
    moduleState()->setAutofocusCompleted(job->getOpticalTrain(), false);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Executing Job " << activeJob()->getName();

    activeJob()->setState(SCHEDJOB_BUSY);
    emit jobsUpdated(moduleState()->getJSONJobs());

    KSNotification::event(QLatin1String("EkosSchedulerJobStart"),
                          i18n("Ekos job started (%1)", activeJob()->getName()), KSNotification::Scheduler);

    // No need to continue evaluating jobs as we already have one.
    moduleState()->setupNextIteration(RUN_JOBCHECK);
    return true;
}

bool SchedulerProcess::saveScheduler(const QUrl &fileURL)
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

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << Qt::endl;
    outstream << "<SchedulerList version='2.2'>" << Qt::endl;
    // ensure to escape special XML characters
    outstream << "<Profile>" << QString(entityXML(strdup(moduleState()->currentProfile().toStdString().c_str()))) <<
                 "</Profile>" << Qt::endl;

    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
    bool useMosaicInfo = !tiles->sequenceFile().isEmpty();

    if (useMosaicInfo)
    {
        outstream << "<Mosaic>" << Qt::endl;
        outstream << "<Target>" << tiles->targetName() << "</Target>" << Qt::endl;
        outstream << "<Group>" << tiles->group() << "</Group>" << Qt::endl;

        auto ccValue = tiles->completionCondition();
        auto ccArg = tiles->completionConditionArg();
        if (ccValue == "FinishSequence")
            outstream << "<FinishSequence/>" << Qt::endl;
        else if (ccValue == "FinishLoop")
            outstream << "<FinishLoop/>" << Qt::endl;
        else if (ccValue == "FinishRepeat")
            outstream << "<FinishRepeat>" << ccArg << "</FinishRepeat>" << Qt::endl;

        outstream << "<Sequence>" << tiles->sequenceFile() << "</Sequence>" << Qt::endl;
        outstream << "<Directory>" << tiles->outputDirectory() << "</Directory>" << Qt::endl;

        outstream << "<FocusEveryN>" << tiles->focusEveryN() << "</FocusEveryN>" << Qt::endl;
        outstream << "<AlignEveryN>" << tiles->alignEveryN() << "</AlignEveryN>" << Qt::endl;
        if (tiles->isTrackChecked())
            outstream << "<TrackChecked/>" << Qt::endl;
        if (tiles->isFocusChecked())
            outstream << "<FocusChecked/>" << Qt::endl;
        if (tiles->isAlignChecked())
            outstream << "<AlignChecked/>" << Qt::endl;
        if (tiles->isGuideChecked())
            outstream << "<GuideChecked/>" << Qt::endl;
        outstream << "<Overlap>" << cLocale.toString(tiles->overlap()) << "</Overlap>" << Qt::endl;
        outstream << "<CenterRA>" << cLocale.toString(tiles->ra0().Hours()) << "</CenterRA>" << Qt::endl;
        outstream << "<CenterDE>" << cLocale.toString(tiles->dec0().Degrees()) << "</CenterDE>" << Qt::endl;
        outstream << "<GridW>" << tiles->gridSize().width() << "</GridW>" << Qt::endl;
        outstream << "<GridH>" << tiles->gridSize().height() << "</GridH>" << Qt::endl;
        outstream << "<FOVW>" << cLocale.toString(tiles->mosaicFOV().width()) << "</FOVW>" << Qt::endl;
        outstream << "<FOVH>" << cLocale.toString(tiles->mosaicFOV().height()) << "</FOVH>" << Qt::endl;
        outstream << "<CameraFOVW>" << cLocale.toString(tiles->cameraFOV().width()) << "</CameraFOVW>" << Qt::endl;
        outstream << "<CameraFOVH>" << cLocale.toString(tiles->cameraFOV().height()) << "</CameraFOVH>" << Qt::endl;
        outstream << "</Mosaic>" << Qt::endl;
    }

    int index = 0;
    for (auto &job : moduleState()->jobs())
    {
        outstream << "<Job>" << Qt::endl;

        // ensure to escape special XML characters
        outstream << "<JobType lead='" << (job->isLead() ? "true" : "false") << "'/>" << Qt::endl;
        if (job->isLead())
        {
            outstream << "<Name>" << QString(entityXML(strdup(job->getName().toStdString().c_str()))) << "</Name>" << Qt::endl;
            outstream << "<Group>" << QString(entityXML(strdup(job->getGroup().toStdString().c_str()))) << "</Group>" << Qt::endl;
            outstream << "<Coordinates>" << Qt::endl;
            outstream << "<J2000RA>" << cLocale.toString(job->getTargetCoords().ra0().Hours()) << "</J2000RA>" << Qt::endl;
            outstream << "<J2000DE>" << cLocale.toString(job->getTargetCoords().dec0().Degrees()) << "</J2000DE>" << Qt::endl;
            outstream << "</Coordinates>" << Qt::endl;
        }

        if (! job->getOpticalTrain().isEmpty())
            outstream << "<OpticalTrain>" << QString(entityXML(strdup(job->getOpticalTrain().toStdString().c_str()))) <<
                         "</OpticalTrain>" << Qt::endl;

        if (job->isLead() && job->getFITSFile().isValid() && job->getFITSFile().isEmpty() == false)
            outstream << "<FITS>" << job->getFITSFile().toLocalFile() << "</FITS>" << Qt::endl;
        else
            outstream << "<PositionAngle>" << job->getPositionAngle() << "</PositionAngle>" << Qt::endl;

        outstream << "<Sequence>" << job->getSequenceFile().toLocalFile() << "</Sequence>" << Qt::endl;

        if (useMosaicInfo && index < tiles->tiles().size())
        {
            auto oneTile = tiles->tiles().at(index++);
            outstream << "<TileCenter>" << Qt::endl;
            outstream << "<X>" << cLocale.toString(oneTile->center.x()) << "</X>" << Qt::endl;
            outstream << "<Y>" << cLocale.toString(oneTile->center.y()) << "</Y>" << Qt::endl;
            outstream << "<Rotation>" << cLocale.toString(oneTile->rotation) << "</Rotation>" << Qt::endl;
            outstream << "</TileCenter>" << Qt::endl;
        }

        if (job->isLead())
        {
            outstream << "<StartupCondition>" << Qt::endl;
            if (job->getFileStartupCondition() == START_ASAP)
                outstream << "<Condition>ASAP</Condition>" << Qt::endl;
            else if (job->getFileStartupCondition() == START_AT)
                outstream << "<Condition value='" << job->getStartAtTime().toString(Qt::ISODate) << "'>At</Condition>"
                          << Qt::endl;
            outstream << "</StartupCondition>" << Qt::endl;

            outstream << "<Constraints>" << Qt::endl;
            if (job->hasMinAltitude())
                outstream << "<Constraint value='" << cLocale.toString(job->getMinAltitude()) << "'>MinimumAltitude</Constraint>" <<
                          Qt::endl;
            if (job->getMinMoonSeparation() > 0)
                outstream << "<Constraint value='" << cLocale.toString(job->getMinMoonSeparation()) << "'>MoonSeparation</Constraint>"
                          << Qt::endl;
            if (job->getMaxMoonAltitude() < 90)
                outstream << "<Constraint value='" << cLocale.toString(job->getMaxMoonAltitude()) << "'>MoonMaxAltitude</Constraint>"
                          << Qt::endl;
            if (job->getEnforceTwilight())
                outstream << "<Constraint>EnforceTwilight</Constraint>" << Qt::endl;
            if (job->getEnforceArtificialHorizon())
                outstream << "<Constraint>EnforceArtificialHorizon</Constraint>" << Qt::endl;
            outstream << "</Constraints>" << Qt::endl;
        }

        outstream << "<CompletionCondition>" << Qt::endl;
        if (job->getCompletionCondition() == FINISH_SEQUENCE)
            outstream << "<Condition>Sequence</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == FINISH_REPEAT)
            outstream << "<Condition value='" << cLocale.toString(job->getRepeatsRequired()) << "'>Repeat</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == FINISH_LOOP)
            outstream << "<Condition>Loop</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == FINISH_AT)
            outstream << "<Condition value='" << job->getFinishAtTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << Qt::endl;
        outstream << "</CompletionCondition>" << Qt::endl;

        if (job->isLead())
        {
            outstream << "<Steps>" << Qt::endl;
            if (job->getStepPipeline() & SchedulerJob::USE_TRACK)
                outstream << "<Step>Track</Step>" << Qt::endl;
            if (job->getStepPipeline() & SchedulerJob::USE_FOCUS)
                outstream << "<Step>Focus</Step>" << Qt::endl;
            if (job->getStepPipeline() & SchedulerJob::USE_ALIGN)
                outstream << "<Step>Align</Step>" << Qt::endl;
            if (job->getStepPipeline() & SchedulerJob::USE_GUIDE)
                outstream << "<Step>Guide</Step>" << Qt::endl;
            outstream << "</Steps>" << Qt::endl;
        }
        outstream << "</Job>" << Qt::endl;
    }

    outstream << "<SchedulerAlgorithm value='" << ALGORITHM_GREEDY << "'/>" << Qt::endl;
    outstream << "<ErrorHandlingStrategy value='" << Options::errorHandlingStrategy() << "'>" << Qt::endl;
    if (Options::rescheduleErrors())
        outstream << "<RescheduleErrors />" << Qt::endl;
    outstream << "<delay>" << Options::errorHandlingStrategyDelay() << "</delay>" << Qt::endl;
    outstream << "</ErrorHandlingStrategy>" << Qt::endl;

    outstream << "<StartupProcedure enabled='" << (Options::schedulerStartupEnabled() ? "true" : "false") << "'>" << Qt::endl;
    if (moduleState()->preStartupScriptURL().isEmpty() == false)
        outstream << "<PreStartupQueue>" << moduleState()->preStartupScriptURL().toString(QUrl::PreferLocalFile) <<
                     "</PreStartupQueue>" << Qt::endl;
    if (moduleState()->postStartupScriptURL().isEmpty() == false)
        outstream << "<PostStartupQueue>" << moduleState()->postStartupScriptURL().toString(QUrl::PreferLocalFile) <<
                     "</PostStartupQueue>" << Qt::endl;
    outstream << "</StartupProcedure>" << Qt::endl;

    outstream << "<ShutdownProcedure enabled='" << (Options::schedulerShutdownEnabled() ? "true" : "false") << "'>" << Qt::endl;
    if (moduleState()->preShutdownScriptURL().isEmpty() == false)
        outstream << "<PreShutdownQueue>" << moduleState()->preShutdownScriptURL().toString(QUrl::PreferLocalFile) <<
                     "</PreShutdownQueue>" << Qt::endl;
    if (moduleState()->postShutdownScriptURL().isEmpty() == false)
        outstream << "<PostShutdownQueue>" << moduleState()->postShutdownScriptURL().toString(QUrl::PreferLocalFile) <<
                     "</PostShutdownQueue>" << Qt::endl;
    outstream << "</ShutdownProcedure>" << Qt::endl;

    outstream << "</SchedulerList>" << Qt::endl;

    appendLogText(i18n("Scheduler list saved to %1", fileURL.toLocalFile()));
    file.close();
    moduleState()->setDirty(false);
    return true;
}

void SchedulerProcess::checkAlignment(const QVariantMap &metadata, const QString &trainname)
{
    // check if the metadata comes from the lead job
    if (activeJob() == nullptr || (activeJob()->getOpticalTrain() != "" &&  activeJob()->getOpticalTrain() != trainname))
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Ignoring metadata from train =" << trainname << "for alignment check.";
        return;
    }

    // If the scheduler is idle or shutting down, or if the active job no longer requires light frames,
    // do not proceed with alignment check.
    if (moduleState()->schedulerState() == SCHEDULER_IDLE ||
            moduleState()->schedulerState() == SCHEDULER_SHUTDOWN ||
            !activeJob()->getLightFramesRequired())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job" << activeJob()->getName() <<
                                          "is complete or scheduler is stopping, skipping alignment check.";
        return;
    }

    if (activeJob()->getStepPipeline() & SchedulerJob::USE_ALIGN &&
            metadata["type"].toInt() == FRAME_LIGHT &&
            Options::alignCheckFrequency() > 0 &&
            moduleState()->increaseSolverIteration() >= Options::alignCheckFrequency())
    {
        moduleState()->resetSolverIteration();

        auto filename = metadata["filename"].toString();
        auto exposure = metadata["exposure"].toDouble();

        qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking alignment on train =" << trainname << "for" << filename;

        constexpr double minSolverSeconds = 5.0;
        double solverTimeout = std::max(exposure - 2, minSolverSeconds);
        if (solverTimeout >= minSolverSeconds)
        {
            auto profiles = getDefaultAlignOptionsProfiles();

            SSolver::Parameters parameters;
            // Get solver parameters
            // In case of exception, use first profile
            try
            {
                parameters = profiles.at(Options::solveOptionsProfile());
            }
            catch (std::out_of_range const &)
            {
                parameters = profiles[0];
            }

            // Double search radius
            parameters.search_radius = parameters.search_radius * 2;
            m_Solver.reset(new SolverUtils(parameters, solverTimeout),  &QObject::deleteLater);
            connect(m_Solver.get(), &SolverUtils::done, this, &Ekos::SchedulerProcess::solverDone, Qt::UniqueConnection);
            //connect(m_Solver.get(), &SolverUtils::newLog, this, &Ekos::Scheduler::appendLogText, Qt::UniqueConnection);

            auto width = metadata["width"].toUInt() / (metadata["binx"].isValid() ? metadata["binx"].toUInt() : 1);
            auto height = metadata["height"].toUInt() / (metadata["biny"].isValid() ? metadata["biny"].toUInt() : 1);

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

void SchedulerProcess::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &Ekos::SchedulerProcess::solverDone);

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
            startAstrometry();
        }
    }
}

void SchedulerProcess::queueExecutionCompleted()
{
    if (moduleState()->startupState() == STARTUP_PRE_DEVICES_RUNNING)
    {
        appendLogText(i18n("Pre-startup queue completed successfully. Starting Ekos..."));
        // Pre-startup tasks done, transition to state that will start Ekos/INDI
        moduleState()->setStartupState(STARTUP_PRE_DEVICES);
        return;
    }

    if (moduleState()->startupState() == STARTUP_POST_DEVICES_RUNNING)
    {
        appendLogText(i18n("Post-startup queue completed successfully."));
        moduleState()->setStartupState(STARTUP_COMPLETE);

        // If we just completed post-startup queue after weather recovery (not during normal startup),
        // we need to explicitly resume scheduler execution since we returned early from wakeUpScheduler()
        if (moduleState()->schedulerState() == SCHEDULER_IDLE || moduleState()->schedulerState() == SCHEDULER_PAUSED)
        {
            appendLogText(i18n("Resuming scheduler after safety recovery startup tasks..."));
            execute();
        }
        return;
    }

    if (moduleState()->shutdownState() == SHUTDOWN_PRE_QUEUE_RUNNING)
    {
        appendLogText(i18n("Pre-shutdown queue completed successfully."));
        // In case we have shutdown due to weather event
        // Mark shutdown as complete since it is only a soft-shutdown
        if (moduleState()->weatherGracePeriodActive())
        {
            moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
            // Because this is only a soft shutdown
            // Reset startup stage to POST_DEVICES we do not need to run
            // any pre-INDI steps.
            moduleState()->setStartupState(STARTUP_POST_DEVICES);
        }
        else
            // Pre-shutdown complete, now proceed to stop Ekos/INDI
            moduleState()->setShutdownState(SHUTDOWN_STOPPING_EKOS);
        return;
    }

    if (moduleState()->shutdownState() == SHUTDOWN_POST_QUEUE_RUNNING)
    {
        appendLogText(i18n("Post-shutdown queue completed successfully."));
        moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
        return;
    }
}

void SchedulerProcess::queueItemFailed(QueueItem *item, const QString &error)
{
    Q_UNUSED(item);

    if (moduleState()->startupState() == STARTUP_PRE_DEVICES_RUNNING
            || moduleState()->startupState() == STARTUP_POST_DEVICES_RUNNING)
    {
        appendLogText(i18n("Startup queue failed: %1", error));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    if (moduleState()->shutdownState() == SHUTDOWN_PRE_QUEUE_RUNNING)
    {
        appendLogText(i18n("Pre-shutdown queue failed: %1", error));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    if (moduleState()->shutdownState() == SHUTDOWN_POST_QUEUE_RUNNING)
    {
        appendLogText(i18n("Post-shutdown queue failed: %1", error));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }
}

bool SchedulerProcess::appendEkosScheduleList(const QString &fileURL)
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

    // remember previous job
    SchedulerJob *lastLead = nullptr;

    // retrieve optical trains names to ensure that only known trains are used
    const QStringList allTrainNames = OpticalTrainManager::Instance()->getTrainNames();
    QStringList remainingTrainNames = allTrainNames;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *tag = tagXMLEle(ep);
                if (!strcmp(tag, "Job"))
                {
                    SchedulerJob *newJob = SchedulerUtils::createJob(ep, lastLead);
                    // remember new lead if such one has been created
                    if (newJob->isLead())
                    {
                        lastLead = newJob;
                        // reset usable train names
                        remainingTrainNames = allTrainNames;
                    }
                    // check train name
                    const QString trainname = newJob->getOpticalTrain();
                    bool allowedName = (newJob->isLead() && trainname.isEmpty()) || allTrainNames.contains(trainname);
                    bool availableName = (newJob->isLead() && trainname.isEmpty()) || !remainingTrainNames.isEmpty();

                    if (!allowedName && availableName)
                    {
                        const QString message = trainname.isEmpty() ?
                                                i18n("Warning: train name is empty, selecting \"%1\".", remainingTrainNames.first()) :
                                                i18n("Warning: train name %2 does not exist, selecting \"%1\".", remainingTrainNames.first(), trainname);
                        appendLogText(message);
                        if(KMessageBox::warningContinueCancel(nullptr, message, i18n("Select optical train"), KStandardGuiItem::cont(),
                                                              KStandardGuiItem::cancel(), "correct_missing_train_warning") != KMessageBox::Continue)
                            break;

                        newJob->setOpticalTrain(remainingTrainNames.first());
                        remainingTrainNames.removeFirst();
                    }
                    else if (!availableName)
                    {
                        const QString message = i18n("Warning: no available train name for scheduler job, select the optical train name manually.");
                        appendLogText(message);

                        if(KMessageBox::warningContinueCancel(nullptr, message, i18n("Select optical train"), KStandardGuiItem::cont(),
                                                              KStandardGuiItem::cancel(), "correct_missing_train_warning") != KMessageBox::Continue)
                            break;
                    }

                    emit addJob(newJob);
                }
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
                // disabled, there is only one algorithm
                else if (!strcmp(tag, "SchedulerAlgorithm"))
                {
                    int algIndex = cLocale.toInt(findXMLAttValu(ep, "value"));
                    if (algIndex != ALGORITHM_GREEDY)
                        appendLogText(i18n("Warning: The Classic scheduler algorithm has been retired. Switching you to the Greedy algorithm."));
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
                    // Read the enabled attribute
                    const char *enabledStr = findXMLAttValu(ep, "enabled");
                    if (enabledStr[0] != '\0')
                        Options::setSchedulerStartupEnabled(!strcmp(enabledStr, "true"));

                    XMLEle *procedure;
                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = tagXMLEle(procedure);

                        if (!strcmp(proc, "PreStartupQueue"))
                        {
                            moduleState()->setPreStartupScriptURL(QUrl::fromUserInput(pcdataXMLEle(procedure)));
                        }
                        else if (!strcmp(proc, "PostStartupQueue"))
                        {
                            moduleState()->setPostStartupScriptURL(QUrl::fromUserInput(pcdataXMLEle(procedure)));
                        }
                    }
                }
                else if (!strcmp(tag, "ShutdownProcedure"))
                {
                    // Read the enabled attribute
                    const char *enabledStr = findXMLAttValu(ep, "enabled");
                    if (enabledStr[0] != '\0')
                        Options::setSchedulerShutdownEnabled(!strcmp(enabledStr, "true"));

                    XMLEle *procedure;

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = tagXMLEle(procedure);

                        if (!strcmp(proc, "PreShutdownQueue"))
                        {
                            moduleState()->setPreShutdownScriptURL(QUrl::fromUserInput(pcdataXMLEle(procedure)));
                        }
                        else if (!strcmp(proc, "PostShutdownQueue"))
                        {
                            moduleState()->setPostShutdownScriptURL(QUrl::fromUserInput(pcdataXMLEle(procedure)));
                        }
                    }
                }
            }
            delXMLEle(root);
            emit syncGUIToGeneralSettings();
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            moduleState()->setSchedulerState(old_state);
            return false;
        }
    }

    moduleState()->setDirty(false);
    delLilXML(xmlParser);
    emit updateSchedulerURL(fileURL);

    moduleState()->setSchedulerState(old_state);
    return true;
}

void SchedulerProcess::appendLogText(const QString &logentry)
{
    if (logentry.isEmpty())
        return;

    /* FIXME: user settings for log length */
    int const max_log_count = 2000;
    if (moduleState()->logText().size() > max_log_count)
        moduleState()->logText().removeLast();

    moduleState()->logText().prepend(i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                                           SchedulerModuleState::getLocalTime().toString("yyyy-MM-ddThh:mm:ss"), logentry));

    qCInfo(KSTARS_EKOS_SCHEDULER) << logentry;

    emit newLog(logentry);
}

void SchedulerProcess::clearLog()
{
    moduleState()->logText().clear();
    emit newLog(QString());
}

void SchedulerProcess::setAlignStatus(AlignState status)
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED || activeJob() == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Align State" << Ekos::getAlignStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SCHEDJOB_SCHEDULED == activeJob()->getState())
    {
        QDateTime const now = SchedulerModuleState::getLocalTime();
        if (now < activeJob()->getStartupTime())
            return;
    }

    if (activeJob()->getStage() == SCHEDSTAGE_ALIGNING)
    {
        // Is solver complete?
        if (status == Ekos::ALIGN_COMPLETE)
        {
            appendLogText(i18n("Job '%1' alignment is complete.", activeJob()->getName()));
            moduleState()->resetAlignFailureCount();

            moduleState()->updateJobStage(SCHEDSTAGE_ALIGN_COMPLETE);

            // If we solved a FITS file, let's use its center coords as our target.
            if (activeJob()->getFITSFile().isEmpty() == false)
            {
                QDBusReply<QList<double >> solutionReply = alignInterface()->call("getTargetCoords");
                if (solutionReply.isValid())
                {
                    QList<double> const values = solutionReply.value();
                    activeJob()->setTargetCoords(dms(values[0] * 15.0), dms(values[1]), KStarsData::Instance()->ut().djd());
                }
            }
            getNextAction();
        }
        else if (status == Ekos::ALIGN_FAILED || status == Ekos::ALIGN_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' alignment failed.", activeJob()->getName()));

            if (moduleState()->increaseAlignFailureCount())
            {
                if (Options::resetMountModelOnAlignFail() && moduleState()->maxFailureAttempts() - 1 < moduleState()->alignFailureCount())
                {
                    appendLogText(i18n("Warning: job '%1' forcing mount model reset after failing alignment #%2.", activeJob()->getName(),
                                       moduleState()->alignFailureCount()));
                    mountInterface()->call(QDBus::AutoDetect, "resetModel");
                }
                appendLogText(i18n("Restarting %1 alignment procedure...", activeJob()->getName()));
                startAstrometry();
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' alignment procedure failed, marking aborted.", activeJob()->getName()));
                activeJob()->setState(SCHEDJOB_ABORTED);

                findNextJob();
            }
        }
    }
}

void SchedulerProcess::setGuideStatus(GuideState status)
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED || activeJob() == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Guide State" << Ekos::getGuideStatusString(status);

    /* If current job is scheduled and has not started yet, wait */
    if (SCHEDJOB_SCHEDULED == activeJob()->getState())
    {
        QDateTime const now = SchedulerModuleState::getLocalTime();
        if (now < activeJob()->getStartupTime())
            return;
    }

    if (activeJob()->getStage() == SCHEDSTAGE_GUIDING)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Calibration & Guide stage...";

        // If calibration stage complete?
        if (status == Ekos::GUIDE_GUIDING)
        {
            appendLogText(i18n("Job '%1' guiding is in progress.", activeJob()->getName()));
            moduleState()->resetGuideFailureCount();
            // if guiding recovered while we are waiting, abort the restart
            moduleState()->cancelGuidingTimer();

            moduleState()->updateJobStage(SCHEDSTAGE_GUIDING_COMPLETE);
            getNextAction();
        }
        else if (status == Ekos::GUIDE_CALIBRATION_ERROR ||
                 status == Ekos::GUIDE_ABORTED)
        {
            if (status == Ekos::GUIDE_ABORTED)
                appendLogText(i18n("Warning: job '%1' guiding failed.", activeJob()->getName()));
            else
                appendLogText(i18n("Warning: job '%1' calibration failed.", activeJob()->getName()));

            // if the timer for restarting the guiding is already running, we do nothing and
            // wait for the action triggered by the timer. This way we avoid that a small guiding problem
            // abort the scheduler job

            if (moduleState()->isGuidingTimerActive())
                return;

            if (moduleState()->increaseGuideFailureCount())
            {
                if (status == Ekos::GUIDE_CALIBRATION_ERROR &&
                        Options::realignAfterCalibrationFailure())
                {
                    appendLogText(i18n("Restarting %1 alignment procedure...", activeJob()->getName()));
                    startAstrometry();
                }
                else
                {
                    appendLogText(i18n("Job '%1' is guiding, guiding procedure will be restarted in %2 seconds.", activeJob()->getName(),
                                       (RESTART_GUIDING_DELAY_MS * moduleState()->guideFailureCount()) / 1000));
                    moduleState()->startGuidingTimer(RESTART_GUIDING_DELAY_MS * moduleState()->guideFailureCount());
                }
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' guiding procedure failed, marking aborted.", activeJob()->getName()));
                activeJob()->setState(SCHEDJOB_ABORTED);

                findNextJob();
            }
        }
    }
}

void SchedulerProcess::setCaptureStatus(CaptureState status, const QString &trainname)
{
    if (activeJob() == nullptr || !m_activeJobs.contains(trainname))
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Capture State" << Ekos::getCaptureStatusString(status) << "train =" << trainname;

    SchedulerJob *job = m_activeJobs[trainname];

    /* If current job is scheduled and has not started yet, wait */
    if (SCHEDJOB_SCHEDULED == job->getState())
    {
        QDateTime const now = SchedulerModuleState::getLocalTime();
        if (now < job->getStartupTime())
            return;
    }

    if (job->getStage() == SCHEDSTAGE_CAPTURING)
    {
        if (status == Ekos::CAPTURE_PROGRESS && (job->getStepPipeline() & SchedulerJob::USE_ALIGN))
        {
            // alignment is only relevant for the lead job
            if (job->isLead())
            {
                // JM 2021.09.20
                // Re-set target coords in align module
                // When capture starts, alignment module automatically rests target coords to mount coords.
                // However, we want to keep align module target synced with the scheduler target and not
                // the mount coord
                const SkyPoint targetCoords = activeJob()->getTargetCoords();
                QList<QVariant> targetArgs;
                targetArgs << targetCoords.ra0().Hours() << targetCoords.dec0().Degrees();
                alignInterface()->callWithArgumentList(QDBus::AutoDetect, "setTargetCoords", targetArgs);
            }
        }
        else if (status == Ekos::CAPTURE_ABORTED)
        {
            appendLogText(i18n("[%2] Warning: job '%1' failed to capture target.", job->getName(), trainname));

            if (job->isLead())
            {
                // if capturing on the lead has failed for less than MAX_FAILURE_ATTEMPTS times
                if (moduleState()->increaseCaptureFailureCount())
                {
                    job->setState(SCHEDJOB_ABORTED);

                    // If capture failed due to guiding error, let's try to restart that
                    if (activeJob()->getStepPipeline() & SchedulerJob::USE_GUIDE)
                    {
                        // Check if it is guiding related.
                        Ekos::GuideState gStatus = getGuidingStatus();
                        if (gStatus == Ekos::GUIDE_ABORTED ||
                                gStatus == Ekos::GUIDE_CALIBRATION_ERROR ||
                                gStatus == GUIDE_DITHERING_ERROR)
                        {
                            appendLogText(i18n("[%2] Job '%1' is capturing, is restarting its guiding procedure (attempt #%3 of %4).",
                                               activeJob()->getName(), trainname,
                                               moduleState()->captureFailureCount(), moduleState()->maxFailureAttempts()));
                            startGuiding(true);
                            return;
                        }
                    }

                    /* FIXME: it's not clear whether it is actually possible to continue capturing when capture fails this way */
                    appendLogText(i18n("Warning: job '%1' failed its capture procedure, restarting capture.", activeJob()->getName()));
                    startCapture(true);
                }
                else
                {
                    /* FIXME: it's not clear whether this situation can be recovered at all */
                    appendLogText(i18n("[%2] Warning: job '%1' failed its capture procedure, marking aborted.", job->getName(), trainname));
                    activeJob()->setState(SCHEDJOB_ABORTED);
                    // abort follower capture jobs as well
                    stopCapturing("", true);

                    findNextJob();
                }
            }
            else
            {
                if (job->leadJob()->getStage() == SCHEDSTAGE_CAPTURING)
                {
                    // recover only when the lead job is capturing.
                    appendLogText(i18n("[%2] Follower job '%1' has been aborted, is restarting.", job->getName(), trainname));
                    job->setState(SCHEDJOB_ABORTED);
                    startSingleCapture(job, true);
                }
                else
                {
                    appendLogText(i18n("[%2] Follower job '%1' has been aborted.", job->getName(), trainname));
                    job->setState(SCHEDJOB_ABORTED);
                }
            }
        }
        else if (status == Ekos::CAPTURE_COMPLETE)
        {
            KSNotification::event(QLatin1String("EkosScheduledImagingFinished"),
                                  i18n("[%2] Job (%1) - Capture finished", job->getName(), trainname), KSNotification::Scheduler);

            if (job->isLead())
            {
                activeJob()->setState(SCHEDJOB_COMPLETE);
                findNextJob();
            }
            else
            {
                // Re-evaluate all jobs, without selecting a new job
                evaluateJobs(true);

                if (job->getCompletionCondition() == FINISH_LOOP ||
                        (job->getCompletionCondition() == FINISH_REPEAT && job->getRepeatsRemaining() > 0))
                {
                    job->setState(SCHEDJOB_BUSY);
                    startSingleCapture(job, false);
                }
                else
                {
                    // follower job is complete
                    job->setState(SCHEDJOB_COMPLETE);
                    job->setStage(SCHEDSTAGE_COMPLETE);
                }
            }
        }
        else if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
        {
            // We received a new image, but we don't know precisely where so update the storage map and re-estimate job times.
            // FIXME: rework this once capture storage is reworked
            if (Options::rememberJobProgress())
            {
                updateCompletedJobsCount(true);

                for (const auto &job : moduleState()->jobs())
                    SchedulerUtils::estimateJobTime(job, moduleState()->capturedFramesCount(), this);
            }
            // Else if we don't remember the progress on jobs, increase the completed count for the current job only - no cross-checks
            else
                activeJob()->setCompletedCount(job->getCompletedCount() + 1);

            // reset the failure counter only if the image comes from the lead job
            if (job->isLead())
                moduleState()->resetCaptureFailureCount();
        }
    }
}

void SchedulerProcess::setFocusStatus(FocusState status, const QString &trainname)
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED || activeJob() == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Train " << trainname << "focus state" << Ekos::getFocusStatusString(status);

    // ensure that there is an active job with the given train name
    if (m_activeJobs.contains(trainname) == false)
        return;

    SchedulerJob *currentJob = m_activeJobs[trainname];

    /* If current job is scheduled and has not started yet, wait */
    if (SCHEDJOB_SCHEDULED == activeJob()->getState())
    {
        QDateTime const now = SchedulerModuleState::getLocalTime();
        if (now < activeJob()->getStartupTime())
            return;
    }

    if (activeJob()->getStage() == SCHEDSTAGE_FOCUSING)
    {
        // Is focus complete?
        if (status == Ekos::FOCUS_COMPLETE)
        {
            appendLogText(i18n("Job '%1' focusing train '%2' is complete.", currentJob->getName(), trainname));

            moduleState()->setAutofocusCompleted(trainname, true);

            if (moduleState()->autofocusCompleted())
            {
                moduleState()->updateJobStage(SCHEDSTAGE_FOCUS_COMPLETE);
                getNextAction();
            }
        }
        else if (status == Ekos::FOCUS_FAILED || status == Ekos::FOCUS_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' focusing failed.", currentJob->getName()));

            if (moduleState()->increaseFocusFailureCount(trainname))
            {
                appendLogText(i18n("Job '%1' for train '%2' is restarting its focusing procedure.", currentJob->getName(), trainname));
                startFocusing(currentJob);
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' on train '%2' focusing procedure failed, marking aborted.", activeJob()->getName(),
                                   trainname));
                activeJob()->setState(SCHEDJOB_ABORTED);

                findNextJob();
            }
        }
    }
}

void SchedulerProcess::setMountStatus(ISD::Mount::Status status)
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED || activeJob() == nullptr)
        return;

    // avoid log flooding
    if (m_lastMountStatus != status)
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount State changed to" << ISD::Mount::getMountStatusString(status);

    m_lastMountStatus = status;

    /* If current job is scheduled and has not started yet, wait */
    if (SCHEDJOB_SCHEDULED == activeJob()->getState())
        if (static_cast<QDateTime const>(SchedulerModuleState::getLocalTime()) < activeJob()->getStartupTime())
            return;

    switch (activeJob()->getStage())
    {
        case SCHEDSTAGE_SLEWING:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Slewing stage...";
            if (status == ISD::Mount::MOUNT_TRACKING)
            {
                appendLogText(i18n("Job '%1' slew is complete.", activeJob()->getName()));
                moduleState()->updateJobStage(SCHEDSTAGE_SLEW_COMPLETE);
                /* getNextAction is deferred to checkJobStage for dome support */
            }
            else if (status == ISD::Mount::MOUNT_ERROR)
            {
                appendLogText(i18n("Warning: job '%1' slew failed, marking terminated due to errors.", activeJob()->getName()));
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            else if (status == ISD::Mount::MOUNT_IDLE)
            {
                appendLogText(i18n("Warning: job '%1' found not slewing, restarting.", activeJob()->getName()));
                moduleState()->updateJobStage(SCHEDSTAGE_IDLE);
                getNextAction();
            }
        }
        break;

        case SCHEDSTAGE_RESLEWING:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Re-slewing stage...";

            if (status == ISD::Mount::MOUNT_TRACKING)
            {
                appendLogText(i18n("Job '%1' repositioning is complete.", activeJob()->getName()));
                moduleState()->updateJobStage(SCHEDSTAGE_RESLEWING_COMPLETE);
                /* getNextAction is deferred to checkJobStage for dome support */
            }
            else if (status == ISD::Mount::MOUNT_ERROR)
            {
                appendLogText(i18n("Warning: job '%1' repositioning failed, marking terminated due to errors.", activeJob()->getName()));
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            else if (status == ISD::Mount::MOUNT_IDLE)
            {
                appendLogText(i18n("Warning: job '%1' found not repositioning, restarting.", activeJob()->getName()));
                moduleState()->updateJobStage(SCHEDSTAGE_IDLE);
                getNextAction();
            }
        }
        break;

        // In case we are either focusing, aligning, or guiding
        // and mount is parked, we need to abort.
        case SCHEDSTAGE_FOCUSING:
        case SCHEDSTAGE_ALIGNING:
        case SCHEDSTAGE_GUIDING:
            if (status == ISD::Mount::MOUNT_PARKED)
            {
                appendLogText(i18n("Warning: Mount is parked while scheduler for job '%1' is active. Aborting.", activeJob()->getName()));
                stop();
            }
            break;

        // For capturing, it's more complicated because a mount can be parked by a calibration job.
        // so we only abort if light frames are required AND no calibration park mount is required
        case SCHEDSTAGE_CAPTURING:
            if (status == ISD::Mount::MOUNT_PARKED && activeJob() && activeJob()->getLightFramesRequired()
                    && activeJob()->getCalibrationMountPark() == false)
            {
                appendLogText(i18n("Warning: Mount is parked while scheduler for job '%1' is active. Aborting.", activeJob()->getName()));
                stop();
            }
            break;

        default:
            break;
    }
}

void SchedulerProcess::setSafetyStatus(IPState status)
{
    // Map IPState to ISD::Weather::Status for compatibility
    ISD::Weather::Status weatherStatus;
    switch (status)
    {
        case IPS_IDLE:
            weatherStatus = ISD::Weather::WEATHER_IDLE;
            break;
        case IPS_OK:
            weatherStatus = ISD::Weather::WEATHER_OK;
            break;
        case IPS_BUSY:
            weatherStatus = ISD::Weather::WEATHER_WARNING;
            break;
        case IPS_ALERT:
            weatherStatus = ISD::Weather::WEATHER_ALERT;
            break;
        default:
            weatherStatus = ISD::Weather::WEATHER_IDLE;
            break;
    }

    setWeatherStatus(weatherStatus, true);
}

void SchedulerProcess::setWeatherStatus(ISD::Weather::Status status, bool fromStandaloneSafetyMonitor)
{
    ISD::Weather::Status newStatus = status;

    // If we have a standalone safety monitor connected and this update is from the observatory interface,
    // we should only accept it if it's worse than the current status or ignore it completely
    if (m_StandaloneSafetyMonitor != nullptr && !fromStandaloneSafetyMonitor)
    {
        // Standalone safety monitor takes precedence - ignore observatory interface weather updates
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
        "Ignoring observatory interface weather status update because standalone safety monitor is active. "
                                       << "Observatory status:" << status << "Current status:" << moduleState()->weatherStatus();
        return;
    }

    if (newStatus == moduleState()->weatherStatus())
        return;

    ISD::Weather::Status oldStatus = moduleState()->weatherStatus();
    moduleState()->setWeatherStatus(newStatus);

    // Stop shutdown timer in case weather improves.
    if (m_WeatherShutdownTimer.isActive() && newStatus != ISD::Weather::WEATHER_ALERT)
        m_WeatherShutdownTimer.stop();

    // If we're in a preemptive shutdown due to weather and weather improves, wake up
    if ( (moduleState()->preemptiveShutdown() || moduleState()->weatherShutdownMonitoring()) &&
            oldStatus != ISD::Weather::WEATHER_OK &&
            newStatus == ISD::Weather::WEATHER_OK)
    {
        appendLogText(i18n("Safety has improved. Resuming operations."));
        moduleState()->setWeatherGracePeriodActive(false);
        moduleState()->setWeatherShutdownMonitoring(false);
        moduleState()->setupNextIteration(RUN_WAKEUP, 10);
    }
    // Check if the weather enforcement is on and weather is critical
    else if (activeJob() && Options::schedulerWeather() && (newStatus == ISD::Weather::WEATHER_ALERT &&
             moduleState()->schedulerState() != Ekos::SCHEDULER_IDLE &&
             moduleState()->schedulerState() != Ekos::SCHEDULER_SHUTDOWN))
    {
        m_WeatherShutdownTimer.start(Options::schedulerWeatherShutdownDelay() * 1000);
        appendLogText(i18n("Safety alert detected. Starting soft shutdown procedure in %1 seconds.",
                           Options::schedulerWeatherShutdownDelay()));
    }

    // forward weather state
    emit newWeatherStatus(status);
}

void SchedulerProcess::startShutdownDueToWeather()
{
    if (activeJob() && Options::schedulerWeather() && (moduleState()->weatherStatus() == ISD::Weather::WEATHER_ALERT &&
            moduleState()->schedulerState() != Ekos::SCHEDULER_IDLE &&
            moduleState()->schedulerState() != Ekos::SCHEDULER_SHUTDOWN))
    {
        // Abort current job but keep it in the queue
        if (activeJob())
        {
            stopCurrentJobAction();
            emit updateJobTable();
        }

        // Park mount, dome, etc. but don't exit completely
        // Set up preemptive shutdown with the grace period window to wait for weather to improve
        QDateTime wakeupTime = SchedulerModuleState::getLocalTime().addSecs(Options::schedulerWeatherGracePeriod() * 60);
        moduleState()->setWeatherGracePeriodActive(true);
        moduleState()->enablePreemptiveShutdown(wakeupTime);

        appendLogText(i18n("Observatory scheduled for soft shutdown until safety improves or until %1.",
                           wakeupTime.toString()));

        // Schedule wakeup after grace period expires
        int const gracePeriodSeconds = Options::schedulerWeatherGracePeriod() * 60;
        moduleState()->setupNextIteration(RUN_WAKEUP,
                                          std::lround(((gracePeriodSeconds + 1) * 1000) / KStarsData::Instance()->clock()->scale()));

        // Initiate shutdown procedure
        emit schedulerSleeping(true, true);
        moduleState()->setShutdownState(SHUTDOWN_IDLE);
        checkShutdownState();
    }
}

void SchedulerProcess::checkStartupProcedure()
{
    if (checkStartupState() == false)
        QTimer::singleShot(1000, this, &SchedulerProcess::checkStartupProcedure);
}

void SchedulerProcess::checkShutdownProcedure()
{
    if (checkShutdownState())
    {
        // shutdown completed
        if (moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
        {
            appendLogText(i18n("Manual shutdown procedure completed successfully."));
            // Stop Ekos
            if (Options::stopEkosAfterShutdown())
                stopEkos();
        }
        else if (moduleState()->shutdownState() == SHUTDOWN_ERROR)
            appendLogText(i18n("Manual shutdown procedure terminated due to errors."));

        moduleState()->setShutdownState(SHUTDOWN_IDLE);
    }
    else
        // If shutdown procedure is not finished yet, let's check again in 1 second.
        QTimer::singleShot(1000, this, &SchedulerProcess::checkShutdownProcedure);

}



SkyPoint SchedulerProcess::mountCoords()
{
    QVariant var = mountInterface()->property("equatorialCoords");

    // result must be two double values
    if (var.isValid() == false || var.canConvert<QList<double >> () == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << "Warning: reading equatorial coordinates received an unexpected value:" << var;
        return SkyPoint();
    }
    // check if we received exactly two values
    const QList<double> coords = var.value<QList<double >> ();
    if (coords.size() != 2)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << "Warning: reading equatorial coordinates received" << coords.size() <<
                                             "instead of 2 values: " << coords;
        return SkyPoint();
    }

    return SkyPoint(coords[0], coords[1]);
}

bool SchedulerProcess::isMountParked()
{
    if (mountInterface().isNull())
        return false;
    // First check if the mount is able to park - if it isn't, getParkingStatus will reply PARKING_ERROR and status won't be clear
    //QDBusReply<bool> const parkCapableReply = mountInterface->call(QDBus::AutoDetect, "canPark");
    QVariant canPark = mountInterface()->property("canPark");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount can park:" << (!canPark.isValid() ? "invalid" : (canPark.toBool() ? "T" : "F"));

    if (canPark.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount canPark request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        manageConnectionLoss();
        return false;
    }
    else if (canPark.toBool() == true)
    {
        // If it is able to park, obtain its current status
        //QDBusReply<int> const mountReply  = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
        QVariant parkingStatus = mountInterface()->property("parkStatus");
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

        if (parkingStatus.isValid() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parking status property is invalid %1.").arg(
                                                  mountInterface()->lastError().type());
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



GuideState SchedulerProcess::getGuidingStatus()
{
    QVariant guideStatus = guideInterface()->property("status");
    Ekos::GuideState gStatus = static_cast<Ekos::GuideState>(guideStatus.toInt());

    return gStatus;
}

const QString &SchedulerProcess::profile() const
{
    return moduleState()->currentProfile();
}

void SchedulerProcess::setProfile(const QString &newProfile)
{
    moduleState()->setCurrentProfile(newProfile);
}

QString SchedulerProcess::currentJobName()
{
    auto job = moduleState()->activeJob();
    return ( job != nullptr ? job->getName() : QString() );
}

QString SchedulerProcess::currentJobJson()
{
    auto job = moduleState()->activeJob();
    if( job != nullptr )
    {
        return QString( QJsonDocument( job->toJson() ).toJson() );
    }
    else
    {
        return QString();
    }
}

QString SchedulerProcess::jsonJobs()
{
    return QString( QJsonDocument( moduleState()->getJSONJobs() ).toJson() );
}

QStringList SchedulerProcess::logText()
{
    return moduleState()->logText();
}

bool SchedulerProcess::isDomeParked()
{
    if (domeInterface().isNull())
        return false;

    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    return status == ISD::PARK_PARKED;
}

void SchedulerProcess::simClockScaleChanged(float newScale)
{
    if (moduleState()->currentlySleeping())
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
    moduleState()->tickleTimer().stop();
}

void SchedulerProcess::simClockTimeChanged()
{
    moduleState()->calculateDawnDusk();

    // If the Scheduler is not running, reset all jobs and re-evaluate from a new current start point
    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
        startJobEvaluation();
}

void SchedulerProcess::setINDICommunicationStatus(CommunicationStatus status)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler INDI status is" << status;

    moduleState()->setIndiCommunicationStatus(status);
}

void SchedulerProcess::setEkosCommunicationStatus(CommunicationStatus status)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler Ekos status is" << status;

    moduleState()->setEkosCommunicationStatus(status);
}



void SchedulerProcess::checkInterfaceReady(QDBusInterface * iface)
{
    if (iface == mountInterface())
    {
        if (mountInterface()->property("canPark").isValid())
            moduleState()->setMountReady(true);
    }
    else if (iface == capInterface())
    {
        if (capInterface()->property("canPark").isValid())
            moduleState()->setCapReady(true);
    }
    else if (iface == observatoryInterface())
    {
        QVariant status = observatoryInterface()->property("status");
        if (status.isValid())
            setWeatherStatus(static_cast<ISD::Weather::Status>(status.toInt()));
    }
    else if (iface == weatherInterface())
    {
        QVariant status = weatherInterface()->property("status");
        if (status.isValid())
            setWeatherStatus(static_cast<ISD::Weather::Status>(status.toInt()));
    }
    else if (iface == domeInterface())
    {
        if (domeInterface()->property("canPark").isValid())
            moduleState()->setDomeReady(true);
    }
    else if (iface == captureInterface())
    {
        if (captureInterface()->property("coolerControl").isValid())
            moduleState()->setCaptureReady(true);
    }
    // communicate state to UI
    emit interfaceReady(iface);
}

void SchedulerProcess::registerNewModule(const QString &name)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Registering new Module (" << name << ")";

    if (name == "Focus")
    {
        delete focusInterface();
        setFocusInterface(new QDBusInterface(kstarsInterfaceString, focusPathString, focusInterfaceString,
                                             QDBusConnection::sessionBus(), this));
        connect(focusInterface(), SIGNAL(newStatus(Ekos::FocusState, const QString)), this,
                SLOT(setFocusStatus(Ekos::FocusState, const QString)), Qt::UniqueConnection);
    }
    else if (name == "Capture")
    {
        delete captureInterface();
        setCaptureInterface(new QDBusInterface(kstarsInterfaceString, capturePathString, captureInterfaceString,
                                               QDBusConnection::sessionBus(), this));

        connect(captureInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(captureInterface(), SIGNAL(newStatus(Ekos::CaptureState, const QString, int)), this,
                SLOT(setCaptureStatus(Ekos::CaptureState, const QString)), Qt::UniqueConnection);
        connect(captureInterface(), SIGNAL(captureComplete(QVariantMap, const QString)), this, SLOT(checkAlignment(QVariantMap,
                const QString)),
                Qt::UniqueConnection);
        checkInterfaceReady(captureInterface());
    }
    else if (name == "Mount")
    {
        delete mountInterface();
        setMountInterface(new QDBusInterface(kstarsInterfaceString, mountPathString, mountInterfaceString,
                                             QDBusConnection::sessionBus(), this));

        connect(mountInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
        connect(mountInterface(), SIGNAL(newStatus(ISD::Mount::Status)), this, SLOT(setMountStatus(ISD::Mount::Status)),
                Qt::UniqueConnection);

        checkInterfaceReady(mountInterface());
    }
    else if (name == "Align")
    {
        delete alignInterface();
        setAlignInterface(new QDBusInterface(kstarsInterfaceString, alignPathString, alignInterfaceString,
                                             QDBusConnection::sessionBus(), this));
        connect(alignInterface(), SIGNAL(newStatus(Ekos::AlignState)), this, SLOT(setAlignStatus(Ekos::AlignState)),
                Qt::UniqueConnection);
    }
    else if (name == "Guide")
    {
        delete guideInterface();
        setGuideInterface(new QDBusInterface(kstarsInterfaceString, guidePathString, guideInterfaceString,
                                             QDBusConnection::sessionBus(), this));
        connect(guideInterface(), SIGNAL(newStatus(Ekos::GuideState)), this,
                SLOT(setGuideStatus(Ekos::GuideState)), Qt::UniqueConnection);
    }
    else if (name == "Observatory")
    {
        delete observatoryInterface();
        setObservatoryInterface(new QDBusInterface(kstarsInterfaceString, observatoryPathString, observatoryInterfaceString,
                                QDBusConnection::sessionBus(), this));
        connect(observatoryInterface(), SIGNAL(newStatus(ISD::Weather::Status)), this,
                SLOT(setWeatherStatus(ISD::Weather::Status)), Qt::UniqueConnection);
        checkInterfaceReady(observatoryInterface());
    }
}

void SchedulerProcess::registerNewDevice(const QString &name, int interface)
{
    Q_UNUSED(name)

    if (interface & INDI::BaseDevice::DOME_INTERFACE)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(INDI::BaseDevice::DOME_INTERFACE);
        QDBusReply<QStringList> paths = indiInterface()->callWithArgumentList(QDBus::AutoDetect, "getDevicesPaths",
                                        dbusargs);
        if (paths.error().type() == QDBusError::NoError && !paths.value().isEmpty())
        {
            // Select last device in case a restarted caused multiple instances in the tree
            setDomePathString(paths.value().last());
            delete domeInterface();
            setDomeInterface(new QDBusInterface(kstarsInterfaceString, domePathString,
                                                domeInterfaceString,
                                                QDBusConnection::sessionBus(), this));
            connect(domeInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
            checkInterfaceReady(domeInterface());
        }
    }

    if (interface & INDI::BaseDevice::DUSTCAP_INTERFACE)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(INDI::BaseDevice::DUSTCAP_INTERFACE);
        QDBusReply<QStringList> paths = indiInterface()->callWithArgumentList(QDBus::AutoDetect, "getDevicesPaths",
                                        dbusargs);
        if (paths.error().type() == QDBusError::NoError && !paths.value().isEmpty())
        {
            // Select last device in case a restarted caused multiple instances in the tree
            setDustCapPathString(paths.value().last());
            delete capInterface();
            setCapInterface(new QDBusInterface(kstarsInterfaceString, dustCapPathString,
                                               dustCapInterfaceString,
                                               QDBusConnection::sessionBus(), this));
            connect(capInterface(), SIGNAL(ready()), this, SLOT(syncProperties()));
            checkInterfaceReady(capInterface());
        }
    }
}

bool SchedulerProcess::createJobSequence(XMLEle * root, const QString &prefix, const QString &outputDir)
{
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Job"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp(tagXMLEle(subEP), "TargetName"))
                {
                    // Set the target name in sequence file, though scheduler will overwrite this later
                    editXMLEle(subEP, prefix.toLatin1().constData());
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

XMLEle *SchedulerProcess::getSequenceJobRoot(const QString &filename) const
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



bool SchedulerProcess::canCountCaptures(const SchedulerJob &job)
{
    QList<QSharedPointer<SequenceJob >> seqjobs;
    bool hasAutoFocus = false;
    SchedulerJob tempJob = job;
    if (SchedulerUtils::loadSequenceQueue(tempJob.getSequenceFile().toLocalFile(), &tempJob, seqjobs, hasAutoFocus,
                                          nullptr) == false)
        return false;

    for (auto oneSeqJob : seqjobs)
    {
        if (oneSeqJob->getUploadMode() == ISD::Camera::UPLOAD_REMOTE)
            return false;
    }
    return true;
}

void SchedulerProcess::updateCompletedJobsCount(bool forced)
{
    /* Use a temporary map in order to limit the number of file searches */
    CapturedFramesMap newFramesCount;

    /* FIXME: Capture storage cache is refreshed too often, feature requires rework. */

    /* Check if one job is idle or requires evaluation - if so, force refresh */
    forced |= std::any_of(moduleState()->jobs().begin(),
                          moduleState()->jobs().end(), [](SchedulerJob * oneJob) -> bool
    {
        SchedulerJobStatus const state = oneJob->getState();
        return state == SCHEDJOB_IDLE || state == SCHEDJOB_EVALUATION;});

    /* If update is forced, clear the frame map */
    if (forced)
        moduleState()->capturedFramesCount().clear();

    /* Enumerate SchedulerJobs to count captures that are already stored */
    for (SchedulerJob *oneJob : moduleState()->jobs())
    {
        // This is like newFramesCount, but reset on every job.
        // It is useful for properly calling addProgress().
        CapturedFramesMap newJobFramesCount;

        QList<QSharedPointer<SequenceJob >> seqjobs;
        bool hasAutoFocus = false;

        //oneJob->setLightFramesRequired(false);
        /* Look into the sequence requirements, bypass if invalid */
        if (SchedulerUtils::loadSequenceQueue(oneJob->getSequenceFile().toLocalFile(), oneJob, seqjobs, hasAutoFocus,
                                              this) == false)
        {
            appendLogText(i18n("Warning: job '%1' has inaccessible sequence '%2', marking invalid.", oneJob->getName(),
                               oneJob->getSequenceFile().toLocalFile()));
            oneJob->setState(SCHEDJOB_INVALID);
            continue;
        }

        oneJob->clearProgress();
        /* Enumerate the SchedulerJob's SequenceJobs to count captures stored for each */
        for (auto oneSeqJob : seqjobs)
        {
            /* Only consider captures stored on client (Ekos) side */
            /* FIXME: ask the remote for the file count */
            if (oneSeqJob->getUploadMode() == ISD::Camera::UPLOAD_REMOTE)
                continue;

            /* FIXME: this signature path is incoherent when there is no filter wheel on the setup - bugfix should be elsewhere though */
            QString const signature = oneSeqJob->getSignature();

            /* If signature was processed during this run, keep it */
            if (newFramesCount.constEnd() != newFramesCount.constFind(signature))
            {
                if (newJobFramesCount.constEnd() == newJobFramesCount.constFind(signature))
                {
                    // Even though we've seen this before, we haven't seen it for this SchedulerJob.
                    const int count = newFramesCount.constFind(signature).value();
                    newJobFramesCount[signature] = count;
                    oneJob->addProgress(count, oneSeqJob);
                }
                continue;
            }
            int count = 0;

            CapturedFramesMap::const_iterator const earlierRunIterator =
                moduleState()->capturedFramesCount().constFind(signature);

            if (moduleState()->capturedFramesCount().constEnd() != earlierRunIterator)
                // If signature was processed during an earlier run, use the earlier count.
                count = earlierRunIterator.value();
            else
                // else recount captures already stored
                count =  PlaceholderPath::getCompletedFiles(signature);

            newFramesCount[signature] = count;
            newJobFramesCount[signature] = count;
            oneJob->addProgress(count, oneSeqJob);
        }

        // determine whether we need to continue capturing, depending on captured frames
        SchedulerUtils::updateLightFramesRequired(oneJob, seqjobs, newFramesCount);
    }

    moduleState()->setCapturedFramesCount(newFramesCount);

    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Frame map summary:";
        CapturedFramesMap::const_iterator it = moduleState()->capturedFramesCount().constBegin();
        for (; it != moduleState()->capturedFramesCount().constEnd(); it++)
            qCDebug(KSTARS_EKOS_SCHEDULER) << " " << it.key() << ':' << it.value();
    }
}

SchedulerJob *SchedulerProcess::activeJob()
{
    return  moduleState()->activeJob();
}

void SchedulerProcess::updateSafetyMonitorConnection()
{
    // Read the current connection string from settings
    QString connectionString = Options::schedulerSafetyMonitorConnectionString();

    // If weather monitoring is disabled, nothing to update
    if (!Options::schedulerWeather())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Weather monitoring is disabled, skipping safety monitor update";
        return;
    }

    // If we don't have a standalone safety monitor yet, initialize it if connection string is provided
    if (m_StandaloneSafetyMonitor == nullptr && !connectionString.isEmpty())
    {
        m_StandaloneSafetyMonitor = new SchedulerSafetyMonitor(this);
        connect(m_StandaloneSafetyMonitor, &SchedulerSafetyMonitor::newSafetyStatus, this, &SchedulerProcess::setSafetyStatus);
        connect(m_StandaloneSafetyMonitor, &SchedulerSafetyMonitor::newLog, this, &SchedulerProcess::appendLogText);
        m_StandaloneSafetyMonitor->initStandaloneSafetyMonitor(connectionString);
        appendLogText(i18n("Safety monitor initialized with connection string: %1", connectionString));
    }
    // If we have a standalone safety monitor, update its connection
    else if (m_StandaloneSafetyMonitor != nullptr)
    {
        m_StandaloneSafetyMonitor->updateConnectionString(connectionString);
    }
}

void SchedulerProcess::printStates(const QString &label)
{
    qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                   QString("%1 %2 %3%4 %5 %6 %7 %8 %9\n")
                                   .arg(label)
                                   .arg(timerStr(moduleState()->timerState()))
                                   .arg(getSchedulerStatusString(moduleState()->schedulerState()))
                                   .arg((moduleState()->timerState() == RUN_JOBCHECK && activeJob() != nullptr) ?
                                        QString("(%1 %2)").arg(SchedulerJob::jobStatusString(activeJob()->getState()))
                                        .arg(SchedulerJob::jobStageString(activeJob()->getStage())) : "")
                                   .arg(ekosStateString(moduleState()->ekosState()))
                                   .arg(indiStateString(moduleState()->indiState()))
                                   .arg(startupStateString(moduleState()->startupState()))
                                   .arg(shutdownStateString(moduleState()->shutdownState()))
                                   .arg(parkWaitStateString(moduleState()->parkWaitState())).toLatin1().data();
    foreach (auto j, moduleState()->jobs())
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("job %1 %2\n").arg(j->getName()).arg(SchedulerJob::jobStatusString(
                                           j->getState())).toLatin1().data();
}

} // Ekos namespace
