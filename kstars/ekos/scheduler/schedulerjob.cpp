/*  Ekos Scheduler Job
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "schedulerjob.h"

#include "dms.h"
#include "kstarsdata.h"
#include "Options.h"
#include "scheduler.h"

#include <knotification.h>

#include <QTableWidgetItem>

void SchedulerJob::setName(const QString &value)
{
    name = value;
    updateJobCells();
}

void SchedulerJob::setStartupCondition(const StartupCondition &value)
{
    startupCondition = value;

    /* Keep startup time and condition valid */
    if (value == START_ASAP)
        startupTime = QDateTime();

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);
}

void SchedulerJob::setStartupTime(const QDateTime &value)
{
    startupTime = value;

    /* Keep startup time and condition valid */
    if (value.isValid())
        startupCondition = START_AT;
    else
        startupCondition = fileStartupCondition;

    // Refresh altitude - invalid date/time is taken care of when rendering
    altitudeAtStartup = Ekos::Scheduler::findAltitude(targetCoords, startupTime, &isSettingAtStartup);

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);
}

void SchedulerJob::setSequenceFile(const QUrl &value)
{
    sequenceFile = value;
}

void SchedulerJob::setFITSFile(const QUrl &value)
{
    fitsFile = value;
}

void SchedulerJob::setMinAltitude(const double &value)
{
    minAltitude = value;
}

void SchedulerJob::setMinMoonSeparation(const double &value)
{
    minMoonSeparation = value;
}

void SchedulerJob::setEnforceWeather(bool value)
{
    enforceWeather = value;
}

void SchedulerJob::setCompletionTime(const QDateTime &value)
{
    /* If completion time is valid, automatically switch condition to FINISH_AT */
    if (value.isValid())
    {
        setCompletionCondition(FINISH_AT);
        completionTime = value;
        altitudeAtCompletion = Ekos::Scheduler::findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
        setEstimatedTime(-1);
    }
    /* If completion time is invalid, and job is looping, keep completion time undefined */
    else if (FINISH_LOOP == completionCondition)
    {
        completionTime = QDateTime();
        altitudeAtCompletion = Ekos::Scheduler::findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
        setEstimatedTime(-1);
    }
    /* If completion time is invalid, deduce completion from startup and duration */
    else if (startupTime.isValid())
    {
        completionTime = startupTime.addSecs(estimatedTime);
        altitudeAtCompletion = Ekos::Scheduler::findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
        updateJobCells();
    }
    /* Else just refresh estimated time - which update job cells */
    else setEstimatedTime(estimatedTime);


    /* Invariants */
    Q_ASSERT_X(completionTime.isValid() ?
            (FINISH_AT == completionCondition || FINISH_REPEAT == completionCondition || FINISH_SEQUENCE == completionCondition) :
            FINISH_LOOP == completionCondition,
            __FUNCTION__, "Valid completion time implies job is FINISH_AT/REPEAT/SEQUENCE, else job is FINISH_LOOP.");
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;

    // Update repeats requirement, looping jobs have none
    switch (completionCondition)
    {
        case FINISH_LOOP:
            setCompletionTime(QDateTime());
            /* Fall through */
        case FINISH_AT:
            if (0 < getRepeatsRequired())
                setRepeatsRequired(0);
            break;

        case FINISH_SEQUENCE:
            if (1 != getRepeatsRequired())
                setRepeatsRequired(1);
            break;

        case FINISH_REPEAT:
            if (0 == getRepeatsRequired())
                setRepeatsRequired(1);
            break;

        default: break;
    }

    updateJobCells();
}

void SchedulerJob::setStepPipeline(const StepPipeline &value)
{
    stepPipeline = value;
}

void SchedulerJob::setState(const JOBStatus &value)
{
    state = value;

    /* FIXME: move this to Scheduler, SchedulerJob is mostly a model */
    if (JOB_ERROR == state)
        KNotification::event(QLatin1String("EkosSchedulerJobFail"), i18n("Ekos job failed (%1)", getName()));

    /* If job becomes invalid, automatically reset its startup characteristics, and force its duration to be reestimated */
    if (JOB_INVALID == value)
    {
        setStartupCondition(fileStartupCondition);
        setStartupTime(fileStartupTime);
        setEstimatedTime(-1);
    }

    /* If job is aborted, automatically reset its startup characteristics */
    if (JOB_ABORTED == value)
    {
        setStartupCondition(fileStartupCondition);
        /* setStartupTime(fileStartupTime); */
    }

    updateJobCells();
}

void SchedulerJob::setLeadTime(const int64_t &value)
{
    leadTime = value;
    updateJobCells();
}

