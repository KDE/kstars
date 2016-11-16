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
#include <QProcess>

#include "ui_scheduler.h"
#include "scheduler.h"
#include "schedulerjob.h"
#include "ekos/auxiliary/QProgressIndicator.h"
#include "ekos/align/align.h"

class KSMoon;
class GeoLocation;
class SkyObject;

namespace Ekos
{

class SequenceJob;

/**
 * @brief The Ekos scheduler is a simple scheduler class to orchestrate automated multi object observation jobs.
 * @author Jasem Mutlaq
 * @version 1.2
 */
class Scheduler : public QWidget, public Ui::Scheduler
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Scheduler")

public:
    typedef enum { SCHEDULER_IDLE, SCHEDULER_STARTUP, SCHEDULER_RUNNIG, SCHEDULER_PAUSED, SCHEDULER_SHUTDOWN, SCHEDULER_ABORTED } SchedulerState;
    typedef enum { EKOS_IDLE, EKOS_STARTING, EKOS_READY } EkosState;
    typedef enum { INDI_IDLE, INDI_CONNECTING, INDI_PROPERTY_CHECK, INDI_READY } INDIState;
    typedef enum { STARTUP_IDLE, STARTUP_SCRIPT, STARTUP_UNPARK_DOME, STARTUP_UNPARKING_DOME, STARTUP_UNPARK_MOUNT, STARTUP_UNPARKING_MOUNT, STARTUP_UNPARK_CAP, STARTUP_UNPARKING_CAP, STARTUP_ERROR, STARTUP_COMPLETE } StartupState;
    typedef enum { SHUTDOWN_IDLE, SHUTDOWN_PARK_CAP, SHUTDOWN_PARKING_CAP, SHUTDOWN_PARK_MOUNT, SHUTDOWN_PARKING_MOUNT, SHUTDOWN_PARK_DOME, SHUTDOWN_PARKING_DOME, SHUTDOWN_SCRIPT, SHUTDOWN_SCRIPT_RUNNING, SHUTDOWN_ERROR, SHUTDOWN_COMPLETE } ShutdownState;
    typedef enum { PARKWAIT_IDLE, PARKWAIT_PARK, PARKWAIT_PARKING, PARKWAIT_PARKED, PARKWAIT_UNPARK, PARKWAIT_UNPARKING, PARKWAIT_UNPARKED, PARKWAIT_ERROR } ParkWaitStatus;

     Scheduler();
    ~Scheduler();

     void appendLogText(const QString &);
     QString getLogText() { return logText.join("\n"); }
     void clearLog();

     void addObject(SkyObject *object);

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
      * @brief getNextAction Checking for the next appropriate action regarding the current state of the scheduler  and execute it
      */
     void getNextAction();

     /**
      * @brief stopindi Stoping the indi services
      */
     void stopEkos();

     /**
      * @brief stopGuiding After guiding is done we need to stop the process
      */
     void stopGuiding();

     /**
      * @brief setGOTOMode set the GOTO mode for the solver
      * @param mode 0 For Sync, 1 for SlewToTarget, 2 for Nothing
      */
     void setGOTOMode(Align::GotoMode mode);

     /**
      * @brief findAltitude Find altitude given a specific time
      * @param target Target
      * @param when date time to find altitude
      * @return Altitude of the target at the specific date and time given.
      */
     static double findAltitude(const SkyPoint & target, const QDateTime when);

     /** @defgroup SchedulerDBusInterface Ekos DBus Interface - Scheduler Module
      * Ekos::Align interface provides primary functions to run and stop the scheduler.
     */

     /*@{*/

     /** DBUS interface function.
      * @brief Start the scheduler main loop and evaluate jobs and execute them accordingly.
      */
     Q_SCRIPTABLE Q_NOREPLY void start();

     /** DBUS interface function.
      * @brief Stop the scheduler.
      */
     Q_SCRIPTABLE Q_NOREPLY void stop();


     /** DBUS interface function.
      * @brief Loads the Ekos Scheduler List (.esl) file.
      * @param fileURL path to a file
      * @return true if loading file is successful, false otherwise.
      */
     Q_SCRIPTABLE bool loadScheduler(const QString &fileURL);

