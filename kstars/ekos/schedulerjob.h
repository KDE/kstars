/*  Ekos Scheduler Job
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef SchedulerJob_H
#define SchedulerJob_H

#include <QUrl>
#include <QTableWidgetItem>

#include "dms.h"
#include "skypoint.h"

class SchedulerJob
{
public:
    SchedulerJob();
    ~SchedulerJob();

    typedef enum { JOB_IDLE, JOB_EVALUATION, JOB_SCHEDULED, JOB_BUSY, JOB_ERROR, JOB_ABORTED, JOB_INVALID, JOB_COMPLETE } JOBStatus;
    typedef enum { STAGE_IDLE, STAGE_SLEWING, STAGE_SLEW_COMPLETE, STAGE_FOCUSING, STAGE_FOCUS_COMPLETE, STAGE_ALIGNING, STAGE_ALIGN_COMPLETE, STAGE_CALIBRATING, STAGE_GUIDING, STAGE_CAPTURING, STAGE_COMPLETE} JOBStage;
    typedef enum { FITS_IDLE, FITS_SOLVING, FITS_COMPLETE, FITS_ERROR } FITSStatus;
    typedef enum { START_NOW, START_CULMINATION, START_AT } StartupCondition;
    typedef enum { FINISH_SEQUENCE, FINISH_LOOP, FINISH_AT } CompletionCondition;
    typedef enum { USE_NONE  = 0,
                   USE_FOCUS = 1 << 0,
                   USE_ALIGN = 1 << 1,
                   USE_GUIDE = 1 << 2 } ModuleUsage;

    QString getName() const;
    void setName(const QString &value);

    void setTargetCoords(dms ra, dms dec);
    SkyPoint & getTargetCoords();

    StartupCondition getStartingCondition() const;
    void setStartupCondition(const StartupCondition &value);

    QDateTime getStartupTime() const;
    void setStartupTime(const QDateTime &value);

    QUrl getSequenceFile() const;
    void setSequenceFile(const QUrl &value);

    QUrl getFITSFile() const;
    void setFITSFile(const QUrl &value);

    double getMinAltitude() const;
    void setMinAltitude(const double &value);

    double getMinMoonSeparation() const;
    void setMinMoonSeparation(const double &value);

    bool getEnforceWeather() const;
    void setEnforceWeather(bool value);

    bool getNoMeridianFlip() const;
    void setNoMeridianFlip(bool value);

    QDateTime getCompletionTime() const;
    void setCompletionTime(const QDateTime &value);

    CompletionCondition getCompletionCondition() const;
    void setCompletionCondition(const CompletionCondition &value);

    ModuleUsage getModuleUsage() const;
    void setModuleUsage(const ModuleUsage &value);

    void setStatusCell(QTableWidgetItem *cell) { statusCell = cell; }

    JOBStatus getState() const;
    void setState(const JOBStatus &value);

    FITSStatus getFITSState() const;
    void setFITSState(const FITSStatus &value);

    int getScore() const;
    void setScore(int value);

    uint16_t getCulminationOffset() const;
    void setCulminationOffset(const uint16_t &value);

    QTableWidgetItem *getStartupCell() const;
    void setStartupCell(QTableWidgetItem *value);

    void setDateTimeDisplayFormat(const QString &value);

    JOBStage getStage() const;
    void setStage(const JOBStage &value);

private:

    QString name;
    SkyPoint targetCoords;
    JOBStatus state;
    FITSStatus fitsState;

    JOBStage stage;

    StartupCondition startupCondition;
    CompletionCondition completionCondition;

    QDateTime startupTime;
    QDateTime completionTimeEdit;

    QUrl sequenceFile;
    QUrl fitsFile;

    double minAltitude;
    double minMoonSeparation;

    bool enforceWeather;
    bool noMeridianFlip;    

    ModuleUsage moduleUsage;

    QTableWidgetItem* statusCell, *startupCell;

    int score;
    uint16_t culminationOffset;

    double Dawn, Dusk;    
    QString dateTimeDisplayFormat;


};

#endif // SchedulerJob_H
