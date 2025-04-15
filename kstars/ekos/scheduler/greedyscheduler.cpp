/*  Ekos Scheduler Greedy Algorithm
    SPDX-FileCopyrightText: Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "greedyscheduler.h"

#include <ekos_scheduler_debug.h>

#include "Options.h"
#include "scheduler.h"
#include "schedulermodulestate.h"
#include "ekos/ekos.h"
#include "ui_scheduler.h"
#include "schedulerjob.h"
#include "schedulerutils.h"

#define TEST_PRINT if (false) fprintf

// Can make the scheduling a bit faster by sampling every other minute instead of every minute.
constexpr int SCHEDULE_RESOLUTION_MINUTES = 2;

namespace Ekos
{

GreedyScheduler::GreedyScheduler()
{
}

void GreedyScheduler::setParams(bool restartImmediately, bool restartQueue,
                                bool rescheduleErrors, int abortDelay,
                                int errorHandlingDelay)
{
    setRescheduleAbortsImmediate(restartImmediately);
    setRescheduleAbortsQueue(restartQueue);
    setRescheduleErrors(rescheduleErrors);
    setAbortDelaySeconds(abortDelay);
    setErrorDelaySeconds(errorHandlingDelay);
}

// The possible changes made to a job in jobs are:
// Those listed in prepareJobsForEvaluation()
// Those listed in selectNextJob
// job->clearCache()
// job->updateJobCells()

void GreedyScheduler::scheduleJobs(const QList<SchedulerJob *> &jobs,
                                   const QDateTime &now,
                                   const QMap<QString, uint16_t> &capturedFramesCount,
                                   ModuleLogger *logger)
{
    for (auto job : jobs)
        job->clearCache();

    QDateTime when;
    QElapsedTimer timer;
    timer.start();
    scheduledJob = nullptr;
    schedule.clear();

    prepareJobsForEvaluation(jobs, now, capturedFramesCount, logger);

    // consider only lead jobs for scheduling, scheduling data is propagated to its follower jobs
    const QList<SchedulerJob *> leadJobs = SchedulerUtils::filterLeadJobs(jobs);

    scheduledJob = selectNextJob(leadJobs, now, nullptr, SIMULATE, &when, nullptr, nullptr, &capturedFramesCount);
    auto schedule = getSchedule();
    if (logger != nullptr)
    {
        if (!schedule.empty())
        {
            // Print in reverse order ?! The log window at the bottom of the screen
            // prints "upside down" -- most recent on top -- and I believe that view
            // is more important than the log file (where we can invert when debugging).
            for (int i = schedule.size() - 1; i >= 0; i--)
                logger->appendLogText(GreedyScheduler::jobScheduleString(schedule[i]));
            logger->appendLogText(QString("Scheduler plan for the next 48 hours starting %1 (%2)s:")
                                  .arg(now.toString()).arg(timer.elapsed() / 1000.0));
        }
        else
        {
            if (jobs.size() > 0)
            {
                const int numJobs = jobs.size();
                QString reason;
                auto now = SchedulerModuleState::getLocalTime().addSecs(12 * 3600);
                QDateTime soon = now.addSecs(3600);
                for (int i = numJobs; i > 0; i--)
                {
                    const auto &job = jobs[i - 1];
                    QDateTime nextEnd = job->getNextEndTime(now, SCHEDULE_RESOLUTION_MINUTES, &reason, soon);
                    logger->appendLogText(QString("(%1) %2: cannot run because: %3").arg(i).arg(job->getName()).arg(reason));
                }
                logger->appendLogText(QString("*****************In 12 Hours******************"));
                now = SchedulerModuleState::getLocalTime();
                soon = now.addSecs(3600);
                for (int i = numJobs; i > 0; i--)
                {
                    const auto &job = jobs[i - 1];
                    QDateTime nextEnd = job->getNextEndTime(now, SCHEDULE_RESOLUTION_MINUTES, &reason, soon);
                    logger->appendLogText(QString("(%1) %2: cannot run because: %3").arg(i).arg(job->getName()).arg(reason));
                }
                logger->appendLogText(QString("******************** Now ********************"));
                logger->appendLogText(QString("To debug, set the time to a time when you believe a job should be runnable."));
            }
            logger->appendLogText(QString("Scheduler: Sorry, no jobs are runnable for the next 3 days."));
        }
    }
    if (scheduledJob != nullptr)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER)
                << QString("Greedy Scheduler scheduling next job %1 at %2")
                .arg(scheduledJob->getName(), when.toString("hh:mm"));
        scheduledJob->setState(SCHEDJOB_SCHEDULED);
        scheduledJob->setStartupTime(when);
    }

    for (auto job : jobs)
        job->clearCache();
}

// The changes made to a job in jobs are:
//  Those listed in selectNextJob()
// Not a const method because it sets the schedule class variable.
bool GreedyScheduler::checkJob(const QList<SchedulerJob *> &jobs,
                               const QDateTime &now,
                               const SchedulerJob * const currentJob)
{
    // Don't interrupt a job that just started.
    if (currentJob && currentJob->getStateTime().secsTo(now) < 5)
        return true;

    QDateTime startTime;

    // Simulating in checkJob() is only done to update the schedule which is a GUI convenience.
    // Do it only if its quick and not more frequently than once per minute.
    SimulationType simType = SIMULATE_EACH_JOB_ONCE;
    if (m_SimSeconds > 0.5 ||
            (m_LastCheckJobSim.isValid() && m_LastCheckJobSim.secsTo(now) < 60))
        simType = DONT_SIMULATE;

    const SchedulerJob *next = selectNextJob(jobs, now, currentJob, simType, &startTime);
    if (next == currentJob && now.secsTo(startTime) <= 1)
    {
        if (simType != DONT_SIMULATE)
            m_LastCheckJobSim = now;

        return true;
    }
    else
    {
        // We need to interrupt the current job. There's a higher-priority one to run.
        qCDebug(KSTARS_EKOS_SCHEDULER)
                << QString("Greedy Scheduler bumping current job %1 for %2 at %3")
                .arg(currentJob->getName(), next ? next->getName() : "---", now.toString("hh:mm"));
        return false;
    }
}

// The changes made to a job in jobs are:
// job->setState(JOB_COMPLETE|JOB_EVALUATION|JOB_INVALID|JOB_COMPLETEno_change)
// job->setEstimatedTime(0|-1|-2|time)
// job->setInitialFilter(filter)
// job->setLightFramesRequired(bool)
// job->setInSequenceFocus(bool);
// job->setCompletedIterations(completedIterations);
// job->setCapturedFramesMap(capture_map);
// job->setSequenceCount(count);
// job->setEstimatedTimePerRepeat(time);
// job->setEstimatedTimeLeftThisRepeat(time);
// job->setEstimatedStartupTime(time);
// job->setCompletedCount(count);

void GreedyScheduler::prepareJobsForEvaluation(
    const QList<SchedulerJob *> &jobs, const QDateTime &now,
    const QMap<QString, uint16_t> &capturedFramesCount, ModuleLogger *logger, bool reestimateJobTimes) const
{
    // Remove some finished jobs from eval.
    foreach (SchedulerJob *job, jobs)
    {
        job->clearSimulatedSchedule();
        switch (job->getCompletionCondition())
        {
            case FINISH_AT:
                /* If planned finishing time has passed, the job is set to IDLE waiting for a next chance to run */
                if (job->getFinishAtTime().isValid() && job->getFinishAtTime() < now)
                {
                    job->setState(SCHEDJOB_COMPLETE);
                    continue;
                }
                break;

            case FINISH_REPEAT:
                // In case of a repeating jobs, let's make sure we have more runs left to go
                // If we don't, re-estimate imaging time for the scheduler job before concluding
                if (job->getRepeatsRemaining() == 0)
                {
                    if (logger != nullptr) logger->appendLogText(i18n("Job '%1' has no more batches remaining.", job->getName()));
                    job->setState(SCHEDJOB_COMPLETE);
                    job->setEstimatedTime(0);
                    continue;
                }
                break;

            default:
                break;
        }
    }

    // Change the state to eval or ERROR/ABORTED for all jobs that will be evaluated.
    foreach (SchedulerJob *job, jobs)
    {
        switch (job->getState())
        {
            case SCHEDJOB_INVALID:
            case SCHEDJOB_COMPLETE:
                // If job is invalid or complete, bypass evaluation.
                break;

            case SCHEDJOB_ERROR:
            case SCHEDJOB_ABORTED:
                // These will be evaluated, but we'll have a delay to start.
                break;
            case SCHEDJOB_IDLE:
            case SCHEDJOB_BUSY:
            case SCHEDJOB_SCHEDULED:
            case SCHEDJOB_EVALUATION:
            default:
                job->setState(SCHEDJOB_EVALUATION);
                break;
        }
    }

    // Estimate the job times
    foreach (SchedulerJob *job, jobs)
    {
        if (job->getState() == SCHEDJOB_INVALID || job->getState() == SCHEDJOB_COMPLETE)
            continue;

        // -1 = Job is not estimated yet
        // -2 = Job is estimated but time is unknown
        // > 0  Job is estimated and time is known
        if (reestimateJobTimes)
        {
            job->setEstimatedTime(-1);
            if (SchedulerUtils::estimateJobTime(job, capturedFramesCount, logger) == false)
            {
                job->setState(SCHEDJOB_INVALID);
                continue;
            }
        }
        if (job->getEstimatedTime() == 0)
        {
            job->setRepeatsRemaining(0);
            // set job including its followers to complete state
            job->setState(SCHEDJOB_COMPLETE, true);
            continue;
        }
        TEST_PRINT(stderr, "JOB %s estimated time: %ld state %d\n", job->getName().toLatin1().data(), job->getEstimatedTime(),
                   job->getState());
    }
}

