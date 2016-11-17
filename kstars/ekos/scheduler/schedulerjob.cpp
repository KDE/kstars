/*  Ekos Scheduler Job
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <KLocalizedString>

#include "schedulerjob.h"
#include "kstarsdata.h"

SchedulerJob::SchedulerJob()
{
    startupCondition    = START_ASAP;
    completionCondition = FINISH_SEQUENCE;
    stepPipeline         = USE_NONE;
    state               = JOB_IDLE;
    stage               = STAGE_IDLE;

    timeSlotAllocated   = false;
    inSequenceFocus     = false;
    enforceWeather      = false;

    priority            = 10;

    statusCell          = NULL;
    startupCell         = NULL;
    estimatedTimeCell   = NULL;
    minAltitude         = -1;
    minMoonSeparation   = -1;
    estimatedTime       = -1;
    culminationOffset   = 0;
}

SchedulerJob::~SchedulerJob()
{

}

QString SchedulerJob::getName() const
{
return name;
}

void SchedulerJob::setName(const QString &value)
{
name = value;
}

SkyPoint & SchedulerJob::getTargetCoords()
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

QDateTime SchedulerJob::getStartupTime() const
{
    return startupTime;
}

void SchedulerJob::setStartupTime(const QDateTime &value)
{
    startupTime = value;

    if (startupCell)
        startupCell->setText(startupTime.toString(dateTimeDisplayFormat));

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

SchedulerJob::JOBStatus SchedulerJob::getState() const
{
    return state;
}

void SchedulerJob::setState(const JOBStatus &value)
{
    state = value;

    if (statusCell == NULL)
        return;

    switch (state)
    {
        case JOB_IDLE:
            statusCell->setText(i18n("Idle"));
            break;

        case JOB_EVALUATION:
            statusCell->setText(i18n("Evaluating"));
            break;

        case JOB_SCHEDULED:
            statusCell->setText(i18n("Scheduled"));
            break;

        case JOB_BUSY:
            statusCell->setText(i18n("Running"));
            break;

        case JOB_INVALID:
            statusCell->setText(i18n("Invalid"));
            break;

        case JOB_COMPLETE:
            statusCell->setText(i18n("Complete"));
            break;

        case JOB_ABORTED:
            statusCell->setText(i18n("Aborted"));
            break;

        case JOB_ERROR:
            statusCell->setText(i18n("Error"));
            break;

        default:
            statusCell->setText(i18n("Unknown"));
            break;


    }
}

/*SchedulerJob::FITSStatus SchedulerJob::getFITSState() const
{
    return fitsState;
}

void SchedulerJob::setFITSState(const FITSStatus &value)
{
    fitsState = value;

    switch (fitsState)
    {
        case FITS_SOLVING:
            statusCell->setText(i18n("Solving FITS"));
            break;

        case FITS_ERROR:
            statusCell->setText(i18n("Solver failed"));
            break;

        case FITS_COMPLETE:
            statusCell->setText(i18n("Solver completed"));
            break;

        default:
        break;
    }
}*/

int SchedulerJob::getScore() const
{
    return score;
}

void SchedulerJob::setScore(int value)
{
    score = value;
}

int16_t SchedulerJob::getCulminationOffset() const
{
    return culminationOffset;
}

void SchedulerJob::setCulminationOffset(const int16_t &value)
{
    culminationOffset = value;
}

QTableWidgetItem *SchedulerJob::getStartupCell() const
{
    return startupCell;
}

void SchedulerJob::setStartupCell(QTableWidgetItem *value)
{
    startupCell = value;
}
void SchedulerJob::setDateTimeDisplayFormat(const QString &value)
{
    dateTimeDisplayFormat = value;
}

SchedulerJob::JOBStage SchedulerJob::getStage() const
{
    return stage;
}

void SchedulerJob::setStage(const JOBStage &value)
{
    stage = value;

    switch (stage)
    {
        case STAGE_SLEWING:
            statusCell->setText(i18n("Slewing"));
            break;

        case STAGE_SLEW_COMPLETE:
            statusCell->setText(i18n("Slew complete"));
            break;

        case STAGE_FOCUSING:
        case STAGE_POSTALIGN_FOCUSING:
            statusCell->setText(i18n("Focusing"));
            break;

        case STAGE_FOCUS_COMPLETE:
            statusCell->setText(i18n("Focus complete"));
            break;

        case STAGE_ALIGNING:
            statusCell->setText(i18n("Aligning"));
            break;

        case STAGE_ALIGN_COMPLETE:
            statusCell->setText(i18n("Align complete"));
            break;

        case STAGE_RESLEWING:
            statusCell->setText(i18n("Repositioning"));
            break;

        case STAGE_RESLEWING_COMPLETE:
            statusCell->setText(i18n("Repositioning complete"));
            break;

        /*case STAGE_CALIBRATING:
            statusCell->setText(i18n("Calibrating"));
            break;*/

        case STAGE_GUIDING:
            statusCell->setText(i18n("Guiding"));
            break;

        case STAGE_CAPTURING:
            statusCell->setText(i18n("Capturing"));
            break;

        default:
        break;
    }
}

SchedulerJob::StartupCondition SchedulerJob::getFileStartupCondition() const
{
    return fileStartupCondition;
}

void SchedulerJob::setFileStartupCondition(const StartupCondition &value)
{
    fileStartupCondition = value;
}
double SchedulerJob::getEstimatedTime() const
{
    return estimatedTime;
}

void SchedulerJob::setEstimatedTime(const double &value)
{
    estimatedTime = value;

    if (estimatedTimeCell)
    {
        // Seconds to ms
        QTime estimatedTime = QTime::fromMSecsSinceStartOfDay(value*1000);
        estimatedTimeCell->setText(estimatedTime.toString("HH:mm:ss"));
    }
}
bool SchedulerJob::getTimeSlotAllocated() const
{
    return timeSlotAllocated;
}

void SchedulerJob::setTimeSlotAllocated(bool value)
{
    timeSlotAllocated = value;
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

QString SchedulerJob::getProfile() const
{
    return profile;
}

void SchedulerJob::setProfile(const QString &value)
{
    profile = value;
}

QTableWidgetItem *SchedulerJob::getEstimatedTimeCell() const
{
    return estimatedTimeCell;
}

void SchedulerJob::setEstimatedTimeCell(QTableWidgetItem *value)
{
    estimatedTimeCell = value;
}

bool SchedulerJob::getLightFramesRequired() const
{
    return lightFramesRequired;
}

void SchedulerJob::setLightFramesRequired(bool value)
{
    lightFramesRequired = value;
}



void SchedulerJob::setTargetCoords(dms ra, dms dec)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.updateCoordsNow(KStarsData::Instance()->updateNum());
}


