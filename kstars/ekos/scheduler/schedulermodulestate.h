/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QProcess>
#include "ekos/ekos.h"
#include "indi/indiweather.h"
#include "kstarsdatetime.h"
#include "geolocation.h"
#include "schedulertypes.h"
#include "ekos/capture/capturetypes.h"
#include <QDateTime>
#include <QUrl>

class SchedulerJob;

namespace Ekos
{

class SchedulerProcess;
class SchedulerJob;

/**
 * @class SchedulerState
 * @brief The SchedulerState class holds all attributes defining the scheduler's state.
 */
class SchedulerModuleState : public QObject
{
        Q_OBJECT

    public:


        SchedulerModuleState();

        // ////////////////////////////////////////////////////////////////////
        // Overall scheduler state
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief init Set initial conditions that need to be set before starting
         */
        void init();

        // ////////////////////////////////////////////////////////////////////
        // profiles and scheduler jobs
        // ////////////////////////////////////////////////////////////////////

        const QString &currentProfile() const
        {
            return m_currentProfile;
        }
        /**
         * @brief setCurrentProfile Set the current profile name.
         * @param newName new current profile name
         * @param signal send an update efent if true
         */
        void setCurrentProfile(const QString &newName, bool signal = true);

        const QStringList &profiles() const
        {
            return m_profiles;
        }
        void updateProfiles(const QStringList &newProfiles);

        SchedulerJob *activeJob(const QString &trainname = "") const;
        void setActiveJob(SchedulerJob *newActiveJob);

        /**
         * @brief Return the master jobs only, slave jobs are ignored
         */
        QList<SchedulerJob *> leadJobs();

        /**
         * @brief Return the slave jobs only, master jobs are ignored
         */
        QList<SchedulerJob *> followerJobs();

        QList<SchedulerJob *> &mutlableJobs()
        {
            return m_jobs;
        }
        const QList<SchedulerJob *> &jobs() const
        {
            return m_jobs;
        }

        void setJobs(QList<SchedulerJob *> &newJobs)
        {
            m_jobs = newJobs;
        }

        /**
         * @brief updateStage Helper function that updates the stage label of the active job.
         */
        void updateJobStage(SchedulerJobStage stage);

        /**
         * @brief getJSONJobs get jobs in JSON format
         * @return
         */
        QJsonArray getJSONJobs();


        // ////////////////////////////////////////////////////////////////////
        // state attributes accessors
        // ////////////////////////////////////////////////////////////////////


        bool dirty() const
        {
            return m_dirty;
        }
        void setDirty(bool value)
        {
            m_dirty = value;
        }

        // (coarse grained) execution state of the scheduler
        const SchedulerState &schedulerState() const
        {
            return m_schedulerState;
        }
        void setSchedulerState(const SchedulerState &newState);

        const StartupState &startupState() const
        {
            return m_startupState;
        }

        int currentPosition() const
        {
            return m_currentPosition;
        }
        void setCurrentPosition(int newCurrentPosition);

        void setStartupState(StartupState state);

        const QUrl &startupScriptURL() const
        {
            return m_startupScriptURL;
        }
        void setStartupScriptURL(const QUrl &newURL)
        {
            m_startupScriptURL = newURL;
        }

        const ShutdownState &shutdownState() const
        {
            return m_shutdownState;
        }
        void setShutdownState(ShutdownState state);

        const QUrl &shutdownScriptURL() const
        {
            return m_shutdownScriptURL;
        }
        void setShutdownScriptURL(const QUrl &newShutdownScriptURL)
        {
            m_shutdownScriptURL = newShutdownScriptURL;
        }

        const ParkWaitState &parkWaitState() const
        {
            return m_parkWaitState;
        }
        void setParkWaitState(ParkWaitState state);

        /**
         * @brief True if the scheduler is between iterations and delaying longer than the typical update period.
         */
        bool currentlySleeping()
        {
            return iterationTimer().isActive() && timerState() == RUN_WAKEUP;
        }

        // ////////////////////////////////////////////////////////////////////
        // job handling
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief removeJob Remove the job from the job list at the given position.
         * If this is the currently active job, don't remove it and return false.
         * @return true iff removing succeeded
         */
        bool removeJob(const int currentRow);

        /**
         * @brief refreshSlaveLists walk through the jobs and update the slave lists
         */
        void refreshFollowerLists();

        /**
         * @brief walk through the job list and find the first master job
         */
        SchedulerJob *findLead(int position, bool upward = true);


