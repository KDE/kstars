/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "schedulertypes.h"
#include "ekos/capture/capturetypes.h"
#include "ekos/auxiliary/modulelogger.h"
#include "dms.h"
#include <lilxml.h>

#include <QString>
#include <QUrl>
#include <QDateTime>

class SkyPoint;
class GeoLocation;

namespace Ekos {

class SchedulerJob;
class SequenceJob;

class SchedulerUtils
{
public:
    SchedulerUtils();


    /**
     * @brief createJob Create job from its XML representation
     * @param root XML description of the job to be created
     * @param leadJob candidate for lead (only necessary if the new job's type is follower)
     */
    static SchedulerJob *createJob(XMLEle *root, SchedulerJob *leadJob);

    /**
     * @brief setupJob Initialize a job with all fields accessible from the UI.
     */
    static void setupJob(SchedulerJob &job, const QString &name, bool isLead, const QString &group, const QString &train, const dms &ra, const dms &dec,
                         double djd, double rotation, const QUrl &sequenceUrl, const QUrl &fitsUrl, StartupCondition startup,
                         const QDateTime &startupTime, CompletionCondition completion, const QDateTime &completionTime, int completionRepeats,
                         double minimumAltitude, double minimumMoonSeparation, double maxMoonAltitude, bool enforceWeather, bool enforceTwilight,
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
    static uint16_t fillCapturedFramesMap(const CapturedFramesMap &expected, const CapturedFramesMap &capturedFramesCount, SchedulerJob &schedJob,
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
         * @return true if the time could be estimated, false if the corresponding sequence file is invalid
         */
    static bool estimateJobTime(SchedulerJob *schedJob, const CapturedFramesMap &capturedFramesCount, ModuleLogger *logger);

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
    static uint16_t calculateExpectedCapturesMap(const QList<SequenceJob *> &seqJobs, CapturedFramesMap &expected);

    /**
         * @brief findAltitude Find altitude given a specific time
         * @param target Target
         * @param when date time to find altitude
         * @param is_setting whether target is setting at the argument time (optional).
         * @param debug outputs calculation to log file (optional).
         * @return Altitude of the target at the specific date and time given.
         * @warning This function uses the current KStars geolocation.
         */
    static double findAltitude(const SkyPoint &target, const QDateTime &when, bool *is_setting = nullptr, GeoLocation *geo = nullptr, bool debug = false);

    /**
     * @brief create a new list with only the master jobs from the input
     */
    static QList<SchedulerJob *> filterLeadJobs(const QList<SchedulerJob *> &jobs);
};


} // namespace
