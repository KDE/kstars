/*  Ekos Scheduler Greedy Algorithm
    SPDX-FileCopyrightText: Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "greedyscheduler.h"

#include <ekos_scheduler_debug.h>

#include "Options.h"
#include "scheduler.h"
#include "ekos/ekos.h"
#include "ui_scheduler.h"

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

QList<SchedulerJob *> GreedyScheduler::scheduleJobs(const QList<SchedulerJob *> &jobs,
        const QDateTime &now,
        const QMap<QString, uint16_t> &capturedFramesCount,
        Scheduler *scheduler)
{
    for (auto job : jobs)
        job->clearCache();

    SchedulerJob::enableGraphicsUpdates(false);
    QDateTime when;
    QElapsedTimer timer;
    timer.start();
    scheduledJob = nullptr;
    schedule.clear();

    QList<SchedulerJob *> sortedJobs =
        prepareJobsForEvaluation(jobs, now, capturedFramesCount, scheduler);

    scheduledJob = selectNextJob(sortedJobs, now, nullptr, true, &when, nullptr, nullptr, &capturedFramesCount);
    auto schedule = getSchedule();
    if (scheduler != nullptr)
    {
        if (!schedule.empty())
        {
            // Print in reverse order ?! The log window at the bottom of the screen
            // prints "upside down" -- most recent on top -- and I believe that view
            // is more important than the log file (where we can invert when debugging).
            for (int i = schedule.size() - 1; i >= 0; i--)
                scheduler->appendLogText(GreedyScheduler::jobScheduleString(schedule[i]));
            scheduler->appendLogText(QString("Greedy Scheduler plan for the next 48 hours starting %1 (%2)s:")
                                     .arg(now.toString()).arg(timer.elapsed() / 1000.0));
        }
        else scheduler->appendLogText(QString("Greedy Scheduler: empty plan (%1s)").arg(timer.elapsed() / 1000.0));
    }
    if (scheduledJob != nullptr)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER)
                << QString("Greedy Scheduler scheduling next job %1 at %2")
                .arg(scheduledJob->getName(), when.toString("hh:mm"));
        scheduledJob->setState(SchedulerJob::JOB_SCHEDULED);
        scheduledJob->setStartupTime(when);
        foreach (auto job, sortedJobs)
            job->updateJobCells();
    }
    // The graphics would get updated many times during scheduling, which can
    // cause significant cpu usage. No need for that, so we turn off updates
    // at the start of this method, and then update all jobs once here.
    SchedulerJob::enableGraphicsUpdates(true);
    for (auto job : sortedJobs)
    {
        job->updateJobCells();
        job->clearCache();
    }

    return sortedJobs;
}

bool GreedyScheduler::checkJob(const QList<SchedulerJob *> &jobs,
                               const QDateTime &now,
                               SchedulerJob *currentJob)
{
    // Don't interrupt a job that just started.
    if (currentJob && currentJob->getStateTime().secsTo(now) < 5)
        return true;

    QDateTime startTime;
    SchedulerJob *next = selectNextJob(jobs, now, currentJob, false, &startTime);
    if (next == currentJob && now.secsTo(startTime) <= 1)
    {
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

QList<SchedulerJob *> GreedyScheduler::prepareJobsForEvaluation(
    const QList<SchedulerJob *> &jobs, const QDateTime &now,
    const QMap<QString, uint16_t> &capturedFramesCount, Scheduler *scheduler, bool reestimateJobTimes)
{
    QList<SchedulerJob *> sortedJobs = jobs;
    // Remove some finished jobs from eval.
    foreach (SchedulerJob *job, sortedJobs)
    {
        switch (job->getCompletionCondition())
        {
            case SchedulerJob::FINISH_AT:
                /* If planned finishing time has passed, the job is set to IDLE waiting for a next chance to run */
                if (job->getCompletionTime().isValid() && job->getCompletionTime() < now)
                {
                    job->setState(SchedulerJob::JOB_COMPLETE);
                    continue;
                }
                break;

            case SchedulerJob::FINISH_REPEAT:
                // In case of a repeating jobs, let's make sure we have more runs left to go
                // If we don't, re-estimate imaging time for the scheduler job before concluding
                if (job->getRepeatsRemaining() == 0)
                {
                    if (scheduler != nullptr) scheduler->appendLogText(i18n("Job '%1' has no more batches remaining.", job->getName()));
                    job->setState(SchedulerJob::JOB_COMPLETE);
                    job->setEstimatedTime(0);
                    continue;
                }
                break;

            default:
                break;
        }
    }

    // Change the state to eval or ERROR/ABORTED for all jobs that will be evaluated.
    foreach (SchedulerJob *job, sortedJobs)
    {
        switch (job->getState())
        {
            case SchedulerJob::JOB_INVALID:
            case SchedulerJob::JOB_COMPLETE:
                // If job is invalid or complete, bypass evaluation.
                break;

            case SchedulerJob::JOB_ERROR:
            case SchedulerJob::JOB_ABORTED:
                // These will be evaluated, but we'll have a delay to start.
                break;
            case SchedulerJob::JOB_IDLE:
            case SchedulerJob::JOB_BUSY:
            case SchedulerJob::JOB_SCHEDULED:
            case SchedulerJob::JOB_EVALUATION:
            default:
                job->setState(SchedulerJob::JOB_EVALUATION);
                break;
        }
    }

    // Estimate the job times
    foreach (SchedulerJob *job, sortedJobs)
    {
        if (job->getState() == SchedulerJob::JOB_INVALID || job->getState() == SchedulerJob::JOB_COMPLETE)
            continue;

        // -1 = Job is not estimated yet
        // -2 = Job is estimated but time is unknown
        // > 0  Job is estimated and time is known
        if (reestimateJobTimes)
        {
            job->setEstimatedTime(-1);
            if (Scheduler::estimateJobTime(job, capturedFramesCount, scheduler) == false)
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
    }

    return sortedJobs;
}