        // ////////////////////////////////////////////////////////////////////
        // Controls for the preemptive shutdown feature.
        // ////////////////////////////////////////////////////////////////////
        // Is the scheduler shutting down until later when it will resume a job?
        void enablePreemptiveShutdown(const QDateTime &wakeupTime);
        void disablePreemptiveShutdown();
        const QDateTime &preemptiveShutdownWakeupTime() const;
        bool preemptiveShutdown() const;


        // ////////////////////////////////////////////////////////////////////
        // overall EKOS state
        // ////////////////////////////////////////////////////////////////////
        EkosState ekosState() const
        {
            return m_ekosState;
        }
        void setEkosState(EkosState state);
        // last communication result with EKOS
        CommunicationStatus ekosCommunicationStatus() const
        {
            return m_EkosCommunicationStatus;
        }
        void setEkosCommunicationStatus(CommunicationStatus newEkosCommunicationStatus)
        {
            m_EkosCommunicationStatus = newEkosCommunicationStatus;
        }
        // counter for failed EKOS connection attempts
        void resetEkosConnectFailureCount(uint8_t newEkosConnectFailureCount = 0)
        {
            m_ekosConnectFailureCount = newEkosConnectFailureCount;
        }
        bool increaseEkosConnectFailureCount();

        void resetParkingCapFailureCount(uint8_t value = 0)
        {
            m_parkingCapFailureCount = value;
        }
        bool increaseParkingCapFailureCount();
        void resetParkingMountFailureCount(uint8_t value = 0)
        {
            m_parkingMountFailureCount = value;
        }
        bool increaseParkingMountFailureCount();
        uint8_t parkingMountFailureCount() const
        {
            return m_parkingMountFailureCount;
        }
        void resetParkingDomeFailureCount(uint8_t value = 0)
        {
            m_parkingDomeFailureCount = value;
        }
        bool increaseParkingDomeFailureCount();

        int indexToUse() const
        {
            return m_IndexToUse;
        }
        void setIndexToUse(int newIndexToUse)
        {
            m_IndexToUse = newIndexToUse;
        }

        int healpixToUse() const
        {
            return m_HealpixToUse;
        }
        void setHealpixToUse(int newHealpixToUse)
        {
            m_HealpixToUse = newHealpixToUse;
        }

        CapturedFramesMap &capturedFramesCount()
        {
            return m_CapturedFramesCount;
        }

        void setCapturedFramesCount(const CapturedFramesMap &newCapturedFramesCount)
        {
            m_CapturedFramesCount = newCapturedFramesCount;
        }

        void setWeatherGracePeriodActive(bool active)
        {
            m_WeatherGracePeriodActive = active;
        }
        bool weatherGracePeriodActive() const
        {
            return m_WeatherGracePeriodActive;
        }

        /**
         * @brief resetFailureCounters Reset all failure counters
         */
        void resetFailureCounters();

        // ////////////////////////////////////////////////////////////////////
        // overall INDI state
        // ////////////////////////////////////////////////////////////////////
        INDIState indiState() const
        {
            return m_indiState;
        }
        void setIndiState(INDIState state);
        // last communication result with INDI
        CommunicationStatus indiCommunicationStatus() const
        {
            return m_INDICommunicationStatus;
        }
        void setIndiCommunicationStatus(CommunicationStatus newINDICommunicationStatus)
        {
            m_INDICommunicationStatus = newINDICommunicationStatus;
            emit indiCommunicationStatusChanged(m_INDICommunicationStatus);
        }
        // counters for failed INDI connection attempts
        void resetIndiConnectFailureCount(uint8_t newIndiConnectFailureCount = 0)
        {
            m_indiConnectFailureCount = newIndiConnectFailureCount;
        }
        bool increaseIndiConnectFailureCount();
        /**
         * @brief isINDIConnected Determines the status of the INDI connection.
         * @return True if INDI connection is up and usable, else false.
         */
        bool isINDIConnected() const
        {
            return (indiCommunicationStatus() == Ekos::Success);
        }
        // ////////////////////////////////////////////////////////////////////
        // device states
        // ////////////////////////////////////////////////////////////////////
        bool mountReady() const
        {
            return m_MountReady;
        }
        void setMountReady(bool readiness)
        {
            m_MountReady = readiness;
        }
        bool captureReady() const
        {
            return m_CaptureReady;
        }
        void setCaptureReady(bool readiness)
        {
            m_CaptureReady = readiness;
        }
        bool domeReady() const
        {
            return m_DomeReady;
        }
        void setDomeReady(bool readiness)
        {
            m_DomeReady = readiness;
        }
        bool capReady() const
        {
            return m_CapReady;
        }
        void setCapReady(bool readiness)
        {
            m_CapReady = readiness;
        }

