/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project:
    SPDX-FileCopyrightText: 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_scheduler.h"
#include "schedulertypes.h"
#include "ekos/align/align.h"
#include "indi/indiweather.h"
#include "schedulerjob.h"

#include <lilxml.h>

#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QDBusInterface>

#include <cstdint>

class QProgressIndicator;

class GeoLocation;
class SkyObject;
class KConfigDialog;
class TestSchedulerUnit;
class SolverUtils;
class TestEkosSchedulerOps;

namespace Ekos
{

class SequenceJob;
class GreedyScheduler;
class SchedulerProcess;
class SchedulerModuleState;

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

        /** @brief Columns, in the same order as UI. */
        typedef enum
        {
            SCHEDCOL_NAME = 0,
            SCHEDCOL_STATUS,
            SCHEDCOL_CAPTURES,
            SCHEDCOL_ALTITUDE,
            SCHEDCOL_STARTTIME,
            SCHEDCOL_ENDTIME,
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
        Scheduler(const QString path, const QString interface,
                  const QString &ekosPathStr, const QString &ekosInterfaceStr);
        ~Scheduler() = default;

        QString getCurrentJobName();

        // shortcut
        SchedulerJob *activeJob();

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
             * @brief stopGuiding After guiding is done we need to stop the process
             */
        void stopGuiding();

        /**
             * @brief setSolverAction set the GOTO mode for the solver
             * @param mode 0 For Sync, 1 for SlewToTarget, 2 for Nothing
             */
        void setSolverAction(Align::GotoMode mode);

        /**
         * @brief importMosaic Import mosaic into planner and generate jobs for the scheduler.
         * @param payload metadata for the mosaic information.
         * @note Only Telescopius.com mosaic format is now supported.
         */
        bool importMosaic(const QJsonObject &payload);

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

        Ekos::SchedulerState status();

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
        static void setupJob(SchedulerJob &job, const QString &name, const QString &group, const dms &ra,
            const dms &dec, double djd, double rotation, const QUrl &sequenceUrl, const QUrl &fitsUrl,
            SchedulerJob::StartupCondition startup, const QDateTime &startupTime,
            SchedulerJob::CompletionCondition completion, const QDateTime &completionTime, int completionRepeats,
            double minimumAltitude, double minimumMoonSeparation, bool enforceWeather, bool enforceTwilight,
            bool enforceArtificialHorizon, bool track, bool focus, bool align, bool guide);

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
             * @param schedJob the SchedulerJob is modified according to the contents of the sequence queue
             * @param jobs the returned values read from the file
             * @param hasAutoFocus a return value indicating whether autofocus can be triggered by the sequence.
              * @param scheduler instance of the scheduler used for logging. Can be nullptr.
             * @return Estimated time in seconds.
             */

        static bool loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs,
                                      bool &hasAutoFocus, Scheduler *scheduler);

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

        // Scheduler Settings
        void setSettings(const QJsonObject &settings);

        void setUpdateInterval(int ms)
        {
            m_UpdatePeriodMs = ms;
        }
        int getUpdateInterval()
        {
            return m_UpdatePeriodMs;
        }

        // Primary Settings
        void setPrimarySettings(const QJsonObject &settings);

        // Job Startup Conditions
        void setJobStartupConditions(const QJsonObject &settings);

        // Job Constraints
        void setJobConstraints(const QJsonObject &settings);

        // Job Completion Conditions
        void setJobCompletionConditions(const QJsonObject &settings);

        // Observatory Startup Procedure
        void setObservatoryStartupProcedure(const QJsonObject &settings);

        // Aborted Job Managemgent Settings
        void setAbortedJobManagementSettings(const QJsonObject &settings);

        // Observatory Shutdown Procedure
        void setObservatoryShutdownProcedure(const QJsonObject &settings);

        /**
         * @brief Remove a job from current table row.
         * @param index
         */
        void removeJob();

        /**
         * @brief Remove a job by selecting a table row.
         * @param index
         */
        void removeOneJob(int index);

        /**
         * @brief addJob Add a new job from form values
         */
        void addJob();

        /**
         * @brief addToQueue Construct a SchedulerJob and add it to the queue or save job settings from current form values.
         * jobUnderEdit determines whether to add or edit
         */
        void saveJob();

