/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "schedulerprocess.h"
#include "schedulermodulestate.h"
#include "greedyscheduler.h"
#include "schedulerutils.h"
#include "schedulerjob.h"
#include "ekos/capture/sequencejob.h"
#include "Options.h"
#include "ksmessagebox.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "indi/indistd.h"
#include "skymapcomposite.h"
#include "mosaiccomponent.h"
#include "mosaictiles.h"
#include <ekos_scheduler_debug.h>

#include <QDBusReply>

#define RESTART_GUIDING_DELAY_MS  5000

// This is a temporary debugging printout introduced while gaining experience developing
// the unit tests in test_ekos_scheduler_ops.cpp.
// All these printouts should be eventually removed.

namespace Ekos
{

SchedulerProcess::SchedulerProcess(QSharedPointer<SchedulerModuleState> state)
{
    m_moduleState = state;
    m_GreedyScheduler = new GreedyScheduler();
}

void SchedulerProcess::execute()
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

        // restart aborted jobs immediately, if error handling strategy is set to "restart immediately"
        if (Options::errorHandlingStrategy() == ERROR_RESTART_IMMEDIATELY &&
                (activeJob()->getState() == SCHEDJOB_ABORTED ||
                 (activeJob()->getState() == SCHEDJOB_ERROR && Options::rescheduleErrors())))
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
        if (SchedulerModuleState::getLocalTime().secsTo(activeJob()->getCompletionTime()) <= 0)
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
                captureInterface()->call(QDBus::BlockWithGui, "abort");
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

        moduleState()->setupNextIteration(RUN_SCHEDULER);
    }
}

void SchedulerProcess::startScheduler()
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

void SchedulerProcess::stopScheduler()
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

    moduleState()->setSchedulerState(SCHEDULER_IDLE);
    moduleState()->setParkWaitState(PARKWAIT_IDLE);
    moduleState()->setEkosState(EKOS_IDLE);
    moduleState()->setIndiState(INDI_IDLE);

    // Only reset startup state to idle if the startup procedure was interrupted before it had the chance to complete.
    // Or if we're doing a soft shutdown
    if (moduleState()->startupState() != STARTUP_COMPLETE || moduleState()->preemptiveShutdown())
    {
        if (moduleState()->startupState() == STARTUP_SCRIPT)
        {
            scriptProcess().disconnect();
            scriptProcess().terminate();
        }

        moduleState()->setStartupState(STARTUP_IDLE);
    }
    // Reset startup state to unparking phase (dome -> mount -> cap)
    // We do not want to run the startup script again but unparking should be checked
    // whenever the scheduler is running again.
    else if (moduleState()->startupState() == STARTUP_COMPLETE)
    {
        if (Options::schedulerUnparkDome())
            moduleState()->setStartupState(STARTUP_UNPARK_DOME);
        else if (Options::schedulerUnparkMount())
            moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
        else if (Options::schedulerOpenDustCover())
            moduleState()->setStartupState(STARTUP_UNPARK_CAP);
    }

    moduleState()->setShutdownState(SHUTDOWN_IDLE);

    moduleState()->setActiveJob(nullptr);
    moduleState()->resetFailureCounters();
    moduleState()->setAutofocusCompleted(false);

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

    if (scriptProcess().state() == QProcess::Running)
        scriptProcess().terminate();

    // report success
    emit schedulerStopped();
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
             (job->getStepPipeline() & SchedulerJob::USE_TRACK) &&
             // schedulerParkMount->isEnabled() &&
             Options::schedulerParkMount())
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

    // If the mount was parked by a pause or the end-user, unpark
    if (isMountParked())
    {
        moduleState()->setParkWaitState(PARKWAIT_UNPARK);
        return;
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

    // Check if autofocus is supported
    QDBusReply<bool> focusModeReply;
    focusModeReply = focusInterface()->call(QDBus::AutoDetect, "canAutoFocus");

    if (focusModeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' canAutoFocus request received DBUS error: %2").arg(
                                              activeJob()->getName(), QDBusError::errorString(focusModeReply.error().type()));
        if (!manageConnectionLoss())
        {
            activeJob()->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }

    if (focusModeReply.value() == false)
    {
        appendLogText(i18n("Warning: job '%1' is unable to proceed with autofocus, not supported.", activeJob()->getName()));
        activeJob()->setStepPipeline(
            static_cast<SchedulerJob::StepPipeline>(activeJob()->getStepPipeline() & ~SchedulerJob::USE_FOCUS));
        moduleState()->updateJobStage(SCHEDSTAGE_FOCUS_COMPLETE);
        getNextAction();
        return;
    }

    // Clear the HFR limit value set in the capture module
    captureInterface()->call(QDBus::AutoDetect, "clearAutoFocusHFR");

    QDBusMessage reply;

    // We always need to reset frame first
    if ((reply = focusInterface()->call(QDBus::AutoDetect, "resetFrame")).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' resetFrame request received DBUS error: %2").arg(
                                              activeJob()->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
        {
            activeJob()->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }


    // If we have a LIGHT filter set, let's set it.
    if (!activeJob()->getInitialFilter().isEmpty())
    {
        focusInterface()->setProperty("filter", activeJob()->getInitialFilter());
    }

    // Set autostar if full field option is false
    if (Options::focusUseFullField() == false)
    {
        QList<QVariant> autoStar;
        autoStar.append(true);
        if ((reply = focusInterface()->callWithArgumentList(QDBus::AutoDetect, "setAutoStarEnabled", autoStar)).type() ==
                QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setAutoFocusStar request received DBUS error: %1").arg(
                                                  activeJob()->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
            {
                activeJob()->setState(SCHEDJOB_ERROR);
                findNextJob();
            }
            return;
        }
    }

    // Start auto-focus
    if ((reply = focusInterface()->call(QDBus::AutoDetect, "start")).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' startFocus request received DBUS error: %2").arg(
                                              activeJob()->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
        {
            activeJob()->setState(SCHEDJOB_ERROR);
            findNextJob();
        }
        return;
    }

    moduleState()->updateJobStage(SCHEDSTAGE_FOCUSING);
    appendLogText(i18n("Job '%1' is focusing.", activeJob()->getName()));
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

    captureInterface()->setProperty("targetName", activeJob()->getName());

    QString url = activeJob()->getSequenceFile().toLocalFile();

    if (restart == false)
    {
        QList<QVariant> dbusargs;
        dbusargs.append(url);
        // override targets from sequence queue file
        QVariant targetName(activeJob()->getName());
        dbusargs.append(targetName);
        QDBusReply<bool> const captureReply = captureInterface()->callWithArgumentList(QDBus::AutoDetect,
                                              "loadSequenceQueue",
                                              dbusargs);
        if (captureReply.error().type() != QDBusError::NoError)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' loadSequenceQueue request received DBUS error: %1").arg(activeJob()->getName()).arg(
                                                  captureReply.error().message());
            if (!manageConnectionLoss())
                activeJob()->setState(SCHEDJOB_ERROR);
            return;
        }
        // Check if loading sequence fails for whatever reason
        else if (captureReply.value() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' loadSequenceQueue request failed").arg(activeJob()->getName());
            if (!manageConnectionLoss())
                activeJob()->setState(SCHEDJOB_ERROR);
            return;
        }
    }


    CapturedFramesMap fMap = activeJob()->getCapturedFramesMap();

    for (auto &e : fMap.keys())
    {
        QList<QVariant> dbusargs;
        QDBusMessage reply;

        dbusargs.append(e);
        dbusargs.append(fMap.value(e));
        if ((reply = captureInterface()->callWithArgumentList(QDBus::AutoDetect, "setCapturedFramesMap",
                     dbusargs)).type() ==
                QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) <<
                                              QString("Warning: job '%1' setCapturedFramesCount request received DBUS error: %1").arg(activeJob()->getName()).arg(
                                                  reply.errorMessage());
            if (!manageConnectionLoss())
                activeJob()->setState(SCHEDJOB_ERROR);
            return;
        }
    }

    // Start capture process
    captureInterface()->call(QDBus::AutoDetect, "start");

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