     /** DBUS interface function.
      * @brief Resets all jobs to IDLE
      */
     Q_SCRIPTABLE void resetAllJobs();

    /** @}*/

protected slots:

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
      * @brief Selects sequence queue.
      */
     void selectStartupScript();

     /**
      * @brief Selects sequence queue.
      */
     void selectShutdownScript();

     /**
      * @brief addToQueue Construct a SchedulerJob and add it to the queue or save job settings from current form values.
      * jobUnderEdit determines whether to add or edit
      */
     void saveJob();

     /**
      * @brief addJob Add a new job from form values
      */
     void addJob();

     /**
      * @brief editJob Edit an observation job
      * @param i index model in queue table
      */
     void loadJob(QModelIndex i);

     /**
      * @brief removeJob Remove a job from the currently selected row. If no row is selected, it remove the last job in the queue.
      */
     void removeJob();

     void toggleScheduler();
     void pause();
     void save();
     void saveAs();
     void load();

     void resetJobState(QModelIndex i);

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

     /**
      * @brief stopCurrentJobAction Stop whatever action taking place in the current job (eg. capture, guiding...etc).
      */
     void stopCurrentJobAction();

     /**
      * @brief readProcessOutput read running script process output and display it in Ekos
      */
     void readProcessOutput();

     /**
      * @brief checkProcessExit Check script process exist status. This is called when the process exists either normally or abnormally.
      * @param exitCode exit code from the script process. Depending on the exist code, the status of startup/shutdown procedure is set accordingly.
      */
     void checkProcessExit(int exitCode);

     /**
      * @brief watchJobChanges Watch any changes in form values and apply changes to current job selection or ignore any changes
      * @param enable True to watch changes and apply them to current job, false to ignore changes
      */
     void watchJobChanges(bool enable);
     /**
      * @brief setDirty Call it to mark the Ekos Scheduler List for change. Next time save button is invoked, the complete content is written to disk.
      */
     void setDirty();

     /**
      * @brief resumeCheckStatus If the scheduler primary loop was suspended due to weather or sleep event, resume it again.
      */
     void resumeCheckStatus();

     /**
      * @brief checkWeather Check weather status and act accordingly depending on the current status of the scheduler and running jobs.
      */
     void checkWeather();

     /**
      * @brief wakeUpScheduler Wake up scheduler from sleep state
      */
     void wakeUpScheduler();

     /**
      * @brief startJobEvaluation Start job evaluation only without starting the scheduler process itself. Display the result to the user.
      */
     void startJobEvaluation();

     /**
      * @brief startMosaicTool Start Mosaic tool and create jobs if necessary.
      */
     void startMosaicTool();

     /**
      * @brief displayTwilightWarning Display twilight warning to user if it is unchecked.
      */
     void checkTwilightWarning(bool enabled);

     void runStartupProcedure();
     void checkStartupProcedure();

     void runShutdownProcedure();
     void checkShutdownProcedure();

signals:
        void newLog();
        void weatherChanged(IPState state);
        void newTarget(const QString &);

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

        void executeScript(const QString &filename);

        int16_t getDarkSkyScore(const QDateTime & observationDateTime);

        /**
         * @brief getAltitudeScore Get the altitude score of an object. The higher the better
         * @param job Active job
         * @param when At what time to check the target altitude
         * @return Altitude score. Altitude below minimum default of 15 degrees but above horizon get -20 score. Bad altitude below minimum required altitude or below horizon get -1000 score.
         */
        int16_t getAltitudeScore(SchedulerJob *job, QDateTime when);

        /**
         * @brief getMoonSeparationScore Get moon separation score. The further apart, the better, up a maximum score of 20.
         * @param job Target job
         * @param when What time to check the moon separation?
         * @return Moon separation score
         */
        int16_t getMoonSeparationScore(SchedulerJob *job, QDateTime when);

        /**
         * @brief calculateJobScore Calculate job dark sky score, altitude score, and moon separation scores and returns the sum.
         * @param job job to evaluate
         * @param when time to evaluate constraints
         * @return Total score
         */
        int16_t calculateJobScore(SchedulerJob *job, QDateTime when);

