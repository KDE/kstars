/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "schedulertypes.h"
#include "schedulerjob.h"
#include "ekos/auxiliary/modulelogger.h"
#include "ekos/align/align.h"
#include "ekos/auxiliary/solverutils.h"
#include "indi/indiweather.h"
#include "dms.h"

#include <QObject>
#include <QPointer>
#include <QtDBus/QDBusInterface>
#include <QProcess>

namespace Ekos
{

class GreedyScheduler;
class SchedulerModuleState;

/**
 * @class SchedulerProcess
 * @brief The SchedulerProcess class holds the entire business logic for controlling the
 * execution of the EKOS scheduler.
 */
class SchedulerProcess : public QObject, public ModuleLogger
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Scheduler")
    Q_PROPERTY(Ekos::SchedulerState status READ status NOTIFY newStatus)
    Q_PROPERTY(QString profile READ profile WRITE setProfile)
    Q_PROPERTY(QString currentJobName READ currentJobName)
    Q_PROPERTY(QString currentJobJson READ currentJobJson)
    Q_PROPERTY(QString jsonJobs READ jsonJobs)
    Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

public:
    SchedulerProcess(QSharedPointer<SchedulerModuleState> state, const QString &ekosPathStr, const QString &ekosInterfaceStr);


    // ////////////////////////////////////////////////////////////////////
    // external DBUS interface
    // ////////////////////////////////////////////////////////////////////

    SchedulerState status();

    // ////////////////////////////////////////////////////////////////////
    // process steps
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief execute Execute the schedule, start if idle or paused.
     */
    Q_SCRIPTABLE void execute();

    /**
     * @brief findNextJob Check if the job met the completion criteria, and if it did, then it search for next job candidate.
     *  If no jobs are found, it starts the shutdown stage.
     */
    void findNextJob();

    /**
     * @brief stopCurrentJobAction Stop whatever action taking place in the current job (eg. capture, guiding...etc).
     */
    Q_SCRIPTABLE void stopCurrentJobAction();

    /**
      * @brief executeJob After the best job is selected, we call this in order to start the process that will execute the job.
      * checkJobStatus slot will be connected in order to figure the exact state of the current job each second
      * @return True if job is accepted and can be executed, false otherwise.
      */
    bool executeJob(SchedulerJob *job);

    /**
     * @brief wakeUpScheduler Wake up scheduler from sleep state
     */
    Q_SCRIPTABLE void wakeUpScheduler();

    /** DBUS interface function.
     * @brief Setup the main loop and start.
     */
    Q_SCRIPTABLE Q_NOREPLY void start();

    /** DBUS interface function.
     * @brief stop Stop the scheduler execution. If stopping succeeded,
     * a {@see #schedulerStopped()} signal is emitted
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
    Q_SCRIPTABLE Q_NOREPLY void setSequence(const QString &sequenceFileURL);

    /** DBUS interface function.
     * @brief Resets all jobs to IDLE
     */
    Q_SCRIPTABLE Q_NOREPLY void resetAllJobs();

    /**
     * @brief shouldSchedulerSleep Check if the scheduler needs to sleep until the job is ready
     * @param job Job to check
     * @return True if we set the scheduler to sleep mode. False, if not required and we need to execute now
     */
    bool shouldSchedulerSleep(SchedulerJob *job);

    /**
     * @brief mountCoords read the equatorial coordinates from the mount
     * @return {RA, DEC}
     */
    SkyPoint mountCoords();

    /**
     * @brief startSlew DBus call for initiating slew
     */
    Q_SCRIPTABLE Q_NOREPLY void startSlew();

    /**
     * @brief startFocusing DBus call for feeding ekos the specified settings and initiating focus operation
     */
    Q_SCRIPTABLE Q_NOREPLY void startFocusing();

    /**
     * @brief startAstrometry initiation of the capture and solve operation. We change the job state
     * after solver is started
     */
    Q_SCRIPTABLE Q_NOREPLY void startAstrometry();

    /**
     * @brief startGuiding After ekos is fed the calibration options, we start the guiding process
     * @param resetCalibration By default calibration is not reset until it is explicitly requested
     */
    Q_SCRIPTABLE Q_NOREPLY void startGuiding(bool resetCalibration = false);

    /**
     * @brief stopGuiding After guiding is done we need to stop the process
     */
    Q_SCRIPTABLE Q_NOREPLY void stopGuiding();

    /**
     * @brief processGuidingTimer Check the guiding timer, and possibly restart guiding.
     */
    void processGuidingTimer();