void SchedulerProcess::executeScript(const QString &filename)
{
    appendLogText(i18n("Executing script %1...", filename));

    connect(&scriptProcess(), &QProcess::readyReadStandardOutput, this, &SchedulerProcess::readProcessOutput);

    connect(&scriptProcess(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus)
    {
        checkProcessExit(exitCode);
    });

    QStringList arguments;
    scriptProcess().start(filename, arguments);
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
                stopScheduler();
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
                stopScheduler();
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
                    stopScheduler();
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
                    stopScheduler();
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
            // If dome unparking is required then we wait for dome interface
            if (Options::schedulerUnparkDome() && moduleState()->domeReady() == false)
            {
                if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
                {
                    moduleState()->startCurrentOperationTimer();
                    appendLogText(i18n("Warning: dome device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                appendLogText(i18n("Dome unpark required but dome is not yet ready."));
                return false;
            }

            // If mount unparking is required then we wait for mount interface
            if (Options::schedulerUnparkMount() && moduleState()->mountReady() == false)
            {
                if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
                {
                    moduleState()->startCurrentOperationTimer();
                    appendLogText(i18n("Warning: mount device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount unpark required but mount is not yet ready.";
                return false;
            }

            // If cap unparking is required then we wait for cap interface
            if (Options::schedulerOpenDustCover() && moduleState()->capReady() == false)
            {
                if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
                {
                    moduleState()->startCurrentOperationTimer();
                    appendLogText(i18n("Warning: cap device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap unpark required but cap is not yet ready.";
                return false;
            }

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
            return true;
        }

        case INDI_READY:
            return true;
    }

    return false;
}

bool SchedulerProcess::completeShutdown()
{
    // If INDI is not done disconnecting, try again later
    if (moduleState()->indiState() == INDI_DISCONNECTING
            && checkINDIState() == false)
        return false;

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

    if (moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
        appendLogText(i18n("Shutdown complete."));
    else
        appendLogText(i18n("Shutdown procedure failed, aborting..."));

    // Stop Scheduler
    stopScheduler();

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

void SchedulerProcess::checkCapParkingStatus()
{
    if (capInterface().isNull())
        return;

    QVariant parkingStatus = capInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              capInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parked."));
                moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
            }
            moduleState()->resetParkingCapFailureCount();
            break;

        case ISD::PARK_UNPARKED:
            if (moduleState()->startupState() == STARTUP_UNPARKING_CAP)
            {
                moduleState()->setStartupState(STARTUP_COMPLETE);
                appendLogText(i18n("Cap unparked."));
            }
            moduleState()->resetParkingCapFailureCount();
            break;

        case ISD::PARK_PARKING:
        case ISD::PARK_UNPARKING:
            // TODO make the timeouts configurable by the user
            if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseParkingCapFailureCount())
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
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parking error."));
                moduleState()->setShutdownState(SHUTDOWN_ERROR);
            }
            else if (moduleState()->startupState() == STARTUP_UNPARKING_CAP)
            {
                appendLogText(i18n("Cap unparking error."));
                moduleState()->setStartupState(STARTUP_ERROR);
            }
            moduleState()->resetParkingCapFailureCount();
            break;

        default:
            break;
    }
}

void SchedulerProcess::checkMountParkingStatus()
{
    if (mountInterface().isNull())
        return;

    QVariant parkingStatus = mountInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        //case Mount::PARKING_OK:
        case ISD::PARK_PARKED:
            // If we are starting up, we will unpark the mount in checkParkWaitState soon
            // If we are shutting down and mount is parked, proceed to next step
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);

            // Update parking engine state
            if (moduleState()->parkWaitState() == PARKWAIT_PARKING)
                moduleState()->setParkWaitState(PARKWAIT_PARKED);

            appendLogText(i18n("Mount parked."));
            moduleState()->resetParkingMountFailureCount();
            break;

        //case Mount::UNPARKING_OK:
        case ISD::PARK_UNPARKED:
            // If we are starting up and mount is unparked, proceed to next step
            // If we are shutting down, we will park the mount in checkParkWaitState soon
            if (moduleState()->startupState() == STARTUP_UNPARKING_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);

            // Update parking engine state
            if (moduleState()->parkWaitState() == PARKWAIT_UNPARKING)
                moduleState()->setParkWaitState(PARKWAIT_UNPARKED);

            appendLogText(i18n("Mount unparked."));
            moduleState()->resetParkingMountFailureCount();
            break;

        // FIXME: Create an option for the parking/unparking timeout.

        //case Mount::UNPARKING_BUSY:
        case ISD::PARK_UNPARKING:
            if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseParkingMountFailureCount())
                {
                    appendLogText(i18n("Warning: mount unpark operation timed out on attempt %1/%2. Restarting operation...",
                                       moduleState()->parkingMountFailureCount(), moduleState()->maxFailureAttempts()));
                    unParkMount();
                }
                else
                {
                    appendLogText(i18n("Warning: mount unpark operation timed out on last attempt."));
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";

            break;

        //case Mount::PARKING_BUSY:
        case ISD::PARK_PARKING:
            if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseParkingMountFailureCount())
                {
                    appendLogText(i18n("Warning: mount park operation timed out on attempt %1/%2. Restarting operation...",
                                       moduleState()->parkingMountFailureCount(),
                                       moduleState()->maxFailureAttempts()));
                    parkMount();
                }
                else
                {
                    appendLogText(i18n("Warning: mount park operation timed out on last attempt."));
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Parking mount in progress...";

            break;

        //case Mount::PARKING_ERROR:
        case ISD::PARK_ERROR:
            if (moduleState()->startupState() == STARTUP_UNPARKING_MOUNT)
            {
                appendLogText(i18n("Mount unparking error."));
                moduleState()->setStartupState(STARTUP_ERROR);
                moduleState()->resetParkingMountFailureCount();
            }
            else if (moduleState()->shutdownState() == SHUTDOWN_PARKING_MOUNT)
            {
                if (moduleState()->increaseParkingMountFailureCount())
                {
                    appendLogText(i18n("Warning: mount park operation failed on attempt %1/%2. Restarting operation...",
                                       moduleState()->parkingMountFailureCount(),
                                       moduleState()->maxFailureAttempts()));
                    parkMount();
                }
                else
                {
                    appendLogText(i18n("Mount parking error."));
                    moduleState()->setShutdownState(SHUTDOWN_ERROR);
                    moduleState()->resetParkingMountFailureCount();
                }

            }
            else if (moduleState()->parkWaitState() == PARKWAIT_PARKING)
            {
                appendLogText(i18n("Mount parking error."));
                moduleState()->setParkWaitState(PARKWAIT_ERROR);
                moduleState()->resetParkingMountFailureCount();
            }
            else if (moduleState()->parkWaitState() == PARKWAIT_UNPARKING)
            {
                appendLogText(i18n("Mount unparking error."));
                moduleState()->setParkWaitState(PARKWAIT_ERROR);
                moduleState()->resetParkingMountFailureCount();
            }
            break;

        //case Mount::PARKING_IDLE:
        // FIXME Does this work as intended? check!
        case ISD::PARK_UNKNOWN:
            // Last parking action did not result in an action, so proceed to next step
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);

            // Last unparking action did not result in an action, so proceed to next step
            if (moduleState()->startupState() == STARTUP_UNPARKING_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);

            // Update parking engine state
            if (moduleState()->parkWaitState() == PARKWAIT_PARKING)
                moduleState()->setParkWaitState(PARKWAIT_PARKED);
            else if (moduleState()->parkWaitState() == PARKWAIT_UNPARKING)
                moduleState()->setParkWaitState(PARKWAIT_UNPARKED);

            moduleState()->resetParkingMountFailureCount();
            break;
    }
}

void SchedulerProcess::checkDomeParkingStatus()
{
    if (domeInterface().isNull())
        return;

    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_DOME)
            {
                appendLogText(i18n("Dome parked."));

                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            moduleState()->resetParkingDomeFailureCount();
            break;

        case ISD::PARK_UNPARKED:
            if (moduleState()->startupState() == STARTUP_UNPARKING_DOME)
            {
                moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
                appendLogText(i18n("Dome unparked."));
            }
            moduleState()->resetParkingDomeFailureCount();
            break;

        case ISD::PARK_PARKING:
        case ISD::PARK_UNPARKING:
            // TODO make the timeouts configurable by the user
            if (moduleState()->getCurrentOperationMsec() > (120 * 1000))
            {
                if (moduleState()->increaseParkingDomeFailureCount())
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
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_DOME)
            {
                if (moduleState()->increaseParkingDomeFailureCount())
                {
                    appendLogText(i18n("Dome parking failed. Restarting operation..."));
                    parkDome();
                }
                else
                {
                    appendLogText(i18n("Dome parking error."));
                    moduleState()->setShutdownState(SHUTDOWN_ERROR);
                    moduleState()->resetParkingDomeFailureCount();
                }
            }
            else if (moduleState()->startupState() == STARTUP_UNPARKING_DOME)
            {
                if (moduleState()->increaseParkingDomeFailureCount())
                {
                    appendLogText(i18n("Dome unparking failed. Restarting operation..."));
                    unParkDome();
                }
                else
                {
                    appendLogText(i18n("Dome unparking error."));
                    moduleState()->setStartupState(STARTUP_ERROR);
                    moduleState()->resetParkingDomeFailureCount();
                }
            }
            break;

        default:
            break;
    }
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

            // If Ekos is already started, we skip the script and move on to dome unpark step
            // unless we do not have light frames, then we skip all
            //QDBusReply<int> isEkosStarted;
            //isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            //if (isEkosStarted.value() == Ekos::Success)
            if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
            {
                if (moduleState()->startupScriptURL().isEmpty() == false)
                    appendLogText(i18n("Ekos is already started, skipping startup script..."));

                if (!activeJob() || activeJob()->getLightFramesRequired())
                    moduleState()->setStartupState(STARTUP_UNPARK_DOME);
                else
                    moduleState()->setStartupState(STARTUP_COMPLETE);
                return true;
            }

            if (moduleState()->currentProfile() != i18n("Default"))
            {
                QList<QVariant> profile;
                profile.append(moduleState()->currentProfile());
                ekosInterface()->callWithArgumentList(QDBus::AutoDetect, "setProfile", profile);
            }

            if (moduleState()->startupScriptURL().isEmpty() == false)
            {
                moduleState()->setStartupState(STARTUP_SCRIPT);
                executeScript(moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile));
                return false;
            }

            moduleState()->setStartupState(STARTUP_UNPARK_DOME);
            return false;
        }

        case STARTUP_SCRIPT:
            return false;

        case STARTUP_UNPARK_DOME:
            // If there is no job in case of manual startup procedure,
            // or if the job requires light frames, let's proceed with
            // unparking the dome, otherwise startup process is complete.
            if (activeJob() == nullptr || activeJob()->getLightFramesRequired())
            {
                if (Options::schedulerUnparkDome())
                    unParkDome();
                else
                    moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
            }
            else
            {
                moduleState()->setStartupState(STARTUP_COMPLETE);
                return true;
            }

            break;

        case STARTUP_UNPARKING_DOME:
            checkDomeParkingStatus();
            break;

        case STARTUP_UNPARK_MOUNT:
            if (Options::schedulerUnparkMount())
                unParkMount();
            else
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);
            break;

        case STARTUP_UNPARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case STARTUP_UNPARK_CAP:
            if (Options::schedulerOpenDustCover())
                unParkCap();
            else
                moduleState()->setStartupState(STARTUP_COMPLETE);
            break;

        case STARTUP_UNPARKING_CAP:
            checkCapParkingStatus();
            break;

        case STARTUP_COMPLETE:
            return true;

        case STARTUP_ERROR:
            stopScheduler();
            return true;
    }

    return false;
}

