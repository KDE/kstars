/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "schedulertypes.h"
#include "ekos/auxiliary/modulelogger.h"
#include "dms.h"
#include "libindi/lilxml.h"

#include <QString>
#include <QUrl>

namespace Ekos {

class SchedulerJob;
class SequenceJob;

class SchedulerUtils
{
public:
    SchedulerUtils();


    /**
     * @brief createJob Create job from its XML representation
     */
    static SchedulerJob *createJob(XMLEle *root);

    /**
     * @brief setupJob Initialize a job with all fields accessible from the UI.
     */
    static void setupJob(SchedulerJob &job, const QString &name, const QString &group, const dms &ra, const dms &dec,
                         double djd, double rotation, const QUrl &sequenceUrl, const QUrl &fitsUrl, StartupCondition startup,
                         const QDateTime &startupTime, CompletionCondition completion, const QDateTime &completionTime, int completionRepeats,
                         double minimumAltitude, double minimumMoonSeparation, bool enforceWeather, bool enforceTwilight,
                         bool enforceArtificialHorizon, bool track, bool focus, bool align, bool guide);


    /**
     * @brief Fill the map signature -> frame count so that a single iteration of the scheduled job creates as many frames as possible
     *        in addition to the already captured ones, but does not the expected amount.
     * @param expected map signature -> expected frames count
     * @param capturedFramesCount map signature -> already captured frames count
     * @param schedJob scheduler job for which these calculations are done
     * @param capture_map map signature -> frame count that will be handed over to the capture module to control that a single iteration
     *        of the scheduler job creates as many frames as possible, but does not exceed the expected ones.
     * @param completedIterations How many times has the job completed its capture sequence (for repeated jobs).
     * @return total number of captured frames, truncated to the maximal number of frames the scheduler job could produce
     */
    static uint16_t fillCapturedFramesMap(const QMap<QString, uint16_t> &expected, const CapturedFramesMap &capturedFramesCount, SchedulerJob &schedJob,
                                          CapturedFramesMap &capture_map, int &completedIterations);


    /**
     * @brief Update the flag for the given job whether light frames are required
     * @param oneJob scheduler job where the flag should be updated
     * @param seqjobs list of capture sequences of the job
     * @param framesCount map capture signature -> frame count
     * @return true iff the job need to capture light frames
     */
    static void updateLightFramesRequired(SchedulerJob *oneJob, const QList<SequenceJob *> &seqjobs, const CapturedFramesMap &framesCount);

    /**
     * @brief processJobInfo a utility used by loadSequenceQueue() to help it read a capture sequence file
     * @param root the filename
     * @param schedJob the SchedulerJob is modified accoring to the contents of the sequence queue
     * @return a capture sequence
     */
    static SequenceJob *processSequenceJobInfo(XMLEle *root, SchedulerJob *schedJob);

    /**
         * @brief loadSequenceQueue Loads what's necessary to estimate job completion time from a capture sequence queue file
         * @param fileURL the filename
         * @param schedJob the SchedulerJob is modified according to the contents of the sequence queue
         * @param jobs the returned values read from the file
         * @param hasAutoFocus a return value indicating whether autofocus can be triggered by the sequence.
         * @param logger module logging utility
         */

    static bool loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs, bool &hasAutoFocus, ModuleLogger *logger);

    /**
         * @brief estimateJobTime Estimates the time the job takes to complete based on the sequence file and what modules to utilize during the observation run.
         * @param job target job
         * @param capturedFramesCount a map of what's been captured already
         * @param logger module logging utility
         */
    static bool estimateJobTime(SchedulerJob *schedJob, const QMap<QString, uint16_t> &capturedFramesCount, ModuleLogger *logger);

    /**
     * @brief timeHeuristics Estimates the number of seconds of overhead above and beyond imaging time, used by estimateJobTime.
     * @param schedJob the scheduler job.
     * @return seconds of overhead.
     */
    static int timeHeuristics(const SchedulerJob *schedJob);

    /**
     * @brief Calculate the map signature -> expected number of captures from the given list of capture sequence jobs,
     *        i.e. the expected number of captures from a single scheduler job run.
     * @param seqJobs list of capture sequence jobs
     * @param expected map to be filled
     * @return total expected number of captured frames of a single run of all jobs
     */
    static uint16_t calculateExpectedCapturesMap(const QList<SequenceJob *> &seqJobs, QMap<QString, uint16_t> &expected);
};


} // namespace