namespace
{
// Don't Allow INVALID or COMPLETE jobs to be scheduled.
// Allow ABORTED if one of the rescheduleAbort... options are true.
// Allow ERROR if rescheduleErrors is true.
bool allowJob(const SchedulerJob *job, bool rescheduleAbortsImmediate, bool rescheduleAbortsQueue, bool rescheduleErrors)
{
    if (job->getState() == SCHEDJOB_INVALID || job->getState() == SCHEDJOB_COMPLETE)
        return false;
    if (job->getState() == SCHEDJOB_ABORTED && !rescheduleAbortsImmediate && !rescheduleAbortsQueue)
        return false;
    if (job->getState() == SCHEDJOB_ERROR && !rescheduleErrors)
        return false;
    return true;
}

// Returns the first possible time a job may be scheduled. That is, it doesn't
// evaluate the job, but rather just computes the needed delay (for ABORT and ERROR jobs)
// or returns now for other jobs.
QDateTime firstPossibleStart(const SchedulerJob *job, const QDateTime &now,
                             bool rescheduleAbortsQueue, int abortDelaySeconds,
                             bool rescheduleErrors, int errorDelaySeconds)
{
    QDateTime possibleStart = now;
    const QDateTime &abortTime = job->getLastAbortTime();
    const QDateTime &errorTime = job->getLastErrorTime();

    if (abortTime.isValid() && rescheduleAbortsQueue)
    {
        auto abortStartTime = abortTime.addSecs(abortDelaySeconds);
        if (abortStartTime > now)
            possibleStart = abortStartTime;
    }


    if (errorTime.isValid() && rescheduleErrors)
    {
        auto errorStartTime = errorTime.addSecs(errorDelaySeconds);
        if (errorStartTime > now)
            possibleStart = errorStartTime;
    }

    if (!possibleStart.isValid() || possibleStart < now)
        possibleStart = now;
    return possibleStart;
}
}  // namespace

