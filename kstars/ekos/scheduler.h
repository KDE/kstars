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
     Scheduler();
    ~Scheduler();
     int checkWeather();
     void processSession(Schedulerjob o);
     void stopindi();
     void startEkos();
     void updateJobInfo(Schedulerjob *o);
     void appendLogText(const QString &);
     QString getLogText() { return logText.join("\n"); }

public slots:
    void selectSlot();
    void addToTableSlot();
    void removeTableSlot();
    void setSequenceSlot();
    void startSlot();
    void saveSlot();
    void evaluateJobs();

signals:
        void newLog();

private:
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface *focusinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos/Focus",
                                                   "org.kde.kstars.Ekos.Focus",
                                                   bus,
                                                   this);
    QDBusConnection bus2 = QDBusConnection::sessionBus();
    QDBusInterface *ekosinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos",
                                                   "org.kde.kstars.Ekos",
                                                   bus2,
                                                   this);

    QDBusConnection bus3 = QDBusConnection::sessionBus();
    QDBusInterface *captureinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos/Capture",
                                                   "org.kde.kstars.Ekos.Capture",
                                                   bus3,
                                                   this);

    QDBusConnection bus4 = QDBusConnection::sessionBus();
    QDBusInterface *mountinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos/Mount",
                                                   "org.kde.kstars.Ekos.Mount",
                                                   bus4,
                                                   this);
    QDBusConnection bus5 = QDBusConnection::sessionBus();
    QDBusInterface *aligninterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos/Align",
                                                   "org.kde.kstars.Ekos.Align",
                                                   bus5,
                                                   this);
    QProgressIndicator *pi;
    Ekos::Scheduler *ui;
    KSMoon Moon;
    SkyPoint *moon = &Moon;
    int tableCountRow=0;
    int tableCountCol;
    int iterations = 0;
    QVector<Schedulerjob> objects;
    SkyObject *o;
    QStringList logText;
};
}

#endif // SCHEDULER_H
