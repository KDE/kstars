/*
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "captureutils.h"
#include "sequencejob.h"
#include "sequencequeue.h"

#include "ksnotification.h"
#include "ekos_capture_debug.h"

namespace Ekos
{

CapturedFramesMap CaptureUtils::calculateExpectedCapturesMap(QSharedPointer<SequenceQueue> sequenceQueue)
{
    CapturedFramesMap expected;
    const int repeats = sequenceQueue->repeatsTarget();
    for (auto &seqJob : sequenceQueue->allJobs())
    {
        const QString signature = seqJob->getCoreProperty(SequenceJob::SJ_Signature).toString();
        expected[signature] += static_cast<uint16_t>(seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt() * repeats);
    }
    return expected;
}

void CaptureUtils::updateCompletedFramesCount(QSharedPointer<SequenceQueue> sequenceQueue, bool force,
        const QString &signature)
{
    // determine the set of signatures to avoid double updates
    QSet<QString> signatures;
    if (signature.isEmpty())
    {
        for (SequenceJob *oneSeqJob : sequenceQueue->allJobs())
        {
            /* Only consider captures stored on client (Ekos) side */
            /* FIXME: ask the remote for the file count */
            if (oneSeqJob->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
                continue;

            signatures.insert(oneSeqJob->getSignature());
        }
    }
    else
        signatures.insert(signature);

    // map signature --> job list
    // we need this to distribute the completed frames to all those jobs in the sequence with the same signature
    QMap<QString, QList<SequenceJob *>> signature2jobMap;
    for (SequenceJob *oneSeqJob : sequenceQueue->allJobs())
        signature2jobMap[oneSeqJob->getSignature()].append(oneSeqJob);

    int totalCompletedRuns = -1;

    bool rememberJobProgress = Options::rememberJobProgress();
    bool iterationCompleted = true;

    if (rememberJobProgress)
    {
        /* Iterate over all signatures and update the captured frames counts. */
        /* FIXME: this signature path is incoherent when there is no filter wheel on the setup - bugfix should be elsewhere though */
        for (auto oneSignature : signatures)
        {
            const int completed = PlaceholderPath::getCompletedFiles(oneSignature);
            // If signature was processed during an earlier run, do nothing
            if (force || !sequenceQueue->capturedFramesMap()->contains(oneSignature))
                sequenceQueue->capturedFramesMap()->insert(oneSignature, completed);

            // update the total capture counts of the jobs with the current signature and distribute them equally among them.
            const int jobsCountWithSignature = signature2jobMap[oneSignature].count();
            // simple case - only one job with this signature
            if (jobsCountWithSignature <= 1)
            {
                // simply use the completed files number
                auto job = signature2jobMap[oneSignature].first();
                job->setTotalCompleted(completed);
                // update the number of completed iterations by searching for the minimum over all jobs
                const int singleRunCount = job->getCoreProperty(SequenceJob::SJ_Count).toInt();
                const int completedRuns =  singleRunCount > 0 ? (completed / singleRunCount) : 0;
                // check if the iteration is completed
                iterationCompleted &= (completed % singleRunCount == 0);
                // update the number of completed iterations by searching for the minimum over all jobs
                // for the first job simply take its value
                totalCompletedRuns = totalCompletedRuns < 0 ? completedRuns : std::min(totalCompletedRuns, completedRuns);
            }
            else
            {
                // complex case: more than one job with the same signature
                // first determine how much frames equal to one full sequence run
                auto sameSignatureJobs = signature2jobMap[oneSignature];
                int singleRunCount = 0;
                for (auto job : sameSignatureJobs)
                    singleRunCount += job->getCoreProperty(SequenceJob::SJ_Count).toInt();

                // now distribute the completed frames to the jobs with the same signature
                const int completedRuns = singleRunCount > 0 ? (completed / singleRunCount) : 0;
                int rest = singleRunCount > 0 ? (completed % singleRunCount) : completed;

                // update the number of completed iterations by searching for the minimum over all jobs
                // for the first job simply take its value
                totalCompletedRuns = totalCompletedRuns < 0 ? completedRuns : std::min(totalCompletedRuns, completedRuns);

                for (auto job : sameSignatureJobs)
                {
                    const int count = job->getCoreProperty(SequenceJob::SJ_Count).toInt();
                    // counts per run x completed runs
                    int framesCount = count * completedRuns;
                    if (rest > 0)
                    {
                        // consume the rest, from top to down
                        framesCount += std::min(rest, count);
                        rest = std::max(0, rest - count);
                    }
                    job->setTotalCompleted(framesCount);
                    iterationCompleted &= (framesCount % count == 0);
                }
            }
        }
    }
    else
    {
        foreach(auto job, sequenceQueue->allJobs())
        {
            const int count = job->getCoreProperty(SequenceJob::SJ_Count).toInt();
            const int completedRuns = count > 0 ? job->totalCompleted() / count : 0;
            // update the number of completed iterations by searching for the minimum over all jobs
            // for the first job simply take its value
            totalCompletedRuns = totalCompletedRuns < 0 ? completedRuns : std::min(totalCompletedRuns, completedRuns);
            iterationCompleted &= (job->getCompleted() % count == 0);
        }
    }

    // determine the iteration that is currently running and count up if all single sequences have reached their count target
    const int currentIteration = iterationCompleted ? std::min(totalCompletedRuns + 1,
                                 sequenceQueue->repeatsTarget()) : totalCompletedRuns;

    // update the completed iterations
    sequenceQueue->setRepeatsCompleted(totalCompletedRuns);

    // update the completed counters
    foreach (auto job, sequenceQueue->allJobs())
    {
        const int count = job->getCoreProperty(SequenceJob::SJ_Count).toInt();
        const int jobruns = job->totalCompleted() / count;
        if (!sequenceQueue->loopSequence() && jobruns > 0 &&
                (jobruns > currentIteration || jobruns >= sequenceQueue->repeatsTarget()))
        {
            job->setCompleted(job->getCoreProperty(SequenceJob::SJ_Count).toInt());
            if (job->getStatus() != JOB_BUSY)
                job->done();
        }
        else
        {
            job->setCompleted(job->totalCompleted() % job->getCoreProperty(SequenceJob::SJ_Count).toInt());
            if (job->getStatus() != JOB_BUSY)
                job->resetStatus(JOB_IDLE);
        }
    }
}