// Consider all jobs marked as JOB_EVALUATION/ABORT/ERROR. Assume ordered by highest priority first.
// - Find the job with the earliest start time (given constraints like altitude, twilight, ...)
//   that can run for at least 10 minutes before a higher priority job.
// - START_AT jobs are given the highest priority, whereever on the list they may be,
//   as long as they can start near their designated start times.
// - Compute a schedule for the next 2 days, if fullSchedule is true, otherwise
//   just look for the next job.
// - If currentJob is not nullptr, this method is really evaluating whether
//   that job can continue to be run, or if can't meet constraints, or if it
//   should be preempted for another job.
//
// This does not modify any of the jobs in jobs if there is no simType is DONT_SIMULATE.
// If we are simulating, then jobs may change in the following ways:
//  job->setGreedyCompletionTime()
//  job->setState(state);
//  job->setStartupTime(time);
//  job->setStopReason(reason);
// The only reason this isn't a const method is because it sets the schedule class variable.
SchedulerJob *GreedyScheduler::selectNextJob(const QList<SchedulerJob *> &jobs, const QDateTime &now,
        const SchedulerJob * const currentJob, SimulationType simType, QDateTime *when,
        QDateTime *nextInterruption, QString *interruptReason,
        const QMap<QString, uint16_t> *capturedFramesCount)
{
    TEST_PRINT(stderr, "selectNextJob(%s)\n", now.toString().toLatin1().data());
    // Don't schedule a job that will be preempted in less than MIN_RUN_SECS.
    constexpr int MIN_RUN_SECS = 10 * 60;

    // Don't preempt a job for another job that is more than MAX_INTERRUPT_SECS in the future.
    constexpr int MAX_INTERRUPT_SECS = 30;

    // Don't interrupt START_AT jobs unless they can no longer run, or they're interrupted by another START_AT.
    bool currentJobIsStartAt = (currentJob && currentJob->getFileStartupCondition() == START_AT &&
                                currentJob->getStartAtTime().isValid());
    QDateTime nextStart;
    SchedulerJob * nextJob = nullptr;
    QString interruptStr;

    for (int i = 0; i < jobs.size(); ++i)
    {
        SchedulerJob * const job = jobs[i];
        const bool evaluatingCurrentJob = (currentJob && (job == currentJob));

        TEST_PRINT(stderr, " considering %s (%s)\n", job->getName().toLatin1().data(), evaluatingCurrentJob ? "evaluating" : "");

        if (!allowJob(job, rescheduleAbortsImmediate, rescheduleAbortsQueue, rescheduleErrors))
        {
            TEST_PRINT(stderr, "  not allowed\n");
            continue;
        }

        // If the job state is abort or error, might have to delay the first possible start time.
        QDateTime startSearchingtAt = firstPossibleStart(
                                          job, now, rescheduleAbortsQueue, abortDelaySeconds, rescheduleErrors, errorDelaySeconds);

        TEST_PRINT(stderr, "  start searching at %s\n", startSearchingtAt.toString().toLatin1().data());
        // Find the first time this job can meet all its constraints.
        // I found that passing in an "until" 4th argument actually hurt performance, as it reduces
        // the effectiveness of the cache that getNextPossibleStartTime uses.
        const QDateTime startTime = job->getNextPossibleStartTime(startSearchingtAt, SCHEDULE_RESOLUTION_MINUTES,
                                    evaluatingCurrentJob);
        TEST_PRINT(stderr, "  startTime %s\n", startTime.toString().toLatin1().data());

        if (startTime.isValid())
        {
            if (nextJob == nullptr)
            {
                // We have no other solutions--this is our best solution so far.
                nextStart = startTime;
                nextJob = job;
                if (nextInterruption) *nextInterruption = QDateTime();
                interruptStr = "";
            }
            else if (Options::greedyScheduling())
            {
                // Allow this job to be scheduled if it can run this many seconds
                // before running into a higher priority job.
                const int runSecs = evaluatingCurrentJob ? MAX_INTERRUPT_SECS : MIN_RUN_SECS;

                // Don't interrupt a START_AT for higher priority job
                if (evaluatingCurrentJob && currentJobIsStartAt)
                {
                    if (nextInterruption) *nextInterruption = QDateTime();
                    nextStart = startTime;
                    nextJob = job;
                    interruptStr = "";
                }
                else if (startTime.secsTo(nextStart) > runSecs)
                {
                    // We can start a lower priority job if it can run for at least runSecs
                    // before getting bumped by the previous higher priority job.
                    if (nextInterruption) *nextInterruption = nextStart;
                    interruptStr = QString("interrupted by %1").arg(nextJob->getName());
                    nextStart = startTime;
                    nextJob = job;
                }
            }
            // If scheduling, and we have a solution close enough to now, none of the lower priority
            // jobs can possibly be scheduled.
            if (!currentJob && nextStart.isValid() && now.secsTo(nextStart) < MIN_RUN_SECS)
                break;
        }
        else if (evaluatingCurrentJob)
        {
            // No need to keep searching past the current job if we're evaluating it
            // and it had no startTime.  It needs to be stopped.
            *when = QDateTime();
            return nullptr;
        }

        if (evaluatingCurrentJob) break;
    }
    if (nextJob != nullptr)
    {
        // The exception to the simple scheduling rules above are START_AT jobs, which
        // are given highest priority, irrespective of order. If nextJob starts less than
        // MIN_RUN_SECS before an on-time START_AT job, then give the START_AT job priority.
        // However, in order for the START_AT job to interrupt a current job, it must start now.
        for (int i = 0; i < jobs.size(); ++i)
        {
            SchedulerJob * const atJob = jobs[i];
            if (atJob == nextJob)
                continue;
            const QDateTime atTime = atJob->getStartAtTime();
            if (atJob->getFileStartupCondition() == START_AT && atTime.isValid())
            {
                if (!allowJob(atJob, rescheduleAbortsImmediate, rescheduleAbortsQueue, rescheduleErrors))
                    continue;
                // If the job state is abort or error, might have to delay the first possible start time.
                QDateTime startSearchingtAt = firstPossibleStart(
                                                  atJob, now, rescheduleAbortsQueue, abortDelaySeconds, rescheduleErrors,
                                                  errorDelaySeconds);
                // atTime above is the user-specified start time. atJobStartTime is the time it can
                // actually start, given all the constraints (altitude, twilight, etc).
                const QDateTime atJobStartTime = atJob->getNextPossibleStartTime(startSearchingtAt, SCHEDULE_RESOLUTION_MINUTES, currentJob
                                                 && (atJob == currentJob));
                if (atJobStartTime.isValid())
                {
                    // This difference between the user-specified start time, and the time it can really start.
                    const double startDelta = atJobStartTime.secsTo(atTime);
                    if (fabs(startDelta) < 20 * 60)
                    {
                        // If we're looking for a new job to start, then give the START_AT priority
                        // if it's within 10 minutes of its user-specified time.
                        // However, if we're evaluating the current job (called from checkJob() above)
                        // then only interrupt it if the START_AT job can start very soon.
                        const int gap = currentJob == nullptr ? MIN_RUN_SECS : 30;
                        if (nextStart.secsTo(atJobStartTime) <= gap)
                        {
                            nextJob = atJob;
                            nextStart = atJobStartTime;
                            if (nextInterruption) *nextInterruption = QDateTime(); // Not interrupting atJob
                        }
                        else if (nextInterruption)
                        {
                            // The START_AT job was not chosen to start now, but it's still possible
                            // that this atJob will be an interrupter.
                            if (!nextInterruption->isValid() ||
                                    atJobStartTime.secsTo(*nextInterruption) < 0)
                            {
                                *nextInterruption = atJobStartTime;
                                interruptStr = QString("interrupted by %1").arg(atJob->getName());
                            }
                        }
                    }
                }
            }
        }

        // If the selected next job is part of a group, then we may schedule other members of the group if
        // - the selected job is a repeating job and
        // - another group member is runnable now and
        // - that group mnember is behind the selected job's iteration.
        if (nextJob && !nextJob->getGroup().isEmpty() && Options::greedyScheduling() && nextJob->getCompletedIterations() > 0)
        {
            TEST_PRINT(stderr, "      Considering GROUPS (%d jobs) selected %s\n", jobs.size(), nextJob->getName().toLatin1().data());
            // Iterate through the jobs list, first finding the selected job, the looking at all jobs after that.
            bool foundSelectedJob = false;
            for (int i = 0; i < jobs.size(); ++i)
            {
                SchedulerJob * const job = jobs[i];
                if (job == nextJob)
                {
                    foundSelectedJob = true;
                    continue;
                }

                TEST_PRINT(stderr, "        Job %s (group %s) %s (%d vs %d iterations) %s\n",
                           job->getName().toLatin1().data(),  (job->getGroup() != nextJob->getGroup()) ? "Different" : "Same",
                           foundSelectedJob ? "Found" : "not found yet",
                           job->getCompletedIterations(), nextJob->getCompletedIterations(),
                           allowJob(job, rescheduleAbortsImmediate, rescheduleAbortsQueue, rescheduleErrors) ? "allowed" : "not allowed");
                // Only jobs with lower priority than nextJob--higher priority jobs already have been considered and rejected.
                // Only consider jobs in the same group as nextJob
                // Only consider jobs with fewer iterations than nextJob.
                // Only consider jobs that are allowed.
                if (!foundSelectedJob ||
                        (job->getGroup() != nextJob->getGroup()) ||
                        (job->getCompletedIterations() >= nextJob->getCompletedIterations()) ||
                        !allowJob(job, rescheduleAbortsImmediate, rescheduleAbortsQueue, rescheduleErrors))
                    continue;

                const bool evaluatingCurrentJob = (currentJob && (job == currentJob));

                // If the job state is abort or error, might have to delay the first possible start time.
                QDateTime startSearchingtAt = firstPossibleStart(
                                                  job, now, rescheduleAbortsQueue, abortDelaySeconds, rescheduleErrors, errorDelaySeconds);

                // Find the first time this job can meet all its constraints.
                const QDateTime startTime = job->getNextPossibleStartTime(startSearchingtAt, SCHEDULE_RESOLUTION_MINUTES,
                                            evaluatingCurrentJob);

                // Only consider jobs that can start soon.
                if (!startTime.isValid() || startTime.secsTo(nextStart) > MAX_INTERRUPT_SECS)
                    continue;

                // Don't interrupt a START_AT for higher priority job
                if (evaluatingCurrentJob && currentJobIsStartAt)
                {
                    if (nextInterruption) *nextInterruption = QDateTime();
                    nextStart = startTime;
                    nextJob = job;
                    interruptStr = "";
                }
                else if (startTime.secsTo(nextStart) >= -MAX_INTERRUPT_SECS)
                {
                    // Use this group member, keeping the old interruption variables.
                    nextStart = startTime;
                    nextJob = job;
                }
            }
        }
    }
    if (when != nullptr) *when = nextStart;
    if (interruptReason != nullptr) *interruptReason = interruptStr;

    // Needed so display says "Idle" for unscheduled jobs.
    // This will also happen in simulate, but that isn't called if nextJob is null.
    // Must test for !nextJob. setState() inside unsetEvaluation has a nasty side effect
    // of clearing the estimated time.
    if (!nextJob)
        unsetEvaluation(jobs);

    QElapsedTimer simTimer;
    simTimer.start();
    const int simDays = SIM_HOURS * 3600;
    if (simType != DONT_SIMULATE && nextJob != nullptr)
    {
        QDateTime simulationLimit = now.addSecs(simDays);
        schedule.clear();
        QDateTime simEnd = simulate(jobs, now, simulationLimit, capturedFramesCount, simType);

        // This covers the scheduler's "repeat after completion" option,
        // which only applies if rememberJobProgress is false.
        if (!Options::rememberJobProgress() && Options::schedulerRepeatEverything())
        {
            int repeats = 0, maxRepeats = 5;
            while (simEnd.isValid() && simEnd.secsTo(simulationLimit) > 0 && ++repeats < maxRepeats)
            {
                simEnd = simEnd.addSecs(60);
                simEnd = simulate(jobs, simEnd, simulationLimit, nullptr, simType);
            }
        }
        m_SimSeconds = simTimer.elapsed() / 1000.0;
        TEST_PRINT(stderr, "********************************* simulate(%s,%d) took %.3fs\n",
                   simType == SIMULATE ? "SIM" : "ONLY_1", SIM_HOURS, m_SimSeconds);
    }

    return nextJob;
}