bool SchedulerProcess::checkShutdownState()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking shutdown state...";

    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    switch (moduleState()->shutdownState())
    {
        case SHUTDOWN_IDLE:

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting shutdown process...";

            moduleState()->setActiveJob(nullptr);
            moduleState()->setupNextIteration(RUN_SHUTDOWN);
            emit shutdownStarted();

            if (Options::schedulerWarmCCD())
            {
                appendLogText(i18n("Warming up CCD..."));

                // Turn it off
                //QVariant arg(false);
                //captureInterface->call(QDBus::AutoDetect, "setCoolerControl", arg);
                if (captureInterface())
                {
                    qCDebug(KSTARS_EKOS_SCHEDULER) << "Setting coolerControl=false";
                    captureInterface()->setProperty("coolerControl", false);
                }
            }

            // The following steps require a connection to the INDI server
            if (moduleState()->isINDIConnected())
            {
                if (Options::schedulerCloseDustCover())
                {
                    moduleState()->setShutdownState(SHUTDOWN_PARK_CAP);
                    return false;
                }

                if (Options::schedulerParkMount())
                {
                    moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
                    return false;
                }

                if (Options::schedulerParkDome())
                {
                    moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);
                    return false;
                }
            }
            else appendLogText(i18n("Warning: Bypassing parking procedures, no INDI connection."));

            if (moduleState()->shutdownScriptURL().isEmpty() == false)
            {
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
                return false;
            }

            moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
            return true;

        case SHUTDOWN_PARK_CAP:
            if (!moduleState()->isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            else if (Options::schedulerCloseDustCover())
                parkCap();
            else
                moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
            break;

        case SHUTDOWN_PARKING_CAP:
            checkCapParkingStatus();
            break;

        case SHUTDOWN_PARK_MOUNT:
            if (!moduleState()->isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            else if (Options::schedulerParkMount())
                parkMount();
            else
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);
            break;

        case SHUTDOWN_PARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case SHUTDOWN_PARK_DOME:
            if (!moduleState()->isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            else if (Options::schedulerParkDome())
                parkDome();
            else
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            break;

        case SHUTDOWN_PARKING_DOME:
            checkDomeParkingStatus();
            break;

        case SHUTDOWN_SCRIPT:
            if (moduleState()->shutdownScriptURL().isEmpty() == false)
            {
                // Need to stop Ekos now before executing script if it happens to stop INDI
                if (moduleState()->ekosState() != EKOS_IDLE && Options::shutdownScriptTerminatesINDI())
                {
                    stopEkos();
                    return false;
                }

                moduleState()->setShutdownState(SHUTDOWN_SCRIPT_RUNNING);
                executeScript(moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile));
            }
            else
                moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
            break;

        case SHUTDOWN_SCRIPT_RUNNING:
            return false;

        case SHUTDOWN_COMPLETE:
            return completeShutdown();

        case SHUTDOWN_ERROR:
            stopScheduler();
            return true;
    }

    return false;
}