        uint16_t captureBatch() const
        {
            return m_captureBatch;
        }
        void resetCaptureBatch()
        {
            m_captureBatch = 0;
        }
        uint16_t increaseCaptureBatch()
        {
            return m_captureBatch++;
        }

        uint8_t captureFailureCount() const
        {
            return m_captureFailureCount;
        }
        void resetCaptureFailureCount()
        {
            m_captureFailureCount = 0;
        }
        bool increaseCaptureFailureCount();

        uint8_t focusFailureCount(const QString &trainname) const
        {
            return m_focusFailureCount[trainname];
        }
        void resetFocusFailureCount(const QString &trainname)
        {
            m_focusFailureCount[trainname] = 0;
        }
        void resetFocusFailureCount()
        {
            m_focusFailureCount.clear();
        }
        bool increaseFocusFailureCount(const QString &trainname);

        bool increaseAllFocusFailureCounts();

        bool autofocusCompleted(const QString &trainname) const;
        void setAutofocusCompleted(const QString &trainname, bool value);
        bool autofocusCompleted() const;

        void resetAutofocusCompleted()
        {
            m_autofocusCompleted.clear();
        }

        uint8_t guideFailureCount() const
        {
            return m_guideFailureCount;
        }
        void resetGuideFailureCount()
        {
            m_guideFailureCount = 0;
        }
        bool increaseGuideFailureCount();

        uint8_t alignFailureCount() const
        {
            return m_alignFailureCount;
        }
        void resetAlignFailureCount()
        {
            m_alignFailureCount = 0;
        }
        bool increaseAlignFailureCount();

        int restartGuidingInterval() const
        {
            return m_restartGuidingInterval;
        }

        const KStarsDateTime &restartGuidingTime() const
        {
            return m_restartGuidingTime;
        }

        ISD::Weather::Status weatherStatus() const
        {
            return m_weatherStatus;
        }
        void setWeatherStatus(ISD::Weather::Status newWeatherStatus)
        {
            m_weatherStatus = newWeatherStatus;
        }

        // ////////////////////////////////////////////////////////////////////
        // Timers and time
        // ////////////////////////////////////////////////////////////////////
        // Returns milliseconds since startCurrentOperationTImer() was called.
        qint64 getCurrentOperationMsec() const;
        // Starts the above operation timer.
        // TODO. It would be better to make this a class and give each operation its own timer.
        // TODO. These should be disabled once no longer relevant.
        // These are implement with a KStarsDateTime instead of a QTimer type class
        // so that the simulated clock can be used.
        void startCurrentOperationTimer();

        // Controls for the guiding timer, which restarts guiding after failure.
        void cancelGuidingTimer();
        bool isGuidingTimerActive();
        void startGuidingTimer(int milliseconds);

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


        // ////////////////////////////////////////////////////////////////////
        // Astronomical calculations
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief calculateDawnDusk find the next astronomical dawn and dusk after the current date and time of observation
         */
        static void calculateDawnDusk(QDateTime const &when, QDateTime &nDawn, QDateTime &nDusk);

        /**
         * @brief calculateDawnDusk Calculate dawn and dusk times for today
         */
        void calculateDawnDusk();

        static QDateTime Dawn()
        {
            return m_Dawn;
        }
        static QDateTime Dusk()
        {
            return m_Dusk;
        }
        static QDateTime PreDawnDateTime()
        {
            return m_PreDawnDateTime;
        }

        /** @brief Setter used in testing to fix the geo location. Otherwise getter gets from KStars instance. */
        /** @{ */
        static const GeoLocation *getGeo();
        static void setGeo(GeoLocation *geo)
        {
            storedGeo = geo;
        }
        static bool hasGeo();

        // ////////////////////////////////////////////////////////////////////
        // Scheduler iterations
        // ////////////////////////////////////////////////////////////////////

        // Setup the parameters for the next scheduler iteration.
        // When milliseconds is not passed in, it uses m_UpdatePeriodMs.
        void setupNextIteration(SchedulerTimerState nextState);
        void setupNextIteration(SchedulerTimerState nextState, int milliseconds);

        SchedulerTimerState timerState() const
        {
            return m_timerState;
        }

        void setTimerState(SchedulerTimerState newTimerState)
        {
            m_timerState = newTimerState;
        }

