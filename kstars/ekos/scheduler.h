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
#include "Schedulerjob.h"

namespace Ekos {

class Scheduler : public QWidget, public Ui::Scheduler
{
    Q_OBJECT

public:
     Scheduler();
    ~Scheduler();
     int checkWeather();
     void processSession(int i,QUrl filename);
     void stopindi();

public slots:
    void selectSlot();
    void addToTableSlot();
    void removeTableSlot();
    void setSequenceSlot();
    void startSlot();
    void saveSlot();

private:

    Ekos::Scheduler *ui;
    int tableCountRow=0;
    int tableCountCol;
    QVector<ObservableA> objects;
    SkyObject *o;
};
}

#endif // SCHEDULER_H
