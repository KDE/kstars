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

#include "dms.h"
#include "skypoint.h"

class SchedulerJob
{
public:
    SchedulerJob();
    ~SchedulerJob();

    typedef enum { START_NOW, START_CULMINATION, START_AT } StartupCondition;

    QString getName() const;
    void setName(const QString &value);

    void setTargetCoords(dms ra, dms dec);
    const SkyPoint & getTargetCoords() const;

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

    QUrl getFitsFile() const;
    void setFitsFile(const QUrl &value);

private:

    QString name;
    SkyPoint targetCoords;

    StartupCondition startupCondition;

    QDateTime startupTime;

    QUrl sequenceFile;
    QUrl fitsFile;


};

#endif // SchedulerJob_H
