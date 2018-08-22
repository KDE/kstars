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
#include "scheduler.h"

#include <knotification.h>

#include <QTableWidgetItem>

void SchedulerJob::setName(const QString &value)
{
    name = value;
    updateJobCell();
}

void SchedulerJob::setStartupCondition(const StartupCondition &value)
{
    startupCondition = value;
    if (value == START_ASAP)
        startupTime = QDateTime();
    updateJobCell();
}

void SchedulerJob::setStartupTime(const QDateTime &value)
{
    startupTime = value;

    if (value.isValid())
        startupCondition = START_AT;

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
    /* If argument completion time is valid, automatically switch condition to FINISH_AT */
    if (value.isValid())
    {
        setCompletionCondition(FINISH_AT);
        completionTime = value;
    }
    /* If completion time is not valid, but startup time is, deduce completion from startup and duration */
    else if (startupTime.isValid())
    {
        completionTime = startupTime.addSecs(estimatedTime);
    }

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;
    updateJobCell();
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

    updateJobCell();
}

void SchedulerJob::setScore(int value)
{
    score = value;
    updateJobCell();
}

void SchedulerJob::setCulminationOffset(const int16_t &value)
{
    culminationOffset = value;
}

void SchedulerJob::setSequenceCount(const int count)
{
    sequenceCount = count;
    updateJobCell();
}

void SchedulerJob::setNameCell(QTableWidgetItem *value)
{
    nameCell = value;
    updateJobCell();
}

void SchedulerJob::setCompletedCount(const int count)
{
    completedCount = count;
    updateJobCell();
}

void SchedulerJob::setStatusCell(QTableWidgetItem *value)
{
    statusCell = value;
    updateJobCell();
    if (statusCell)
        statusCell->setToolTip(i18n("Current status of job '%1', managed by the Scheduler.\n"
                                    "If invalid, the Scheduler was not able to find a proper observation time for the target.\n"
                                    "If aborted, the Scheduler missed the scheduled time or encountered transitory issues and will reschedule the job.\n"
                                    "If complete, the Scheduler verified that all sequence captures requested were stored, including repeats.",
                                    name));
}

void SchedulerJob::setStartupCell(QTableWidgetItem *value)
{
    startupCell = value;
    updateJobCell();
    if (startupCell)
        startupCell->setToolTip(i18n("Startup time for job '%1', as estimated by the Scheduler.\n"
                                     "Fixed time from user or culmination time is marked with a chronometer symbol. ",
                                     name));
}

void SchedulerJob::setCompletionCell(QTableWidgetItem *value)
{
    completionCell = value;
    updateJobCell();
    if (completionCell)
        completionCell->setToolTip(i18n("Completion time for job '%1', as estimated by the Scheduler.\n"
                                        "Can be specified by the user to limit duration of looping jobs.\n"
                                        "Fixed time from user is marked with a chronometer symbol. ",
                                        name));
}

void SchedulerJob::setCaptureCountCell(QTableWidgetItem *value)
{
    captureCountCell = value;
    updateJobCell();
    if (captureCountCell)
        captureCountCell->setToolTip(i18n("Count of captures stored for job '%1', based on its sequence job.\n"
                                          "This is a summary, additional specific frame types may be required to complete the job.",
                                          name));
}

void SchedulerJob::setScoreCell(QTableWidgetItem *value)
{
    scoreCell = value;
    updateJobCell();
    if (scoreCell)
        scoreCell->setToolTip(i18n("Current score for job '%1', from its altitude, moon separation and sky darkness.\n"
                                   "Negative if adequate altitude is not achieved yet or if there is no proper observation time today.\n"
                                   "The Scheduler will refresh scores when picking a new candidate job.",
                                   name));
}

void SchedulerJob::setDateTimeDisplayFormat(const QString &value)
{
    dateTimeDisplayFormat = value;
    updateJobCell();
}

void SchedulerJob::setStage(const JOBStage &value)
{
    stage = value;
    updateJobCell();
}

void SchedulerJob::setStageCell(QTableWidgetItem *cell)
{
    stageCell = cell;
    updateJobCell();
}

void SchedulerJob::setStageLabel(QLabel *label)
{
    stageLabel = label;
    updateJobCell();
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
    /* If startup and completion times are fixed, estimated time cannot change */
    if (START_AT == startupCondition && FINISH_AT == completionCondition)
    {
        estimatedTime = startupTime.secsTo(completionTime);
    }
    /* If startup time is fixed but not completion time, estimated time adjusts completion time */
    else if (START_AT == startupCondition)
    {
        estimatedTime = value;
        completionTime = startupTime.addSecs(value);
    }
    /* If completion time is fixed but not startup time, estimated time adjusts startup time */
    /* FIXME: adjusting startup time will probably not work, because jobs are scheduled from first available altitude */
    else if (FINISH_AT == completionCondition)
    {
        estimatedTime = value;
        startupTime = completionTime.addSecs(-value);
    }
    /* Else estimated time is simply stored as is */
    else estimatedTime = value;

    updateJobCell();
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
    updateJobCell();
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
    updateJobCell();
}

