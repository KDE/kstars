/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QProcess>
#include "ekos/ekos.h"
#include "kstarsdatetime.h"
#include "schedulertypes.h"
#include <QDateTime>
#include <QUrl>

class SchedulerJob;

namespace Ekos
{

class SchedulerProcess;

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

    SchedulerJob *activeJob() const
    {
        return m_activeJob;
    }
    void setActiveJob(SchedulerJob *newActiveJob)
    {
        m_activeJob = newActiveJob;
    }

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
    void setSchedulerState(const SchedulerState &newState)
    {
        m_schedulerState = newState;
    }

    const StartupState &startupState() const
    {
        return m_startupState;
    }
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
    uint8_t increaseEkosConnectFailureCount()
    {
        return ++m_ekosConnectFailureCount;
    }

    void resetParkingCapFailureCount(uint8_t value = 0)
    {
        m_parkingCapFailureCount = value;
    }
    uint8_t increaseParkingCapFailureCount()
    {
        return ++m_parkingCapFailureCount;
    }
    void resetParkingMountFailureCount(uint8_t value = 0)
    {
        m_parkingMountFailureCount = value;
    }
    uint8_t increaseParkingMountFailureCount()
    {
        return ++m_parkingMountFailureCount;
    }
    uint8_t parkingMountFailureCount() const
    {
        return m_parkingMountFailureCount;
    }
    void resetParkingDomeFailureCount(uint8_t value = 0)
    {
        m_parkingDomeFailureCount = value;
    }
    uint8_t increaseParkingDomeFailureCount()
    {
        return ++m_parkingDomeFailureCount;
    }

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
    }
    // counters for failed INDI connection attempts
    void resetIndiConnectFailureCount(uint8_t newIndiConnectFailureCount = 0)
    {
        m_indiConnectFailureCount = newIndiConnectFailureCount;
    }
    uint8_t increaseIndiConnectFailureCount()
    {
        return ++m_indiConnectFailureCount;
    }
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


    // ////////////////////////////////////////////////////////////////////
    // Timers
    // ////////////////////////////////////////////////////////////////////
    // Returns milliseconds since startCurrentOperationTImer() was called.
    qint64 getCurrentOperationMsec() const;
    // Starts the above operation timer.
    // TODO. It would be better to make this a class and give each operation its own timer.
    // TODO. These should be disabled once no longer relevant.
    // These are implement with a KStarsDateTime instead of a QTimer type class
    // so that the simulated clock can be used.
    void startCurrentOperationTimer();

    // The various preemptiveShutdown states are controlled by this one variable.
    QDateTime m_preemptiveShutdownWakeupTime;


signals:
    // ////////////////////////////////////////////////////////////////////
    // communication with the UI
    // ////////////////////////////////////////////////////////////////////
    // State change of EKOS
    void ekosStateChanged(EkosState state);
    // State change of INDI
    void indiStateChanged(INDIState state);
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

private:
    // ////////////////////////////////////////////////////////////////////
    // Scheduler jobs
    // ////////////////////////////////////////////////////////////////////
    // List of all jobs as entered by the user or file
    QList<SchedulerJob *> m_jobs;
    // Active job
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
    // Shutdown script URL
    QUrl m_shutdownScriptURL;
    // states of parking
    ParkWaitState m_parkWaitState { PARKWAIT_IDLE };
    // current profile
    QString m_currentProfile;
    // all profiles
    QStringList m_profiles;
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


    // ////////////////////////////////////////////////////////////////////
    // counters
    // ////////////////////////////////////////////////////////////////////
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

    // ////////////////////////////////////////////////////////////////////
    // timers
    // ////////////////////////////////////////////////////////////////////
    // Generic time to track timeout of current operation in progress.
    // Used by startCurrentOperationTimer() and getCurrentOperationMsec().
    KStarsDateTime currentOperationTime;
    bool currentOperationTimeStarted { false };
};
} // Ekos namespace