bool SchedulerProcess::checkParkWaitState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    if (moduleState()->parkWaitState() == PARKWAIT_IDLE)
        return true;

    // qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking Park Wait State...";

    switch (moduleState()->parkWaitState())
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
            stopScheduler();
            return true;

    }

    return false;
}

void SchedulerProcess::runStartupProcedure()
{
    if (moduleState()->startupState() == STARTUP_IDLE
            || moduleState()->startupState() == STARTUP_ERROR
            || moduleState()->startupState() == STARTUP_COMPLETE)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);

            appendLogText(i18n("Warning: executing startup procedure manually..."));
            moduleState()->setStartupState(STARTUP_IDLE);
            checkStartupState();
            QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));

        });

        KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to execute the startup procedure manually?"));
    }
    else
    {
        switch (moduleState()->startupState())
        {
            case STARTUP_IDLE:
                break;

            case STARTUP_SCRIPT:
                scriptProcess().terminate();
                break;

            case STARTUP_UNPARK_DOME:
                break;

            case STARTUP_UNPARKING_DOME:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting unparking dome...";
                domeInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_MOUNT:
                break;

            case STARTUP_UNPARKING_MOUNT:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting unparking mount...";
                mountInterface()->call(QDBus::AutoDetect, "abort");
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

        moduleState()->setStartupState(STARTUP_IDLE);

        appendLogText(i18n("Startup procedure terminated."));
    }

}