void SchedulerJob::setScore(int value)
{
    score = value;
    updateJobCells();
}

void SchedulerJob::setCulminationOffset(const int16_t &value)
{
    culminationOffset = value;
}

void SchedulerJob::setSequenceCount(const int count)
{
    sequenceCount = count;
    updateJobCells();
}

void SchedulerJob::setNameCell(QTableWidgetItem *value)
{
    nameCell = value;
}

void SchedulerJob::setCompletedCount(const int count)
{
    completedCount = count;
    updateJobCells();
}

void SchedulerJob::setStatusCell(QTableWidgetItem *value)
{
    statusCell = value;
    if (nullptr != statusCell)
        statusCell->setToolTip(i18n("Current status of job '%1', managed by the Scheduler.\n"
                                    "If invalid, the Scheduler was not able to find a proper observation time for the target.\n"
                                    "If aborted, the Scheduler missed the scheduled time or encountered transitory issues and will reschedule the job.\n"
                                    "If complete, the Scheduler verified that all sequence captures requested were stored, including repeats.",
                                    name));
}

void SchedulerJob::setAltitudeCell(QTableWidgetItem *value)
{
    altitudeCell = value;
    if (nullptr != altitudeCell)
        altitudeCell->setToolTip(i18n("Current altitude of the target of job '%1'.\n"
                                     "The altitude at startup, if available, is displayed between parentheses.\n"
                                     "A rising target is indicated with an arrow going up.\n"
                                     "A setting target is indicated with an arrow going down.",
                                     name));
}

void SchedulerJob::setStartupCell(QTableWidgetItem *value)
{
    startupCell = value;
    if (nullptr != startupCell)
        startupCell->setToolTip(i18n("Startup time for job '%1', as estimated by the Scheduler.\n"
                                     "Fixed time from user or culmination time is marked with a chronometer symbol. ",
                                     name));
}

void SchedulerJob::setCompletionCell(QTableWidgetItem *value)
{
    completionCell = value;
    if (nullptr != completionCell)
        completionCell->setToolTip(i18n("Completion time for job '%1', as estimated by the Scheduler.\n"
                                        "You may specify a fixed time to limit duration of looping jobs. "
                                        "A warning symbol indicates the altitude at completion may cause the job to abort before completion.\n",
                                        name));
}

void SchedulerJob::setCaptureCountCell(QTableWidgetItem *value)
{
    captureCountCell = value;
    if (nullptr != captureCountCell)
        captureCountCell->setToolTip(i18n("Count of captures stored for job '%1', based on its sequence job.\n"
                                          "This is a summary, additional specific frame types may be required to complete the job.",
                                          name));
}

void SchedulerJob::setScoreCell(QTableWidgetItem *value)
{
    scoreCell = value;
    if (nullptr != scoreCell)
        scoreCell->setToolTip(i18n("Current score for job '%1', from its altitude, moon separation and sky darkness.\n"
                                   "Negative if adequate altitude is not achieved yet or if there is no proper observation time today.\n"
                                   "The Scheduler will refresh scores when picking a new candidate job.",
                                   name));
}

void SchedulerJob::setLeadTimeCell(QTableWidgetItem *value)
{
    leadTimeCell = value;
    if (nullptr != leadTimeCell)
        leadTimeCell->setToolTip(i18n("Time interval from the job which precedes job '%1'.\n"
                                   "Adjust the Lead Time in Ekos options to increase that duration and leave time for jobs to complete.\n"
                                   "Rearrange jobs to minimize that duration and optimize your imaging time.",
                                   name));
}

void SchedulerJob::setDateTimeDisplayFormat(const QString &value)
{
    dateTimeDisplayFormat = value;
    updateJobCells();
}

void SchedulerJob::setStage(const JOBStage &value)
{
    stage = value;
    updateJobCells();
}

void SchedulerJob::setStageCell(QTableWidgetItem *cell)
{
    stageCell = cell;
    // FIXME: Add a tool tip if cell is used
}

void SchedulerJob::setStageLabel(QLabel *label)
{
    stageLabel = label;
}

void SchedulerJob::setFileStartupCondition(const StartupCondition &value)
{
    fileStartupCondition = value;
}

void SchedulerJob::setFileStartupTime(const QDateTime &value)
{
    fileStartupTime = value;
}