        QJsonArray getJSONJobs();

        void toggleScheduler();

        QJsonObject getSchedulerSettings();
        /**
             * @brief createJobSequence Creates a job sequence for the mosaic tool given the prefix and output dir. The currently selected sequence file is modified
             * and a new version given the supplied parameters are saved to the output directory
             * @param prefix Prefix to set for the job sequence
             * @param outputDir Output dir to set for the job sequence
             * @return True if new file is saved, false otherwise
             */
        bool createJobSequence(XMLEle *root, const QString &prefix, const QString &outputDir);

        XMLEle *getSequenceJobRoot(const QString &filename) const;

        /**
             * @brief saveScheduler Save scheduler jobs to a file
             * @param path path of a file
             * @return true on success, false on failure.
             */
        Q_SCRIPTABLE bool saveScheduler(const QUrl &fileURL);

        /** Update table cells for all jobs.
         *  For framing assistant
         */
        void updateTable();


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

        void setAlgorithm(int alg);

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
           * @brief registerNewDevice register interfaces associated with devices
           * @param name Device name
           * @param interface Device driver interface
           */
        void registerNewDevice(const QString &name, int interface);

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
        void setMountStatus(ISD::Mount::Status status);
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
             * @brief editJob Edit an observation job
             * @param i index model in queue table
             */
        void loadJob(QModelIndex i);

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
         * @brief syncGUIToGeneralSettings set all UI fields that are not job specific
         */
        void syncGUIToGeneralSettings();

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
        void checkJobStageEplogue();

        /**
             * @brief findNextJob Check if the job met the completion criteria, and if it did, then it search for next job candidate. If no jobs are found, it starts the shutdown stage.
             */
        void findNextJob();

        /**
             * @brief stopCurrentJobAction Stop whatever action taking place in the current job (eg. capture, guiding...etc).
             */
        void stopCurrentJobAction();

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
             * @brief displayTwilightWarning Display twilight warning to user if it is unchecked.
             */
        void checkTwilightWarning(bool enabled);

        void setINDICommunicationStatus(Ekos::CommunicationStatus status);
        void setEkosCommunicationStatus(Ekos::CommunicationStatus status);

        void simClockScaleChanged(float);
        void simClockTimeChanged();