// The only reason this isn't a const method is because it sets the schedule class variable
QDateTime GreedyScheduler::simulate(const QList<SchedulerJob *> &jobs, const QDateTime &time, const QDateTime &endTime,
                                    const QMap<QString, uint16_t> *capturedFramesCount, SimulationType simType)
{
    TEST_PRINT(stderr, "%d simulate()\n", __LINE__);
    // Make a deep copy of jobs
    QList<SchedulerJob *> copiedJobs;
    QList<SchedulerJob *> scheduledJobs;
    QDateTime simEndTime;

    foreach (SchedulerJob *job, jobs)
    {
        SchedulerJob *newJob = new SchedulerJob();
        // Make sure the copied class pointers aren't affected!
        *newJob = *job;
        // clear follower job lists to avoid links to existing jobs
        newJob->followerJobs().clear();
        newJob->clearSimulatedSchedule();
        copiedJobs.append(newJob);
        job->setStopTime(QDateTime());
    }

    // The number of jobs we have that can be scheduled,
    // and the number of them where a simulated start has been scheduled.
    int numStartupCandidates = 0, numStartups = 0;
    // Reset the start times.
    foreach (SchedulerJob *job, copiedJobs)
    {
        job->setStartupTime(QDateTime());
        const auto state = job->getState();
        if (state == SCHEDJOB_SCHEDULED || state == SCHEDJOB_EVALUATION ||
                state == SCHEDJOB_BUSY || state == SCHEDJOB_IDLE)
            numStartupCandidates++;
    }

    QMap<QString, uint16_t> capturedFramesCopy;
    if (capturedFramesCount != nullptr)
        capturedFramesCopy = *capturedFramesCount;
    QList<SchedulerJob *>simJobs = copiedJobs;
    prepareJobsForEvaluation(copiedJobs, time, capturedFramesCopy, nullptr, false);

    QDateTime simTime = time;
    int iterations = 0;
    bool exceededIterations = false;
    QHash<SchedulerJob*, int> workDone;
    QHash<SchedulerJob*, int> originalIteration, originalSecsLeftIteration;

    for(int i = 0; i < simJobs.size(); ++i)
        workDone[simJobs[i]] = 0.0;

    while (true)
    {
        QDateTime jobStartTime;
        QDateTime jobInterruptTime;
        QString interruptReason;
        // Find the next job to be scheduled, when it starts, and when a higher priority
        // job might preempt it, why it would be preempted.
        // Note: 4th arg, fullSchedule, must be false or we'd loop forever.
        SchedulerJob *selectedJob =
            selectNextJob(simJobs, simTime, nullptr, DONT_SIMULATE, &jobStartTime, &jobInterruptTime, &interruptReason);
        if (selectedJob == nullptr)
            break;

        TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("%1 starting at %2 interrupted at \"%3\" reason \"%4\"")
                   .arg(selectedJob->getName()).arg(jobStartTime.toString("MM/dd hh:mm"))
                   .arg(jobInterruptTime.toString("MM/dd hh:mm")).arg(interruptReason).toLatin1().data());
        // Are we past the end time?
        if (endTime.isValid() && jobStartTime.secsTo(endTime) < 0) break;

        // It's possible there are start_at jobs that can preempt this job.
        // Find the next start_at time, and use that as an end constraint to getNextEndTime
        // if it's before jobInterruptTime.
        QDateTime nextStartAtTime;
        foreach (SchedulerJob *job, simJobs)
        {
            if (job != selectedJob &&
                    job->getStartupCondition() == START_AT &&
                    jobStartTime.secsTo(job->getStartupTime()) > 0 &&
                    (job->getState() == SCHEDJOB_EVALUATION ||
                     job->getState() == SCHEDJOB_SCHEDULED))
            {
                QDateTime startAtTime = job->getStartupTime();
                if (!nextStartAtTime.isValid() || nextStartAtTime.secsTo(startAtTime) < 0)
                    nextStartAtTime = startAtTime;
            }
        }
        // Check to see if the above start-at stop time is before the interrupt stop time.
        QDateTime constraintStopTime = jobInterruptTime;
        if (nextStartAtTime.isValid() &&
                (!constraintStopTime.isValid() ||
                 nextStartAtTime.secsTo(constraintStopTime) < 0))
        {
            constraintStopTime = nextStartAtTime;
            TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  job will be interrupted by a START_AT job").toLatin1().data());
        }

        QString constraintReason;
        // Get the time that this next job would fail its constraints, and a human-readable explanation.
        QDateTime jobConstraintTime = selectedJob->getNextEndTime(jobStartTime, SCHEDULE_RESOLUTION_MINUTES, &constraintReason,
                                      constraintStopTime);
        if (nextStartAtTime.isValid() && jobConstraintTime.isValid() &&
                std::abs(jobConstraintTime.secsTo(nextStartAtTime)) < 2 * SCHEDULE_RESOLUTION_MINUTES)
            constraintReason = "interrupted by start-at job";
        TEST_PRINT(stderr, "%d   %s\n", __LINE__,     QString("  constraint \"%1\" reason \"%2\"")
                   .arg(jobConstraintTime.toString("MM/dd hh:mm")).arg(constraintReason).toLatin1().data());
        QDateTime jobCompletionTime;
        TEST_PRINT(stderr, "%d   %s\n", __LINE__,
                   QString("  estimated time = %1").arg(selectedJob->getEstimatedTime()).toLatin1().data());
        if (selectedJob->getEstimatedTime() > 0)
        {
            // Estimate when the job might complete, if it was allowed to run without interruption.
            const int timeLeft = selectedJob->getEstimatedTime() - workDone[selectedJob];
            jobCompletionTime = jobStartTime.addSecs(timeLeft);
            TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  completion \"%1\" time left %2s")
                       .arg(jobCompletionTime.toString("MM/dd hh:mm")).arg(timeLeft).toLatin1().data());
        }
        // Consider the 3 stopping times computed above (preemption, constraints missed, and completion),
        // see which comes soonest, and set the jobStopTime and jobStopReason.
        QDateTime jobStopTime = jobInterruptTime;
        QString stopReason = jobStopTime.isValid() ? interruptReason : "";
        if (jobConstraintTime.isValid() && (!jobStopTime.isValid() || jobStopTime.secsTo(jobConstraintTime) < 0))
        {
            stopReason = constraintReason;
            jobStopTime = jobConstraintTime;
            TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  picked constraint").toLatin1().data());
        }
        if (jobCompletionTime.isValid() && (!jobStopTime.isValid() || jobStopTime.secsTo(jobCompletionTime) < 0))
        {
            stopReason = "job completion";
            jobStopTime = jobCompletionTime;
            TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  picked completion").toLatin1().data());
        }

        // This if clause handles the simulation of scheduler repeat groups
        // which applies to scheduler jobs with repeat-style completion conditions.
        if (!selectedJob->getGroup().isEmpty() &&
                (selectedJob->getCompletionCondition() == FINISH_LOOP ||
                 selectedJob->getCompletionCondition() == FINISH_REPEAT ||
                 selectedJob->getCompletionCondition() == FINISH_AT))
        {
            if (originalIteration.find(selectedJob) == originalIteration.end())
                originalIteration[selectedJob] = selectedJob->getCompletedIterations();
            if (originalSecsLeftIteration.find(selectedJob) == originalSecsLeftIteration.end())
                originalSecsLeftIteration[selectedJob] = selectedJob->getEstimatedTimeLeftThisRepeat();

            // Estimate the time it would take to complete the current repeat, if this is a repeated job.
            int leftThisRepeat = selectedJob->getEstimatedTimeLeftThisRepeat();
            int secsPerRepeat = selectedJob->getEstimatedTimePerRepeat();
            int secsLeftThisRepeat = (workDone[selectedJob] < leftThisRepeat) ?
                                     leftThisRepeat - workDone[selectedJob] : secsPerRepeat;

            TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  sec per repeat %1 sec left this repeat %2")
                       .arg(secsPerRepeat).arg(secsLeftThisRepeat).toLatin1().data());

            if (workDone[selectedJob] == 0)
            {
                secsLeftThisRepeat += selectedJob->getEstimatedStartupTime();
                TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  adding %1 to secsLeftThisRepeat")
                           .arg(selectedJob->getEstimatedStartupTime()).arg(secsLeftThisRepeat).toLatin1().data());
            }

            // If it would finish a repeat, run one repeat and see if it would still be scheduled.
            if (secsLeftThisRepeat > 0 &&
                    (!jobStopTime.isValid() || secsLeftThisRepeat < jobStartTime.secsTo(jobStopTime)))
            {
                auto tempStart = jobStartTime;
                auto tempInterrupt = jobInterruptTime;
                auto tempReason = stopReason;
                SchedulerJob keepJob = *selectedJob;

                auto t = jobStartTime.addSecs(secsLeftThisRepeat);
                int iteration = selectedJob->getCompletedIterations();
                int iters = 0, maxIters = 20;  // just in case...
                while ((!jobStopTime.isValid() || t.secsTo(jobStopTime) > 0) && iters++ < maxIters)
                {
                    selectedJob->setCompletedIterations(++iteration);
                    TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("  iteration=%1").arg(iteration).toLatin1().data());
                    SchedulerJob *next = selectNextJob(simJobs, t, nullptr, DONT_SIMULATE, &tempStart, &tempInterrupt, &tempReason);
                    if (next != selectedJob)
                    {
                        stopReason = "interrupted for group member";
                        jobStopTime = t;
                        TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString(" switched to group member %1 at %2")
                                   .arg(next == nullptr ? "null" : next->getName()).arg(t.toString("MM/dd hh:mm")).toLatin1().data());

                        break;
                    }
                    t = t.addSecs(secsPerRepeat);
                }
                *selectedJob = keepJob;
            }
        }

        // Increment the work done, for the next time this job might be scheduled in this simulation.
        if (jobStopTime.isValid())
        {
            const int secondsRun =   jobStartTime.secsTo(jobStopTime);
            workDone[selectedJob] += secondsRun;

            if ((originalIteration.find(selectedJob) != originalIteration.end()) &&
                    (originalSecsLeftIteration.find(selectedJob) != originalSecsLeftIteration.end()))
            {
                int completedIterations = originalIteration[selectedJob];
                if (workDone[selectedJob] >= originalSecsLeftIteration[selectedJob] &&
                        selectedJob->getEstimatedTimePerRepeat() > 0)
                    completedIterations +=
                        1 + (workDone[selectedJob] - originalSecsLeftIteration[selectedJob]) / selectedJob->getEstimatedTimePerRepeat();
                TEST_PRINT(stderr, "%d   %s\n", __LINE__,
                           QString("  work sets interations=%1").arg(completedIterations).toLatin1().data());
                selectedJob->setCompletedIterations(completedIterations);
            }
        }

        // Set the job's startupTime, but only for the first time the job will be scheduled.
        // This will be used by the scheduler's UI when displaying the job schedules.
        if (!selectedJob->getStartupTime().isValid())
        {
            numStartups++;
            selectedJob->setStartupTime(jobStartTime);
            selectedJob->setStopTime(jobStopTime);
            selectedJob->setStopReason(stopReason);
            selectedJob->setState(SCHEDJOB_SCHEDULED);
            scheduledJobs.append(selectedJob);
            TEST_PRINT(stderr, "%d  %s\n", __LINE__, QString("  Scheduled: %1 %2 -> %3 %4 work done %5s")
                       .arg(selectedJob->getName()).arg(selectedJob->getStartupTime().toString("MM/dd hh:mm"))
                       .arg(selectedJob->getStopTime().toString("MM/dd hh:mm")).arg(selectedJob->getStopReason())
                       .arg(workDone[selectedJob]).toLatin1().data());
        }
        else
        {
            TEST_PRINT(stderr, "%d  %s\n", __LINE__, QString("  Added: %1 %2 -> %3 %4 work done %5s")
                       .arg(selectedJob->getName()).arg(jobStartTime.toString("MM/dd hh:mm"))
                       .arg(jobStopTime.toString("MM/dd hh:mm")).arg(stopReason)
                       .arg(workDone[selectedJob]).toLatin1().data());
        }

        // Compute if the simulated job should be considered complete because of work done.
        if (selectedJob->getEstimatedTime() >= 0 &&
                workDone[selectedJob] >= selectedJob->getEstimatedTime())
        {
            selectedJob->setState(SCHEDJOB_COMPLETE);
            TEST_PRINT(stderr, "%d  %s\n", __LINE__, QString("   job %1 is complete")
                       .arg(selectedJob->getName()).toLatin1().data());
        }
        selectedJob->appendSimulatedSchedule(JobSchedule(nullptr, jobStartTime, jobStopTime, stopReason));
        schedule.append(JobSchedule(jobs[copiedJobs.indexOf(selectedJob)], jobStartTime, jobStopTime, stopReason));
        simEndTime = jobStopTime;
        simTime = jobStopTime.addSecs(60);

        // End the simulation if we've crossed endTime, or no further jobs could be started,
        // or if we've simply run too long.
        if (!simTime.isValid()) break;
        if (endTime.isValid() && simTime.secsTo(endTime) < 0) break;

        if (++iterations > std::max(20, numStartupCandidates))
        {
            exceededIterations = true;
            TEST_PRINT(stderr, "%d  %s\n", __LINE__, QString("ending simulation after %1 iterations")
                       .arg(iterations).toLatin1().data());

            break;
        }
        if (simType == SIMULATE_EACH_JOB_ONCE)
        {
            bool allJobsProcessedOnce = true;
            for (const auto job : simJobs)
            {
                if (allowJob(job, rescheduleAbortsImmediate, rescheduleAbortsQueue, rescheduleErrors) &&
                        !job->getStartupTime().isValid())
                {
                    allJobsProcessedOnce = false;
                    break;
                }
            }
            if (allJobsProcessedOnce)
            {
                TEST_PRINT(stderr, "%d  ending simulation, all jobs processed once\n", __LINE__);
                break;
            }
        }
    }

    // This simulation has been run using a deep-copy of the jobs list, so as not to interfere with
    // some of their stored data. However, we do wish to update several fields of the "real" scheduleJobs.
    // Note that the original jobs list and "copiedJobs" should be in the same order..
    for (int i = 0; i < jobs.size(); ++i)
    {
        if (scheduledJobs.indexOf(copiedJobs[i]) >= 0)
        {
            // If this is a simulation where the job is already running, don't change its state or startup time.
            if (jobs[i]->getState() != SCHEDJOB_BUSY)
            {
                jobs[i]->setState(SCHEDJOB_SCHEDULED);
                jobs[i]->setStartupTime(copiedJobs[i]->getStartupTime());
            }
            // Can't set the standard completionTime as it affects getEstimatedTime()
            jobs[i]->setStopTime(copiedJobs[i]->getStopTime());
            jobs[i]->setStopReason(copiedJobs[i]->getStopReason());
            if (simType == SIMULATE)
                jobs[i]->setSimulatedSchedule(copiedJobs[i]->getSimulatedSchedule());
        }
    }
    // This should go after above loop. unsetEvaluation calls setState() which clears
    // certain fields from the state for IDLE states.
    unsetEvaluation(jobs);

    return exceededIterations ? QDateTime() : simEndTime;
}

void GreedyScheduler::unsetEvaluation(const QList<SchedulerJob *> &jobs) const
{
    for (int i = 0; i < jobs.size(); ++i)
    {
        if (jobs[i]->getState() == SCHEDJOB_EVALUATION)
            jobs[i]->setState(SCHEDJOB_IDLE);
    }
}

QString GreedyScheduler::jobScheduleString(const JobSchedule &jobSchedule)
{
    return QString("%1\t%2 --> %3 \t%4")
           .arg(jobSchedule.job->getName(), -10)
           .arg(jobSchedule.startTime.toString("MM/dd  hh:mm"),
                jobSchedule.stopTime.toString("hh:mm"), jobSchedule.stopReason);
}

void GreedyScheduler::printSchedule(const QList<JobSchedule> &schedule)
{
    foreach (auto &line, schedule)
    {
        fprintf(stderr, "%s\n", QString("%1 %2 --> %3 (%4)")
                .arg(jobScheduleString(line)).toLatin1().data());
    }
}

}  // namespace Ekos