void SchedulerJob::setEstimatedTime(const int64_t &value)
{
    /* Estimated time is generally the difference between startup and completion times:
     * - It is fixed when startup and completion times are fixed, that is, we disregard the argument
     * - Else mostly it pushes completion time from startup time
     * 
     * However it cannot advance startup time when completion time is fixed because of the way jobs are scheduled.
     * This situation requires a warning in the user interface when there is not enough time for the job to process.
     */

    /* If startup and completion times are fixed, estimated time cannot change - disregard the argument */
    if (START_ASAP != fileStartupCondition && FINISH_AT == completionCondition)
    {
        estimatedTime = startupTime.secsTo(completionTime);
    }
    /* If completion time isn't fixed, estimated time adjusts completion time */
    else if (FINISH_AT != completionCondition && FINISH_LOOP != completionCondition)
    {
        estimatedTime = value;
        completionTime = startupTime.addSecs(value);
        altitudeAtCompletion = Ekos::Scheduler::findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
    }
    /* Else estimated time is simply stored as is - covers FINISH_LOOP from setCompletionTime */
    else estimatedTime = value;

    updateJobCells();
}

void SchedulerJob::setInSequenceFocus(bool value)
{
    inSequenceFocus = value;
}

void SchedulerJob::setPriority(const uint8_t &value)
{
    priority = value;
}

void SchedulerJob::setEnforceTwilight(bool value)
{
    enforceTwilight = value;
}

void SchedulerJob::setEstimatedTimeCell(QTableWidgetItem *value)
{
    estimatedTimeCell = value;
    if (estimatedTimeCell)
        estimatedTimeCell->setToolTip(i18n("Duration job '%1' will take to complete when started, as estimated by the Scheduler.\n"
                                       "Depends on the actions to be run, and the sequence job to be processed.",
                                       name));
}

void SchedulerJob::setLightFramesRequired(bool value)
{
    lightFramesRequired = value;
}

void SchedulerJob::setRepeatsRequired(const uint16_t &value)
{
    repeatsRequired = value;

    // Update completion condition to be compatible
    if (1 < repeatsRequired)
    {
        if (FINISH_REPEAT != completionCondition)
            setCompletionCondition(FINISH_REPEAT);
    }
    else if (0 < repeatsRequired)
    {
        if (FINISH_SEQUENCE != completionCondition)
            setCompletionCondition(FINISH_SEQUENCE);
    }
    else
    {
        if (FINISH_LOOP != completionCondition)
            setCompletionCondition(FINISH_LOOP);
    }

    updateJobCells();
}

void SchedulerJob::setRepeatsRemaining(const uint16_t &value)
{
    repeatsRemaining = value;
    updateJobCells();
}

void SchedulerJob::setCapturedFramesMap(const CapturedFramesMap &value)
{
    capturedFramesMap = value;
}

void SchedulerJob::setTargetCoords(dms& ra, dms& dec)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->ut().djd());
}

