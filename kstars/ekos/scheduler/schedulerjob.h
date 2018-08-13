/*  Ekos Scheduler Job
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "skypoint.h"

#include <QUrl>
#include <QMap>

class QTableWidgetItem;
class QLabel;

class dms;

class SchedulerJob
{
  public:
    SchedulerJob() = default;

    /** @brief States of a SchedulerJob. */
    typedef enum {
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
    typedef enum {
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
    typedef enum {
        START_ASAP,
        START_CULMINATION,
        START_AT
    } StartupCondition;

    /** @brief Conditions under which a SchedulerJob may complete. */
    typedef enum {
        FINISH_SEQUENCE,
        FINISH_REPEAT,
        FINISH_LOOP,
        FINISH_AT
    } CompletionCondition;

    /** @brief Actions that may be processed when running a SchedulerJob.
     * FIXME: StepPipeLine is actually a mask, change this into a bitfield.
     */
    typedef enum {
        USE_NONE  = 0,
        USE_TRACK = 1 << 0,
        USE_FOCUS = 1 << 1,
        USE_ALIGN = 1 << 2,
        USE_GUIDE = 1 << 3
    } StepPipeline;

    /** @brief Coordinates of the target of this job. */
    /** @{ */
    SkyPoint const & getTargetCoords() const { return targetCoords; }
    void setTargetCoords(dms& ra, dms& dec);
    /** @} */

    /** @brief Capture sequence this job uses while running. */
    /** @{ */
    QUrl getSequenceFile() const { return sequenceFile; }
    void setSequenceFile(const QUrl &value);
    /** @} */

    /** @brief FITS file whose plate solve produces target coordinates. */
    /** @{ */
    QUrl getFITSFile() const { return fitsFile; }
    void setFITSFile(const QUrl &value);
    /** @} */

    /** @brief Minimal target altitude to process this job */
    /** @{ */
    double getMinAltitude() const { return minAltitude; }
    void setMinAltitude(const double &value);
    /** @} */

    /** @brief Minimal Moon separation to process this job. */
    /** @{ */
    double getMinMoonSeparation() const { return minMoonSeparation; }
    void setMinMoonSeparation(const double &value);
    /** @} */

    /** @brief Whether to restrict this job to good weather. */
    /** @{ */
    bool getEnforceWeather() const { return enforceWeather; }
    void setEnforceWeather(bool value);
    /** @} */

    /** @brief Mask of actions to process for this job. */
    /** @{ */
    StepPipeline getStepPipeline() const { return stepPipeline; }
    void setStepPipeline(const StepPipeline &value);
    /** @} */

    /** @brief Condition under which this job starts. */
    /** @{ */
    StartupCondition getStartupCondition() const { return startupCondition; }
    void setStartupCondition(const StartupCondition &value);
    /** @} */

    /** @brief Condition under which this job completes. */
    /** @{ */
    CompletionCondition getCompletionCondition() const { return completionCondition; }
    void setCompletionCondition(const CompletionCondition &value);
    /** @} */

    /** @brief Target culmination proximity under which this job starts. */
    /** @{ */
    int16_t getCulminationOffset() const { return culminationOffset; }
    void setCulminationOffset(const int16_t &value);
    /** @} */

    /** @brief Timestamp format to use when displaying information about this job. */
    /** @{ */
    QString const & getDateTimeDisplayFormat() const { return dateTimeDisplayFormat; }
    void setDateTimeDisplayFormat(const QString &value);
    /** @} */

    /** @brief Original startup condition, as entered by the user. */
    /** @{ */
    StartupCondition getFileStartupCondition() const { return fileStartupCondition; }
    void setFileStartupCondition(const StartupCondition &value);
    /** @} */

    /** @brief Original time at which the job must start, as entered by the user. */
    /** @{ */
    QDateTime getFileStartupTime() const { return fileStartupTime; }
    void setFileStartupTime(const QDateTime &value);
    /** @} */

    /** @brief Whether this job requires re-focus while running its capture sequence. */
    /** @{ */
    bool getInSequenceFocus() const { return inSequenceFocus; }
    void setInSequenceFocus(bool value);
    /** @} */

    /** @brief Job priority, low priority value means most prioritary. */
    /** @{ */
    uint8_t getPriority() const { return priority; }
    void setPriority(const uint8_t &value);
    /** @} */

    /** @brief Whether to restrict job to night time. */
    /** @{ */
    bool getEnforceTwilight() const { return enforceTwilight; }
    void setEnforceTwilight(bool value);
    /** @} */

    /** @brief Current name of the scheduler job. */
    /** @{ */
    QString getName() const { return name; }
    void setName(const QString &value);
    /** @} */

    /** @brief Shortcut to widget cell for job name in the job queue table. */
    /** @{ */
    QTableWidgetItem *getNameCell() const { return nameCell; }
    void setNameCell(QTableWidgetItem *cell);
    /** @} */

    /** @brief Current state of the scheduler job.
     * Setting state to JOB_ABORTED automatically resets the startup characteristics.
     * Setting state to JOB_INVALID automatically resets the startup characteristics and the duration estimation.
     * @see SchedulerJob::setStartupCondition, SchedulerJob::setFileStartupCondition, SchedulerJob::setStartupTime
     * and SchedulerJob::setFileStartupTime.
     */
    /** @{ */
    JOBStatus getState() const { return state; }
    void setState(const JOBStatus &value);
    /** @} */

    /** @brief Shortcut to widget cell for job state in the job queue table. */
    /** @{ */
    QTableWidgetItem *getStatusCell() const { return statusCell; }
    void setStatusCell(QTableWidgetItem *cell);
    /** @} */

    /** @brief Current stage of the scheduler job. */
    /** @{ */
    JOBStage getStage() const { return stage; }
    void setStage(const JOBStage &value);
    /** @} */

    /** @brief Shortcut to widget cell for job stage in the job queue table. */
    /** @{ */
    QTableWidgetItem *getStageCell() const { return stageCell; }
    void setStageCell(QTableWidgetItem *cell);
    QLabel *getStageLabel() const { return stageLabel; }
    void setStageLabel(QLabel *label);
    /** @} */

    /** @brief Number of captures required in the associated sequence. */
    /** @{ */
    int getSequenceCount() const { return sequenceCount; }
    void setSequenceCount(const int count);
    /** @} */

    /** @brief Number of captures completed in the associated sequence. */
    /** @{ */
    int getCompletedCount() const { return completedCount; }
    void setCompletedCount(const int count);
    /** @} */

    /** @brief Shortcut to widget cell for captures in the job queue table. */
    /** @{ */
    QTableWidgetItem *getCaptureCountCell() const { return captureCountCell; }
    void setCaptureCountCell(QTableWidgetItem *value);
    /** @} */

    /** @brief Time at which the job must start. */
    /** @{ */
    QDateTime getStartupTime() const { return startupTime; }
    void setStartupTime(const QDateTime &value);
    /** @} */

    /** @brief Shortcut to widget cell for startup time in the job queue table. */
    /** @{ */
    QTableWidgetItem *getStartupCell() const { return startupCell; }
    void setStartupCell(QTableWidgetItem *value);
    /** @} */

    /** @brief Time after which the job is considered complete. */
    /** @{ */
    QDateTime getCompletionTime() const { return completionTime; }
    void setCompletionTime(const QDateTime &value);
    /** @} */

    /** @brief Shortcut to widget cell for completion time in the job queue table. */
    /** @{ */
    QTableWidgetItem *getCompletionCell() const { return completionCell; }
    void setCompletionCell(QTableWidgetItem *value);
    /** @} */

    /** @brief Estimation of the time the job will take to process. */
    /** @{ */
    int64_t getEstimatedTime() const { return estimatedTime; }
    void setEstimatedTime(const int64_t &value);
    /** @} */

    /** @brief Shortcut to widget cell for estimated time in the job queue table. */
    /** @{ */
    QTableWidgetItem *getEstimatedTimeCell() const { return estimatedTimeCell; }
    void setEstimatedTimeCell(QTableWidgetItem *value);
    /** @} */

    /** @brief Current score of the scheduler job. */
    /** @{ */
    int getScore() const { return score; }
    void setScore(int value);
    /** @} */

    /** @brief Shortcut to widget cell for job score in the job queue table. */
    /** @{ */
    QTableWidgetItem *getScoreCell() const { return scoreCell; }
    void setScoreCell(QTableWidgetItem *value);
    /** @} */

    /** @brief Whether this job requires light frames, or only calibration frames. */
    /** @{ */
    bool getLightFramesRequired() const { return lightFramesRequired; }
    void setLightFramesRequired(bool value);
    /** @} */

    /** @brief Number of times this job must be repeated (in terms of capture count). */
    /** @{ */
    uint16_t getRepeatsRequired() const { return repeatsRequired; }
    void setRepeatsRequired(const uint16_t &value);
    /** @} */

    /** @brief Number of times this job still has to be repeated (in terms of capture count). */
    /** @{ */
    uint16_t getRepeatsRemaining() const { return repeatsRemaining; }
    void setRepeatsRemaining(const uint16_t &value);
    /** @} */

    /** @brief The map of capture counts for this job, keyed by its capture storage signatures. */
    /** @{ */
    typedef QMap<QString, uint16_t> CapturedFramesMap;
    const CapturedFramesMap& getCapturedFramesMap() const { return capturedFramesMap; }
    void setCapturedFramesMap(const CapturedFramesMap &value);
    /** @} */

    /** @brief Refresh all cells connected to this SchedulerJob. */
    void updateJobCell();

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
     * @fixme This is a weak comparison, but that's what the scheduler looks at to decide completion. 
     */
    bool isDuplicateOf(SchedulerJob const *a_job) const { return this != a_job && name == a_job->name && sequenceFile == a_job->sequenceFile; }

    /** @brief Compare ::SchedulerJob instances based on score. This is a qSort predicate, deprecated in QT5.
     * @arg a, b are ::SchedulerJob instances to compare.
     * @return true if the score of b is lower than the score of a.
     * @return false if the score of b is higher than or equal to the score of a.
     */
    static bool decreasingScoreOrder(SchedulerJob const *a, SchedulerJob const *b);

    /** @brief Compare ::SchedulerJob instances based on priority. This is a qSort predicate, deprecated in QT5.
     * @arg a, b are ::SchedulerJob instances to compare.
     * @return true if the priority of a is lower than the priority of b.
     * @return false if the priority of a is higher than or equal to the priority of b.
     */
    static bool increasingPriorityOrder(SchedulerJob const *a, SchedulerJob const *b);

    /** @brief Compare ::SchedulerJob instances based on altitude. This is a qSort predicate, deprecated in QT5.
     * @arg a, b are ::SchedulerJob instances to compare.
     * @return true if the altitude of b is lower than the altitude of a.
     * @return false if the altitude of b is higher than or equal to the altitude of a.
     */
    static bool decreasingAltitudeOrder(SchedulerJob const *a, SchedulerJob const *b);

    /** @brief Compare ::SchedulerJob instances based on startup time. This is a qSort predicate, deprecated in QT5.
     * @arg a, b are ::SchedulerJob instances to compare.
     * @return true if the startup time of a is sooner than the priority of b.
     * @return false if the startup time of a is later than or equal to the priority of b.
     */
    static bool increasingStartupTimeOrder(SchedulerJob const *a, SchedulerJob const *b);

private:
    QString name;
    SkyPoint targetCoords;
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

    QUrl sequenceFile;
    QUrl fitsFile;

    double minAltitude { -1 };
    double minMoonSeparation { -1 };

    bool enforceWeather { false };
    bool enforceTwilight { false };

    StepPipeline stepPipeline { USE_NONE };

    /** @internal Widget cell/label shortcuts. */
    /** @{ */
    QTableWidgetItem *nameCell { nullptr };
    QLabel *nameLabel { nullptr };
    QTableWidgetItem *statusCell { nullptr };
    QTableWidgetItem *stageCell { nullptr };
    QLabel *stageLabel { nullptr };
    QTableWidgetItem *startupCell { nullptr };
    QTableWidgetItem *completionCell { nullptr };
    QTableWidgetItem *estimatedTimeCell { nullptr };
    QTableWidgetItem *captureCountCell { nullptr };
    QTableWidgetItem *scoreCell { nullptr };
    /** @} */

    int score { 0 };
    int16_t culminationOffset { 0 };
    uint8_t priority { 10 };
    int64_t estimatedTime { -1 };
    uint16_t repeatsRequired { 1 };
    uint16_t repeatsRemaining { 1 };
    bool inSequenceFocus { false };

    QString dateTimeDisplayFormat;

    bool lightFramesRequired { false };

    QMap<QString, uint16_t> capturedFramesMap;
};