    /**
     * @brief startCapture The current job file name is solved to an url which is fed to ekos. We then start the capture process
     * @param restart Set to true if the goal to restart an existing sequence. The only difference is that when a sequence is restarted, sequence file
     * is not loaded from disk again since that results in erasing all the history of the capture process.
     */
    Q_SCRIPTABLE Q_NOREPLY void startCapture(bool restart = false);

    void startSingleCapture(SchedulerJob *job, bool restart = false);

    /**
     * @brief updateCompletedJobsCount For each scheduler job, examine sequence job storage and count captures.
     * @param forced forces recounting captures unconditionally if true, else only IDLE, EVALUATION or new jobs are examined.
     */
    void updateCompletedJobsCount(bool forced = false);

    /**
     * @brief setSolverAction set the GOTO mode for the solver
     * @param mode 0 For Sync, 1 for SlewToTarget, 2 for Nothing
     */
    void setSolverAction(Align::GotoMode mode);

    /**
     * @brief loadProfiles Load the existing EKOS profiles
     */
    void loadProfiles();

    /**
     * @brief checkEkosState Check ekos startup stages and take whatever action necessary to get Ekos up and running
     * @return True if Ekos is running, false if Ekos start up is in progress.
     */
    bool checkEkosState();

    /**
     * @brief checkINDIState Check INDI startup stages and take whatever action necessary to get INDI devices connected.
     * @return True if INDI devices are connected, false if it is under progress.
     */
    bool checkINDIState();

    /**
     * @brief completeShutdown Try to complete the scheduler shutdown
     * @return false iff some further action is required
     */
    Q_SCRIPTABLE bool completeShutdown();

    /**
     * @brief disconnectINDI disconnect all INDI devices from server.
     */
    Q_SCRIPTABLE Q_NOREPLY void disconnectINDI();

    /**
     * @brief manageConnectionLoss Mitigate loss of connection with the INDI server.
     * @return true if connection to Ekos/INDI should be attempted again, false if not mitigation is available or needed.
     */
    bool manageConnectionLoss();

    /**
     * @brief checkDomeParkingStatus check dome parking status and updating corresponding states accordingly.
     */
    void checkCapParkingStatus();

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
    Q_SCRIPTABLE bool checkParkWaitState();

    /**
     * @brief runStartupProcedure Execute the startup of the scheduler itself to be prepared
     * for running scheduler jobs.
     */
    Q_SCRIPTABLE Q_NOREPLY void runStartupProcedure();

    /**
     * @brief runShutdownProcedure Shutdown the scheduler itself and EKOS (if configured to do so).
     */
    Q_SCRIPTABLE Q_NOREPLY void runShutdownProcedure();

    /**
     * @brief setPaused pausing the scheduler
     */
    Q_SCRIPTABLE Q_NOREPLY void setPaused();

    /**
     * @brief resetJobs Reset all jobs counters
     */
    Q_SCRIPTABLE Q_NOREPLY void resetJobs();

    /**
     * @brief selectActiveJob Select the job that should be executed
     */
    void selectActiveJob(const QList<SchedulerJob *> &jobs);

    /**
     * @brief startJobEvaluation Start job evaluation only without starting the scheduler process itself. Display the result to the user.
     */
    Q_SCRIPTABLE Q_NOREPLY void startJobEvaluation();

    /**
     * @brief evaluateJobs evaluates the current state of each objects and gives each one a score based on the constraints.
     * Given that score, the scheduler will decide which is the best job that needs to be executed.
     */
    Q_SCRIPTABLE Q_NOREPLY void evaluateJobs(bool evaluateOnly);

    /**
     * @brief checkJobStatus Check the overall state of the scheduler, Ekos, and INDI. When all is OK, it calls evaluateJobs() when no job is current or executeJob() if a job is selected.
     * @return False if this function needs to be called again later, true if situation is stable and operations may continue.
     */
    bool checkStatus();

    /**
     * @brief getNextAction Checking for the next appropriate action regarding the current state of the scheduler  and execute it
     */
    void getNextAction();

    /**
     * @brief Repeatedly runs a scheduler iteration and then sleeps timerInterval millisconds
     * and run the next iteration. This continues until the sleep time is negative.
     */
    void iterate();

    /**
     * @brief Run a single scheduler iteration.
     */
    int runSchedulerIteration();

    /**
     * @brief checkJobStage Check the progress of the job states and make DBUS calls to start the next stage until the job is complete.
     */
    void checkJobStage();
    void checkJobStageEpilogue();

    /**
     * @brief applyConfig Apply configuration changes from the global configuration dialog.
     */
    void applyConfig();