        /**
         * @brief getWeatherScore Get weather condition score.
         * @return If weather condition OK, return 0, if warning return -500, if alert return -1000
         */
        int16_t getWeatherScore();

        /**
         * @brief calculateAltitudeTime calculate the altitude time given the minimum altitude given.
         * @param job active target
         * @param minAltitude minimum altitude required
         * @param minMoonAngle minimum separation from the moon. -1 to ignore.
         * @return True if found a time in the night where the object is at or above the minimum altitude, false otherise.
         */
        bool    calculateAltitudeTime(SchedulerJob *job, double minAltitude, double minMoonAngle=-1);

        /**
         * @brief calculateCulmination find culmination time adjust for the job offset
         * @param job Active job
         * @return True if culmination time adjust for offset is a valid time in the night
         */
        bool    calculateCulmination(SchedulerJob *job);

        /**
         * @brief calculateDawnDusk Get dawn and dusk times for today
         */
        void    calculateDawnDusk();

        /**
         * @brief checkEkosState Check ekos startup stages and take whatever action necessary to get Ekos up and running
         * @return True if Ekos is running, false if Ekos start up is in progress.
         */
        bool    checkEkosState();

        /**
         * @brief checkINDIState Check INDI startup stages and take whatever action necessary to get INDI devices connected.
         * @return True if INDI devices are connected, false if it is under progress.
         */
        bool    checkINDIState();

        /**
         * @brief checkStartupState Check startup procedure stages and make sure all stages are complete.
         * @return True if startup is complete, false otherwise.
         */
        bool    checkStartupState();

        /**
         * @brief checkShutdownState Check shutdown procedure stages and make sure all stages are complete.
         * @return
         */
        bool    checkShutdownState();

        /**
         * @brief checkParkWaitState Check park wait state.
         * @return If parking/unparking in progress, return false. If parking/unparking complete, return true.
         */
        bool    checkParkWaitState();

        /**
         * @brief parkMount Park mount
         */
        void    parkMount();

        /**
         * @brief unParkMount Unpark mount
         */
        void    unParkMount();

        /**
         * @return True if mount is parked
         */
        bool isMountParked();

        /**
         * @brief parkDome Park dome
         */
        void    parkDome();

        /**
         * @brief unParkDome Unpark dome
         */
        void    unParkDome();

        /**
         * @return True if dome is parked
         */
        bool isDomeParked();

        /**
         * @brief parkCap Close dust cover
         */
        void    parkCap();

        /**
         * @brief unCap Open dust cover
         */
        void    unParkCap();

        /**
         * @brief checkMountParkingStatus check mount parking status and updating corresponding states accordingly.
         */
        void checkMountParkingStatus();

        /**
         * @brief checkDomeParkingStatus check dome parking status and updating corresponding states accordingly.
         */
        void checkDomeParkingStatus();

        /**
         * @brief checkDomeParkingStatus check dome parking status and updating corresponding states accordingly.
         */
        void checkCapParkingStatus();

        /**
         * @brief saveScheduler Save scheduler jobs to a file
         * @param path path of a file
         * @return true on success, false on failure.
         */
        bool saveScheduler(const QUrl &fileURL);

        /**
         * @brief processJobInfo Process the job information from a scheduler file and populate jobs accordingly
         * @param root XML root element of JOB
         * @return true on success, false on failure.
         */
        bool processJobInfo(XMLEle *root);

        /**
         * @brief getCurrentMoonSeparation Get current moon separation in degrees at current time for the given job
         * @param job scheduler job
         * @return Separation in degrees
         */
        double getCurrentMoonSeparation(SchedulerJob *job);

        /**
         * @brief updatePreDawn Update predawn time depending on current time and user offset
         */
        void updatePreDawn();

        /**
         * @brief estimateJobTime Estimates the time the job takes to complete based on the sequence file and what modules to utilize during the observation run.
         * @param job target job
         * @return Estimated time in seconds.
         */
        bool estimateJobTime(SchedulerJob *schedJob);