void SchedulerProcess::runShutdownProcedure()
{
    if (moduleState()->shutdownState() == SHUTDOWN_IDLE
            || moduleState()->shutdownState() == SHUTDOWN_ERROR
            || moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
            appendLogText(i18n("Warning: executing shutdown procedure manually..."));
            moduleState()->setShutdownState(SHUTDOWN_IDLE);
            checkShutdownState();
            QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to execute the shutdown procedure manually?"));
    }
    else
    {
        switch (moduleState()->shutdownState())
        {
            case SHUTDOWN_IDLE:
                break;

            case SHUTDOWN_SCRIPT:
                break;

            case SHUTDOWN_SCRIPT_RUNNING:
                scriptProcess().terminate();
                break;

            case SHUTDOWN_PARK_DOME:
                break;

            case SHUTDOWN_PARKING_DOME:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting parking dome...";
                domeInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_MOUNT:
                break;

            case SHUTDOWN_PARKING_MOUNT:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting parking mount...";
                mountInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_CAP:
            case SHUTDOWN_PARKING_CAP:
                break;

            case SHUTDOWN_COMPLETE:
                break;

            case SHUTDOWN_ERROR:
                break;
        }

        moduleState()->setShutdownState(SHUTDOWN_IDLE);

        appendLogText(i18n("Shutdown procedure terminated."));
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
    if (jobs.isEmpty() || std::all_of(jobs.begin(), jobs.end(), neither_scheduled_nor_aborted))
    {
        appendLogText(i18n("No jobs left in the scheduler queue after evaluating."));
        moduleState()->setActiveJob(nullptr);
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
        return;
    }
    moduleState()->setActiveJob(scheduledJob);

}

void SchedulerProcess::evaluateJobs(bool evaluateOnly)
{
    for (auto job : moduleState()->jobs())
        job->clearCache();

    /* Don't evaluate if list is empty */
    if (moduleState()->jobs().isEmpty())
        return;
    /* Start by refreshing the number of captures already present - unneeded if not remembering job progress */
    if (Options::rememberJobProgress())
        updateCompletedJobsCount();

    moduleState()->calculateDawnDusk();

    getGreedyScheduler()->scheduleJobs(moduleState()->jobs(), SchedulerModuleState::getLocalTime(),
                                       moduleState()->capturedFramesCount(), this);
    // schedule or job states might have been changed, update the table

    if (!evaluateOnly && moduleState()->schedulerState() == SCHEDULER_RUNNING)
        // At this step, we finished evaluating jobs.
        // We select the first job that has to be run, per schedule.
        selectActiveJob(moduleState()->jobs());
    else
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos finished evaluating jobs, no job selection required.";

    emit jobsUpdated(moduleState()->getJSONJobs());
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
            stopScheduler();
            return true;
        }

        // #4 Check if startup procedure Phase #1 is complete (Startup script)
        if ((moduleState()->startupState() == STARTUP_IDLE
                && checkStartupState() == false)
                || moduleState()->startupState() == STARTUP_SCRIPT)
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
        if (moduleState()->startupState() > STARTUP_SCRIPT
                && moduleState()->startupState() < STARTUP_ERROR
                && checkStartupState() == false)
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

    connect(&moduleState()->iterationTimer(), &QTimer::timeout, this, &SchedulerProcess::iterate, Qt::UniqueConnection);
    moduleState()->iterationTimer().setSingleShot(true);
    moduleState()->iterationTimer().start(msSleep);

}

