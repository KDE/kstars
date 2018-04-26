/*  Ekos Scheduler Job
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "schedulerjob.h"
#include "scheduler.h"

#include "dms.h"
#include "kstarsdata.h"

#include <knotification.h>

#include <QTableWidgetItem>

SchedulerJob::SchedulerJob()
{
}

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
    updateJobCell();
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
    completionTime = value;
    updateJobCell();
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;
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
}

void SchedulerJob::setStartupCell(QTableWidgetItem *value)
{
    startupCell = value;
    updateJobCell();
}

void SchedulerJob::setCompletionCell(QTableWidgetItem *value)
{
    completionCell = value;
    updateJobCell();
}

void SchedulerJob::setCaptureCountCell(QTableWidgetItem *value)
{
    captureCountCell = value;
    updateJobCell();
}

void SchedulerJob::setScoreCell(QTableWidgetItem *value)
{
    scoreCell = value;
    updateJobCell();
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

void SchedulerJob::setEstimatedTime(const int64_t &value)
{
    estimatedTime = value;
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

void SchedulerJob::setCapturedFramesMap(const QMap<QString, uint16_t> &value)
{
    capturedFramesMap = value;
}

void SchedulerJob::setTargetCoords(dms& ra, dms& dec)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.updateCoordsNow(KStarsData::Instance()->updateNum());
}

/* FIXME: unrelated to model, move this in the view */
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
        /* Translated string cache - overkill, probably, and doesn't warn about missing enums like switch/case shouldi ; also, not thread-safe */
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
            stageLabel->setText(QString("%1: %2").arg(name).arg(stageStrings.value(stage, stageStringUnknown)));
        }
    }

    if (startupCell)
    {
        startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
        startupCell->tableWidget()->resizeColumnToContents(startupCell->column());
    }

    if (completionCell)
    {
        completionCell->setText(completionTime.toString(dateTimeDisplayFormat));
        completionCell->tableWidget()->resizeColumnToContents(completionCell->column());
    }

    if (estimatedTimeCell)
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

    if (captureCountCell)
    {
        captureCountCell->setText(QString("%1/%2").arg(completedCount).arg(sequenceCount));
        captureCountCell->tableWidget()->resizeColumnToContents(captureCountCell->column());
    }

    if (scoreCell)
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
    startupTime = fileStartupCondition == START_AT ? startupTime : QDateTime();
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
