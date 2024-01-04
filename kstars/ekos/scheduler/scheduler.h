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
#include "ekos/auxiliary/modulelogger.h"

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
class Scheduler : public QWidget, public Ui::Scheduler, public ModuleLogger
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Scheduler")
        Q_PROPERTY(Ekos::SchedulerState status READ status NOTIFY newStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QString profile READ profile WRITE setProfile)

        friend class FramingAssistantUI;

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

        /** @brief Constructor, the starndard scheduler constructor. */
        Scheduler();
        /** @brief DebugConstructor, a constructor used in testing with a mock ekos. */
        Scheduler(const QString path, const QString interface,
                  const QString &ekosPathStr, const QString &ekosInterfaceStr);
        ~Scheduler() = default;

        QString getCurrentJobName();

        // shortcut
        SchedulerJob *activeJob();

        void appendLogText(const QString &) override;
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }
        Q_SCRIPTABLE void clearLog();
        void applyConfig();

        void addObject(SkyObject *object);

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
        void addJob(SchedulerJob *job = nullptr);

        /**
         * @brief createJob Create a new job from form values.
         * @param job job to be filled from UI values
         * @return true iff update was successful
         */
        bool fillJobFromUI(SchedulerJob *job);

        /**
         * @brief addToQueue Construct a SchedulerJob and add it to the queue or save job settings from current form values.
         * jobUnderEdit determines whether to add or edit
         */
        void saveJob(SchedulerJob *job = nullptr);

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

        XMLEle *getSequenceJobRoot(const QString &filename);

        /**
             * @brief saveScheduler Save scheduler jobs to a file
             * @param path path of a file
             * @return true on success, false on failure.
             */
        Q_SCRIPTABLE bool saveScheduler(const QUrl &fileURL);

        // the state machine
        QSharedPointer<SchedulerModuleState> moduleState() const
        {
            return m_moduleState;
        }

        // Settings
        QVariantMap getAllSettings() const;
        void setAllSettings(const QVariantMap &settings);
        
private:

        void setAlgorithm(int alg);

        friend TestSchedulerUnit;

        // TODO: See above TODO. End of static methods that might be moved to
        // a separate Scheduler-related class.

        /*@{*/
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

        /**
         * @brief updateJobTable Update the job's row in the job table. If the row does not exist, it will
         * be created on the fly. If job is null, update the entire table
         * @param job
         */
        void updateJobTable(SchedulerJob *job = nullptr);

        /**
         * @brief insertJobTableRow Insert a new row (empty) into the job table
         * @param row row number (starting with 0)
         * @param above insert above the given row (=true) or below (=false)
         */
        void insertJobTableRow(int row, bool above = true);

        /**
         * @brief Update the style of a cell, depending on the job's state
         */
        void updateCellStyle(SchedulerJob *job, QTableWidgetItem *cell);

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
         * @brief updateSchedulerURL Update scheduler URL after succesful loading a new file.
         */
        void updateSchedulerURL(const QString &fileURL);

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
        void settingsUpdated(const QVariantMap &settings);

private:
        /**
             * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
             * Given that score, the scheduler will decide which is the best job that needs to be executed.
             */
        void evaluateJobs(bool evaluateOnly);
        void selectActiveJob(const QList<SchedulerJob *> &jobs);

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
             * @brief checkShutdownState Check shutdown procedure stages and make sure all stages are complete.
             * @return
             */
        bool checkShutdownState();

        /** @internal Change the current job, updating associated widgets.
         * @param job is an existing SchedulerJob to set as current, or nullptr.
         */
        void setActiveJob(SchedulerJob *job);

        /**
         * @brief processFITSSelection When a FITS file is selected, open it and try to guess
         * the object name, and its J2000 RA/DE to fill the UI with such info automatically.
         */
        void processFITSSelection(const QUrl &url);

        /**
         * @brief updateProfiles React upon changed profiles and update the UI
         */
        void updateProfiles();

        /**
         * @brief updateStageLabel Helper function that updates the stage label and has to be placed
         * after all commands that have altered the stage of activeJob(). Hint: Uses updateJobStageUI().
         */
        void updateJobStage(SchedulerJobStage stage);

        /**
         * @brief updateStageLabel Helper function that updates the stage label.
         */
        void updateJobStageUI(SchedulerJobStage stage);

        /**
            * @brief updateCompletedJobsCount For each scheduler job, examine sequence job storage and count captures.
            * @param forced forces recounting captures unconditionally if true, else only IDLE, EVALUATION or new jobs are examined.
            */
        void updateCompletedJobsCount(bool forced = false);

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

        ////////////////////////////////////////////////////////////////////
        /// Settings
        ////////////////////////////////////////////////////////////////////

        /**
         * @brief Connect GUI elements to sync settings once updated.
         */
        void connectSettings();
        /**
         * @brief Stop updating settings when GUI elements are updated.
         */
        void disconnectSettings();
        /**
         * @brief loadSettings Load setting from Options and set them accordingly.
         */
        void loadGlobalSettings();

        /**
         * @brief syncSettings When checkboxes, comboboxes, or spin boxes are updated, save their values in the
         * global and per-train settings.
         */
        void syncSettings();

        /**
         * @brief syncControl Sync setting to widget. The value depends on the widget type.
         * @param settings Map of all settings
         * @param key name of widget to sync
         * @param widget pointer of widget to set
         * @return True if sync successful, false otherwise
         */
        bool syncControl(const QVariantMap &settings, const QString &key, QWidget * widget);

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
        // process engine implementing all process steps
        QSharedPointer<SchedulerProcess> m_process;
        QSharedPointer<SchedulerProcess> process()
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

        /// Counter to keep debug logging in check
        uint8_t checkJobStageCounter { 0 };
        /// Call checkWeather when weatherTimer time expires. It is equal to the UpdatePeriod time in INDI::Weather device.
        //QTimer weatherTimer;

        QUrl dirPath;

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

        // True if the scheduler is between iterations and delaying longer than the typical update period.
        bool currentlySleeping();
        // Used by the constructor in testing mainly so a mock ekos could be used.
        void setupScheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr);
        // Prints all the relative state variables set during an iteration. For debugging.
        void printStates(const QString &label);


        /// Target coordinates for pointing check
        QSharedPointer<SolverUtils> m_Solver;
        // Used when solving position every nth capture.
        uint32_t m_SolverIteration {0};

        void syncGreedyParams();
        QPointer<Ekos::GreedyScheduler> m_GreedyScheduler;

        friend TestEkosSchedulerOps;
        QPointer<GreedyScheduler> &getGreedyScheduler()
        {
            return m_GreedyScheduler;
        }

        QVariantMap m_Settings;
        QVariantMap m_GlobalSettings;
};
}