    /**
     * @brief saveScheduler Save scheduler jobs to a file
     * @param path path of a file
     * @return true on success, false on failure.
     */
    Q_SCRIPTABLE bool saveScheduler(const QUrl &fileURL);

    /**
     * @brief appendEkosScheduleList Append the contents of an ESL file to the queue.
     * @param fileURL File URL to load contents from.
     * @return True if contents were loaded successfully, else false.
     */
    Q_SCRIPTABLE bool appendEkosScheduleList(const QString &fileURL);

    /**
     * @brief appendLogText Append a new line to the logging.
     */
    Q_SCRIPTABLE Q_NOREPLY void appendLogText(const QString &logentry) override;

    /**
     * @brief clearLog Clear log entry
     */
    Q_SCRIPTABLE void clearLog();

    /**
     * @return True if mount is parked
     */
    Q_SCRIPTABLE bool isMountParked();
    /**
     * @return True if dome is parked
     */
    Q_SCRIPTABLE bool isDomeParked();

    void simClockScaleChanged(float);
    void simClockTimeChanged();

    // ////////////////////////////////////////////////////////////////////
    // state machine and scheduler
    // ////////////////////////////////////////////////////////////////////
    QSharedPointer<SchedulerModuleState> m_moduleState;
    QSharedPointer<SchedulerModuleState> moduleState() const
    {
        return m_moduleState;
    }

    QPointer<Ekos::GreedyScheduler> m_GreedyScheduler;
    QPointer<GreedyScheduler> &getGreedyScheduler()
    {
        return m_GreedyScheduler;
    }

    // ////////////////////////////////////////////////////////////////////
    // DBUS interfaces to devices
    // ////////////////////////////////////////////////////////////////////

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

