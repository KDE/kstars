/*  Ekos Scheduler Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project by Daniel Leu <daniel_mihai.leu@cti.pub.ro>

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

class KSMoon;
class GeoLocation;

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
    typedef enum { SCHEDULER_IDLE, SCHEDULER_RUNNIG, SCHEDULER_ABORTED } SchedulerState;
    typedef enum { EKOS_IDLE, EKOS_STARTING, EKOS_READY } EkosState;
    typedef enum { INDI_IDLE, INDI_CONNECTING, INDI_READY } INDIState;

     Scheduler();
    ~Scheduler();

     void appendLogText(const QString &);
     QString getLogText() { return logText.join("\n"); }
     void clearLog();

     /**
      * @brief updateJobInfo Updates the state cell of the current job
      * @param o the current job that is being evaluated
      */
     //void updateJobInfo(SchedulerJob *o);

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
     void startCalibrating();
     /**
      * @brief startCapture The current job file name is solved to an url which is fed to ekos. We then start the capture process
      */
     void startCapture();
     /**
      * @brief getNextAction Checking for the next appropiate action regarding the current state of the scheduler  and execute it
      */
     void getNextAction();

     /**
      * @brief stopindi Stoping the indi services
      */
     void stopINDI();
     /**
      * @brief stopGuiding After guiding is done we need to stop the process
      */
     void stopGuiding();

     /**
      * @brief setGOTOMode set the GOTO mode for the solver
      * @param mode 0 For Sync, 1 for SlewToTarget, 2 for Nothing
      */
     void setGOTOMode(int mode);

     void startFITSSolving();
     void getFITSAstrometryResults();


     /**
      * @brief start Start scheduler main loop and evaluate jobs and execute them accordingly
      */
     void start();

    void stop();

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
     void addJob();

     /**
      * @brief editJob Edit an observation job
      * @param i index model in queue table
      */
     void editJob(QModelIndex i);

     /**
      * @brief removeJob Remove a job from the currently selected row. If no row is selected, it remove the last job in the queue.
      */
     void removeJob();     

     void toggleScheduler();
     void save();
     void load();

     void resetJobEdit();

     /**
      * @brief checkJobStatus Check the overall state of the scheduler, Ekos, and INDI. When all is OK, it call evaluateJobs();
      */
     void checkStatus();

     /**
      * @brief checkJobStage Check the progress of the job states and make DBUS call to start the next stage until the job is complete.
      */
     void checkJobStage();

     /**
      * @brief findNextJob Check if the job met the completion criteria, and if it did, then it search for next job candidate. If no jobs are found, it starts the shutdown stage.
      */
     void findNextJob();

     void stopEkosAction();


#if 0


     /**
      * @brief removeFromQueue Removing the object from the table and from the list
      */
     void removeFromQueue();
     /**
      * @brief setSequenceSlot File select functionality for the sequence file
      */
     void setSequence();





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

        /**
         * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
         * Given that score, the scheduler will decide which is the best job that needs to be executed.
         */
        void evaluateJobs();

        /**
         * @brief executeJob After the best job is selected, we call this in order to start the process that will execute the job.
         * checkJobStatus slot will be connected in order to figure the exact state of the current job each second
         * @param value
         */
        void executeJob(SchedulerJob *job);

        int16_t getDarkSkyScore(const QTime & observationTime);
        int16_t getAltitudeScore(SchedulerJob *job, const SkyPoint & target);
        int16_t getMoonSeparationScore(SchedulerJob *job, const SkyPoint & target);
        int16_t getWeatherScore(SchedulerJob *job);

        bool    calculateAltitudeTime(SchedulerJob *job, double minAltitude);
        bool    calculateCulmination(SchedulerJob *job);
        void    calculateDawnDusk();

        bool    checkEkosState();
        bool    checkINDIState();
        bool    checkFITSJobState();


    Ekos::Scheduler *ui;

    //DBus interfaces
    QDBusInterface *focusInterface;
    QDBusInterface *ekosInterface;
    QDBusInterface *captureInterface;
    QDBusInterface *mountInterface;
    QDBusInterface *alignInterface;
    QDBusInterface *guideInterface;

    SchedulerState state;
    EkosState ekosState;
    INDIState indiState;

    QProgressIndicator *pi;

    QList<SchedulerJob *> jobs;
    QList<SchedulerJob *> fitsJobs;
    SchedulerJob *currentJob;

    QUrl sequenceURL;
    QUrl fitsURL;

    QStringList logText;

    bool jobUnderEdit;
    // Was job modified and needs saving?
    bool mDirty;

    KSMoon *moon;

    GeoLocation *geo;

    double Dawn, Dusk;


};
}

#endif // SCHEDULER_H
