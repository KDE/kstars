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

    typedef enum { JOB_IDLE, JOB_BUSY, JOB_ERROR, JOB_ABORTED, JOB_DONE } JOBStatus;
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

#if 0
    enum JobState{IDLE, READY, SLEWING, SLEW_COMPLETE, FOCUSING, FOCUSING_COMPLETE, ALIGNING, ALIGNING_COMPLETE, GUIDING, GUIDING_COMPLETE,
                     CAPTURING, CAPTURING_COMPLETE, ABORTED};
#endif

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

    QDateTime getcompletionTimeEdit() const;
    void setcompletionTimeEdit(const QDateTime &value);

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

private:

    QString name;
    SkyPoint targetCoords;
    JOBStatus state;
    FITSStatus fitsState;

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

    QTableWidgetItem* statusCell;

    int score;
    uint16_t culminationOffset;


};

#endif // SchedulerJob_H