void SchedulerJob::setRepeatsRemaining(const uint16_t &value)
{
    repeatsRemaining = value;
    updateJobCell();
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

void SchedulerJob::updateJobCell()
{
    if (nameCell)
    {
        nameCell->setText(name);
        nameCell->tableWidget()->resizeColumnToContents(nameCell->column());
    }

    if (nameLabel)
    {
        nameLabel->setText(name + QString(":"));
    }

    if (statusCell)
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
        statusCell->tableWidget()->resizeColumnToContents(statusCell->column());
    }

    if (stageCell || stageLabel)
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
        if (stageCell)
        {
            stageCell->setText(stageStrings.value(stage, stageStringUnknown));
            stageCell->tableWidget()->resizeColumnToContents(stageCell->column());
        }
        if (stageLabel)
        {
            stageLabel->setText(QString("%1: %2").arg(name, stageStrings.value(stage, stageStringUnknown)));
        }
    }

    if (startupCell && startupCell->tableWidget())
    {
        /* Display a startup time if job is running, scheduled to run or about to be re-scheduled */
        if (JOB_SCHEDULED == state || JOB_BUSY == state || JOB_ABORTED == state) switch (fileStartupCondition)
        {
            /* If the original condition is START_AT/START_CULMINATION, startup time is fixed */
            case START_AT:
            case START_CULMINATION:
                startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
                startupCell->setIcon(QIcon::fromTheme("chronometer"));
                break;

            /* If the original condition is START_ASAP, startup time is informational */
            case START_ASAP:
                startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
                startupCell->setIcon(QIcon());
                break;

            /* Else do not display any startup time */
            default:
                startupCell->setText(QString());
                startupCell->setIcon(QIcon());
                break;
        }
        /* Display a missed startup time if job is invalid */
        else if (JOB_INVALID == state && START_AT == fileStartupCondition)
        {
            startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
            startupCell->setIcon(QIcon::fromTheme("chronometer"));
        }
        /* Else do not display any startup time */
        else
        {
            startupCell->setText(QString());
            startupCell->setIcon(QIcon());
        }

        startupCell->tableWidget()->resizeColumnToContents(startupCell->column());
    }

    if (completionCell && completionCell->tableWidget())
    {
        /* Display a completion time if job is running, scheduled to run or about to be re-scheduled */
        if (JOB_SCHEDULED == state || JOB_BUSY == state || JOB_ABORTED == state) switch (completionCondition)
        {
            case FINISH_LOOP:
                completionCell->setText(QString("-"));
                completionCell->setIcon(QIcon());
                break;

            case FINISH_AT:
                completionCell->setText(completionTime.toString(dateTimeDisplayFormat));
                completionCell->setIcon(QIcon::fromTheme("chronometer"));
                break;

            case FINISH_SEQUENCE:
            case FINISH_REPEAT:
            default:
                completionCell->setText(completionTime.toString(dateTimeDisplayFormat));
                completionCell->setIcon(QIcon());
                break;
        }
        /* Else do not display any completion time */
        else
        {
            completionCell->setText(QString());
            completionCell->setIcon(QIcon());
        }

        completionCell->tableWidget()->resizeColumnToContents(completionCell->column());
    }

    if (estimatedTimeCell && estimatedTimeCell->tableWidget())
    {
        if (0 < estimatedTime)
            /* Seconds to ms - this doesn't follow dateTimeDisplayFormat, which renders YMD too */
            estimatedTimeCell->setText(QTime::fromMSecsSinceStartOfDay(estimatedTime*1000).toString("HH:mm:ss"));
        else if(0 == estimatedTime)
            /* FIXME: this special case could be merged with the previous, kept for future to indicate actual duration */
            estimatedTimeCell->setText("00:00:00");
        else
            /* Invalid marker */
            estimatedTimeCell->setText("-");

        estimatedTimeCell->tableWidget()->resizeColumnToContents(estimatedTimeCell->column());
    }

    if (captureCountCell && captureCountCell->tableWidget())
    {
        captureCountCell->setText(QString("%1/%2").arg(completedCount).arg(sequenceCount));
        captureCountCell->tableWidget()->resizeColumnToContents(captureCountCell->column());
    }

    if (scoreCell && scoreCell->tableWidget())
    {
        if (0 <= score)
            scoreCell->setText(QString("%1").arg(score));
        else
            /* FIXME: negative scores are just weird for the end-user */
            scoreCell->setText(QString("<0"));

        scoreCell->tableWidget()->resizeColumnToContents(scoreCell->column());
    }
}

void SchedulerJob::reset()
{
    state = JOB_IDLE;
    stage = STAGE_IDLE;
    estimatedTime = -1;
    startupCondition = fileStartupCondition;
    startupTime = fileStartupCondition == START_AT ? fileStartupTime : QDateTime();
    /* No change to culmination offset */
    repeatsRemaining = repeatsRequired;
}

bool SchedulerJob::decreasingScoreOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getScore() > job2->getScore();
}

bool SchedulerJob::increasingPriorityOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getPriority() < job2->getPriority();
}

bool SchedulerJob::decreasingAltitudeOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return  Ekos::Scheduler::findAltitude(job1->getTargetCoords(), job1->getStartupTime()) >
            Ekos::Scheduler::findAltitude(job2->getTargetCoords(), job2->getStartupTime());
}

bool SchedulerJob::increasingStartupTimeOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getStartupTime() < job2->getStartupTime();
}
