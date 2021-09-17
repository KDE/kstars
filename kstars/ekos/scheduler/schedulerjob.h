/*  Ekos Scheduler Job
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"

#include <QUrl>
#include <QMap>
#include "ksmoon.h"
#include "kstarsdatetime.h"

class ArtificialHorizon;
class QTableWidgetItem;
class QLabel;
class KSMoon;
class TestSchedulerUnit;
class TestEkosSchedulerOps;
class dms;

class SchedulerJob
{
    public:
        SchedulerJob();

        /** @brief States of a SchedulerJob. */
        typedef enum
        {
            JOB_IDLE,       /**< Job was just created, and is not evaluated yet */
            JOB_EVALUATION, /**< Job is being evaluated */
            JOB_SCHEDULED,  /**< Job was evaluated, and has a schedule */
            JOB_BUSY,       /**< Job is being processed */
            JOB_ERROR,      /**< Job encountered a fatal issue while processing, and must be reset manually */
            JOB_ABORTED,    /**< Job encountered a transitory issue while processing, and will be rescheduled */
            JOB_INVALID,    /**< Job has an incorrect configuration, and cannot proceed */
            JOB_COMPLETE    /**< Job finished all required captures */
        } JOBStatus;

        /** @brief Running stages of a SchedulerJob. */
        typedef enum
        {
            STAGE_IDLE,
            STAGE_SLEWING,
            STAGE_SLEW_COMPLETE,
            STAGE_FOCUSING,
            STAGE_FOCUS_COMPLETE,
            STAGE_ALIGNING,
            STAGE_ALIGN_COMPLETE,
            STAGE_RESLEWING,
            STAGE_RESLEWING_COMPLETE,
            STAGE_POSTALIGN_FOCUSING,
            STAGE_POSTALIGN_FOCUSING_COMPLETE,
            STAGE_GUIDING,
            STAGE_GUIDING_COMPLETE,
            STAGE_CAPTURING,
            STAGE_COMPLETE
        } JOBStage;

        /** @brief Conditions under which a SchedulerJob may start. */
        typedef enum
        {
            START_ASAP,
            START_CULMINATION,
            START_AT
        } StartupCondition;

        /** @brief Conditions under which a SchedulerJob may complete. */
        typedef enum
        {
            FINISH_SEQUENCE,
            FINISH_REPEAT,
            FINISH_LOOP,
            FINISH_AT
        } CompletionCondition;

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

        /** @brief Coordinates of the target of this job. */
        /** @{ */
        SkyPoint const &getTargetCoords() const
        {
            return targetCoords;
        }
        void setTargetCoords(const dms &ra, const dms &dec, double djd);
        /** @} */

        double getRotation()
        {
            return rotation;
        }
        void setRotation(double rotation);

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
            return fitsFile;
        }
        void setFITSFile(const QUrl &value);
        /** @} */

        /** @brief Minimal target altitude to process this job */
        /** @{ */
        double getMinAltitude() const
        {
            return minAltitude;
        }
        void setMinAltitude(const double &value);
        /** @} */

        /** @brief Does this job have a min-altitude parameter. */
        /** @{ */
        bool hasMinAltitude() const
        {
            return UNDEFINED_ALTITUDE < minAltitude;
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
            return minMoonSeparation;
        }
        void setMinMoonSeparation(const double &value);
        /** @} */

        /** @brief Whether to restrict this job to good weather. */
        /** @{ */
        bool getEnforceWeather() const
        {
            return enforceWeather;
        }
        void setEnforceWeather(bool value);
        /** @} */

        /** @brief Mask of actions to process for this job. */
        /** @{ */
        StepPipeline getStepPipeline() const
        {
            return stepPipeline;
        }
        void setStepPipeline(const StepPipeline &value);
        /** @} */

        /** @brief Condition under which this job starts. */
        /** @{ */
        StartupCondition getStartupCondition() const
        {
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

        /** @brief Target culmination proximity under which this job starts. */
        /** @{ */
        int16_t getCulminationOffset() const
        {
            return culminationOffset;
        }
        void setCulminationOffset(const int16_t &value);
        /** @} */

        /** @brief Timestamp format to use when displaying information about this job. */
        /** @{ */
        QString const &getDateTimeDisplayFormat() const
        {
            return dateTimeDisplayFormat;
        }
        void setDateTimeDisplayFormat(const QString &value);
        /** @} */

        /** @brief Original startup condition, as entered by the user. */
        /** @{ */
        StartupCondition getFileStartupCondition() const
        {
            return fileStartupCondition;
        }
        void setFileStartupCondition(const StartupCondition &value);
        /** @} */

        /** @brief Original time at which the job must start, as entered by the user. */
        /** @{ */
        QDateTime getFileStartupTime() const
        {
            return fileStartupTime;
        }
        void setFileStartupTime(const QDateTime &value);
        /** @} */

        /** @brief Whether this job requires re-focus while running its capture sequence. */
        /** @{ */
        bool getInSequenceFocus() const
        {
            return inSequenceFocus;
        }
        void setInSequenceFocus(bool value);
        /** @} */

        /** @brief Job priority, low priority value means most prioritary. */
        /** @{ */
        uint8_t getPriority() const
        {
            return priority;
        }
        void setPriority(const uint8_t &value);
        /** @} */

        /** @brief Whether to restrict job to night time. */
        /** @{ */
        bool getEnforceTwilight() const
        {
            return enforceTwilight;
        }
        void setEnforceTwilight(bool value);
        /** @} */

        /** @brief Whether to restrict job to night time. */
        /** @{ */
        bool getEnforceArtificialHorizon() const
        {
            return enforceArtificialHorizon;
        }
        void setEnforceArtificialHorizon(bool value);
        /** @} */

        /** @brief Current name of the scheduler job. */
        /** @{ */
        QString getName() const
        {
            return name;
        }
        void setName(const QString &value);
        /** @} */

        /** @brief Shortcut to widget cell for job name in the job queue table. */
        /** @{ */
        QTableWidgetItem *getNameCell() const
        {
            return nameCell;
        }
        void setNameCell(QTableWidgetItem *cell);
        /** @} */

        /** @brief Current state of the scheduler job.
         * Setting state to JOB_ABORTED automatically resets the startup characteristics.
         * Setting state to JOB_INVALID automatically resets the startup characteristics and the duration estimation.
         * @see SchedulerJob::setStartupCondition, SchedulerJob::setFileStartupCondition, SchedulerJob::setStartupTime
         * and SchedulerJob::setFileStartupTime.
         */
        /** @{ */
        JOBStatus getState() const
        {
            return state;
        }
        void setState(const JOBStatus &value);
        /** @} */

        /** @brief Shortcut to widget cell for job state in the job queue table. */
        /** @{ */
        QTableWidgetItem *getStatusCell() const
        {
            return statusCell;
        }
        void setStatusCell(QTableWidgetItem *cell);
        /** @} */

        /** @brief Current stage of the scheduler job. */
        /** @{ */
        JOBStage getStage() const
        {
            return stage;
        }
        void setStage(const JOBStage &value);
        /** @} */

        /** @brief Shortcut to widget cell for job stage in the job queue table. */
        /** @{ */
        QTableWidgetItem *getStageCell() const
        {
            return stageCell;
        }
        void setStageCell(QTableWidgetItem *cell);
        QLabel *getStageLabel() const
        {
            return stageLabel;
        }
        void setStageLabel(QLabel *label);
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

        /** @brief Shortcut to widget cell for captures in the job queue table. */
        /** @{ */
        QTableWidgetItem *getCaptureCountCell() const
        {
            return captureCountCell;
        }
        void setCaptureCountCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Time at which the job must start. */
        /** @{ */
        QDateTime getStartupTime() const
        {
            return startupTime;
        }
        void setStartupTime(const QDateTime &value);
        /** @} */

        /** @brief Shortcut to widget cell for startup time in the job queue table. */
        /** @{ */
        QTableWidgetItem *getStartupCell() const
        {
            return startupCell;
        }
        void setStartupCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Shortcut to widget cell for altitude in the job queue table. */
        /** @{ */
        QTableWidgetItem *getAltitudeCell() const
        {
            return altitudeCell;
        }
        void setAltitudeCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Time after which the job is considered complete. */
        /** @{ */
        QDateTime getCompletionTime() const
        {
            return completionTime;
        }
        void setCompletionTime(const QDateTime &value);
        /** @} */

        /** @brief Shortcut to widget cell for completion time in the job queue table. */
        /** @{ */
        QTableWidgetItem *getCompletionCell() const
        {
            return completionCell;
        }
        void setCompletionCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Estimation of the time the job will take to process. */
        /** @{ */
        int64_t getEstimatedTime() const
        {
            return estimatedTime;
        }
        void setEstimatedTime(const int64_t &value);
        /** @} */

        /** @brief Shortcut to widget cell for estimated time in the job queue table. */
        /** @{ */
        QTableWidgetItem *getEstimatedTimeCell() const
        {
            return estimatedTimeCell;
        }
        void setEstimatedTimeCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Estimation of the lead time the job will have to process. */
        /** @{ */
        int64_t getLeadTime() const
        {
            return leadTime;
        }
        void setLeadTime(const int64_t &value);
        /** @} */

        /** @brief Shortcut to widget cell for estimated time in the job queue table. */
        /** @{ */
        QTableWidgetItem *getLeadTimeCell() const
        {
            return leadTimeCell;
        }
        void setLeadTimeCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Current score of the scheduler job. */
        /** @{ */
        int getScore() const
        {
            return score;
        }
        void setScore(int value);
        /** @} */

        /** @brief Shortcut to widget cell for job score in the job queue table. */
        /** @{ */
        QTableWidgetItem *getScoreCell() const
        {
            return scoreCell;
        }
        void setScoreCell(QTableWidgetItem *value);
        /** @} */

        /** @brief Whether this job requires light frames, or only calibration frames. */
        /** @{ */
        bool getLightFramesRequired() const
        {
            return lightFramesRequired;
        }
        void setLightFramesRequired(bool value);
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

        /** @brief The map of capture counts for this job, keyed by its capture storage signatures. */
        /** @{ */
        typedef QMap<QString, uint16_t> CapturedFramesMap;
        const CapturedFramesMap &getCapturedFramesMap() const
        {
            return capturedFramesMap;
        }
        void setCapturedFramesMap(const CapturedFramesMap &value);
        /** @} */

        /** @brief Refresh all cells connected to this SchedulerJob. */
        void updateJobCells();

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
            return this != a_job && name == a_job->name && sequenceFile == a_job->sequenceFile;
        }

        /** @brief Compare ::SchedulerJob instances based on score.
         * @todo This is a qSort predicate, deprecated in QT5.
         * @arg a, b are ::SchedulerJob instances to compare.
         * @return true if the score of b is lower than the score of a.
         * @return false if the score of b is higher than or equal to the score of a.
         */
        static bool decreasingScoreOrder(SchedulerJob const *a, SchedulerJob const *b);

        /** @brief Compare ::SchedulerJob instances based on priority.
         * @todo This is a qSort predicate, deprecated in QT5.
         * @arg a, b are ::SchedulerJob instances to compare.
         * @return true if the priority of a is lower than the priority of b.
         * @return false if the priority of a is higher than or equal to the priority of b.
         */
        static bool increasingPriorityOrder(SchedulerJob const *a, SchedulerJob const *b);

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

        /** @brief Compare ::SchedulerJob instances based on startup time.
         * @todo This is a qSort predicate, deprecated in QT5.
         * @arg a, b are ::SchedulerJob instances to compare.
         * @return true if the startup time of a is sooner than the priority of b.
         * @return false if the startup time of a is later than or equal to the priority of b.
         */
        static bool increasingStartupTimeOrder(SchedulerJob const *a, SchedulerJob const *b);

        /**
             * @brief getAltitudeScore Get the altitude score of an object. The higher the better
             * @param when date and time to check the target altitude, now if omitted.
             * @param altPtr returns the altitude in degrees if not a nullptr.
             * @return Altitude score. Target altitude below minimum altitude required by job or setting target under 3 degrees below minimum altitude get bad score.
             */
        int16_t getAltitudeScore(QDateTime const &when = QDateTime(), double *altPtr = nullptr) const;

        /**
             * @brief getMoonSeparationScore Get moon separation score. The further apart, the better, up a maximum score of 20.
             * @param when date and time to check the moon separation, now if omitted.
             * @return Moon separation score
             */
        int16_t getMoonSeparationScore(QDateTime const &when = QDateTime()) const;

        /**
             * @brief getCurrentMoonSeparation Get current moon separation in degrees at current time for the given job
             * @param job scheduler job
             * @return Separation in degrees
             */
        double getCurrentMoonSeparation() const;

        /**
             * @brief calculateAltitudeTime calculate the altitude time given the minimum altitude given.
             * @param when date and time to start searching from, now if omitted.
             * @return The date and time the target is at or above the argument altitude, valid if found, invalid if not achievable (always under altitude).
             */
        QDateTime calculateAltitudeTime(QDateTime const &when = QDateTime()) const;

        /**
             * @brief calculateCulmination find culmination time adjust for the job offset
             * @param when date and time to start searching from, now if omitted
             * @return The date and time the target is in entering the culmination interval, valid if found, invalid if not achievable (currently always valid).
             */
        QDateTime calculateCulmination(QDateTime const &when = QDateTime()) const;

        /**
             * @brief calculateDawnDusk find the next astronomical dawn and dusk after the current date and time of observation
             */
        static void calculateDawnDusk(QDateTime const &when, QDateTime &dawn, QDateTime &dusk);

        /**
             * @brief getNextAstronomicalTwilightDawn
             * @return a local time QDateTime locating the first astronomical dawn after this observation.
             * @note The dawn time takes into account the Ekos dawn offset.
             */
        QDateTime getDawnAstronomicalTwilight() const
        {
            return nextDawn;
        };

        /**
             * @brief getDuskAstronomicalTwilight
             * @return a local-time QDateTime locating the first astronomical dusk after this observation.
             * @note The dusk time takes into account the Ekos dusk offset.
             */
        QDateTime getDuskAstronomicalTwilight() const
        {
            return nextDusk;
        };

        /**
             * @brief runsDuringAstronomicalNightTime
             * @return true if the next dawn/dusk event after this observation is the astronomical dawn, else false.
             * @note This function relies on the guarantee that dawn and dusk are calculated to be the first events after this observation.
             */
        bool runsDuringAstronomicalNightTime() const;

        /**
             * @brief findAltitude Find altitude given a specific time
             * @param target Target
             * @param when date time to find altitude
             * @param is_setting whether target is setting at the argument time (optional).
             * @param debug outputs calculation to log file (optional).
             * @return Altitude of the target at the specific date and time given.
             * @warning This function uses the current KStars geolocation.
             */
        static double findAltitude(const SkyPoint &target, const QDateTime &when, bool *is_setting = nullptr, bool debug = false);

        /**
             * @brief getMinAltitudeConstraint Find minimum allowed altitude for this job at the given azimuth.
             * @param azimuth Azimuth
             * @return Minimum allowed altitude of the target at the specific azimuth.
             */
        double getMinAltitudeConstraint(double azimuth) const;
    private:
        // Private constructor for unit testing.
        SchedulerJob(KSMoon *moonPtr);
        friend TestSchedulerUnit;
        friend TestEkosSchedulerOps;

        /** @brief Setter used in the unit test to fix the local time. Otherwise getter gets from KStars instance. */
        /** @{ */
        static KStarsDateTime getLocalTime();
        static void setLocalTime(KStarsDateTime *time)
        {
            storedLocalTime = time;
        }
        static bool hasLocalTime()
        {
            return storedLocalTime != nullptr;
        }
        /** @} */

        /** @brief Setter used in testing to fix the geo location. Otherwise getter gets from KStars instance. */
        /** @{ */
        static const GeoLocation *getGeo();
        static void setGeo(GeoLocation *geo)
        {
            storedGeo = geo;
        }
        static bool hasGeo()
        {
            return storedGeo != nullptr;
        }
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
        SkyPoint targetCoords;
        double rotation { -1 };
        JOBStatus state { JOB_IDLE };
        JOBStage stage { STAGE_IDLE };

        StartupCondition fileStartupCondition { START_ASAP };
        StartupCondition startupCondition { START_ASAP };
        CompletionCondition completionCondition { FINISH_SEQUENCE };

        int sequenceCount { 0 };
        int completedCount { 0 };

        QDateTime fileStartupTime;
        QDateTime startupTime;
        QDateTime completionTime;

        /* @internal Caches to optimize cell rendering. */
        /* @{ */
        double altitudeAtStartup { 0 };
        double altitudeAtCompletion { 0 };
        bool isSettingAtStartup { false };
        bool isSettingAtCompletion { false };
        /* @} */

        QUrl sequenceFile;
        QUrl fitsFile;

        double minAltitude { UNDEFINED_ALTITUDE };
        double minMoonSeparation { -1 };

        bool enforceWeather { false };
        bool enforceTwilight { false };
        bool enforceArtificialHorizon { false };

        QDateTime nextDawn;
        QDateTime nextDusk;

        StepPipeline stepPipeline { USE_NONE };

        /** @internal Widget cell/label shortcuts. */
        /** @{ */
        QTableWidgetItem *nameCell { nullptr };
        QLabel *nameLabel { nullptr };
        QTableWidgetItem *statusCell { nullptr };
        QTableWidgetItem *stageCell { nullptr };
        QLabel *stageLabel { nullptr };
        QTableWidgetItem *altitudeCell { nullptr };
        QTableWidgetItem *startupCell { nullptr };
        QTableWidgetItem *completionCell { nullptr };
        QTableWidgetItem *estimatedTimeCell { nullptr };
        QTableWidgetItem *captureCountCell { nullptr };
        QTableWidgetItem *scoreCell { nullptr };
        QTableWidgetItem *leadTimeCell { nullptr };
        /** @} */

        int score { 0 };
        int16_t culminationOffset { 0 };
        uint8_t priority { 10 };
        int64_t estimatedTime { -1 };
        int64_t leadTime { 0 };
        uint16_t repeatsRequired { 1 };
        uint16_t repeatsRemaining { 1 };
        bool inSequenceFocus { false };

        QString dateTimeDisplayFormat;

        bool lightFramesRequired { false };

        QMap<QString, uint16_t> capturedFramesMap;

        /// Pointer to Moon object
        KSMoon *moon { nullptr };

        // These are used in testing, instead of KStars::Instance() resources
        static KStarsDateTime *storedLocalTime;
        static GeoLocation *storedGeo;
        static ArtificialHorizon *storedHorizon;
};