        QTimer &iterationTimer()
        {
            return m_iterationTimer;
        }

        QTimer &tickleTimer()
        {
            return m_tickleTimer;
        }

        bool iterationSetup() const
        {
            return m_iterationSetup;
        }
        void setIterationSetup(bool setup)
        {
            m_iterationSetup = setup;
        }

        qint64 startMSecs() const
        {
            return m_startMSecs;
        }
        void setStartMSecs(qint64 value)
        {
            m_startMSecs = value;
        }
        int increaseSchedulerIteration()
        {
            return ++m_schedulerIteration;
        }
        void resetSchedulerIteration()
        {
            m_schedulerIteration = 0;
        }

        int timerInterval() const
        {
            return m_timerInterval;
        }
        void setTimerInterval(int value)
        {
            m_timerInterval = value;
        }

        void setUpdatePeriodMs(int ms)
        {
            m_UpdatePeriodMs = ms;
        }
        int updatePeriodMs() const
        {
            return m_UpdatePeriodMs;
        }

        uint sequenceExecutionCounter() const
        {
            return m_sequenceExecutionCounter;
        }
        void resetSequenceExecutionCounter()
        {
            m_sequenceExecutionCounter = 1;
        }
        void increaseSequenceExecutionCounter()
        {
            m_sequenceExecutionCounter++;
        }

        static uint maxFailureAttempts();

        QStringList &logText()
        {
            return m_logText;
        }
        QString getLogText()
        {
            return logText().join("\n");
        }
        void clearLog();

        /**
         * @brief checkRepeatSequence Check if the entire job sequence might be repeated
         * @return true if the checkbox is set and the number of iterations is below the
         * configured threshold
         */
        bool checkRepeatSequence();

        void resetSolverIteration()
        {
            m_solverIteration = 0;
        }
        uint32_t increaseSolverIteration()
        {
            return ++m_solverIteration;
        }

    signals:
        // ////////////////////////////////////////////////////////////////////
        // communication with the UI
        // ////////////////////////////////////////////////////////////////////
        // State change of EKOS
        void ekosStateChanged(EkosState state);
        // State change of INDI
        void indiStateChanged(INDIState state);
        // High level INDI state changes
        void indiCommunicationStatusChanged(CommunicationStatus status);
        // overall scheduler state changed
        void schedulerStateChanged(SchedulerState state);
        // startup state
        void startupStateChanged(StartupState state);
        // shutdown state
        void shutdownStateChanged(ShutdownState state);
        // parking state
        void parkWaitStateChanged(ParkWaitState state);
        // profiles updated
        void profilesChanged();
        // current profile changed
        void currentProfileChanged();
        // new log text for the module log window
        void newLog(const QString &text);
        // current position in the job list changed
        void currentPositionChanged(int pos);
        // job stage of the current job changed
        void jobStageChanged(SchedulerJobStage stage);
        // night time calculation updated
        void updateNightTime(SchedulerJob const * job = nullptr);


    private:
        // ////////////////////////////////////////////////////////////////////
        // Scheduler jobs
        // ////////////////////////////////////////////////////////////////////
        // List of all jobs as entered by the user or file
        QList<SchedulerJob *> m_jobs;
        // Active master job
        SchedulerJob *m_activeJob { nullptr };

        // ////////////////////////////////////////////////////////////////////
        // state attributes
        // ////////////////////////////////////////////////////////////////////
        // coarse grained state describing the general execution state
        SchedulerState m_schedulerState { SCHEDULER_IDLE };
        // states of the scheduler startup
        StartupState m_startupState { STARTUP_IDLE };
        // Startup script URL
        QUrl m_startupScriptURL;
        // states of the scheduler shutdown
        ShutdownState m_shutdownState { SHUTDOWN_IDLE };
        // current position on the job list - necessary if there is no line selected in the
        // UI, for example after deleting a row.
        int m_currentPosition { -1 };
        // Shutdown script URL
        QUrl m_shutdownScriptURL;
        // states of parking
        ParkWaitState m_parkWaitState { PARKWAIT_IDLE };
        // current profile
        QString m_currentProfile;
        // all profiles
        QStringList m_profiles;
        /// Store all log strings
        QStringList m_logText;
        // Was job modified and needs saving?
        bool m_dirty { false };

        // EKOS state describing whether EKOS is running (remember that the scheduler
        // does not need EKOS running).
        EkosState m_ekosState { EKOS_IDLE };
        // Execution state of INDI
        INDIState m_indiState { INDI_IDLE };
        // Last communication result with EKOS and INDI
        CommunicationStatus m_EkosCommunicationStatus { Ekos::Idle };
        CommunicationStatus m_INDICommunicationStatus { Ekos::Idle };

