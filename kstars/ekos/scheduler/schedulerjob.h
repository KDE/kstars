/*  Ekos Scheduler Job
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"
#include "schedulertypes.h"
#include "ekos/capture/sequencejob.h"

#include <QUrl>
#include <QMap>
#include "kstarsdatetime.h"
#include <QJsonObject>

class ArtificialHorizon;
class KSMoon;
class TestSchedulerUnit;
class TestEkosSchedulerOps;
class dms;

namespace Ekos
{
class SchedulerJob
{
    public:
        SchedulerJob();

        QJsonObject toJson() const;

        /** @brief Actions that may be processed when running a SchedulerJob.
         * FIXME: StepPipeLine is actually a mask, change this into a bitfield.
         */
        typedef enum
        {
            USE_NONE  = 0,
            USE_TRACK = 1 << 0,
            USE_FOCUS = 1 << 1,
            USE_ALIGN = 1 << 2,
            USE_GUIDE = 1 << 3
        } StepPipeline;

        struct JobProgress
        {
            int numCompleted = 0;
            CCDFrameType type;
            bool isDarkFlat = false;
            QMap<SequenceJob::PropertyID, QVariant> properties;
            JobProgress() {}
            JobProgress(int numCompleted, const QSharedPointer<SequenceJob> &job)
                : numCompleted(numCompleted), type(job->getFrameType()),
                  isDarkFlat(job->jobType() == SequenceJob::JOBTYPE_DARKFLAT),
                  properties(job->getCoreProperties()) {};
        };

        /** @brief Coordinates of the target of this job. */
        /** @{ */
        SkyPoint const &getTargetCoords() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getTargetCoords();
            else
                return targetCoords;
        }
        void setTargetCoords(const dms &ra, const dms &dec, double djd);
        /** @} */

        double getPositionAngle()
        {
            return m_PositionAngle;
        }
        void setPositionAngle(double value);

        /** @brief Capture sequence this job uses while running. */
        /** @{ */
        QUrl getSequenceFile() const
        {
            return sequenceFile;
        }
        void setSequenceFile(const QUrl &value);
        /** @} */

        /** @brief FITS file whose plate solve produces target coordinates. */
        /** @{ */
        QUrl getFITSFile() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getFITSFile();
            else
                return fitsFile;
        }
        void setFITSFile(const QUrl &value);
        /** @} */

        /** @brief Minimal target altitude to process this job */
        /** @{ */
        double getMinAltitude() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getMinAltitude();
            else
                return minAltitude;
        }
        void setMinAltitude(const double &value);
        /** @} */

        /** @brief Does this job have a min-altitude parameter. */
        /** @{ */
        bool hasMinAltitude() const
        {
            return UNDEFINED_ALTITUDE < getMinAltitude();
        }
        static constexpr int UNDEFINED_ALTITUDE = -90;
        /** @} */

        /** @brief Does this job have any altitude constraints. */
        /** @{ */
        bool hasAltitudeConstraint() const;
        /** @} */

        /** @brief Minimal Moon separation to process this job. */
        /** @{ */
        double getMinMoonSeparation() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getMinMoonSeparation();
            else
                return minMoonSeparation;
        }
        void setMinMoonSeparation(const double &value);
        /** @} */

        /** @brief Maximal moon altitude to process this job. */
        /** @{ */
        double getMaxMoonAltitude() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getMaxMoonAltitude();
            else
                return maxMoonAltitude;
        }
        void setMaxMoonAltitude(const double &value);
        /** @} */

        /** @brief Whether to restrict this job to good weather. */
        /** @{ */
        bool getEnforceWeather() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getEnforceWeather();
            else
                return enforceWeather;
        }
        void setEnforceWeather(bool value);
        /** @} */

        /** @brief Mask of actions to process for this job. */
        /** @{ */
        StepPipeline getStepPipeline() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getStepPipeline();
            else
                return stepPipeline;
        }
        void setStepPipeline(const StepPipeline &value);
        /** @} */

        /** @brief Condition under which this job starts. */
        /** @{ */
        StartupCondition getStartupCondition() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getStartupCondition();
            else
                return startupCondition;
        }
        void setStartupCondition(const StartupCondition &value);
        /** @} */

        /** @brief Condition under which this job completes. */
        /** @{ */
        CompletionCondition getCompletionCondition() const
        {
            return completionCondition;
        }
        void setCompletionCondition(const CompletionCondition &value);
        /** @} */

        /** @brief Original startup condition, as entered by the user. */
        /** @{ */
        StartupCondition getFileStartupCondition() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getFileStartupCondition();
            else
                return fileStartupCondition;
        }
        void setFileStartupCondition(const StartupCondition &value);
        /** @} */

        /** @brief Original time at which the job must start, as entered by the user. */
        /** @{ */
        QDateTime getStartAtTime() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getStartAtTime();
            else
                return startAtTime;
        }
        void setStartAtTime(const QDateTime &value);
        /** @} */

        /** @brief Whether this job requires re-focus while running its capture sequence. */
        /** @{ */
        bool getInSequenceFocus() const
        {
            return inSequenceFocus;
        }
        void setInSequenceFocus(bool value);
        /** @} */

        /** @brief Whether to restrict job to night time. */
        /** @{ */
        bool getEnforceTwilight() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getEnforceTwilight();
            else
                return enforceTwilight;
        }
        void setEnforceTwilight(bool value);
        /** @} */

        /** @brief Whether to restrict job to night time. */
        /** @{ */
        bool getEnforceArtificialHorizon() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getEnforceArtificialHorizon();
            else
                return enforceArtificialHorizon;
        }
        void setEnforceArtificialHorizon(bool value);
        /** @} */

        /** @brief Current name of the scheduler job. */
        /** @{ */
        QString getName() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getName();
            else
                return name;
        }
        void setName(const QString &value);
        /** @} */

        /** @brief Flag if the scheduler job is a lead job with its own target or if it is a follower job to another lead job. */
        /** @{ */
        bool isLead() const
        {
            return m_isLead;
        }
        void setIsLead(bool value)
        {
            m_isLead = value;
        }
        /** @} */

        /** @brief Group the scheduler job belongs to. */
        /** @{ */
        QString getGroup() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getGroup();
            else
                return group;
        }
        void setGroup(const QString &value);
        /** @} */

        /** @brief Optical train the scheduler job will be using. */
        /** @{ */
        QString getOpticalTrain() const
        {
            return opticalTrain;
        }
        void setOpticalTrain(const QString &value)
        {
            opticalTrain = value;
        }
        /** @} */

        /** @brief Iteration the scheduler job has achieved. This only makes sense for jobs that repeat. */
        /** @{ */
        int getCompletedIterations() const
        {
            return completedIterations;
        }
        void setCompletedIterations(int value);
        /** @} */

        /** @brief Current state of the scheduler job.
         * Setting state to JOB_ABORTED automatically resets the startup characteristics.
         * Setting state to JOB_INVALID automatically resets the startup characteristics and the duration estimation.
         * @see SchedulerJob::setStartupCondition, SchedulerJob::setFileStartupCondition, SchedulerJob::setStartupTime
         * and SchedulerJob::setFileStartupTime.
         */
        /** @{ */
        SchedulerJobStatus getState() const
        {
            return state;
        }

        /**
         * @brief isStopped helper function for idle or aborted state
         */
        bool isStopped() const
        {
            return (state == SCHEDJOB_IDLE || state == SCHEDJOB_ABORTED);
        }
        QDateTime getStateTime() const
        {
            return stateTime;
        }
        QDateTime getLastAbortTime() const
        {
            return lastAbortTime;
        }
        QDateTime getLastErrorTime() const
        {
            return lastErrorTime;
        }
        void setState(const SchedulerJobStatus &value, bool force = false);

        void setFollowerState(const SchedulerJobStatus &value, bool force);

        /**
         * @brief Update the attributes of all followers that are shared with the lead job
         */
        void updateSharedFollowerAttributes();
        /** @} */

        /** @brief Current stage of the scheduler job. */
        /** @{ */
        SchedulerJobStage getStage() const
        {
            return stage;
        }
        void setStage(const SchedulerJobStage &value);
        /** @} */

        /** @brief Number of captures required in the associated sequence. */
        /** @{ */
        int getSequenceCount() const
        {
            return sequenceCount;
        }
        void setSequenceCount(const int count);
        /** @} */

        /** @brief Number of captures completed in the associated sequence. */
        /** @{ */
        int getCompletedCount() const
        {
            return completedCount;
        }
        void setCompletedCount(const int count);
        /** @} */

        /** @brief Time at which the job must start. */
        /** @{ */
        QDateTime getStartupTime() const
        {
            return startupTime;
        }
        void setStartupTime(const QDateTime &value, bool refreshDawnDusk = true);
        /** @} */

        /** @brief Time after which the job is considered to finish. */
        /** @{ */
        QDateTime getFinishAtTime() const
        {
            return finishAtTime;
        }
        QDateTime getStopTime() const
        {
            return stopTime;
        }
        const QString &getStopReason() const
        {
            return stopReason;
        }
        void setStopReason(const QString &reason)
        {
            stopReason = reason;
        }
        void setFinishAtTime(const QDateTime &value);
        /** @} */
        void setStopTime(const QDateTime &value);

        /** @brief Estimation of the time the job will take to process. */
        /** @{ */
        int64_t getEstimatedTime() const
        {
            return estimatedTime;
        }
        void setEstimatedTime(const int64_t &value);
        /** @} */

        /** @brief Estimation of the time the job will take to process each repeat. */
        /** @{ */
        int64_t getEstimatedTimePerRepeat() const
        {
            return estimatedTimePerRepeat;
        }
        void setEstimatedTimePerRepeat(const int64_t &value)
        {
            estimatedTimePerRepeat = value;
        }

        /** @brief Estimation of the time the job will take at startup. */
        /** @{ */
        int64_t getEstimatedStartupTime() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getEstimatedStartupTime();
            else
                return estimatedStartupTime;
        }
        void setEstimatedStartupTime(const int64_t &value)
        {
            estimatedStartupTime = value;
        }

        /** @brief Estimation of the time the job will take to process each repeat. */
        /** @{ */
        int64_t getEstimatedTimeLeftThisRepeat() const
        {
            return estimatedTimeLeftThisRepeat;
        }
        void setEstimatedTimeLeftThisRepeat(const int64_t &value)
        {
            estimatedTimeLeftThisRepeat = value;
        }

        /** @brief Whether this job requires light frames, or only calibration frames. */
        /** @{ */
        bool getLightFramesRequired() const
        {
            return lightFramesRequired;
        }
        void setLightFramesRequired(bool value);
        /** @} */

        /** @brief Whether this job contains a calibration job that requires mount parking. */
        /** @{ */
        bool getCalibrationMountPark() const
        {
            return m_CalibrationMountPark;
        }
        void setCalibrationMountPark(bool value);
        /** @} */

        /** @brief Number of times this job must be repeated (in terms of capture count). */
        /** @{ */
        uint16_t getRepeatsRequired() const
        {
            return repeatsRequired;
        }
        void setRepeatsRequired(const uint16_t &value);
        /** @} */

        /** @brief Number of times this job still has to be repeated (in terms of capture count). */
        /** @{ */
        uint16_t getRepeatsRemaining() const
        {
            return repeatsRemaining;
        }
        void setRepeatsRemaining(const uint16_t &value);
        /** @} */

        void clearProgress()
        {
            m_Progress.clear();
        }
        void addProgress(int numCompleted, const QSharedPointer<SequenceJob> &job)
        {
            m_Progress.append(JobProgress(numCompleted, job));
        }
        /** @brief Human-readable summary of captures completed for this job. */
        /** @{ */
        const QString getProgressSummary() const;
        /** @} */

        /** @brief The map of capture counts for this job, keyed by its capture storage signatures. */
        /** @{ */
        const CapturedFramesMap &getCapturedFramesMap() const
        {
            return capturedFramesMap;
        }
        void setCapturedFramesMap(const CapturedFramesMap &value);
        /** @} */

        /** @brief Resetting a job to original values:
         * - idle state and stage
         * - original startup, none if asap, else user original setting
         * - duration not estimated
         * - full repeat count
         */
        void reset();

        /** @brief Determining whether a SchedulerJob is a duplicate of another.
         * @param a_job is the other SchedulerJob to test duplication against.
         * @return True if objects are different, but name and sequence file are identical, else false.
         * @warning This is a weak comparison, but that's what the scheduler looks at to decide completion.
         */
        bool isDuplicateOf(SchedulerJob const *a_job) const
        {
            return this != a_job && name == a_job->getName() && sequenceFile == a_job->sequenceFile;
        }

        /** @brief Compare ::SchedulerJob instances based on altitude and movement in sky at startup time.
         * @todo This is a qSort predicate, deprecated in QT5.
         * @arg a, b are ::SchedulerJob instances to compare.
         * @arg when is the date/time to use to calculate the altitude to sort with, defaulting to a's startup time.
         * @note To obtain proper sort between several SchedulerJobs, all should have the same startup time.
         * @note Use std:bind to bind a specific date/time to this predicate for altitude calculation.
         * @return true is a is setting but not b.
         * @return false if b is setting but not a.
         * @return true otherwise, if the altitude of b is lower than the altitude of a.
         * @return false otherwise, if the altitude of b is higher than or equal to the altitude of a.
         */
        static bool decreasingAltitudeOrder(SchedulerJob const *a, SchedulerJob const *b, QDateTime const &when = QDateTime());

        /**
             * @brief moonConstraintsOK return true if angle from target to the Moon is > minMoonSeparation and
             *        the moon's altitude is below the given value.
             * @param when date and time to check the Moon separation, now if omitted.
             * @param reason pointer to the reason text in case that the check failed
             * @return true if target is separated enough from the Moon.
             */
        bool moonConstraintsOK(QDateTime const &when = QDateTime(), QString *reason = new QString()) const;

        /**
             * @brief calculateNextTime calculate the next time constraints are met (or missed).
             * @param when date and time to start searching from, now if omitted.
             * @param constraintsAreMet if true, searches for the next time constrains are met, else missed.
             * @return The date and time the target meets or misses constraints.
             */
        QDateTime calculateNextTime(QDateTime const &when, bool checkIfConstraintsAreMet = true, int increment = 1,
                                    QString *reason = nullptr, bool runningJob = false, const QDateTime &until = QDateTime()) const;
        QDateTime getNextPossibleStartTime(const QDateTime &when, int increment = 1, bool runningJob = false,
                                           const QDateTime &until = QDateTime()) const;
        QDateTime getNextEndTime(const QDateTime &start, int increment = 1, QString *reason = nullptr,
                                 const QDateTime &until = QDateTime()) const;

        /**
             * @brief getNextAstronomicalTwilightDawn
             * @return a local time QDateTime locating the first astronomical dawn after this observation.
             * @note The dawn time takes into account the Ekos dawn offset.
             */
        QDateTime getDawnAstronomicalTwilight() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getDawnAstronomicalTwilight();
            else
                return nextDawn;
        };

        /**
             * @brief getDuskAstronomicalTwilight
             * @return a local-time QDateTime locating the first astronomical dusk after this observation.
             * @note The dusk time takes into account the Ekos dusk offset.
             */
        QDateTime getDuskAstronomicalTwilight() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getDuskAstronomicalTwilight();
            else
                return nextDusk;
        };

        /**
             * @brief runsDuringAstronomicalNightTime
             * @param time uses the time given for the check, or, if not valid (the default) uses the job's startup time.
             * @return true if the next dawn/dusk event after this observation is the astronomical dawn, else false.
             * @note This function relies on the guarantee that dawn and dusk are calculated to be the first events after this observation.
             */
        bool runsDuringAstronomicalNightTime(const QDateTime &time = QDateTime(), QDateTime *nextPossibleSuccess = nullptr) const;

        /**
             * @brief satisfiesAltitudeConstraint sees if altitude is allowed for this job at the given azimuth.
             * @param azimuth Azimuth
             * @param altitude Altitude
             * @param altitudeReason a human-readable string explaining why false was returned.
            * @return true if this altitude is permissible for this job
             */
        bool satisfiesAltitudeConstraint(double azimuth, double altitude, QString *altitudeReason = nullptr) const;

        /**
         * @brief setInitialFilter Set initial filter used in the capture sequence. This is used to pass to focus module.
         * @param value Filter name of FIRST LIGHT job in the sequence queue, if any.
         */
        void setInitialFilter(const QString &value);
        const QString &getInitialFilter() const;

        // Convenience debugging methods.
        static QString jobStatusString(SchedulerJobStatus status);
        static QString jobStageString(SchedulerJobStage stage);
        QString jobStartupConditionString(StartupCondition condition) const;
        QString jobCompletionConditionString(CompletionCondition condition) const;

        // Clear the cache that keeps results for getNextPossibleStartTime().
        void clearCache()
        {
            startTimeCache.clear();
        }
        double getAltitudeAtStartup() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->getAltitudeAtStartup();
            else
                return altitudeAtStartup;
        }
        double getAltitudeAtStop() const
        {
            return altitudeAtStop;
        }
        bool isSettingAtStartup() const
        {
            if (m_LeadJob != nullptr)
                return m_LeadJob->isSettingAtStartup();
            else
                return settingAtStartup;
        }
        bool isSettingAtStop() const
        {
            return settingAtStop;
        }

        SchedulerJob *leadJob() const
        {
            return m_LeadJob;
        }
        void setLeadJob(SchedulerJob *newLeadJob)
        {
            m_LeadJob = newLeadJob;
        }

        QList<SchedulerJob *> &followerJobs()
        {
            return m_followerJobs;
        }

        void setStartupFormatted(const QString &value)
        {
            m_StartupFormatted = value;
        }
        const QString &startupFormatted() const
        {
            return m_StartupFormatted;
        }

        void setEndFormatted(const QString &value)
        {
            m_EndFormatted = value;
        }
        const QString &endFormatted() const
        {
            return m_EndFormatted;
        }

        void setAltitudeFormatted(const QString &value)
        {
            m_AltitudeFormatted = value;
        }
        const QString &altitudeFormatted() const
        {
            return m_AltitudeFormatted;
        }

