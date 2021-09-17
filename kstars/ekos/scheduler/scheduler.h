/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project:
    SPDX-FileCopyrightText: 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_scheduler.h"
#include "ekos/align/align.h"
#include "indi/indiweather.h"
#include "schedulerjob.h"

#include <lilxml.h>

#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QtDBus>

#include <cstdint>

class QProgressIndicator;

class GeoLocation;
class SchedulerJob;
class SkyObject;
class KConfigDialog;
class TestSchedulerUnit;

class TestEkosSchedulerOps;

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
        Q_PROPERTY(Ekos::SchedulerState status READ status NOTIFY newStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QString profile READ profile WRITE setProfile)

    public:
        typedef enum { EKOS_IDLE, EKOS_STARTING, EKOS_STOPPING, EKOS_READY } EkosState;
        typedef enum { INDI_IDLE, INDI_CONNECTING, INDI_DISCONNECTING, INDI_PROPERTY_CHECK, INDI_READY } INDIState;
        typedef enum
        {
            STARTUP_IDLE,
            STARTUP_SCRIPT,
            STARTUP_UNPARK_DOME,
            STARTUP_UNPARKING_DOME,
            STARTUP_UNPARK_MOUNT,
            STARTUP_UNPARKING_MOUNT,
            STARTUP_UNPARK_CAP,
            STARTUP_UNPARKING_CAP,
            STARTUP_ERROR,
            STARTUP_COMPLETE
        } StartupState;
        typedef enum
        {
            SHUTDOWN_IDLE,
            SHUTDOWN_PARK_CAP,
            SHUTDOWN_PARKING_CAP,
            SHUTDOWN_PARK_MOUNT,
            SHUTDOWN_PARKING_MOUNT,
            SHUTDOWN_PARK_DOME,
            SHUTDOWN_PARKING_DOME,
            SHUTDOWN_SCRIPT,
            SHUTDOWN_SCRIPT_RUNNING,
            SHUTDOWN_ERROR,
            SHUTDOWN_COMPLETE
        } ShutdownState;
        typedef enum
        {
            PARKWAIT_IDLE,
            PARKWAIT_PARK,
            PARKWAIT_PARKING,
            PARKWAIT_PARKED,
            PARKWAIT_UNPARK,
            PARKWAIT_UNPARKING,
            PARKWAIT_UNPARKED,
            PARKWAIT_ERROR
        } ParkWaitStatus;

        /** @brief options what should happen if an error or abort occurs */
        typedef enum
        {
            ERROR_DONT_RESTART,
            ERROR_RESTART_AFTER_TERMINATION,
            ERROR_RESTART_IMMEDIATELY
        } ErrorHandlingStrategy;

        /** @brief Columns, in the same order as UI. */
        typedef enum
        {
            SCHEDCOL_NAME = 0,
            SCHEDCOL_STATUS,
            SCHEDCOL_CAPTURES,
            SCHEDCOL_ALTITUDE,
            SCHEDCOL_SCORE,
            SCHEDCOL_STARTTIME,
            SCHEDCOL_ENDTIME,
            SCHEDCOL_DURATION,
            SCHEDCOL_LEADTIME,
            SCHEDCOL_COUNT
        } SchedulerColumns;

        /** @brief IterationTypes, the different types of scheduler iterations that are run. */
        typedef enum
        {
            RUN_WAKEUP = 0,
            RUN_SCHEDULER,
            RUN_JOBCHECK,
            RUN_SHUTDOWN,
            RUN_NOTHING
        } SchedulerTimerState;

        /** @brief Constructor, the starndard scheduler constructor. */
        Scheduler();
        /** @brief DebugConstructor, a constructor used in testing with a mock ekos. */
        Scheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr);
        ~Scheduler() = default;

        QString getCurrentJobName();
        SchedulerJob *getCurrentJob()
        {
            return currentJob;
        }

        void appendLogText(const QString &);
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }
        void clearLog();
        void applyConfig();

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
             * @brief startGuiding After ekos is fed the calibration options, we start the guiding process
             * @param resetCalibration By default calibration is not reset until it is explicitly requested
             */
        void startGuiding(bool resetCalibration = false);

        /**
             * @brief startCapture The current job file name is solved to an url which is fed to ekos. We then start the capture process
             * @param restart Set to true if the goal to restart an existing sequence. The only difference is that when a sequence is restarted, sequence file
             * is not loaded from disk again since that results in erasing all the history of the capture process.
             */
        void startCapture(bool restart = false);

        /**
             * @brief getNextAction Checking for the next appropriate action regarding the current state of the scheduler  and execute it
             */
        void getNextAction();

        /**
             * @brief disconnectINDI disconnect all INDI devices from server.
             */
        void disconnectINDI();

        /**
             * @brief stopEkos shutdown Ekos completely
             */
        void stopEkos();

        /**
             * @brief stopGuiding After guiding is done we need to stop the process
             */
        void stopGuiding();

        /**
             * @brief setSolverAction set the GOTO mode for the solver
             * @param mode 0 For Sync, 1 for SlewToTarget, 2 for Nothing
             */
        void setSolverAction(Align::GotoMode mode);

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
             * @brief Remove all scheduler jobs
             */
        Q_SCRIPTABLE Q_NOREPLY void removeAllJobs();

        /** DBUS interface function.
             * @brief Loads the Ekos Scheduler List (.esl) file.
             * @param fileURL path to a file
             * @return true if loading file is successful, false otherwise.
             */
        Q_SCRIPTABLE bool loadScheduler(const QString &fileURL);

        /** DBUS interface function.
         * @brief Set the file URL pointing to the capture sequence file
         * @param sequenceFileURL URL of the capture sequence file
         */
        Q_SCRIPTABLE void setSequence(const QString &sequenceFileURL);

        /** DBUS interface function.
             * @brief Resets all jobs to IDLE
             */
        Q_SCRIPTABLE void resetAllJobs();

        /** DBUS interface function.
             * @brief Resets all jobs to IDLE
             */
        Q_SCRIPTABLE void sortJobsPerAltitude();

        Ekos::SchedulerState status()
        {
            return state;
        }

        void setProfile(const QString &profile)
        {
            schedulerProfileCombo->setCurrentText(profile);
        }
        QString profile()
        {
            return schedulerProfileCombo->currentText();
        }

        /**
         * @brief retrieve the error handling strategy from the UI
         */
        ErrorHandlingStrategy getErrorHandlingStrategy();

        /**
         * @brief select the error handling strategy (no restart, restart after all terminated, restart immediately)
         */
        void setErrorHandlingStrategy (ErrorHandlingStrategy strategy);

        /** @}*/

        // TODO: This section of static public and private methods should someday
        // be moved from Scheduler and placed in a separate class,
        // e.g. SchedulerPlanner or SchedulerJobEval

        /**
             * @brief setupJob Massive initialization of a SchedulerJob for testing and exectution
             * @param job Target
        */
        static void setupJob(
            SchedulerJob &job, const QString &name, int priority, const dms &ra,
            const dms &dec, double djd, double rotation, const QUrl &sequenceUrl, const QUrl &fitsUrl,
            SchedulerJob::StartupCondition startup, const QDateTime &startupTime, int16_t startupOffset,
            SchedulerJob::CompletionCondition completion, const QDateTime &completionTime, int completionRepeats,
            double minimumAltitude, double minimumMoonSeparation, bool enforceWeather, bool enforceTwilight,
            bool enforceArtificialHorizon, bool track, bool focus, bool align, bool guide);

        /**
             * @brief evaluateJobs Computes estimated start and end times for the SchedulerJobs passed in. Returns a proposed schedule.
             * @param jobs The input list of SchedulerJobs to evaluate.
             * @param state The current scheduler state.
             * @param capturedFramesCount which parts of the schedulerJobs have already been completed.
             * @param dawn next dawn, as a KStarsDateTime
             * @param dusk next dusk, as a KStarsDateTime
             * @param rescheduleErrors whether jobs that failed with errors should be rescheduled.
             * @param restartJobs whether jobs that failed for one reason or another shoulc be rescheduled.
             * @param possiblyDelay a return value indicating whether the timer should try scheduling again after a delay.
             * @param scheduler instance of the scheduler used for logging. Can be nullptr.
             * @return Total score
             */
        static QList<SchedulerJob *> evaluateJobs(QList<SchedulerJob *> &jobs, SchedulerState state,
                const QMap<QString, uint16_t> &capturedFramesCount, QDateTime const &dawn, QDateTime const &dusk,
                bool rescheduleErrors, bool restartJobs, bool *possiblyDelay, Scheduler *scheduler);
        /**
             * @brief calculateJobScore Calculate job dark sky score, altitude score, and moon separation scores and returns the sum.
             * @param job Target
             * @param dawn next dawn, as a KStarsDateTime
             * @param dusk next dusk, as a KStarsDateTime
             * @param when date and time to evaluate constraints, now if omitted.
             * @return Total score
             */
        static int16_t calculateJobScore(SchedulerJob const *job, QDateTime const &dawn, QDateTime const &dusk,
                                         QDateTime const &when = QDateTime());

        /**
             * @brief estimateJobTime Estimates the time the job takes to complete based on the sequence file and what modules to utilize during the observation run.
             * @param job target job
             * @param capturedFramesCount a map of what's been captured already
             * @param scheduler instance of the scheduler used for logging. Can be nullptr.
             * @return Estimated time in seconds.
             */
        static bool estimateJobTime(SchedulerJob *schedJob, const QMap<QString, uint16_t> &capturedFramesCount,
                                    Scheduler *scheduler);
        /**
             * @brief loadSequenceQueue Loads what's necessary to estimate job completion time from a capture sequence queue file
             * @param fileURL the filename
             * @param schedJob the SchedulerJob is modified accoring to the contents of the sequence queue
             * @param jobs the returned values read from the file
             * @param hasAutoFocus a return value indicating whether autofocus can be triggered by the sequence.
              * @param scheduler instance of the scheduler used for logging. Can be nullptr.
             * @return Estimated time in seconds.
             */

        static bool loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs,
                                      bool &hasAutoFocus, Scheduler *scheduler);

        /**
             * @brief getDarkSkyScore Get the dark sky score of a date and time. The further from dawn the better.
             * @param when date and time to check the dark sky score, now if omitted
             * @return Dark sky score. Daylight get bad score, as well as pre-dawn to dawn.
             */
        static int16_t getDarkSkyScore(QDateTime const &dawn, QDateTime const &dusk, QDateTime const &when = QDateTime());

        /** @brief Setter used in testing to fix the local time. Otherwise getter gets from KStars instance. */
        /** @{ */
        static KStarsDateTime getLocalTime();
        static void setLocalTime(KStarsDateTime *time)
        {
            storedLocalTime = time;
        }
        static bool hasLocalTime()
        {
            return storedLocalTime != nullptr;
        }
        /** @} */

    private:
        /**
             * @brief processJobInfo a utility used by loadSequenceQueue() to help it read a capture sequence file
             * @param root the filename
             * @param schedJob the SchedulerJob is modified accoring to the contents of the sequence queue
             * @return a capture sequence
             */
        static SequenceJob *processJobInfo(XMLEle *root, SchedulerJob *schedJob);

        /**
             * @brief timeHeuristics Estimates the number of seconds of overhead above and beyond imaging time, used by estimateJobTime.
             * @param schedJob the scheduler job.
             * @return seconds of overhead.
             */
        static int timeHeuristics(const SchedulerJob *schedJob);

        // Used in testing, instead of KStars::Instance() resources
        static KStarsDateTime *storedLocalTime;

        friend TestSchedulerUnit;

        // TODO: See above TODO. End of static methods that might be moved to
        // a separate Scheduler-related class.

        /*@{*/

    private:
        /** @internal Safeguard flag to avoid registering signals from widgets multiple times.
         */
        bool jobChangesAreWatched { false };

    protected:
        /** @internal Enables signal watch on SchedulerJob form values in order to apply changes to current job.
          * @param enable is the toggle flag, true to watch for changes, false to ignore them.
          */
        void watchJobChanges(bool enable);

        /** @internal Marks the currently selected SchedulerJob as modified change.
         *
         * This triggers job re-evaluation.
         * Next time save button is invoked, the complete content is written to disk.
          */
        void setDirty();
        /** @} */

    protected:
        /** @internal Associate job table cells on a row to the corresponding SchedulerJob.
         * @param row is an integer indexing the row to associate cells from, and also the index of the job in the job list..
         */
        void setJobStatusCells(int row);

    protected slots:

        /**
         * @brief registerNewModule Register an Ekos module as it arrives via DBus
         * and create the appropriate DBus interface to communicate with it.
         * @param name of module
         */
        void registerNewModule(const QString &name);

        /**
         * @brief syncProperties Sync startup properties from the various device to enable/disable features in the scheduler
         * like the ability to park/unpark..etc
         */
        void syncProperties();

        /**
         * @brief checkInterfaceReady Sometimes syncProperties() is not sufficient since the ready signal could have fired already
         * and cannot be relied on to know once a module interface is ready. Therefore, we explicitly check if the module interface
         * is ready.
         * @param iface interface to test for readiness.
         */
        void checkInterfaceReady(QDBusInterface *iface);

        void setAlignStatus(Ekos::AlignState status);
        void setGuideStatus(Ekos::GuideState status);
        void setCaptureStatus(Ekos::CaptureState status);
        void setFocusStatus(Ekos::FocusState status);
        void setMountStatus(ISD::Telescope::Status status);
        void setWeatherStatus(ISD::Weather::Status status);

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

        /**
             * @brief setJobAddApply Set first button state to add new job or apply changes.
             */
        void setJobAddApply(bool add_mode);

        /**
             * @brief setJobManipulation Enable or disable job manipulation buttons.
             */
        void setJobManipulation(bool can_reorder, bool can_delete);

        /**
         * @brief set all GUI fields to the values of the given scheduler job
         */
        void syncGUIToJob(SchedulerJob *job);

        /**
             * @brief jobSelectionChanged Update UI state when the job list is clicked once.
             */
        void clickQueueTable(QModelIndex index);

        /**
         * @brief Update scheduler parameters to the currently selected scheduler job
         * @param current table position
         * @param previous table position
         */
        void queueTableSelectionChanged(QModelIndex current, QModelIndex previous);

        /**
             * @brief reorderJobs Change the order of jobs in the UI based on a subset of its jobs.
             */
        bool reorderJobs(QList<SchedulerJob*> reordered_sublist);

        /**
             * @brief moveJobUp Move the selected job up in the job list.
             */
        void moveJobUp();

        /**
            * @brief moveJobDown Move the selected job down in the list.
            */
        void moveJobDown();

        /**
         * @brief shouldSchedulerSleep Check if the scheduler needs to sleep until the job is ready
         * @param currentJob Job to check
         * @return True if we set the scheduler to sleep mode. False, if not required and we need to execute now
         */
        bool shouldSchedulerSleep(SchedulerJob *currentJob);

        bool completeShutdown();
        void toggleScheduler();
        void pause();
        void setPaused();
        void save();
        void saveAs();

        /**
         * @brief load Open a file dialog to select an ESL file, and load its contents.
         * @param clearQueue Clear the queue before loading, or append ESL contents to queue.
         * @param filename If not empty, this file will be used instead of poping up a dialog.
         */
        void load(bool clearQueue, const QString &filename = "");

        /**
         * @brief appendEkosScheduleList Append the contents of an ESL file to the queue.
         * @param fileURL File URL to load contents from.
         * @return True if contents were loaded successfully, else false.
         */
        bool appendEkosScheduleList(const QString &fileURL);

        void resetJobEdit();

        /**
         * @brief updateNightTime update the Twilight restriction with the argument job properties.
         * @param job SchedulerJob for which to display the next dawn and dusk, or the job currently selected if null, or today's next dawn and dusk if no job is selected.
         */
        void updateNightTime(SchedulerJob const * job = nullptr);

        /**
             * @brief checkJobStatus Check the overall state of the scheduler, Ekos, and INDI. When all is OK, it calls evaluateJobs() when no job is current or executeJob() if a job is selected.
             * @return False if this function needs to be called again later, true if situation is stable and operations may continue.
             */
        bool checkStatus();

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
             * @brief manageConnectionLoss Mitigate loss of connection with the INDI server.
             * @return true if connection to Ekos/INDI should be attempted again, false if not mitigation is available or needed.
             */
        bool manageConnectionLoss();

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
             * @brief resumeCheckStatus If the scheduler primary loop was suspended due to weather or sleep event, resume it again.
             */
        void resumeCheckStatus();

        /**
             * @brief checkWeather Check weather status and act accordingly depending on the current status of the scheduler and running jobs.
             */
        //void checkWeather();

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

        void setINDICommunicationStatus(Ekos::CommunicationStatus status);
        void setEkosCommunicationStatus(Ekos::CommunicationStatus status);

        void simClockScaleChanged(float);
        void simClockTimeChanged();

    signals:
        void newLog(const QString &text);
        void newStatus(Ekos::SchedulerState state);
        void weatherChanged(ISD::Weather::Status state);
        void newTarget(const QString &);

    private:
        /**
             * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
             * Given that score, the scheduler will decide which is the best job that needs to be executed.
             */
        void evaluateJobs(bool evaluateOnly);
        void processJobs(QList<SchedulerJob *> sortedJobs, bool jobEvaluationOnly);

        /**
             * @brief executeJob After the best job is selected, we call this in order to start the process that will execute the job.
             * checkJobStatus slot will be connected in order to figure the exact state of the current job each second
             * @param value
             */
        void executeJob(SchedulerJob *job);

        void executeScript(const QString &filename);

        /**
             * @brief getWeatherScore Get current weather condition score.
             * @return If weather condition OK, return score 0, else bad score.
             */
        int16_t getWeatherScore() const;

        /**
             * @brief calculateDawnDusk Get dawn and dusk times for today
             */
        static void calculateDawnDusk();

        /**
             * @brief checkEkosState Check ekos startup stages and take whatever action necessary to get Ekos up and running
             * @return True if Ekos is running, false if Ekos start up is in progress.
             */
        bool checkEkosState();

        /**
             * @brief isINDIConnected Determines the status of the INDI connection.
             * @return True if INDI connection is up and usable, else false.
             */
        bool isINDIConnected();

        /**
             * @brief checkINDIState Check INDI startup stages and take whatever action necessary to get INDI devices connected.
             * @return True if INDI devices are connected, false if it is under progress.
             */
        bool checkINDIState();

        /**
             * @brief checkStartupState Check startup procedure stages and make sure all stages are complete.
             * @return True if startup is complete, false otherwise.
             */
        bool checkStartupState();

        /**
             * @brief checkShutdownState Check shutdown procedure stages and make sure all stages are complete.
             * @return
             */
        bool checkShutdownState();

        /**
             * @brief checkParkWaitState Check park wait state.
             * @return If parking/unparking in progress, return false. If parking/unparking complete, return true.
             */
        bool checkParkWaitState();

        /**
             * @brief parkMount Park mount
             */
        void parkMount();

        /**
             * @brief unParkMount Unpark mount
             */
        void unParkMount();

        /**
             * @return True if mount is parked
             */
        bool isMountParked();

        /**
             * @brief parkDome Park dome
             */
        void parkDome();

        /**
             * @brief unParkDome Unpark dome
             */
        void unParkDome();

        /**
             * @return True if dome is parked
             */
        bool isDomeParked();

        /**
             * @brief parkCap Close dust cover
             */
        void parkCap();

        /**
             * @brief unCap Open dust cover
             */
        void unParkCap();

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
             * @brief createJobSequence Creates a job sequence for the mosaic tool given the prefix and output dir. The currently selected sequence file is modified
             * and a new version given the supplied parameters are saved to the output directory
             * @param prefix Prefix to set for the job sequence
             * @param outputDir Output dir to set for the job sequence
             * @return True if new file is saved, false otherwise
             */
        bool createJobSequence(XMLEle *root, const QString &prefix, const QString &outputDir);

        /** @internal Change the current job, updating associated widgets.
         * @param job is an existing SchedulerJob to set as current, or nullptr.
         */
        void setCurrentJob(SchedulerJob *job);

        /**
         * @brief processFITSSelection When a FITS file is selected, open it and try to guess
         * the object name, and its J2000 RA/DE to fill the UI with such info automatically.
         */
        void processFITSSelection();

        void loadProfiles();

        XMLEle *getSequenceJobRoot();

        /**
            * @brief updateCompletedJobsCount For each scheduler job, examine sequence job storage and count captures.
            * @param forced forces recounting captures unconditionally if true, else only IDLE, EVALUATION or new jobs are examined.
            */
        void updateCompletedJobsCount(bool forced = false);

        int getCompletedFiles(const QString &path, const QString &seqPrefix);

        // retrieve the guiding status
        GuideState getGuidingStatus();

        // Returns milliseconds since startCurrentOperationTImer() was called.
        qint64 getCurrentOperationMsec();
        // Starts the above operation timer.
        void startCurrentOperationTimer();

        // Controls for the guiding timer, which restarts guiding after failure.
        void cancelGuidingTimer();
        bool isGuidingTimerActive();
        void startGuidingTimer(int milliseconds);
        void processGuidingTimer();

        Ekos::Scheduler *ui { nullptr };
        //DBus interfaces
        QPointer<QDBusInterface> focusInterface { nullptr };
        QPointer<QDBusInterface> ekosInterface { nullptr };
        QPointer<QDBusInterface> captureInterface { nullptr };
        QPointer<QDBusInterface> mountInterface { nullptr };
        QPointer<QDBusInterface> alignInterface { nullptr };
        QPointer<QDBusInterface> guideInterface { nullptr };
        QPointer<QDBusInterface> domeInterface { nullptr };
        QPointer<QDBusInterface> weatherInterface { nullptr };
        QPointer<QDBusInterface> capInterface { nullptr };

        // Interface strings for the dbus. Changeable for mocks when testing. Private so only tests can change.
        QString focusInterfaceString { "org.kde.kstars.Ekos.Focus" };
        void setFocusInterfaceString(const QString &interface)
        {
            focusInterfaceString = interface;
        }
        QString focusPathString { "/KStars/Ekos/Focus" };
        void setFocusPathString(const QString &interface)
        {
            focusPathString = interface;
        }

        // This is only used in the constructor
        QString ekosInterfaceString { "org.kde.kstars.Ekos" };
        QString ekosPathString { "/KStars/Ekos" };

        QString mountInterfaceString { "org.kde.kstars.Ekos.Mount" };
        void setMountInterfaceString(const QString &interface)
        {
            mountInterfaceString = interface;
        }
        QString mountPathString { "/KStars/Ekos/Mount" };
        void setMountPathString(const QString &interface)
        {
            mountPathString = interface;
        }

        QString captureInterfaceString { "org.kde.kstars.Ekos.Capture" };
        void setCaptureInterfaceString(const QString &interface)
        {
            captureInterfaceString = interface;
        }
        QString capturePathString { "/KStars/Ekos/Capture" };
        void setCapturePathString(const QString &interface)
        {
            capturePathString = interface;
        }

        QString alignInterfaceString { "org.kde.kstars.Ekos.Align" };
        void setAlignInterfaceString(const QString &interface)
        {
            alignInterfaceString = interface;
        }
        QString alignPathString { "/KStars/Ekos/Align" };
        void setAlignPathString(const QString &interface)
        {
            alignPathString = interface;
        }

        QString guideInterfaceString { "org.kde.kstars.Ekos.Guide" };
        void setGuideInterfaceString(const QString &interface)
        {
            guideInterfaceString = interface;
        }
        QString guidePathString { "/KStars/Ekos/Guide" };
        void setGuidePathString(const QString &interface)
        {
            guidePathString = interface;
        }

        QString domeInterfaceString { "org.kde.kstars.Ekos.Dome" };
        void setDomeInterfaceString(const QString &interface)
        {
            domeInterfaceString = interface;
        }
        QString domePathString { "/KStars/Ekos/Dome" };
        void setDomePathString(const QString &interface)
        {
            domePathString = interface;
        }

        QString weatherInterfaceString { "org.kde.kstars.Ekos.Weather" };
        void setWeatherInterfaceString(const QString &interface)
        {
            weatherInterfaceString = interface;
        }
        QString weatherPathString { "/KStars/Ekos/Weather" };
        void setWeatherPathString(const QString &interface)
        {
            weatherPathString = interface;
        }

        QString dustCapInterfaceString { "org.kde.kstars.Ekos.DustCap" };
        void setDustCapInterfaceString(const QString &interface)
        {
            dustCapInterfaceString = interface;
        }
        QString dustCapPathString { "/KStars/Ekos/DustCap" };
        void setDustCapPathString(const QString &interface)
        {
            dustCapPathString = interface;
        }

        // Scheduler and job state and stages
        SchedulerState state { SCHEDULER_IDLE };
        EkosState ekosState { EKOS_IDLE };
        INDIState indiState { INDI_IDLE };
        StartupState startupState { STARTUP_IDLE };
        ShutdownState shutdownState { SHUTDOWN_IDLE };
        ParkWaitStatus parkWaitState { PARKWAIT_IDLE };
        Ekos::CommunicationStatus m_EkosCommunicationStatus { Ekos::Idle };
        Ekos::CommunicationStatus m_INDICommunicationStatus { Ekos::Idle };
        /// List of all jobs as entered by the user or file
        QList<SchedulerJob *> jobs;
        /// Active job
        SchedulerJob *currentJob { nullptr };
        /// URL to store the scheduler file
        QUrl schedulerURL;
        /// URL for Ekos Sequence
        QUrl sequenceURL;
        /// FITS URL to solve
        QUrl fitsURL;
        /// Startup script URL
        QUrl startupScriptURL;
        /// Shutdown script URL
        QUrl shutdownScriptURL;
        /// Store all log strings
        QStringList m_LogText;
        /// Busy indicator widget
        QProgressIndicator *pi { nullptr };
        /// Are we editing a job right now? Job row index
        int jobUnderEdit { -1 };
        /// Pointer to Geographic location
        GeoLocation *geo { nullptr };
        /// How many repeated job batches did we complete thus far?
        uint16_t captureBatch { 0 };
        /// Startup and Shutdown scripts process
        QProcess scriptProcess;
        /// Store next dawn to calculate dark skies range
        static QDateTime Dawn;
        /// Store next dusk to calculate dark skies range
        static QDateTime Dusk;
        /// Pre-dawn is where we stop all jobs, it is a user-configurable value before Dawn.
        static QDateTime preDawnDateTime;
        /// Was job modified and needs saving?
        bool mDirty { false };
        /// Keep watch of weather status
        ISD::Weather::Status weatherStatus { ISD::Weather::WEATHER_IDLE };
        /// Keep track of how many times we didn't receive weather updates
        uint8_t noWeatherCounter { 0 };

        // Utilities to control the preemptiveShutdown feature.
        // Is the scheduler shutting down until later when it will resume a job?
        void enablePreemptiveShutdown(const QDateTime &wakeupTime);
        void disablePreemptiveShutdown();
        QDateTime getPreemptiveShutdownWakeupTime();
        bool preemptiveShutdown();
        // The various preemptiveShutdown states are controlled by this one variable.
        QDateTime preemptiveShutdownWakeupTime;

        /// Keep track of Load & Slew operation
        bool loadAndSlewProgress { false };
        /// Check if initial autofocus is completed and do not run autofocus until there is a change is telescope position/alignment.
        bool autofocusCompleted { false };
        /// Keep track of INDI connection failures
        uint8_t indiConnectFailureCount { 0 };
        /// Keep track of Ekos connection failures
        uint8_t ekosConnectFailureCount { 0 };
        /// Keep track of Ekos focus module failures
        uint8_t focusFailureCount { 0 };
        /// Keep track of Ekos guide module failures
        uint8_t guideFailureCount { 0 };
        /// Keep track of Ekos align module failures
        uint8_t alignFailureCount { 0 };
        /// Keep track of Ekos capture module failures
        uint8_t captureFailureCount { 0 };
        /// Counter to keep debug logging in check
        uint8_t checkJobStageCounter { 0 };
        /// Call checkWeather when weatherTimer time expires. It is equal to the UpdatePeriod time in INDI::Weather device.
        //QTimer weatherTimer;

        /// Delay for restarting the guider
        /// used by cancelGuidingTimer(), isGuidingTimerActive(), startGuidingTimer
        /// and processGuidingTimer.
        int restartGuidingInterval { -1 };
        KStarsDateTime restartGuidingTime;

        /// Generic time to track timeout of current operation in progress.
        /// Used by startCurrentOperationTimer() and getCurrentOperationMsec().
        KStarsDateTime currentOperationTime;
        bool currentOperationTimeStarted { false };

        QUrl dirPath;

        QMap<QString, uint16_t> m_CapturedFramesCount;

        bool m_MountReady { false };
        bool m_CaptureReady { false };
        bool m_DomeReady { false };
        bool m_CapReady { false };

        // When a module is commanded to perform an action, wait this many milliseconds
        // before check its state again. If State is still IDLE, then it either didn't received the command
        // or there is another problem.
        static const uint32_t ALIGN_INACTIVITY_TIMEOUT      = 120000;
        static const uint32_t FOCUS_INACTIVITY_TIMEOUT      = 120000;
        static const uint32_t CAPTURE_INACTIVITY_TIMEOUT    = 120000;
        static const uint16_t GUIDE_INACTIVITY_TIMEOUT      = 60000;

        // Methods & variables that control the scheduler's iterations.

        // Initializes the scheduler, then calls iterate().
        void run();
        // Repeatedly runs a scheduler iteration and then sleeps timerInterval millisconds
        // and run the next iteration. This continues until the sleep time is negative.
        void iterate();
        // Initialize the scheduler.
        void init();
        // Run a single scheduler iteration.
        int runSchedulerIteration();

        // This is the time between typical scheduler iterations.
        // The time can be modified for testing.
        int m_UpdatePeriodMs = 1000;
        void setUpdateInterval(int ms)
        {
            m_UpdatePeriodMs = ms;
        }
        // Setup the parameters for the next scheduler iteration.
        // When milliseconds is not passed in, it uses m_UpdatePeriodMs.
        void setupNextIteration(SchedulerTimerState nextState);
        void setupNextIteration(SchedulerTimerState nextState, int milliseconds);
        // True if the scheduler is between iterations and delaying longer than the typical update period.
        bool currentlySleeping();
        // Used by the constructor in testing mainly so a mock ekos could be used.
        void setupScheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr);
        // Prints all the relative state variables set during an iteration. For debugging.
        void printStates(const QString &label);


        // The type of scheduler iteration that should be run next.
        SchedulerTimerState timerState { RUN_NOTHING };
        // Variable keeping the number of millisconds the scheduler should wait
        // after the current scheduler iteration.
        int timerInterval { -1 };
        // Whether the scheduler has been setup for the next iteration,
        // that is, whether timerInterval and timerState have been set this iteration.
        bool iterationSetup { false };
        // The timer used to wakeup the scheduler between iterations.
        QTimer iterationTimer;
        // Counter for how many scheduler iterations have been processed.
        int schedulerIteration { 0 };
        // The time when the scheduler first started running iterations.
        qint64 startMSecs { 0 };

        friend TestEkosSchedulerOps;
};
}
