/*  Ekos Scheduler Module
    Copyright (C) 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QMainWindow>
#include "ui_scheduler.h"
#include <QtDBus/QtDBus>
#include "scheduler.h"
#include "kstars.h"
#include "schedulerjob.h"
#include "skyobjects/ksmoon.h"
#include "QProgressIndicator.h"
#include "ui_scheduler.h"

namespace Ekos {

class Scheduler : public QWidget, public Ui::Scheduler
{
    Q_OBJECT

public:
    enum StateChoice{IDLE, STARTING_EKOS, EKOS_STARTED, CONNECTING, CONNECTED,  READY, FINISHED, ABORTED};

     Scheduler();
    ~Scheduler();
     int checkWeather();
     void startEkos();
     void updateJobInfo(Schedulerjob *o);
     void appendLogText(const QString &);
     QString getLogText() { return logText.join("\n"); }
     void startSlew();
     void startFocusing();
     void startAstrometry();
     void startGuiding();
     void startCapture();
     void getNextAction();
     void connectDevices();

     Schedulerjob *getCurrentjob() const;
     void setCurrentjob(Schedulerjob *value);
     void terminateJob(Schedulerjob *value);
     void executeJob(Schedulerjob *value);

     StateChoice getState() const;
     void setState(const StateChoice &value);

public slots:
     void selectSlot();
     void addToTableSlot();
     void removeTableSlot();
    void setSequenceSlot();
    void startSlot();
    void saveSlot();
    void evaluateJobs();
    void checkJobStatus();
    void stopindi();

signals:
        void newLog();

private:
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface *focusinterface;
    QDBusInterface *ekosinterface;
    QDBusInterface *captureinterface;
    QDBusInterface *mountinterface;
    QDBusInterface *aligninterface;
    StateChoice state;
    QProgressIndicator *pi;
    Ekos::Scheduler *ui;
    KSMoon Moon;
    SkyPoint *moon;
    int tableCountRow;
    int tableCountCol;
    int iterations ;
    QVector<Schedulerjob> objects;
    SkyObject *o;
    QStringList logText;
    Schedulerjob *currentjob;
};
}

#endif // SCHEDULER_H