void SchedulerJob::updateJobCells()
{
    if (nullptr != nameCell)
    {
        nameCell->setText(name);
        if (nullptr != nameCell)
            nameCell->tableWidget()->resizeColumnToContents(nameCell->column());
    }

    if (nullptr != nameLabel)
    {
        nameLabel->setText(name + QString(":"));
    }

    if (nullptr != statusCell)
    {
        static QMap<JOBStatus, QString> stateStrings;
        static QString stateStringUnknown;
        if (stateStrings.isEmpty())
        {
            stateStrings[JOB_IDLE] = i18n("Idle");
            stateStrings[JOB_EVALUATION] = i18n("Evaluating");
            stateStrings[JOB_SCHEDULED] = i18n("Scheduled");
            stateStrings[JOB_BUSY] = i18n("Running");
            stateStrings[JOB_INVALID] = i18n("Invalid");
            stateStrings[JOB_COMPLETE] = i18n("Complete");
            stateStrings[JOB_ABORTED] = i18n("Aborted");
            stateStrings[JOB_ERROR] =  i18n("Error");
            stateStringUnknown = i18n("Unknown");
        }
        statusCell->setText(stateStrings.value(state, stateStringUnknown));

        if (nullptr != statusCell->tableWidget())
            statusCell->tableWidget()->resizeColumnToContents(statusCell->column());
    }

    if (nullptr != stageCell || nullptr != stageLabel)
    {
        /* Translated string cache - overkill, probably, and doesn't warn about missing enums like switch/case should ; also, not thread-safe */
        /* FIXME: this should work with a static initializer in C++11, but QT versions are touchy on this, and perhaps i18n can't be used? */
        static QMap<JOBStage, QString> stageStrings;
        static QString stageStringUnknown;
        if (stageStrings.isEmpty())
        {
            stageStrings[STAGE_IDLE] = i18n("Idle");
            stageStrings[STAGE_SLEWING] = i18n("Slewing");
            stageStrings[STAGE_SLEW_COMPLETE] = i18n("Slew complete");
            stageStrings[STAGE_FOCUSING] =
                    stageStrings[STAGE_POSTALIGN_FOCUSING] = i18n("Focusing");
            stageStrings[STAGE_FOCUS_COMPLETE] =
                    stageStrings[STAGE_POSTALIGN_FOCUSING_COMPLETE ] = i18n("Focus complete");
            stageStrings[STAGE_ALIGNING] = i18n("Aligning");
            stageStrings[STAGE_ALIGN_COMPLETE] = i18n("Align complete");
            stageStrings[STAGE_RESLEWING] = i18n("Repositioning");
            stageStrings[STAGE_RESLEWING_COMPLETE] = i18n("Repositioning complete");
            /*stageStrings[STAGE_CALIBRATING] = i18n("Calibrating");*/
            stageStrings[STAGE_GUIDING] = i18n("Guiding");
            stageStrings[STAGE_GUIDING_COMPLETE] = i18n("Guiding complete");
            stageStrings[STAGE_CAPTURING] = i18n("Capturing");
            stageStringUnknown = i18n("Unknown");
        }
        if (nullptr != stageCell)
        {
            stageCell->setText(stageStrings.value(stage, stageStringUnknown));
            if (nullptr != stageCell->tableWidget())
                stageCell->tableWidget()->resizeColumnToContents(stageCell->column());
        }
        if (nullptr != stageLabel)
        {
            stageLabel->setText(QString("%1: %2").arg(name, stageStrings.value(stage, stageStringUnknown)));
        }
    }

    if (nullptr != startupCell)
    {
        /* Display startup time if it is valid */
        if (startupTime.isValid())
        {
            startupCell->setText(QString("%1%2%L3° %4")
                    .arg(altitudeAtStartup < minAltitude ? QString(QChar(0x26A0)) : "")
                    .arg(QChar(isSettingAtStartup ? 0x2193 : 0x2191))
                    .arg(altitudeAtStartup, 0, 'f', 1)
                    .arg(startupTime.toString(dateTimeDisplayFormat)));

            switch (fileStartupCondition)
            {
                /* If the original condition is START_AT/START_CULMINATION, startup time is fixed */
                case START_AT:
                case START_CULMINATION:
                    startupCell->setIcon(QIcon::fromTheme("chronometer"));
                    break;

                /* If the original condition is START_ASAP, startup time is informational */
                case START_ASAP:
                    startupCell->setIcon(QIcon());
                    break;

                default: break;
            }
        }
        /* Else do not display any startup time */
        else
        {
            startupCell->setText("-");
            startupCell->setIcon(QIcon());
        }

        if (nullptr != startupCell->tableWidget())
            startupCell->tableWidget()->resizeColumnToContents(startupCell->column());
    }

    if (nullptr != altitudeCell)
    {
        // FIXME: Cache altitude calculations
        bool is_setting = false;
        double const alt = Ekos::Scheduler::findAltitude(targetCoords, QDateTime(), &is_setting);

        altitudeCell->setText(QString("%1%L2°")
                .arg(QChar(is_setting ? 0x2193 : 0x2191))
                .arg(alt, 0, 'f', 1));

        if (nullptr != altitudeCell->tableWidget())
            altitudeCell->tableWidget()->resizeColumnToContents(altitudeCell->column());
    }

    if (nullptr != completionCell)
    {
        /* Display completion time if it is valid and job is not looping */
        if (FINISH_LOOP != completionCondition && completionTime.isValid())
        {
            completionCell->setText(QString("%1%2%L3° %4")
                    .arg(altitudeAtCompletion < minAltitude ? QString(QChar(0x26A0)) : "")
                    .arg(QChar(isSettingAtCompletion ? 0x2193 : 0x2191))
                    .arg(altitudeAtCompletion, 0, 'f', 1)
                    .arg(completionTime.toString(dateTimeDisplayFormat)));

            switch (completionCondition)
            {
                case FINISH_AT:
                    completionCell->setIcon(QIcon::fromTheme("chronometer"));
                    break;

                case FINISH_SEQUENCE:
                case FINISH_REPEAT:
                default:
                    completionCell->setIcon(QIcon());
                    break;
            }
        }
        /* Else do not display any completion time */
        else
        {
            completionCell->setText("-");
            completionCell->setIcon(QIcon());
        }

        if (nullptr != completionCell->tableWidget())
            completionCell->tableWidget()->resizeColumnToContents(completionCell->column());
    }

    if (nullptr != estimatedTimeCell)
    {
        if (0 < estimatedTime)
            /* Seconds to ms - this doesn't follow dateTimeDisplayFormat, which renders YMD too */
            estimatedTimeCell->setText(QTime::fromMSecsSinceStartOfDay(estimatedTime*1000).toString("HH:mm:ss"));
#if 0
        else if(0 == estimatedTime)
            /* FIXME: this special case could be merged with the previous, kept for future to indicate actual duration */
            estimatedTimeCell->setText("00:00:00");
#endif
        else
            /* Invalid marker */
            estimatedTimeCell->setText("-");

        /* Warn the end-user if estimated time doesn't fit in the startup/completion interval */
        if (estimatedTime < startupTime.secsTo(completionTime))
            estimatedTimeCell->setIcon(QIcon::fromTheme("document-find"));
        else
            estimatedTimeCell->setIcon(QIcon());

        if (nullptr != estimatedTimeCell->tableWidget())
            estimatedTimeCell->tableWidget()->resizeColumnToContents(estimatedTimeCell->column());
    }

    if (nullptr != captureCountCell)
    {
        switch (completionCondition)
        {
            case FINISH_AT:
                // FIXME: Attempt to calculate the number of frames until end - requires detailed imaging time

            case FINISH_LOOP:
                // If looping, display the count of completed frames
                captureCountCell->setText(QString("%L1/-").arg(completedCount));
                break;

            case FINISH_SEQUENCE:
            case FINISH_REPEAT:
            default:
                // If repeating, display the count of completed frames to the count of requested frames
                captureCountCell->setText(QString("%L1/%L2").arg(completedCount).arg(sequenceCount));
                break;
        }

        if (nullptr != captureCountCell->tableWidget())
            captureCountCell->tableWidget()->resizeColumnToContents(captureCountCell->column());
    }

    if (nullptr != scoreCell)
    {
        if (0 <= score)
            scoreCell->setText(QString("%L1").arg(score));
        else
            /* FIXME: negative scores are just weird for the end-user */
            scoreCell->setText("<0");

        if (nullptr != scoreCell->tableWidget())
            scoreCell->tableWidget()->resizeColumnToContents(scoreCell->column());
    }

    if (nullptr != leadTimeCell)
    {
        // Display lead time, plus a warning if lead time is more than twice the lead time of the Ekos options
        switch (state)
        {
            case JOB_INVALID:
            case JOB_ERROR:
            case JOB_COMPLETE:
                leadTimeCell->setText("-");
                break;

            default:
                leadTimeCell->setText(QString("%1%2")
                        .arg(Options::leadTime() * 60 * 2 < leadTime ? QString(QChar(0x26A0)) : "")
                        .arg(QTime::fromMSecsSinceStartOfDay(leadTime*1000).toString("HH:mm:ss")));
                break;
        }

        if (nullptr != leadTimeCell->tableWidget())
            leadTimeCell->tableWidget()->resizeColumnToContents(leadTimeCell->column());
    }
}