int SchedulerProcess::runSchedulerIteration()
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
    printStates(QString("End iteration, sleep %1: ").arg(moduleState()->timerInterval()));
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
    if (!getGreedyScheduler()->checkJob(moduleState()->jobs(), SchedulerModuleState::getLocalTime(), activeJob()))
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
                QVariant const status = focusInterface()->property("status");
                Ekos::FocusState focusStatus = static_cast<Ekos::FocusState>(status.toInt());

                if (focusStatus == Ekos::FOCUS_IDLE || focusStatus == Ekos::FOCUS_WAITING)
                {
                    if (moduleState()->increaseFocusFailureCount())
                    {
                        qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus module timed out. Restarting request...";
                        startFocusing();
                    }
                    else
                    {
                        appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking aborted.", activeJob()->getName()));
                        activeJob()->setState(SCHEDJOB_ABORTED);
                        findNextJob();
                    }
                }
                else moduleState()->startCurrentOperationTimer();
            }
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
    moduleState()->setAutofocusCompleted(false);

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
    outstream << "<SchedulerList version='1.6'>" << Qt::endl;
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

        QString ccArg, ccValue = tiles->completionCondition(&ccArg);
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
        outstream << "<Name>" << QString(entityXML(strdup(job->getName().toStdString().c_str()))) << "</Name>" << Qt::endl;
        outstream << "<Group>" << QString(entityXML(strdup(job->getGroup().toStdString().c_str()))) << "</Group>" << Qt::endl;
        outstream << "<Coordinates>" << Qt::endl;
        outstream << "<J2000RA>" << cLocale.toString(job->getTargetCoords().ra0().Hours()) << "</J2000RA>" << Qt::endl;
        outstream << "<J2000DE>" << cLocale.toString(job->getTargetCoords().dec0().Degrees()) << "</J2000DE>" << Qt::endl;
        outstream << "</Coordinates>" << Qt::endl;

        if (job->getFITSFile().isValid() && job->getFITSFile().isEmpty() == false)
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

        outstream << "<StartupCondition>" << Qt::endl;
        if (job->getFileStartupCondition() == START_ASAP)
            outstream << "<Condition>ASAP</Condition>" << Qt::endl;
        else if (job->getFileStartupCondition() == START_AT)
            outstream << "<Condition value='" << job->getFileStartupTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << Qt::endl;
        outstream << "</StartupCondition>" << Qt::endl;

        outstream << "<Constraints>" << Qt::endl;
        if (job->hasMinAltitude())
            outstream << "<Constraint value='" << cLocale.toString(job->getMinAltitude()) << "'>MinimumAltitude</Constraint>" <<
                      Qt::endl;
        if (job->getMinMoonSeparation() > 0)
            outstream << "<Constraint value='" << cLocale.toString(job->getMinMoonSeparation()) << "'>MoonSeparation</Constraint>"
                      << Qt::endl;
        if (job->getEnforceWeather())
            outstream << "<Constraint>EnforceWeather</Constraint>" << Qt::endl;
        if (job->getEnforceTwilight())
            outstream << "<Constraint>EnforceTwilight</Constraint>" << Qt::endl;
        if (job->getEnforceArtificialHorizon())
            outstream << "<Constraint>EnforceArtificialHorizon</Constraint>" << Qt::endl;
        outstream << "</Constraints>" << Qt::endl;

        outstream << "<CompletionCondition>" << Qt::endl;
        if (job->getCompletionCondition() == FINISH_SEQUENCE)
            outstream << "<Condition>Sequence</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == FINISH_REPEAT)
            outstream << "<Condition value='" << cLocale.toString(job->getRepeatsRequired()) << "'>Repeat</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == FINISH_LOOP)
            outstream << "<Condition>Loop</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == FINISH_AT)
            outstream << "<Condition value='" << job->getCompletionTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << Qt::endl;
        outstream << "</CompletionCondition>" << Qt::endl;

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

        outstream << "</Job>" << Qt::endl;
    }

    outstream << "<SchedulerAlgorithm value='" << ALGORITHM_GREEDY << "'/>" << Qt::endl;
    outstream << "<ErrorHandlingStrategy value='" << Options::errorHandlingStrategy() << "'>" << Qt::endl;
    if (Options::rescheduleErrors())
        outstream << "<RescheduleErrors />" << Qt::endl;
    outstream << "<delay>" << Options::errorHandlingStrategyDelay() << "</delay>" << Qt::endl;
    outstream << "</ErrorHandlingStrategy>" << Qt::endl;

    outstream << "<StartupProcedure>" << Qt::endl;
    if (moduleState()->startupScriptURL().isEmpty() == false)
        outstream << "<Procedure value='" << moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile) <<
                  "'>StartupScript</Procedure>" << Qt::endl;
    if (Options::schedulerUnparkDome())
        outstream << "<Procedure>UnparkDome</Procedure>" << Qt::endl;
    if (Options::schedulerUnparkMount())
        outstream << "<Procedure>UnparkMount</Procedure>" << Qt::endl;
    if (Options::schedulerOpenDustCover())
        outstream << "<Procedure>UnparkCap</Procedure>" << Qt::endl;
    outstream << "</StartupProcedure>" << Qt::endl;

    outstream << "<ShutdownProcedure>" << Qt::endl;
    if (Options::schedulerWarmCCD())
        outstream << "<Procedure>WarmCCD</Procedure>" << Qt::endl;
    if (Options::schedulerCloseDustCover())
        outstream << "<Procedure>ParkCap</Procedure>" << Qt::endl;
    if (Options::schedulerParkMount())
        outstream << "<Procedure>ParkMount</Procedure>" << Qt::endl;
    if (Options::schedulerParkDome())
        outstream << "<Procedure>ParkDome</Procedure>" << Qt::endl;
    if (moduleState()->shutdownScriptURL().isEmpty() == false)
        outstream << "<Procedure value='" << moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile) <<
                  "'>schedulerStartupScript</Procedure>" <<
                  Qt::endl;
    outstream << "</ShutdownProcedure>" << Qt::endl;

    outstream << "</SchedulerList>" << Qt::endl;

    appendLogText(i18n("Scheduler list saved to %1", fileURL.toLocalFile()));
    file.close();
    moduleState()->setDirty(false);
    return true;
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
                    emit addJob(SchedulerUtils::createJob(ep));
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
                QDBusReply<QList<double>> solutionReply = alignInterface()->call("getTargetCoords");
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

