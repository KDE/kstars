/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "schedulertypes.h"
#include "ekos/auxiliary/modulelogger.h"
#include "ekos/align/align.h"
#include "indi/indiweather.h"
#include "dms.h"

#include <QObject>
#include <QPointer>
#include <QDBusInterface>
#include <QProcess>

namespace Ekos
{

class SchedulerJob;
class SchedulerModuleState;

/**
 * @class SchedulerProcess
 * @brief The SchedulerProcess class holds the entire business logic for controlling the
 * execution of the EKOS scheduler.
 */
class SchedulerProcess : public QObject
{
    Q_OBJECT

public:
    SchedulerProcess(QSharedPointer<SchedulerModuleState> state);

    // ////////////////////////////////////////////////////////////////////
    // process steps
    // ////////////////////////////////////////////////////////////////////

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
     * @brief stopGuiding After guiding is done we need to stop the process
     */
    void stopGuiding();

    /**
     * @brief processGuidingTimer Check the guiding timer, and possibly restart guiding.
     */
    void processGuidingTimer();

    /**
     * @brief startCapture The current job file name is solved to an url which is fed to ekos. We then start the capture process
     * @param restart Set to true if the goal to restart an existing sequence. The only difference is that when a sequence is restarted, sequence file
     * is not loaded from disk again since that results in erasing all the history of the capture process.
     */
    void startCapture(bool restart = false);

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
    bool completeShutdown();

    /**
     * @brief disconnectINDI disconnect all INDI devices from server.
     */
    void disconnectINDI();

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
    bool checkParkWaitState();

    /**
     * @brief runStartupProcedure Execute the startup of the scheduler itself to be prepared
     * for running scheduler jobs.
     */
    void runStartupProcedure();

    /**
     * @brief runShutdownProcedure Shutdown the scheduler itself and EKOS (if configured to do so).
     */
    void runShutdownProcedure();

    /**
     * @brief saveScheduler Save scheduler jobs to a file
     * @param path path of a file
     * @return true on success, false on failure.
     */
    bool saveScheduler(const QUrl &fileURL);

    // ////////////////////////////////////////////////////////////////////
    // device handling
    // ////////////////////////////////////////////////////////////////////
    void setAlignStatus(Ekos::AlignState status);
    void setGuideStatus(Ekos::GuideState status);
    void setCaptureStatus(Ekos::CaptureState status);
    void setFocusStatus(Ekos::FocusState status);
    void setMountStatus(ISD::Mount::Status status);
    void setWeatherStatus(ISD::Weather::Status status);

    /**
     * @return True if mount is parked
     */
    bool isMountParked();
    /**
     * @return True if dome is parked
     */
    bool isDomeParked();

    // ////////////////////////////////////////////////////////////////////
    // state machine
    // ////////////////////////////////////////////////////////////////////
    QSharedPointer<SchedulerModuleState> m_moduleState;
    QSharedPointer<SchedulerModuleState> moduleState() const
    {
        return m_moduleState;
    }

    // ////////////////////////////////////////////////////////////////////
    // DBUS interfaces to devices
    // ////////////////////////////////////////////////////////////////////
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
    // ////////////////////////////////////////////////////////////////////
    // helper functions
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief setupJob Initialize a job with all fields accessible from the UI.
     */
    static void setupJob(SchedulerJob &job, const QString &name, const QString &group, const dms &ra, const dms &dec,
                         double djd, double rotation, const QUrl &sequenceUrl, const QUrl &fitsUrl, StartupCondition startup,
                         const QDateTime &startupTime, CompletionCondition completion, const QDateTime &completionTime, int completionRepeats,
                         double minimumAltitude, double minimumMoonSeparation, bool enforceWeather, bool enforceTwilight,
                         bool enforceArtificialHorizon, bool track, bool focus, bool align, bool guide);


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
    static uint16_t fillCapturedFramesMap(const QMap<QString, uint16_t> &expected, const CapturedFramesMap &capturedFramesCount, SchedulerJob &schedJob,
                                          CapturedFramesMap &capture_map, int &completedIterations);

    /**
     * @brief processJobInfo a utility used by loadSequenceQueue() to help it read a capture sequence file
     * @param root the filename
     * @param schedJob the SchedulerJob is modified accoring to the contents of the sequence queue
     * @return a capture sequence
     */
    static SequenceJob *processJobInfo(XMLEle *root, SchedulerJob *schedJob);

    /**
         * @brief loadSequenceQueue Loads what's necessary to estimate job completion time from a capture sequence queue file
         * @param fileURL the filename
         * @param schedJob the SchedulerJob is modified according to the contents of the sequence queue
         * @param jobs the returned values read from the file
         * @param hasAutoFocus a return value indicating whether autofocus can be triggered by the sequence.
         * @param logger module logging utility
         */

    static bool loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs, bool &hasAutoFocus, ModuleLogger *logger);

    /**
         * @brief estimateJobTime Estimates the time the job takes to complete based on the sequence file and what modules to utilize during the observation run.
         * @param job target job
         * @param capturedFramesCount a map of what's been captured already
         * @param logger module logging utility
         */
    static bool estimateJobTime(SchedulerJob *schedJob, const QMap<QString, uint16_t> &capturedFramesCount, ModuleLogger *logger);

    /**
     * @brief Calculate the map signature -> expected number of captures from the given list of capture sequence jobs,
     *        i.e. the expected number of captures from a single scheduler job run.
     * @param seqJobs list of capture sequence jobs
     * @param expected map to be filled
     * @return total expected number of captured frames of a single run of all jobs
     */
    static uint16_t calculateExpectedCapturesMap(const QList<SequenceJob *> &seqJobs, QMap<QString, uint16_t> &expected);

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
     * @brief timeHeuristics Estimates the number of seconds of overhead above and beyond imaging time, used by estimateJobTime.
     * @param schedJob the scheduler job.
     * @return seconds of overhead.
     */
    static int timeHeuristics(const SchedulerJob *schedJob);

    /**
     * @brief getGuidingStatus Retrieve the guiding status.
     */
    GuideState getGuidingStatus();

    QProcess &scriptProcess()
    {
        return m_scriptProcess;
    }

signals:
    // new log text for the module log window
    void newLog(const QString &text);
    // controls for scheduler execution
    void stopScheduler();
    void stopCurrentJobAction();
    void findNextJob();
    void getNextAction();

private:
    // DBus interfaces to devices
    QPointer<QDBusInterface> m_ekosInterface { nullptr };
    QPointer<QDBusInterface> m_indiInterface { nullptr };
    QPointer<QDBusInterface> m_focusInterface { nullptr };
    QPointer<QDBusInterface> m_captureInterface { nullptr };
    QPointer<QDBusInterface> m_mountInterface { nullptr };
    QPointer<QDBusInterface> m_alignInterface { nullptr };
    QPointer<QDBusInterface> m_guideInterface { nullptr };
    QPointer<QDBusInterface> m_domeInterface { nullptr };
    QPointer<QDBusInterface> m_weatherInterface { nullptr };
    QPointer<QDBusInterface> m_capInterface { nullptr };

    // Startup and Shutdown scripts process
    QProcess m_scriptProcess;
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
     * @brief activeJob Shortcut to the active job held in the state machine
     */
    SchedulerJob *activeJob();
};
} // Ekos namespace