    QString observatoryInterfaceString { "org.kde.kstars.Ekos.Observatory" };
    void setObservatoryInterfaceString(const QString &interface)
    {
        observatoryInterfaceString = interface;
    }
    QString observatoryPathString { "/KStars/Ekos/Observatory" };
    void setObservatoryPathString(const QString &interface)
    {
        observatoryPathString = interface;
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

    QPointer<QDBusInterface> ekosInterface() const
    {
        return m_ekosInterface;
    }
    void setEkosInterface(QPointer<QDBusInterface> newInterface)
    {
        m_ekosInterface = newInterface;
    }
    QPointer<QDBusInterface> indiInterface() const
    {
        return m_indiInterface;
    }
    void setIndiInterface(QPointer<QDBusInterface> newInterface)
    {
        m_indiInterface = newInterface;
    }
    QPointer<QDBusInterface> focusInterface() const
    {
        return m_focusInterface;
    }
    void setFocusInterface(QPointer<QDBusInterface> newInterface)
    {
        m_focusInterface = newInterface;
    }
    QPointer<QDBusInterface> captureInterface() const
    {
        return m_captureInterface;
    }
    void setCaptureInterface(QPointer<QDBusInterface> newInterface)
    {
        m_captureInterface = newInterface;
    }
    QPointer<QDBusInterface> mountInterface() const
    {
        return m_mountInterface;
    }
    void setMountInterface(QPointer<QDBusInterface> newInterface)
    {
        m_mountInterface = newInterface;
    }
    QPointer<QDBusInterface> alignInterface() const
    {
        return m_alignInterface;
    }
    void setAlignInterface(QPointer<QDBusInterface> newInterface)
    {
        m_alignInterface = newInterface;
    }
    QPointer<QDBusInterface> guideInterface() const
    {
        return m_guideInterface;
    }
    void setGuideInterface(QPointer<QDBusInterface> newInterface)
    {
        m_guideInterface = newInterface;
    }
    QPointer<QDBusInterface> observatoryInterface() const
    {
        return m_observatoryInterface;
    }
    void setObservatoryInterface(QPointer<QDBusInterface> newInterface)
    {
        m_observatoryInterface = newInterface;
    }
    QPointer<QDBusInterface> domeInterface() const
    {
        return m_domeInterface;
    }
    void setDomeInterface(QPointer<QDBusInterface> newInterface)
    {
        m_domeInterface = newInterface;
    }
    QPointer<QDBusInterface> weatherInterface() const
    {
        return m_weatherInterface;
    }
    void setWeatherInterface(QPointer<QDBusInterface> newInterface)
    {
        m_weatherInterface = newInterface;
    }
    QPointer<QDBusInterface> capInterface() const
    {
        return m_capInterface;
    }
    void setCapInterface(QPointer<QDBusInterface> newInterface)
    {
        m_capInterface = newInterface;
    }

    /**
     * @brief createJobSequence Creates a job sequence for the mosaic tool given the prefix and output dir. The currently selected sequence file is modified
     * and a new version given the supplied parameters are saved to the output directory
     * @param prefix Prefix to set for the job sequence
     * @param outputDir Output dir to set for the job sequence
     * @return True if new file is saved, false otherwise
     */
    bool createJobSequence(XMLEle *root, const QString &prefix, const QString &outputDir);

    /**
     * @brief getSequenceJobRoot Read XML data from capture sequence job
     * @param filename
     * @return
     */
    XMLEle *getSequenceJobRoot(const QString &filename) const;

    /**
     * @brief getGuidingStatus Retrieve the guiding status.
     */
    GuideState getGuidingStatus();

    QProcess &scriptProcess()
    {
        return m_scriptProcess;
    }

    const QString &profile() const;
    void setProfile(const QString &newProfile);

    QString currentJobName();
    QString currentJobJson();
    QString jsonJobs();

    QStringList logText();

signals:
    // new log text for the module log window
    void newLog(const QString &text);
    // status updates
    void newStatus(SchedulerState state);
    void newWeatherStatus(ISD::Weather::Status state);
    void schedulerStopped();
    void shutdownStarted();
    void schedulerSleeping(bool shutdown, bool sleep);
    void schedulerPaused();
    void changeSleepLabel(QString text, bool show = true);
    // state changes
    void jobsUpdated(QJsonArray jobsList);
    void updateJobTable(SchedulerJob *job = nullptr);
    void clearJobTable();
    void changeCurrentSequence(const QString &sequenceFileURL);
    void interfaceReady(QDBusInterface *iface);
    // loading jobs
    void addJob(SchedulerJob *job);
    void syncGreedyParams();
    void syncGUIToGeneralSettings();
    void updateSchedulerURL(const QString &fileURL);
    // distance in arc-seconds measured by plate solving the a captured image and
    // comparing that position to the target position.
    void targetDistance(double distance);
    // required for Analyze timeline
    void jobStarted(const QString &jobName);
    void jobEnded(const QString &jobName, const QString &endReason);


private slots:
    void setINDICommunicationStatus(Ekos::CommunicationStatus status);
    void setEkosCommunicationStatus(Ekos::CommunicationStatus status);

    /**
     * @brief syncProperties Sync startup properties from the various device to enable/disable features in the scheduler
     * like the ability to park/unpark..etc
     */
    void syncProperties()
    {
        checkInterfaceReady(qobject_cast<QDBusInterface*>(sender()));
    }

    // ////////////////////////////////////////////////////////////////////
    // alignment checks
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief checkAlignment Handle one sequence image completion. This is used now only to run alignment check
     * to ensure it does not deviation from current scheduler job target.
     * @param metadata Metadata for image including filename, exposure, filter, hfr..etc.
     * @param trainname name of the optical train that has captured the image
     */
    void checkAlignment(const QVariantMap &metadata, const QString &trainname);

    /**
     * @brief solverDone Process solver solution after it is done.
     * @param timedOut True if the process timed out.
     * @param success True if successful, false otherwise.
     * @param solution The solver solution if successful.
     * @param elapsedSeconds How many seconds elapsed to solve the image.
     */
    void solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);

    /**
     * @brief checkInterfaceReady Sometimes syncProperties() is not sufficient since the ready signal could have fired already
     * and cannot be relied on to know once a module interface is ready. Therefore, we explicitly check if the module interface
     * is ready.
     * @param iface interface to test for readiness.
     */
    void checkInterfaceReady(QDBusInterface *iface);

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

    // ////////////////////////////////////////////////////////////////////
    // device handling
    // ////////////////////////////////////////////////////////////////////
    void setAlignStatus(Ekos::AlignState status);
    void setGuideStatus(Ekos::GuideState status);
    void setCaptureStatus(Ekos::CaptureState status, const QString &trainname);
    void setFocusStatus(Ekos::FocusState status, const QString &trainname);
    void setMountStatus(ISD::Mount::Status status);
    void setWeatherStatus(ISD::Weather::Status status);

private:
    // When a module is commanded to perform an action, wait this many milliseconds
    // before check its state again. If State is still IDLE, then it either didn't received the command
    // or there is another problem.
    static const uint32_t ALIGN_INACTIVITY_TIMEOUT      = 120000;
    static const uint32_t FOCUS_INACTIVITY_TIMEOUT      = 120000;
    static const uint32_t CAPTURE_INACTIVITY_TIMEOUT    = 120000;
    static const uint16_t GUIDE_INACTIVITY_TIMEOUT      = 60000;
    /// Counter to keep debug logging in check
    uint8_t checkJobStageCounter { 0 };