void SchedulerProcess::setFocusStatus(FocusState status)
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED || activeJob() == nullptr)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus State" << Ekos::getFocusStatusString(status);

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
            appendLogText(i18n("Job '%1' focusing is complete.", activeJob()->getName()));

            moduleState()->setAutofocusCompleted(true);

            moduleState()->updateJobStage(SCHEDSTAGE_FOCUS_COMPLETE);

            getNextAction();
        }
        else if (status == Ekos::FOCUS_FAILED || status == Ekos::FOCUS_ABORTED)
        {
            appendLogText(i18n("Warning: job '%1' focusing failed.", activeJob()->getName()));

            if (moduleState()->increaseFocusFailureCount())
            {
                appendLogText(i18n("Job '%1' is restarting its focusing procedure.", activeJob()->getName()));
                // Reset frame to original size.
                focusInterface()->call(QDBus::AutoDetect, "resetFrame");
                // Restart focusing
                qCDebug(KSTARS_EKOS_SCHEDULER) << "startFocusing on 6883";
                startFocusing();
            }
            else
            {
                appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking aborted.", activeJob()->getName()));
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

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount State changed to" << status;

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

        default:
            break;
    }
}

void SchedulerProcess::checkStartupProcedure()
{
    if (checkStartupState() == false)
        QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));
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
        QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));

}


