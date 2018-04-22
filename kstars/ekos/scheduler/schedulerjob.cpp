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

SkyPoint const & SchedulerJob::getTargetCoords() const
{
    return targetCoords;
}

SchedulerJob::StartupCondition SchedulerJob::getStartupCondition() const
{
    return startupCondition;
}

void SchedulerJob::setStartupCondition(const StartupCondition &value)
{
    startupCondition = value;
}

void SchedulerJob::setStartupTime(const QDateTime &value)
{
    startupTime = value;
    updateJobCell();
}

QUrl SchedulerJob::getSequenceFile() const
{
    return sequenceFile;
}

void SchedulerJob::setSequenceFile(const QUrl &value)
{
    sequenceFile = value;
}
QUrl SchedulerJob::getFITSFile() const
{
    return fitsFile;
}
void SchedulerJob::setFITSFile(const QUrl &value)
{
    fitsFile = value;
}
double SchedulerJob::getMinAltitude() const
{
    return minAltitude;
}

void SchedulerJob::setMinAltitude(const double &value)
{
    minAltitude = value;
}
double SchedulerJob::getMinMoonSeparation() const
{
    return minMoonSeparation;
}

void SchedulerJob::setMinMoonSeparation(const double &value)
{
    minMoonSeparation = value;
}

bool SchedulerJob::getEnforceWeather() const
{
    return enforceWeather;
}

void SchedulerJob::setEnforceWeather(bool value)
{
    enforceWeather = value;
}
QDateTime SchedulerJob::getCompletionTime() const
{
    return completionTime;
}

void SchedulerJob::setCompletionTime(const QDateTime &value)
{
    completionTime = value;
}

SchedulerJob::CompletionCondition SchedulerJob::getCompletionCondition() const
{
    return completionCondition;
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;
}

SchedulerJob::StepPipeline SchedulerJob::getStepPipeline() const
{
    return stepPipeline;
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

int16_t SchedulerJob::getCulminationOffset() const
{
    return culminationOffset;
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

QString const & SchedulerJob::getDateTimeDisplayFormat()
{
    return dateTimeDisplayFormat;
}

void SchedulerJob::setDateTimeDisplayFormat(const QString &value)
{
    dateTimeDisplayFormat = value;
}

void SchedulerJob::setStage(const JOBStage &value)
{
    stage = value;
    updateJobCell();
}

SchedulerJob::StartupCondition SchedulerJob::getFileStartupCondition() const
{
    return fileStartupCondition;
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

/* FIXME: unrelated to model, move this in the view */
void SchedulerJob::updateJobCell()
{
    if (nameCell)
    {
        nameCell->setText(name);
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
    }

    if (stageCell)
    {
        /* Translated string cache - overkill, probably, and doesn't warn about missing enums like switch/case shouldi ; also, not thread-safe */
        /* FIXME: this should work with a static initializer in C++11, but QT versions are touchy on this, and perhaps i18n can't be used? */
        static QMap<JOBStage, QString> stageStrings;
        static QString stageStringUnknown;
        if (stageStrings.isEmpty())
        {
            stageStrings[STAGE_IDLE] = "";
            stageStrings[STAGE_SLEWING] = i18n("Slewing");
            stageStrings[STAGE_SLEW_COMPLETE] = i18n("Slew complete");
            stageStrings[STAGE_FOCUSING] =
            stageStrings[STAGE_POSTALIGN_FOCUSING] = i18n("Focusing");
            stageStrings[STAGE_FOCUS_COMPLETE] = i18n("Focus complete");
            stageStrings[STAGE_ALIGNING] = i18n("Aligning");
            stageStrings[STAGE_ALIGN_COMPLETE] = i18n("Align complete");
            stageStrings[STAGE_RESLEWING] = i18n("Repositioning");
            stageStrings[STAGE_RESLEWING_COMPLETE] = i18n("Repositioning complete");
            /*stageStrings[STAGE_CALIBRATING] = i18n("Calibrating"); */
            stageStrings[STAGE_GUIDING] = i18n("Guiding");
            stageStrings[STAGE_CAPTURING] = i18n("Capturing");
            stageStringUnknown = i18n("Unknown");
        }
        stageCell->setText(stageStrings.value(stage, stageStringUnknown));
    }

    if (startupCell)
    {
        startupCell->setText(startupTime.toString(dateTimeDisplayFormat));
    }

    if (completionCell)
    {
        completionCell->setText(completionTime.toString(dateTimeDisplayFormat));
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
    }

    if (captureCountCell)
    {
        captureCountCell->setText(QString("%1/%2").arg(completedCount).arg(sequenceCount));
    }

    if (scoreCell)
    {
        if (0 <= score)
            scoreCell->setText(QString("%1").arg(score));
        else
            /* FIXME: negative scores are just weird for the end-user */
            scoreCell->setText(QString("<0"));
    }
}

bool SchedulerJob::getInSequenceFocus() const
{
    return inSequenceFocus;
}

void SchedulerJob::setInSequenceFocus(bool value)
{
    inSequenceFocus = value;
}

uint8_t SchedulerJob::getPriority() const
{
    return priority;
}

void SchedulerJob::setPriority(const uint8_t &value)
{
    priority = value;
}

bool SchedulerJob::getEnforceTwilight() const
{
    return enforceTwilight;
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

bool SchedulerJob::getLightFramesRequired() const
{
    return lightFramesRequired;
}

void SchedulerJob::setLightFramesRequired(bool value)
{
    lightFramesRequired = value;
}

uint16_t SchedulerJob::getRepeatsRequired() const
{
    return repeatsRequired;
}

void SchedulerJob::setRepeatsRequired(const uint16_t &value)
{
    repeatsRequired = value;
}

uint16_t SchedulerJob::getRepeatsRemaining() const
{
    return repeatsRemaining;
}

void SchedulerJob::setRepeatsRemaining(const uint16_t &value)
{
    repeatsRemaining = value;
    updateJobCell();
}

QMap<QString, uint16_t> SchedulerJob::getCapturedFramesMap() const
{
    return capturedFramesMap;
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
