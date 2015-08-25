/*  Ekos Scheduler Module
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on GSoC 2015 Ekos Scheduler project by Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QMainWindow>
#include <QtDBus/QtDBus>

#include "ui_scheduler.h"
#include "scheduler.h"
#include "schedulerjob.h"
#include "QProgressIndicator.h"


namespace Ekos
{

/**
 * @brief The Ekos scheduler is a simple scheduler class to orchestrate automated multi object observation jobs.
 * @author Jasem Mutlaq
 * @version 1.0
 */
class Scheduler : public QWidget, public Ui::Scheduler
{
    Q_OBJECT

public:
    typedef enum {IDLE, STARTING_EKOS, EKOS_STARTED, CONNECTING, CONNECTED,  READY, FINISHED, SHUTDOWN,
                     PARK_TELESCOPE, WARM_CCD, CLOSE_DOME, ABORTED} SchedulerState;
     Scheduler();
    ~Scheduler();

     void appendLogText(const QString &);
     QString getLogText() { return logText.join("\n"); }
     void clearLog();

 #if 0
     /**
      * @brief startEkos DBus call for starting ekos
      */
     void startEkos();
     /**
      * @brief updateJobInfo Updates the state cell of the current job
      * @param o the current job that is being evaluated
      */
     void updateJobInfo(SchedulerJob *o);

     /**
      * @brief startSlew DBus call for initiating slew
      */
     void startSlew();
     /**
      * @brief startFocusing DBus call for feeding ekos the specified settings and initiating focus operation
      */
     void startFocusing();
     /**
      * @brief startAstrometry initiation of the capture and solve operation. We change the job state
      * after solver is started
      */
     void startAstrometry();
     /**
      * @brief startGuiding After ekos is fed the calibration options, we start the guiging process
      */
     void startGuiding();
     /**
      * @brief startCapture The current job file name is solved to an url which is fed to ekos. We then start the capture process
      */
     void startCapture();
     /**
      * @brief getNextAction Checking for the next appropiate action regarding the current state of the scheduler  and execute it
      */
     void getNextAction();
     /**
      * @brief connectDevices After ekos is started, we connect devices
      */
     void connectDevices();

     /**
      * @brief stopindi Stoping the indi services
      */
     void stopindi();
     /**
      * @brief stopGuiding After guiding is done we need to stop the process
      */
     void stopGuiding();

     /**
      * @brief setGOTOMode set the GOTO mode for the solver
      * @param mode 1 for SlewToTarget, 2 for Nothing
      */
     void setGOTOMode(int mode);
     /**
      * @brief startSolving start the solving process for the FITS job
      */
     void startSolving();
     /**
      * @brief getResults After solver is completed, we get the object coordinates and construct the SchedulerJob object
      */
     void getResults();
     /**
      * @brief processFITS I use this as an intermediary to start the solving process of the FITS objects that are currenty in the list
      * @param value
      */
     void processFITS(SchedulerJob *value);
     /**
      * @brief getNextFITSAction Similar process to the one used on regular objects. This one is used in case of FITS selection method
      */
     void getNextFITSAction();
     /**
      * @brief terminateFITSJob After a FITS object is solved, we check if another FITS object exists. If not, we end the solving process.
      * @param value the current FITS job
      */
     void terminateFITSJob(SchedulerJob *value);

     SchedulerJob *getCurrentjob() const;
     void setCurrentjob(SchedulerJob *value);
     /**
      * @brief terminateJob After a job is completed, we check if we have another one pending. If not, we start the shutdown sequence
      * @param value the current job
      */
     void terminateJob(SchedulerJob *value);
     /**
      * @brief executeJob After the best job is selected, we call this in order to start the process that will execute the job.
      * checkJobStatus slot will be connected in order to fgiure the exact state of the current job each second
      * @param value
      */
     void executeJob(SchedulerJob *value);

     SchedulerState getState() const;
     void setState(const SchedulerState &value);
#endif

public slots:

     /**
      * @brief select object from KStars's find dialog.
      */
     void selectObject();

     /**
      * @brief Selects FITS file for solving.
      */
     void selectFITS();

     /**
      * @brief Selects sequence queue.
      */
     void selectSequence();

     /**
      * @brief addToQueue Construct a SchedulerJob and add it to the queue
      */
     void addToQueue();

#if 0


     /**
      * @brief removeFromQueue Removing the object from the table and from the list
      */
     void removeFromQueue();
     /**
      * @brief setSequenceSlot File select functionality for the sequence file
      */
     void setSequence();
     /**
      * @brief startSlot We connect evaluateJobs() and start the scheduler for the given object list.
      * we make sure that there are objects in the list.
      */
     void start();

     /**
      * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
      * Given that score, the scheduler will decide which is the best job that needs to be executed.
      */
     void evaluateJobs();
     /**
      * @brief checkJobStatus This will run each second until it is diconnected. Thus, it will decide the state of the
      * scheduler at the present moment making sure all the pending operations are resolved.
      */
     void checkJobStatus();

      * @brief solveFITSSlot Checks for any pending FITS objects that need to be solved
      */
     void solveFITS();
     /**
      * @brief solveFITSAction if a FITS job is detected, processFITS() is called and the solving process is started
      */
     void solveFITSAction();
     /**
      * @brief checkFITSStatus Checks the scheduler state each second, making sure all the operations are completed succesfully
      */
     void checkFITSStatus();
#endif

signals:
        void newLog();

private:        
    Ekos::Scheduler *ui;

    //DBus interfaces
    QDBusInterface *focusInterface;
    QDBusInterface *ekosInterface;
    QDBusInterface *captureInterface;
    QDBusInterface *mountInterface;
    QDBusInterface *alignInterface;
    QDBusInterface *guideInterface;

    SchedulerState state;
    QProgressIndicator *pi;

    //The list of pending objects
    //QVector<SchedulerJob> objects;

    QUrl sequenceURL;
    QUrl fitsURL;

    QStringList logText;
    //The current job that is evaluated
    //SchedulerJob *currentjob;

};
}

#endif // SCHEDULER_H