void SchedulerProcess::parkCap()
{
    if (capInterface().isNull())
    {
        appendLogText(i18n("Dust cover park requested but no dust covers detected."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    QVariant parkingStatus = capInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    if (status != ISD::PARK_PARKED)
    {
        moduleState()->setShutdownState(SHUTDOWN_PARKING_CAP);
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Parking dust cap...";
        capInterface()->call(QDBus::AutoDetect, "park");
        appendLogText(i18n("Parking Cap..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Cap already parked."));
        moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
    }
}

void SchedulerProcess::unParkCap()
{
    if (capInterface().isNull())
    {
        appendLogText(i18n("Dust cover unpark requested but no dust covers detected."));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    QVariant parkingStatus = capInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    if (status != ISD::PARK_UNPARKED)
    {
        moduleState()->setStartupState(STARTUP_UNPARKING_CAP);
        capInterface()->call(QDBus::AutoDetect, "unpark");
        appendLogText(i18n("Unparking cap..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Cap already unparked."));
        moduleState()->setStartupState(STARTUP_COMPLETE);
    }
}

void SchedulerProcess::parkMount()
{
    if (mountInterface().isNull())
    {
        appendLogText(i18n("Mount park requested but no mounts detected."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    QVariant parkingStatus = mountInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (moduleState()->shutdownState() == SHUTDOWN_PARK_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);

            moduleState()->setParkWaitState(PARKWAIT_PARKED);
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
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Parking mount...";
            QDBusReply<bool> const mountReply = mountInterface()->call(QDBus::AutoDetect, "park");

            if (mountReply.error().type() != QDBusError::NoError)
            {
                qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount park request received DBUS error: %1").arg(
                                                      QDBusError::errorString(mountReply.error().type()));
                if (!manageConnectionLoss())
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
            }
            else moduleState()->startCurrentOperationTimer();
        }

        // Fall through
        case ISD::PARK_PARKING:
            //case Mount::PARKING_BUSY:
            if (moduleState()->shutdownState() == SHUTDOWN_PARK_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARKING_MOUNT);

            moduleState()->setParkWaitState(PARKWAIT_PARKING);
            appendLogText(i18n("Parking mount in progress..."));
            break;

            // All cases covered above so no need for default
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while parking mount.").arg(mountReply.value());
    }

}

void SchedulerProcess::unParkMount()
{
    if (mountInterface().isNull())
    {
        appendLogText(i18n("Mount unpark requested but no mounts detected."));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    QVariant parkingStatus = mountInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        //case Mount::UNPARKING_OK:
        case ISD::PARK_UNPARKED:
            if (moduleState()->startupState() == STARTUP_UNPARK_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);

            moduleState()->setParkWaitState(PARKWAIT_UNPARKED);
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
            QDBusReply<bool> const mountReply = mountInterface()->call(QDBus::AutoDetect, "unpark");

            if (mountReply.error().type() != QDBusError::NoError)
            {
                qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount unpark request received DBUS error: %1").arg(
                                                      QDBusError::errorString(mountReply.error().type()));
                if (!manageConnectionLoss())
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
            }
            else moduleState()->startCurrentOperationTimer();
        }

        // Fall through
        //case Mount::UNPARKING_BUSY:
        case ISD::PARK_UNPARKING:
            if (moduleState()->startupState() == STARTUP_UNPARK_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARKING_MOUNT);

            moduleState()->setParkWaitState(PARKWAIT_UNPARKING);
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";
            break;

            // All cases covered above
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while unparking mount.").arg(mountReply.value());
    }
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

void SchedulerProcess::parkDome()
{
    // If there is no dome, mark error
    if (domeInterface().isNull())
    {
        appendLogText(i18n("Dome park requested but no domes detected."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    //QDBusReply<int> const domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    //Dome::ParkingStatus status = static_cast<Dome::ParkingStatus>(domeReply.value());
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
    if (status != ISD::PARK_PARKED)
    {
        moduleState()->setShutdownState(SHUTDOWN_PARKING_DOME);
        domeInterface()->call(QDBus::AutoDetect, "park");
        appendLogText(i18n("Parking dome..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Dome already parked."));
        moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
    }
}

void SchedulerProcess::unParkDome()
{
    // If there is no dome, mark error
    if (domeInterface().isNull())
    {
        appendLogText(i18n("Dome unpark requested but no domes detected."));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    if (static_cast<ISD::ParkStatus>(parkingStatus.toInt()) != ISD::PARK_UNPARKED)
    {
        moduleState()->setStartupState(STARTUP_UNPARKING_DOME);
        domeInterface()->call(QDBus::AutoDetect, "unpark");
        appendLogText(i18n("Unparking dome..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        appendLogText(i18n("Dome already unparked."));
        moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
    }
}

GuideState SchedulerProcess::getGuidingStatus()
{
    QVariant guideStatus = guideInterface()->property("status");
    Ekos::GuideState gStatus = static_cast<Ekos::GuideState>(guideStatus.toInt());

    return gStatus;
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

void SchedulerProcess::checkProcessExit(int exitCode)
{
    scriptProcess().disconnect();

    if (exitCode == 0)
    {
        if (moduleState()->startupState() == STARTUP_SCRIPT)
            moduleState()->setStartupState(STARTUP_UNPARK_DOME);
        else if (moduleState()->shutdownState() == SHUTDOWN_SCRIPT_RUNNING)
            moduleState()->setShutdownState(SHUTDOWN_COMPLETE);

        return;
    }

    if (moduleState()->startupState() == STARTUP_SCRIPT)
    {
        appendLogText(i18n("Startup script failed, aborting..."));
        moduleState()->setStartupState(STARTUP_ERROR);
    }
    else if (moduleState()->shutdownState() == SHUTDOWN_SCRIPT_RUNNING)
    {
        appendLogText(i18n("Shutdown script failed, aborting..."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
    }

}

void SchedulerProcess::readProcessOutput()
{
    appendLogText(scriptProcess().readAllStandardOutput().simplified());
}

bool SchedulerProcess::canCountCaptures(const SchedulerJob &job)
{
    QList<SequenceJob*> seqjobs;
    bool hasAutoFocus = false;
    SchedulerJob tempJob = job;
    if (SchedulerUtils::loadSequenceQueue(tempJob.getSequenceFile().toLocalFile(), &tempJob, seqjobs, hasAutoFocus,
                                          nullptr) == false)
        return false;

    for (const SequenceJob *oneSeqJob : seqjobs)
    {
        if (oneSeqJob->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
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
        QList<SequenceJob*> seqjobs;
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
            newFramesCount[signature] = PlaceholderPath::getCompletedFiles(signature);
        }

        // determine whether we need to continue capturing, depending on captured frames
        SchedulerUtils::updateLightFramesRequired(oneJob, seqjobs, newFramesCount);
    }

    moduleState()->setCapturedFramesCount(newFramesCount);

    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Frame map summary:";
        QMap<QString, uint16_t>::const_iterator it = moduleState()->capturedFramesCount().constBegin();
        for (; it != moduleState()->capturedFramesCount().constEnd(); it++)
            qCDebug(KSTARS_EKOS_SCHEDULER) << " " << it.key() << ':' << it.value();
    }
}

SchedulerJob *SchedulerProcess::activeJob()
{
    return  moduleState()->activeJob();
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
        qCDebug(KSTARS_EKOS_SCHEDULER) << "job %s", QString("%1 %2\n").arg(j->getName())
                                       .arg(SchedulerJob::jobStatusString(j->getState())).toLatin1().data();
}

} // Ekos namespace