    // Startup and Shutdown scripts process
    QProcess m_scriptProcess;
    // solver for alignment checks
    QSharedPointer<SolverUtils> m_Solver;

    // mapping train name --> scheduler job
    QMap<QString, SchedulerJob*> m_activeJobs;

    // ////////////////////////////////////////////////////////////////////
    // DBUS interfaces
    // ////////////////////////////////////////////////////////////////////
    // Interface strings for the dbus. Changeable for mocks when testing. Private so only tests can change.
    QString schedulerProcessPathString { "/KStars/Ekos/Scheduler" };
    QString kstarsInterfaceString { "org.kde.kstars" };
    // This is only used in the constructor
    QString ekosInterfaceString { "org.kde.kstars.Ekos" };
    QString ekosPathString { "/KStars/Ekos" };
    QString INDIInterfaceString { "org.kde.kstars.INDI" };
    QString INDIPathString {"/KStars/INDI"};
    // DBus interfaces to devices
    QPointer<QDBusInterface> m_ekosInterface { nullptr };
    QPointer<QDBusInterface> m_indiInterface { nullptr };
    QPointer<QDBusInterface> m_focusInterface { nullptr };
    QPointer<QDBusInterface> m_captureInterface { nullptr };
    QPointer<QDBusInterface> m_mountInterface { nullptr };
    QPointer<QDBusInterface> m_alignInterface { nullptr };
    QPointer<QDBusInterface> m_guideInterface { nullptr };
    QPointer<QDBusInterface> m_observatoryInterface { nullptr };
    QPointer<QDBusInterface> m_domeInterface { nullptr };
    QPointer<QDBusInterface> m_weatherInterface { nullptr };
    QPointer<QDBusInterface> m_capInterface { nullptr };
    // ////////////////////////////////////////////////////////////////////
    // process steps
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief executeScript Execute pre- or post job script
     */
    void executeScript(const QString &filename);

    /**
     * @brief stopEkos shutdown Ekos completely
     */
    void stopEkos();

    /**
     * @brief checkMountParkingStatus check mount parking status and updating corresponding states accordingly.
     */
    void checkMountParkingStatus();

    /**
     * @brief checkDomeParkingStatus check dome parking status and updating corresponding states accordingly.
     */
    void checkDomeParkingStatus();

    /**
     * @brief startFocusing Start focusing on the job
     */
    void startFocusing(SchedulerJob *job);
    // ////////////////////////////////////////////////////////////////////
    // device handling
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief parkCap Close dust cover
     */
    void parkCap();

    /**
     * @brief unCap Open dust cover
     */
    void unParkCap();

    /**
     * @brief parkMount Park mount
     */
    void parkMount();

    /**
     * @brief unParkMount Unpark mount
     */
    void unParkMount();

    /**
     * @brief parkDome Park dome
     */
    void parkDome();

    /**
     * @brief unParkDome Unpark dome
     */
    void unParkDome();

    // ////////////////////////////////////////////////////////////////////
    // helper functions
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief checkStartupProcedure restart regularly {@see #checkStartupState()} until completed
     */
    void checkStartupProcedure();

    /**
     * @brief checkShutdownProcedure Check regularly if the shutdown has completed (see
     * {@see #checkShutdownState()}) and stop EKOS if the corresponding configuration flag is set.
     */
    void checkShutdownProcedure();

    /**
     * @brief checkProcessExit Check script process exist status. This is called when the process exists either normally or abnormally.
     * @param exitCode exit code from the script process. Depending on the exist code, the status of startup/shutdown procedure is set accordingly.
     */
    void checkProcessExit(int exitCode);

    /**
     * @brief readProcessOutput read running script process output and display it in Ekos
     */
    void readProcessOutput();

    /**
     * @brief Returns true if the job is storing its captures on the same machine as the scheduler.
     */
    bool canCountCaptures(const SchedulerJob &job);

    /**
     * @brief activeJob Shortcut to the active job held in the state machine
     */
    SchedulerJob *activeJob();

    // Prints all the relative state variables set during an iteration. For debugging.
    void printStates(const QString &label);

    /**
     * @brief Abort capturing
     * @param train train name, if empty abort the master job, otherwise abort this job's capture
     * @param followersOnly if true and train name is empty, do not abort the lead, but all its followers
     */
    void stopCapturing(QString train = "", bool followersOnly = false);
};
} // Ekos namespace
