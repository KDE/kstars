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
    startupCondition    = START_NOW;
    completionCondition = FINISH_SEQUENCE;
    moduleUsage         = USE_NONE;
    state               = JOB_IDLE;
    fitsState           = FITS_IDLE;

    statusCell          = NULL;
    startupCell         = NULL;
    minAltitude         = -1;
    minMoonSeparation   = -1;

    culminationOffset   = 0;



    #if 0
    NowCheck=false;
    specificTime=false;
    specificAlt=false;
    moonSeparationCheck=false;
    meridianFlip=false;
    isFITSSelected=false;
    whenSeqCompCheck=false;
    loopCheck=false;
    onTimeCheck=false;
    score=0;
    alt=-1;
    isOk = 0;
    #endif
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

SchedulerJob::StartupCondition SchedulerJob::getStartingCondition() const
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
bool SchedulerJob::getNoMeridianFlip() const
{
    return noMeridianFlip;
}

void SchedulerJob::setNoMeridianFlip(bool value)
{
    noMeridianFlip = value;
}
QDateTime SchedulerJob::getcompletionTimeEdit() const
{
    return completionTimeEdit;
}

void SchedulerJob::setcompletionTimeEdit(const QDateTime &value)
{
    completionTimeEdit = value;
}

SchedulerJob::CompletionCondition SchedulerJob::getCompletionCondition() const
{
    return completionCondition;
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;
}

SchedulerJob::ModuleUsage SchedulerJob::getModuleUsage() const
{
    return moduleUsage;
}

void SchedulerJob::setModuleUsage(const ModuleUsage &value)
{
    moduleUsage = value;
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
            statusCell->setText(xi18n("Idle"));
            break;

        case JOB_EVALUATION:
            statusCell->setText(xi18n("Evaluating"));
            break;

        case JOB_SCHEDULED:
            statusCell->setText(xi18n("Scheduled"));
            break;

        case JOB_BUSY:
            statusCell->setText(xi18n("Busy"));
            break;


        case JOB_INVALID:
            statusCell->setText(xi18n("Invalid"));
            break;

        default:
            statusCell->setText(xi18n("Unknown"));
            break;


    }
}

SchedulerJob::FITSStatus SchedulerJob::getFITSState() const
{
    return fitsState;
}

void SchedulerJob::setFITSState(const FITSStatus &value)
{
    fitsState = value;

    switch (fitsState)
    {
        case FITS_SOLVING:
            statusCell->setText(xi18n("Solving FITS"));
            break;

        case FITS_ERROR:
            statusCell->setText(xi18n("Solver failed"));
            break;

        case FITS_COMPLETE:
            statusCell->setText(xi18n("Solver completed"));
            break;

        default:
        break;
    }
}
int SchedulerJob::getScore() const
{
    return score;
}

void SchedulerJob::setScore(int value)
{
    score = value;
}
uint16_t SchedulerJob::getCulminationOffset() const
{
    return culminationOffset;
}

void SchedulerJob::setCulminationOffset(const uint16_t &value)
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




void SchedulerJob::setTargetCoords(dms ra, dms dec)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.updateCoords(KStarsData::Instance()->updateNum());
}