int CaptureUtils::estimateRemainingImagingTime(QSharedPointer<SequenceQueue> sequenceQueue)
{
    if (sequenceQueue->loopSequence())
        return CAPTURE_REMAINING_INFINITE;

    /* This is the map of captured frames for this scheduler job, keyed per storage signature.
     * It will be forwarded to the Capture module in order to capture only what frames are required.
     * If option "Remember Job Progress" is disabled, this map will be empty, and the Capture module will process all requested captures unconditionally.
     */
    CapturedFramesMap capture_map;
    bool const rememberJobProgress = Options::rememberJobProgress();

    double totalImagingTime = 0;

    // Determine number of all captures in the scheduler job
    CapturedFramesMap expected = calculateExpectedCapturesMap(sequenceQueue);

    // Determine the map signature --> exposure time
    QMap<QString, double> exposureTimesMap = sequenceQueue->exposureTimesMap();

    // Determine the number of already captured frames. If the option "Remember Job Progress" is selected, we
    // take the captured frames map from the state machine. Otherwise, we use the captured frames values from all sequence jobs
    CapturedFramesMap capturedFrames;
    if (!rememberJobProgress)
    {
        foreach (SequenceJob *seqJob, sequenceQueue->allJobs())
        {
            const QString sig = seqJob->getSignature();
            capturedFrames[sig] = seqJob->getCompleted() + (capturedFrames.contains(sig) ? capturedFrames[sig] : 0);
        }
    }
    // Calculate the total imaging time taking either the captured frames map or the captured frames counts
    foreach (auto signature, expected.keys())
    {
        if (rememberJobProgress)
            totalImagingTime += std::max(0, (expected[signature] - sequenceQueue->capturedFramesMap()->value(signature))) *
                                exposureTimesMap[signature];
        else
            totalImagingTime += std::max(0, (expected[signature] - capturedFrames[signature])) * exposureTimesMap[signature];
    }

    // TODO: consider wait times etc.

    return totalImagingTime;
}

} // namespace