        /**
         * @brief createJobSequence Creates a job sequence for the mosaic tool given the prefix and output dir. The currently selected sequence file is modified
         * and a new version given the supplied parameters are saved to the output directory
         * @param prefix Prefix to set for the job sequence
         * @param outputDir Output dir to set for the job sequence
         * @return True if new file is saved, false otherwise
         */
        bool createJobSequence(XMLEle *root, const QString &prefix, const QString &outputDir);

        void loadProfiles();

        XMLEle * getSequenceJobRoot();

        bool isWeatherOK(SchedulerJob *job);

        SequenceJob * processJobInfo(XMLEle *root, SchedulerJob *schedJob);
        bool loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob*> &jobs, bool &hasAutoFocus);
        int getCompletedFiles(const QString &path, const QString &seqPrefix);

    Ekos::Scheduler *ui;

    //DBus interfaces
    QDBusInterface *focusInterface;
    QDBusInterface *ekosInterface;
    QDBusInterface *captureInterface;
    QDBusInterface *mountInterface;
    QDBusInterface *alignInterface;
    QDBusInterface *guideInterface;
    QDBusInterface *domeInterface;
    QDBusInterface *weatherInterface;
    QDBusInterface *capInterface;

    // Scheduler and job state and stages
    SchedulerState state;
    EkosState ekosState;
    INDIState indiState;
    StartupState startupState;
    ShutdownState shutdownState;
    ParkWaitStatus parkWaitState;

    QList<SchedulerJob *> jobs;     // List of all jobs as entered by the user or file
    SchedulerJob *currentJob;       // Active job

    QUrl schedulerURL;              // URL to store the scheduler file
    QUrl sequenceURL;               // URL for Ekos Sequence
    QUrl fitsURL;                   // FITS URL to solve
    QUrl startupScriptURL;          // Startup script URL
    QUrl shutdownScriptURL;         // Shutdown script URL

    QStringList logText;            // Store all log strings

    QProgressIndicator *pi;         // Busy indicator widget
    int jobUnderEdit;               // Are we editing a job right now? Job row index

    KSMoon *moon;                   // Pointer to Moon object
    GeoLocation *geo;               // Pointer to Geograpic locatoin

    uint16_t captureBatch;          // How many repeated job batches did we complete thus far?

    QProcess scriptProcess;         // Startup and Shutdown scripts process

    double Dawn, Dusk;              // Store day fraction of dawn and dusk to calculate dark skies range
    QDateTime preDawnDateTime;      // Pre-dawn is where we stop all jobs, it is a user-configurable value before Dawn.
    QDateTime duskDateTime;         // Dusk date time
    bool mDirty;                    // Was job modified and needs saving?
    IPState weatherStatus;          // Keep watch of weather status
    uint8_t noWeatherCounter;       // Keep track of how many times we didn't receive weather updates
    bool preemptiveShutdown;        // Are we shutting down until later?
    bool jobEvaluationOnly;         // Only run job evaluation
    bool loadAndSlewProgress;       // Keep track of Load & Slew operation
    bool autofocusCompleted;        // Check if initial autofocus is completed and do not run autofocus until there is a change is telescope position/alignment.

    uint8_t indiConnectFailureCount;// Keep track of INDI connection failures
    uint8_t focusFailureCount;      // Keep track of Ekos focus module failures
    uint8_t guideFailureCount;      // Keep track of Ekos guide module failures
    uint8_t alignFailureCount;      // Keep track of Ekos align module failures
    uint8_t captureFailureCount;    // Keep track of Ekos capture module failures

    QTimer weatherTimer;            // Call checkWeather when weatherTimer time expires. It is equal to the UpdatePeriod time in INDI::Weather device.
    QTimer sleepTimer;              // Timer to put the scheduler into sleep mode until a job is ready
    QTimer schedulerTimer;          // To call checkStatus
    QTimer jobTimer;                // To call checkJobStage

    QTime currentOperationTime;     // Generic time to track timeout of current operation in progress    

    QUrl dirPath;


};
}

#endif // SCHEDULER_H