        /**
         * @brief solverDone Process solver solution after it is done.
         * @param timedOut True if the process timed out.
         * @param success True if successful, false otherwise.
         * @param solution The solver solution if successful.
         * @param elapsedSeconds How many seconds elapsed to solve the image.
         */
        void solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);

        bool syncControl(const QJsonObject &settings, const QString &key, QWidget * widget);

        /**
         * @brief setCaptureComplete Handle one sequence image completion. This is used now only to run alignment check
         * to ensure it does not deviation from current scheduler job target.
         * @param metadata Metadata for image including filename, exposure, filter, hfr..etc.
         */
        void setCaptureComplete(const QVariantMap &metadata);

    signals:
        void newLog(const QString &text);
        void newStatus(Ekos::SchedulerState state);
        void weatherChanged(ISD::Weather::Status state);
        void newTarget(const QString &);
        // distance in arc-seconds measured by plate solving the a captured image and
        // comparing that position to the target position.
        void targetDistance(double distance);
        // Below 2 are for the Analyze timeline.
        void jobStarted(const QString &jobName);
        void jobEnded(const QString &jobName, const QString &endReason);
        void jobsUpdated(QJsonArray jobsList);

    private:
        /**
             * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
             * Given that score, the scheduler will decide which is the best job that needs to be executed.
             */
        void evaluateJobs(bool evaluateOnly);
        void processJobs(QList<SchedulerJob *> sortedJobs, bool jobEvaluationOnly);

        /**
         * @brief resetJobs Reset all jobs counters
         */
        void resetJobs();

        /**
             * @brief executeJob After the best job is selected, we call this in order to start the process that will execute the job.
             * checkJobStatus slot will be connected in order to figure the exact state of the current job each second
             * @param value
             * @return True if job is accepted and can be executed, false otherwise.
             */
        bool executeJob(SchedulerJob *job);

        /**
             * @brief calculateDawnDusk Get dawn and dusk times for today
             */
        static void calculateDawnDusk();

        /**
             * @brief checkShutdownState Check shutdown procedure stages and make sure all stages are complete.
             * @return
             */
        bool checkShutdownState();

        /**
             * @brief processJobInfo Process the job information from a scheduler file and populate jobs accordingly
             * @param root XML root element of JOB
             * @return true on success, false on failure.
             */
        bool processJobInfo(XMLEle *root);

        /** @internal Change the current job, updating associated widgets.
         * @param job is an existing SchedulerJob to set as current, or nullptr.
         */
        void setCurrentJob(SchedulerJob *job);

        /**
         * @brief processFITSSelection When a FITS file is selected, open it and try to guess
         * the object name, and its J2000 RA/DE to fill the UI with such info automatically.
         */
        void processFITSSelection();

        /**
         * @brief updateProfiles React upon changed profiles and update the UI
         */
        void updateProfiles();


        /**
            * @brief updateCompletedJobsCount For each scheduler job, examine sequence job storage and count captures.
            * @param forced forces recounting captures unconditionally if true, else only IDLE, EVALUATION or new jobs are examined.
            */
        void updateCompletedJobsCount(bool forced = false);

        /**
         * @brief Update the flag for the given job whether light frames are required
         * @param oneJob scheduler job where the flag should be updated
         * @param seqjobs list of capture sequences of the job
         * @param framesCount map capture signature -> frame count
         * @return true iff the job need to capture light frames
         */
        void updateLightFramesRequired(SchedulerJob *oneJob, const QList<SequenceJob *> &seqjobs,
                                       const SchedulerJob::CapturedFramesMap &framesCount);

        /**
         * @brief Calculate the map signature -> expected number of captures from the given list of capture sequence jobs,
         *        i.e. the expected number of captures from a single scheduler job run.
         * @param seqJobs list of capture sequence jobs
         * @param expected map to be filled
         * @return total expected number of captured frames of a single run of all jobs
         */
        static uint16_t calculateExpectedCapturesMap(const QList<SequenceJob *> &seqJobs, QMap<QString, uint16_t> &expected);

        /**
         * @brief Fill the map signature -> frame count so that a single iteration of the scheduled job creates as many frames as possible
         *        in addition to the already captured ones, but does not the expected amount.
         * @param expected map signature -> expected frames count
         * @param capturedFramesCount map signature -> already captured frames count
         * @param schedJob scheduler job for which these calculations are done
         * @param capture_map map signature -> frame count that will be handed over to the capture module to control that a single iteration
         *        of the scheduler job creates as many frames as possible, but does not exceed the expected ones.
         * @param completedIterations How many times has the job completed its capture sequence (for repeated jobs).
         * @return total number of captured frames, truncated to the maximal number of frames the scheduler job could produce
         */
        static uint16_t fillCapturedFramesMap(const QMap<QString, uint16_t> &expected,
                                              const SchedulerJob::CapturedFramesMap &capturedFramesCount,
                                              SchedulerJob &schedJob, SchedulerJob::CapturedFramesMap &capture_map,
                                              int &completedIterations);

        int getCompletedFiles(const QString &path);

        // retrieve the guiding status
        GuideState getGuidingStatus();

        // Controls for the guiding timer, which restarts guiding after failure.
        void cancelGuidingTimer();
        bool isGuidingTimerActive();
        void startGuidingTimer(int milliseconds);
        void processGuidingTimer();

        // Returns true if the job is storing its captures on the same machine as the scheduler.
        bool canCountCaptures(const SchedulerJob &job);

        /**
         * @brief checkRepeatSequence Check if the entire job sequence might be repeated
         * @return true if the checkbox is set and the number of iterations is below the
         * configured threshold
         */
        bool checkRepeatSequence()
        {
            return repeatSequenceCB->isEnabled() && repeatSequenceCB->isChecked() &&
                    (executionSequenceLimit->value() == 0 || sequenceExecutionCounter < executionSequenceLimit->value());
        }

        int sequenceExecutionCounter = 1;

        Ekos::Scheduler *ui { nullptr };

        // Interface strings for the dbus. Changeable for mocks when testing. Private so only tests can change.
        QString schedulerPathString { "/KStars/Ekos/Scheduler" };
        QString kstarsInterfaceString { "org.kde.kstars" };

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

        QString INDIInterfaceString { "org.kde.kstars.INDI" };
        void setINDIInterfaceString(const QString &interface)
        {
            INDIInterfaceString = interface;
        }

        QString INDIPathString {"/KStars/INDI"};
        void setINDIPathString(const QString &interface)
        {
            INDIPathString = interface;
        }

        QString domeInterfaceString { "org.kde.kstars.INDI.Dome" };
        void setDomeInterfaceString(const QString &interface)
        {
            domeInterfaceString = interface;
        }

        QString domePathString;
        void setDomePathString(const QString &interface)
        {
            domePathString = interface;
        }

        QString weatherInterfaceString { "org.kde.kstars.INDI.Weather" };
        void setWeatherInterfaceString(const QString &interface)
        {
            weatherInterfaceString = interface;
        }
        QString weatherPathString;
        void setWeatherPathString(const QString &interface)
        {
            weatherPathString = interface;
        }

        QString dustCapInterfaceString { "org.kde.kstars.INDI.DustCap" };
        void setDustCapInterfaceString(const QString &interface)
        {
            dustCapInterfaceString = interface;
        }
        QString dustCapPathString;
        void setDustCapPathString(const QString &interface)
        {
            dustCapPathString = interface;
        }

        // the state machine holding all states
        QSharedPointer<SchedulerModuleState> m_moduleState;
        QSharedPointer<SchedulerModuleState> moduleState() const
        {
            return m_moduleState;
        }
        // process engine implementing all process steps
        QPointer<SchedulerProcess> m_process;
        QPointer<SchedulerProcess> process()
        {
            return m_process;
        }

        // react upon changes of EKOS and INDI state
        void ekosStateChanged(EkosState state);
        void indiStateChanged(INDIState state);

        // react upon state changes
        void startupStateChanged(StartupState state);
        void shutdownStateChanged(ShutdownState state);
        void parkWaitStateChanged(ParkWaitState state);

        /// URL to store the scheduler file
        QUrl schedulerURL;
        /// URL for Ekos Sequence
        QUrl sequenceURL;
        /// FITS URL to solve
        QUrl fitsURL;
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
        /// Store next dawn to calculate dark skies range
        static QDateTime Dawn;
        /// Store next dusk to calculate dark skies range
        static QDateTime Dusk;
        /// Pre-dawn is where we stop all jobs, it is a user-configurable value before Dawn.
        static QDateTime preDawnDateTime;
        /// Keep watch of weather status
        ISD::Weather::Status weatherStatus { ISD::Weather::WEATHER_IDLE };

        /// Keep track of Load & Slew operation
        bool loadAndSlewProgress { false };
        /// Check if initial autofocus is completed and do not run autofocus until there is a change is telescope position/alignment.
        bool autofocusCompleted { false };
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

        QUrl dirPath;

        QMap<QString, uint16_t> m_CapturedFramesCount;

        // When a module is commanded to perform an action, wait this many milliseconds
        // before check its state again. If State is still IDLE, then it either didn't received the command
        // or there is another problem.
        static const uint32_t ALIGN_INACTIVITY_TIMEOUT      = 120000;
        static const uint32_t FOCUS_INACTIVITY_TIMEOUT      = 120000;
        static const uint32_t CAPTURE_INACTIVITY_TIMEOUT    = 120000;
        static const uint16_t GUIDE_INACTIVITY_TIMEOUT      = 60000;

        // Methods & variables that control the scheduler's iterations.

        // Executes the scheduler
        void execute();
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

        /// Target coordinates for pointing check
        QSharedPointer<SolverUtils> m_Solver;
        // Used when solving position every nth capture.
        uint32_t m_SolverIteration {0};

        // Restricts (the internal solver) to using the index and healpix
        // from the previous solve, if that solve was successful, when
        // doing the pointing check. -1 means no restriction.
        int m_IndexToUse { -1 };
        int m_HealpixToUse { -1 };

        void syncGreedyParams();
        QPointer<Ekos::GreedyScheduler> m_GreedyScheduler;

        friend TestEkosSchedulerOps;
        QPointer<GreedyScheduler> &getGreedyScheduler()
        {
            return m_GreedyScheduler;
        }
};
}