private:
        bool runsDuringAstronomicalNightTimeInternal(const QDateTime &time, QDateTime *minDawnDusk,
                QDateTime *nextPossibleSuccess = nullptr) const;

        // Private constructor for unit testing.
        SchedulerJob(KSMoon *moonPtr);
        friend TestSchedulerUnit;
        friend TestEkosSchedulerOps;

        /** @brief Setter used in the unit test to fix the local time. Otherwise getter gets from KStars instance. */
        /** @{ */
        static KStarsDateTime getLocalTime();
        /** @} */

        /** @brief Setter used in testing to fix the artificial horizon. Otherwise getter gets from KStars instance. */
        /** @{ */
        static const ArtificialHorizon *getHorizon();
        static void setHorizon(ArtificialHorizon *horizon)
        {
            storedHorizon = horizon;
        }
        static bool hasHorizon()
        {
            return storedHorizon != nullptr;
        }

        /** @} */

        QString name;
        bool m_isLead { true };
        QString group;
        QString opticalTrain;
        int completedIterations { 0 };
        SkyPoint targetCoords;
        double m_PositionAngle { -1 };
        SchedulerJobStatus state { SCHEDJOB_IDLE };
        SchedulerJobStage stage { SCHEDSTAGE_IDLE };

        // leader / follower relationship
        SchedulerJob *m_LeadJob { nullptr };
        QList <SchedulerJob *> m_followerJobs;

        // The time that the job stage was set.
        // Used by the Greedy Algorithm to decide when to run JOB_ABORTED jobs again.
        QDateTime stateTime;
        QDateTime lastAbortTime;
        QDateTime lastErrorTime;

        StartupCondition fileStartupCondition { START_ASAP };
        StartupCondition startupCondition { START_ASAP };
        CompletionCondition completionCondition { FINISH_SEQUENCE };

        int sequenceCount { 0 };
        int completedCount { 0 };

        // Defined startup time, used for the StartupCondition START_AT.
        QDateTime startAtTime;
        // Startup time calculated by the scheduler.
        QDateTime startupTime;
        // Completion constraint when the job must finish, used for the CompletionCondition FINISH_AT.
        QDateTime finishAtTime;
        // Next time when the scheduler will stop the job, either because its
        // completed, its constraints aren't met or another job has higher priority.
        QDateTime stopTime;
        // The reason this job is stopping/will-be stopped.
        QString stopReason;

        /* @internal Caches to optimize cell rendering. */
        /* @{ */
        double altitudeAtStartup { 0 };
        double altitudeAtStop { 0 };
        bool settingAtStartup { false };
        bool settingAtStop { false };
        /* @} */

        QUrl sequenceFile;
        QUrl fitsFile;

        double minAltitude { UNDEFINED_ALTITUDE };
        double minMoonSeparation { -1 };
        double maxMoonAltitude { 90 };

        bool enforceWeather { false };
        bool enforceTwilight { false };
        bool enforceArtificialHorizon { false };

        QDateTime nextDawn;
        QDateTime nextDusk;

        StepPipeline stepPipeline { USE_NONE };

        int64_t estimatedTime { -1 };
        int64_t estimatedTimePerRepeat { 0 };
        int64_t estimatedStartupTime { 0 };
        int64_t estimatedTimeLeftThisRepeat { 0 };
        uint16_t repeatsRequired { 1 };
        uint16_t repeatsRemaining { 1 };
        bool inSequenceFocus { false };
        QString m_InitialFilter;

        QString dateTimeDisplayFormat;

        QString m_StartupFormatted, m_EndFormatted, m_AltitudeFormatted;

        bool lightFramesRequired { false };
        bool m_CalibrationMountPark {false};

        CapturedFramesMap capturedFramesMap;

        /// Pointer to Moon object
        KSMoon *moon { nullptr };

        // Used to display human-readable job progress.
        QList<JobProgress> m_Progress;

        // This class is used to cache the results computed in getNextPossibleStartTime()
        // which is called repeatedly by the Greedy scheduler.
        // The cache would need to be cleared if something changes that would affect the
        // start time of jobs (geography, altitide constraints) so it is reset at the start
        // of all schedule calculations--which does not impact its effectiveness.
        class StartTimeCache
        {
                // Keep track of calls to getNextPossibleStartTime, storing the when, until and result.
                struct StartTimeComputation
                {
                    QDateTime from;
                    QDateTime until;
                    QDateTime result;
                };
            public:
                StartTimeCache() {}
                // Check if the computation has been done, and if so, return the previous result.
                bool check(const QDateTime &from, const QDateTime &until,
                           QDateTime *result, QDateTime *newFrom) const;
                // Add a result to the cache.
                void add(const QDateTime &from, const QDateTime &until, const QDateTime &result) const;
                // Clear the cache.
                void clear() const;
            private:
                // Made this mutable and all methods const so that the cache could be
                // used in SchedulerJob const methods.
                mutable QList<StartTimeComputation> startComputations;
        };
        StartTimeCache startTimeCache;

        // These are used in testing, instead of KStars::Instance() resources
        static KStarsDateTime *storedLocalTime;
        static GeoLocation *storedGeo;
        static ArtificialHorizon *storedHorizon;
};
} // Ekos namespace