namespace
{
// Don't Allow INVALID or COMPLETE jobs to be scheduled.
// Allow ABORTED if one of the rescheduleAbort... options are true.
// Allow ERROR if rescheduleErrors is true.
bool allowJob(SchedulerJob *job, bool rescheduleAbortsImmediate, bool rescheduleAbortsQueue, bool rescheduleErrors)
{
    if (job->getState() == SchedulerJob::JOB_INVALID || job->getState() == SchedulerJob::JOB_COMPLETE)
        return false;
    if (job->getState() == SchedulerJob::JOB_ABORTED && !rescheduleAbortsImmediate && !rescheduleAbortsQueue)
        return false;
    if (job->getState() == SchedulerJob::JOB_ERROR && !rescheduleErrors)
        return false;
    return true;
}

// Returns the first possible time a job may be scheduled. That is, it doesn't
// evaluate the job, but rather just computes the needed delay (for ABORT and ERROR jobs)
// or returns now for other jobs.
QDateTime firstPossibleStart(SchedulerJob *job, const QDateTime &now,
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
SchedulerJob *GreedyScheduler::selectNextJob(const QList<SchedulerJob *> &jobs, const QDateTime &now,
        SchedulerJob *currentJob, bool fullSchedule, QDateTime *when,
        QDateTime *nextInterruption, QString *interruptReason,
        const QMap<QString, uint16_t> *capturedFramesCount)
{
    // Don't schedule a job that will be preempted in less than MIN_RUN_SECS.
    constexpr int MIN_RUN_SECS = 10 * 60;

    // Don't preempt a job for another job that is more than MAX_INTERRUPT_SECS in the future.
    constexpr int MAX_INTERRUPT_SECS = 30;

    // Don't interrupt START_AT jobs unless they can no longer run, or they're interrupted by another START_AT.
    bool currentJobIsStartAt = (currentJob && currentJob->getFileStartupCondition() == SchedulerJob::START_AT &&
                                currentJob->getFileStartupTime().isValid());
    QDateTime nextStart;
    SchedulerJob *nextJob = nullptr;
    QString interruptStr;

    for (int i = 0; i < jobs.size(); ++i)
    {
        SchedulerJob *job = jobs[i];
        const bool evaluatingCurrentJob = (currentJob && (job == currentJob));

        if (!allowJob(job, rescheduleAbortsImmediate, rescheduleAbortsQueue, rescheduleErrors))
            continue;

        // If the job state is abort or error, might have to delay the first possible start time.
        QDateTime startSearchingtAt = firstPossibleStart(
                                          job, now, rescheduleAbortsQueue, abortDelaySeconds, rescheduleErrors, errorDelaySeconds);

        // Find the first time this job can meet all its constraints.
        // I found that passing in an "until" 4th argument actually hurt performance, as it reduces
        // the effectiveness of the cache that getNextPossibleStartTime uses.
        const QDateTime startTime = job->getNextPossibleStartTime(startSearchingtAt, SCHEDULE_RESOLUTION_MINUTES,
                                    evaluatingCurrentJob);
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
            else
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
            SchedulerJob *atJob = jobs[i];
            const QDateTime atTime = atJob->getFileStartupTime();
            if (atJob->getFileStartupCondition() == SchedulerJob::START_AT && atTime.isValid())
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
                    if (fabs(startDelta) < 10 * 60)
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
        if (nextJob && !nextJob->getGroup().isEmpty() && nextJob->getCompletedIterations() > 0)
        {
            // Iterate through the jobs list, first finding the selected job, the looking at all jobs after that.
            bool foundSelectedJob = false;
            for (int i = 0; i < jobs.size(); ++i)
            {
                SchedulerJob *job = jobs[i];
                if (job == nextJob)
                {
                    foundSelectedJob = true;
                    continue;
                }

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

    constexpr int twoDays = 48 * 3600;
    if (fullSchedule && nextJob != nullptr)
    {
        QDateTime simulationLimit = now.addSecs(twoDays);
        schedule.clear();
        QDateTime simEnd = simulate(jobs, now, simulationLimit, capturedFramesCount);

        // This covers the scheduler's "repeat after completion" option,
        // which only applies if rememberJobProgress is false.
        if (!Options::rememberJobProgress() && Options::schedulerRepeatSequences())
        {
            int repeats = 0, maxRepeats = 5;
            while (simEnd.isValid() && simEnd.secsTo(simulationLimit) > 0 && ++repeats < maxRepeats)
            {
                simEnd = simEnd.addSecs(60);
                simEnd = simulate(jobs, simEnd, simulationLimit, nullptr);
            }
        }
    }

    return nextJob;
}

QDateTime GreedyScheduler::simulate(const QList<SchedulerJob *> &jobs, const QDateTime &time, const QDateTime &endTime,
                                    const QMap<QString, uint16_t> *capturedFramesCount)
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
        // Don't want to affect the UI
        newJob->setStatusCell(nullptr);
        newJob->setStartupCell(nullptr);
        copiedJobs.append(newJob);
        job->setGreedyCompletionTime(QDateTime());
    }

    // The number of jobs we have that can be scheduled,
    // and the number of them where a simulated start has been scheduled.
    int numStartupCandidates = 0, numStartups = 0;
    // Reset the start times.
    foreach (SchedulerJob *job, copiedJobs)
    {
        job->setStartupTime(QDateTime());
        const auto state = job->getState();
        if (state == SchedulerJob::JOB_SCHEDULED || state == SchedulerJob::JOB_EVALUATION ||
                state == SchedulerJob::JOB_BUSY || state == SchedulerJob::JOB_IDLE)
            numStartupCandidates++;
    }

    QMap<QString, uint16_t> capturedFramesCopy;
    if (capturedFramesCount != nullptr)
        capturedFramesCopy = *capturedFramesCount;
    QList<SchedulerJob *>simJobs =
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
        SchedulerJob *selectedJob = selectNextJob(
                                        simJobs, simTime, nullptr, false, &jobStartTime, &jobInterruptTime, &interruptReason);
        if (selectedJob == nullptr)
            break;

        TEST_PRINT(stderr, "%d   %s\n", __LINE__, QString("%1 starting at %2 interrupted at \"%3\" reason \"%4\"")
                   .arg(selectedJob->getName()).arg(jobStartTime.toString("MM/dd hh:mm"))
                   .arg(jobInterruptTime.toString("MM/dd hh:mm")).arg(interruptReason).toLatin1().data());
        // Are we past the end time?
        if (endTime.isValid() && jobStartTime.secsTo(endTime) < 0) break;

        QString constraintReason;
        // Get the time that this next job would fail its constraints, and a human-readable explanation.
        QDateTime jobConstraintTime = selectedJob->getNextEndTime(jobStartTime, SCHEDULE_RESOLUTION_MINUTES, &constraintReason,
                                      jobInterruptTime);
        TEST_PRINT(stderr, "%d   %s\n", __LINE__,     QString("  constraint \"%1\" reason \"%2\"")
                   .arg(jobConstraintTime.toString("MM/dd hh:mm")).arg(constraintReason).toLatin1().data());
        QDateTime jobCompletionTime;
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
                (selectedJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP ||
                 selectedJob->getCompletionCondition() == SchedulerJob::FINISH_REPEAT ||
                 selectedJob->getCompletionCondition() == SchedulerJob::FINISH_AT))
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

            if (workDone[selectedJob] == 0)
                secsLeftThisRepeat += selectedJob->getEstimatedStartupTime();

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
                    SchedulerJob *next = selectNextJob(simJobs, t, nullptr, false, &tempStart, &tempInterrupt, &tempReason);
                    if (next != selectedJob)
                    {
                        stopReason = "Interrupted for group member";
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
            selectedJob->setGreedyCompletionTime(jobStopTime);
            selectedJob->setStopReason(stopReason);
            selectedJob->setState(SchedulerJob::JOB_SCHEDULED);
            scheduledJobs.append(selectedJob);
            TEST_PRINT(stderr, "%d  %s\n", __LINE__, QString("  Scheduled: %1 %2 -> %3 %4 work done %5s")
                       .arg(selectedJob->getName()).arg(selectedJob->getStartupTime().toString("MM/dd hh:mm"))
                       .arg(selectedJob->getGreedyCompletionTime().toString("MM/dd hh:mm")).arg(selectedJob->getStopReason())
                       .arg(workDone[selectedJob]).toLatin1().data());
        }

        // Compute if the simulated job should be considered complete because of work done.
        if (selectedJob->getEstimatedTime() >= 0 &&
                workDone[selectedJob] >= selectedJob->getEstimatedTime())
        {
            selectedJob->setState(SchedulerJob::JOB_COMPLETE);
            TEST_PRINT(stderr, "%d  %s\n", __LINE__, QString("   job %1 is complete")
                       .arg(selectedJob->getName()).toLatin1().data());
        }
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
    }

    // This simulation has been run using a deep-copy of the jobs list, so as not to interfere with
    // some of their stored data. However, we do wish to update several fields of the "real" scheduleJobs.
    // Note that the original jobs list and "copiedJobs" should be in the same order..
    for (int i = 0; i < jobs.size(); ++i)
    {
        if (scheduledJobs.indexOf(copiedJobs[i]) >= 0)
        {
            jobs[i]->setState(SchedulerJob::JOB_SCHEDULED);
            jobs[i]->setStartupTime(copiedJobs[i]->getStartupTime());
            // Can't set the standard completionTime as it affects getEstimatedTime()
            jobs[i]->setGreedyCompletionTime(copiedJobs[i]->getGreedyCompletionTime());
            jobs[i]->setStopReason(copiedJobs[i]->getStopReason());
        }
    }
    // This should go after above loop. unsetEvaluation calls setState() which clears
    // certain fields from the state for IDLE states.
    unsetEvaluation(jobs);

    return exceededIterations ? QDateTime() : simEndTime;
}

void GreedyScheduler::unsetEvaluation(const QList<SchedulerJob *> &jobs)
{
    for (int i = 0; i < jobs.size(); ++i)
    {
        if (jobs[i]->getState() == SchedulerJob::JOB_EVALUATION)
            jobs[i]->setState(SchedulerJob::JOB_IDLE);
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