        // device readiness
        bool m_MountReady { false };
        bool m_CaptureReady { false };
        bool m_DomeReady { false };
        bool m_CapReady { false };

        // Restricts (the internal solver) to using the index and healpix
        // from the previous solve, if that solve was successful, when
        // doing the pointing check. -1 means no restriction.
        int m_IndexToUse { -1 };
        int m_HealpixToUse { -1 };

        // Check if initial autofocus is completed and do not run autofocus until
        // there is a change is telescope position/alignment.
        QMap <QString, bool> m_autofocusCompleted;

        // Used when solving position every nth capture.
        uint32_t m_solverIteration {0};


        // Keep watch of weather status
        ISD::Weather::Status m_weatherStatus { ISD::Weather::WEATHER_IDLE };

        // ////////////////////////////////////////////////////////////////////
        // counters
        // ////////////////////////////////////////////////////////////////////
        // count for job sequence iteration
        uint m_sequenceExecutionCounter { 1 };
        // Keep track of INDI connection failures
        uint8_t m_indiConnectFailureCount { 0 };
        // Keep track of Ekos connection failures
        uint8_t m_ekosConnectFailureCount { 0 };
        // failures parking dust cap
        uint8_t m_parkingCapFailureCount { 0 };
        // failures parking mount
        uint8_t m_parkingMountFailureCount { 0 };
        // failures parking dome
        uint8_t m_parkingDomeFailureCount { 0 };
        // How many repeated job batches did we complete thus far?
        uint16_t m_captureBatch { 0 };
        // Keep track of Ekos capture module failures
        uint8_t m_captureFailureCount { 0 };
        // Keep track of Ekos focus module failures
        QMap <QString, uint8_t> m_focusFailureCount;
        // Keep track of Ekos guide module failures
        uint8_t m_guideFailureCount { 0 };
        // Keep track of Ekos align module failures
        uint8_t m_alignFailureCount { 0 };
        // frames count for all signatures
        CapturedFramesMap m_CapturedFramesCount;

        // ////////////////////////////////////////////////////////////////////
        // Scheduler iterations
        // ////////////////////////////////////////////////////////////////////

        // The type of scheduler iteration that should be run next.
        SchedulerTimerState m_timerState { RUN_NOTHING };
        // Variable keeping the number of millisconds the scheduler should wait
        // after the current scheduler iteration.
        int m_timerInterval { -1 };
        // Whether the scheduler has been setup for the next iteration,
        // that is, whether timerInterval and timerState have been set this iteration.
        bool m_iterationSetup { false };
        // The timer used to wakeup the scheduler between iterations.
        QTimer m_iterationTimer;
        // The timer used to update the scheduler graph when the scheduler is sleeping.
        QTimer m_tickleTimer;
        // Counter for how many scheduler iterations have been processed.
        int m_schedulerIteration { 0 };
        // The time when the scheduler first started running iterations.
        qint64 m_startMSecs { 0 };
        // This is the time between typical scheduler iterations.
        // The time can be modified for testing.
        int m_UpdatePeriodMs = 1000;

        // ////////////////////////////////////////////////////////////////////
        // time and timers
        // ////////////////////////////////////////////////////////////////////
        // constants for current dawn and dusk
        /// Store next dawn to calculate dark skies range
        static QDateTime m_Dawn;
        /// Store next dusk to calculate dark skies range
        static QDateTime m_Dusk;
        /// Pre-dawn is where we stop all jobs, it is a user-configurable value before Dawn.
        static QDateTime m_PreDawnDateTime;
        // Generic time to track timeout of current operation in progress.
        // Used by startCurrentOperationTimer() and getCurrentOperationMsec().
        KStarsDateTime currentOperationTime;
        bool currentOperationTimeStarted { false };
        // Delay for restarting the guider
        int m_restartGuidingInterval { -1 };
        KStarsDateTime m_restartGuidingTime;
        // Used in testing, instead of KStars::Instance() resources
        static KStarsDateTime *storedLocalTime;
        // The various preemptiveShutdown states are controlled by this one variable.
        QDateTime m_preemptiveShutdownWakeupTime;
        // Are we in weather grace period?
        bool m_WeatherGracePeriodActive {false};

        // These are used in testing, instead of KStars::Instance() resources
        static GeoLocation *storedGeo;

};
} // Ekos namespace