void SchedulerJob::reset()
{
    state = JOB_IDLE;
    stage = STAGE_IDLE;
    estimatedTime = -1;
    leadTime = 0;
    startupCondition = fileStartupCondition;
    startupTime = fileStartupCondition == START_AT ? fileStartupTime : QDateTime();
    /* No change to culmination offset */
    repeatsRemaining = repeatsRequired;
    updateJobCells();
}

bool SchedulerJob::decreasingScoreOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getScore() > job2->getScore();
}

bool SchedulerJob::increasingPriorityOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getPriority() < job2->getPriority();
}

bool SchedulerJob::decreasingAltitudeOrder(SchedulerJob const *job1, SchedulerJob const *job2, QDateTime const &when)
{
    bool A_is_setting = job1->isSettingAtStartup;
    double const altA = when.isValid() ?
        Ekos::Scheduler::findAltitude(job1->getTargetCoords(), when, &A_is_setting) :
        job1->altitudeAtStartup;

    bool B_is_setting = job2->isSettingAtStartup;
    double const altB = when.isValid() ?
        Ekos::Scheduler::findAltitude(job2->getTargetCoords(), when, &B_is_setting) :
        job2->altitudeAtStartup;

    // Sort with the setting target first
    if (A_is_setting && !B_is_setting)
        return true;
    else if (!A_is_setting && B_is_setting)
        return false;

    // If both targets rise or set, sort by decreasing altitude, considering a setting target is prioritary
    return (A_is_setting && B_is_setting) ? altA < altB : altB < altA;
}

bool SchedulerJob::increasingStartupTimeOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getStartupTime() < job2->getStartupTime();
}
