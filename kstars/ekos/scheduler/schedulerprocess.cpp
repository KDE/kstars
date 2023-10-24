/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "schedulerprocess.h"
#include "schedulermodulestate.h"
#include "schedulerjob.h"
#include "Options.h"
#include "ksmessagebox.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "indi/indistd.h"
#include "skymapcomposite.h"
#include "mosaiccomponent.h"
#include "mosaictiles.h"
#include <ekos_scheduler_debug.h>

#include <QDBusReply>

#define MAX_FAILURE_ATTEMPTS 5

// This is a temporary debugging printout introduced while gaining experience developing
// the unit tests in test_ekos_scheduler_ops.cpp.
// All these printouts should be eventually removed.

namespace Ekos
{
SchedulerProcess::SchedulerProcess(QSharedPointer<SchedulerModuleState> state)
{
    m_moduleState = state;
}

void SchedulerProcess::loadProfiles()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Loading profiles";
    QDBusReply<QStringList> profiles = ekosInterface()->call(QDBus::AutoDetect, "getProfiles");

    if (profiles.error().type() == QDBusError::NoError)
        moduleState()->updateProfiles(profiles);
}

void SchedulerProcess::executeScript(const QString &filename)
{
    emit newLog(i18n("Executing script %1...", filename));

    connect(&scriptProcess(), &QProcess::readyReadStandardOutput, this, &SchedulerProcess::readProcessOutput);

    connect(&scriptProcess(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus)
    {
        checkProcessExit(exitCode);
    });

    QStringList arguments;
    scriptProcess().start(filename, arguments);
}

bool SchedulerProcess::checkEkosState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    switch (moduleState()->ekosState())
    {
        case EKOS_IDLE:
        {
            if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
            {
                moduleState()->setEkosState(EKOS_READY);
                return true;
            }
            else
            {
                ekosInterface()->call(QDBus::AutoDetect, "start");
                moduleState()->setEkosState(EKOS_STARTING);
                moduleState()->startCurrentOperationTimer();

                qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos communication status is" << moduleState()->ekosCommunicationStatus() <<
                                              "Starting Ekos...";

                return false;
            }
        }

        case EKOS_STARTING:
        {
            if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
            {
                emit newLog(i18n("Ekos started."));
                moduleState()->resetEkosConnectFailureCount();
                moduleState()->setEkosState(EKOS_READY);
                return true;
            }
            else if (moduleState()->ekosCommunicationStatus() == Ekos::Error)
            {
                if (moduleState()->increaseEkosConnectFailureCount() <= MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Starting Ekos failed. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "start");
                    return false;
                }

                emit newLog(i18n("Starting Ekos failed."));
                emit stopScheduler();
                return false;
            }
            else if (moduleState()->ekosCommunicationStatus() == Ekos::Idle)
                return false;
            // If a minute passed, give up
            else if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseEkosConnectFailureCount() <= MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Starting Ekos timed out. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "stop");
                    QTimer::singleShot(1000, this, [&]()
                    {
                        ekosInterface()->call(QDBus::AutoDetect, "start");
                        moduleState()->startCurrentOperationTimer();
                    });
                    return false;
                }

                emit newLog(i18n("Starting Ekos timed out."));
                emit stopScheduler();
                return false;
            }
        }
        break;

        case EKOS_STOPPING:
        {
            if (moduleState()->ekosCommunicationStatus() == Ekos::Idle)
            {
                emit newLog(i18n("Ekos stopped."));
                moduleState()->setEkosState(EKOS_IDLE);
                return true;
            }
        }
        break;

        case EKOS_READY:
            return true;
    }
    return false;
}

bool SchedulerProcess::checkINDIState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    switch (moduleState()->indiState())
    {
        case INDI_IDLE:
        {
            if (moduleState()->indiCommunicationStatus() == Ekos::Success)
            {
                moduleState()->setIndiState(INDI_PROPERTY_CHECK);
                moduleState()->resetIndiConnectFailureCount();
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI Properties...";
            }
            else
            {
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Connecting INDI devices...";
                ekosInterface()->call(QDBus::AutoDetect, "connectDevices");
                moduleState()->setIndiState(INDI_CONNECTING);

                moduleState()->startCurrentOperationTimer();
            }
        }
        break;

        case INDI_CONNECTING:
        {
            if (moduleState()->indiCommunicationStatus() == Ekos::Success)
            {
                emit newLog(i18n("INDI devices connected."));
                moduleState()->setIndiState(INDI_PROPERTY_CHECK);
            }
            else if (moduleState()->indiCommunicationStatus() == Ekos::Error)
            {
                if (moduleState()->increaseIndiConnectFailureCount() <= MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("One or more INDI devices failed to connect. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "connectDevices");
                }
                else
                {
                    emit newLog(i18n("One or more INDI devices failed to connect. Check INDI control panel for details."));
                    emit stopScheduler();
                }
            }
            // If 30 seconds passed, we retry
            else if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
            {
                if (moduleState()->increaseIndiConnectFailureCount() <= MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("One or more INDI devices timed out. Retrying..."));
                    ekosInterface()->call(QDBus::AutoDetect, "connectDevices");
                    moduleState()->startCurrentOperationTimer();
                }
                else
                {
                    emit newLog(i18n("One or more INDI devices timed out. Check INDI control panel for details."));
                    emit stopScheduler();
                }
            }
        }
        break;

        case INDI_DISCONNECTING:
        {
            if (moduleState()->indiCommunicationStatus() == Ekos::Idle)
            {
                emit newLog(i18n("INDI devices disconnected."));
                moduleState()->setIndiState(INDI_IDLE);
                return true;
            }
        }
        break;

        case INDI_PROPERTY_CHECK:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI properties.";
            // If dome unparking is required then we wait for dome interface
            if (Options::schedulerUnparkDome() && moduleState()->domeReady() == false)
            {
                if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
                {
                    moduleState()->startCurrentOperationTimer();
                    emit newLog(i18n("Warning: dome device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                emit newLog(i18n("Dome unpark required but dome is not yet ready."));
                return false;
            }

            // If mount unparking is required then we wait for mount interface
            if (Options::schedulerUnparkMount() && moduleState()->mountReady() == false)
            {
                if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
                {
                    moduleState()->startCurrentOperationTimer();
                    emit newLog(i18n("Warning: mount device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount unpark required but mount is not yet ready.";
                return false;
            }

            // If cap unparking is required then we wait for cap interface
            if (Options::schedulerOpenDustCover() && moduleState()->capReady() == false)
            {
                if (moduleState()->getCurrentOperationMsec() > (30 * 1000))
                {
                    moduleState()->startCurrentOperationTimer();
                    emit newLog(i18n("Warning: cap device not ready after timeout, attempting to recover..."));
                    disconnectINDI();
                    stopEkos();
                }

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap unpark required but cap is not yet ready.";
                return false;
            }

            // capture interface is required at all times to proceed.
            if (captureInterface().isNull())
                return false;

            if (moduleState()->captureReady() == false)
            {
                QVariant hasCoolerControl = captureInterface()->property("coolerControl");
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Cooler control" << (!hasCoolerControl.isValid() ? "invalid" :
                                               (hasCoolerControl.toBool() ? "True" : "Faklse"));
                if (hasCoolerControl.isValid())
                    moduleState()->setCaptureReady(true);
                else
                    qCWarning(KSTARS_EKOS_SCHEDULER) << "Capture module is not ready yet...";
            }

            moduleState()->setIndiState(INDI_READY);
            moduleState()->resetIndiConnectFailureCount();
            return true;
        }

        case INDI_READY:
            return true;
    }

    return false;
}

bool SchedulerProcess::completeShutdown()
{
    // If INDI is not done disconnecting, try again later
    if (moduleState()->indiState() == INDI_DISCONNECTING
            && checkINDIState() == false)
        return false;

    // Disconnect INDI if required first
    if (moduleState()->indiState() != INDI_IDLE && Options::stopEkosAfterShutdown())
    {
        disconnectINDI();
        return false;
    }

    // If Ekos is not done stopping, try again later
    if (moduleState()->ekosState() == EKOS_STOPPING && checkEkosState() == false)
        return false;

    // Stop Ekos if required.
    if (moduleState()->ekosState() != EKOS_IDLE && Options::stopEkosAfterShutdown())
    {
        stopEkos();
        return false;
    }

    if (moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
        emit newLog(i18n("Shutdown complete."));
    else
        emit newLog(i18n("Shutdown procedure failed, aborting..."));

    // Stop Scheduler
    emit stopScheduler();

    return true;
}

void SchedulerProcess::disconnectINDI()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Disconnecting INDI...";
    moduleState()->setIndiState(INDI_DISCONNECTING);
    ekosInterface()->call(QDBus::AutoDetect, "disconnectDevices");
}

void SchedulerProcess::stopEkos()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Stopping Ekos...";
    moduleState()->setEkosState(EKOS_STOPPING);
    moduleState()->resetEkosConnectFailureCount();
    ekosInterface()->call(QDBus::AutoDetect, "stop");
    moduleState()->setMountReady(false);
    moduleState()->setCaptureReady(false);
    moduleState()->setDomeReady(false);
    moduleState()->setCapReady(false);
}

bool SchedulerProcess::manageConnectionLoss()
{
    if (SCHEDULER_RUNNING != moduleState()->schedulerState())
        return false;

    // Don't manage loss if Ekos is actually down in the state machine
    switch (moduleState()->ekosState())
    {
        case EKOS_IDLE:
        case EKOS_STOPPING:
            return false;

        default:
            break;
    }

    // Don't manage loss if INDI is actually down in the state machine
    switch (moduleState()->indiState())
    {
        case INDI_IDLE:
        case INDI_DISCONNECTING:
            return false;

        default:
            break;
    }

    // If Ekos is assumed to be up, check its state
    //QDBusReply<int> const isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
    if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Ekos is currently connected, checking INDI before mitigating connection loss.");

        // If INDI is assumed to be up, check its state
        if (moduleState()->isINDIConnected())
        {
            // If both Ekos and INDI are assumed up, and are actually up, no mitigation needed, this is a DBus interface error
            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("INDI is currently connected, no connection loss mitigation needed.");
            return false;
        }
    }

    // Stop actions of the current job
    emit stopCurrentJobAction();

    // Acknowledge INDI and Ekos disconnections
    disconnectINDI();
    stopEkos();

    // Let the Scheduler attempt to connect INDI again
    return true;

}

void SchedulerProcess::checkCapParkingStatus()
{
    if (capInterface().isNull())
        return;

    QVariant parkingStatus = capInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              capInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_CAP)
            {
                emit newLog(i18n("Cap parked."));
                moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
            }
            moduleState()->resetParkingCapFailureCount();
            break;

        case ISD::PARK_UNPARKED:
            if (moduleState()->startupState() == STARTUP_UNPARKING_CAP)
            {
                moduleState()->setStartupState(STARTUP_COMPLETE);
                emit newLog(i18n("Cap unparked."));
            }
            moduleState()->resetParkingCapFailureCount();
            break;

        case ISD::PARK_PARKING:
        case ISD::PARK_UNPARKING:
            // TODO make the timeouts configurable by the user
            if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseParkingCapFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Operation timeout. Restarting operation..."));
                    if (status == ISD::PARK_PARKING)
                        parkCap();
                    else
                        unParkCap();
                    break;
                }
            }
            break;

        case ISD::PARK_ERROR:
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_CAP)
            {
                emit newLog(i18n("Cap parking error."));
                moduleState()->setShutdownState(SHUTDOWN_ERROR);
            }
            else if (moduleState()->startupState() == STARTUP_UNPARKING_CAP)
            {
                emit newLog(i18n("Cap unparking error."));
                moduleState()->setStartupState(STARTUP_ERROR);
            }
            moduleState()->resetParkingCapFailureCount();
            break;

        default:
            break;
    }
}

void SchedulerProcess::checkMountParkingStatus()
{
    if (mountInterface().isNull())
        return;

    QVariant parkingStatus = mountInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        //case Mount::PARKING_OK:
        case ISD::PARK_PARKED:
            // If we are starting up, we will unpark the mount in checkParkWaitState soon
            // If we are shutting down and mount is parked, proceed to next step
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);

            // Update parking engine state
            if (moduleState()->parkWaitState() == PARKWAIT_PARKING)
                moduleState()->setParkWaitState(PARKWAIT_PARKED);

            emit newLog(i18n("Mount parked."));
            moduleState()->resetParkingMountFailureCount();
            break;

        //case Mount::UNPARKING_OK:
        case ISD::PARK_UNPARKED:
            // If we are starting up and mount is unparked, proceed to next step
            // If we are shutting down, we will park the mount in checkParkWaitState soon
            if (moduleState()->startupState() == STARTUP_UNPARKING_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);

            // Update parking engine state
            if (moduleState()->parkWaitState() == PARKWAIT_UNPARKING)
                moduleState()->setParkWaitState(PARKWAIT_UNPARKED);

            emit newLog(i18n("Mount unparked."));
            moduleState()->resetParkingMountFailureCount();
            break;

        // FIXME: Create an option for the parking/unparking timeout.

        //case Mount::UNPARKING_BUSY:
        case ISD::PARK_UNPARKING:
            if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseParkingMountFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Warning: mount unpark operation timed out on attempt %1/%2. Restarting operation...",
                                     moduleState()->parkingMountFailureCount(), MAX_FAILURE_ATTEMPTS));
                    unParkMount();
                }
                else
                {
                    emit newLog(i18n("Warning: mount unpark operation timed out on last attempt."));
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";

            break;

        //case Mount::PARKING_BUSY:
        case ISD::PARK_PARKING:
            if (moduleState()->getCurrentOperationMsec() > (60 * 1000))
            {
                if (moduleState()->increaseParkingMountFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Warning: mount park operation timed out on attempt %1/%2. Restarting operation...",
                                     moduleState()->parkingMountFailureCount(),
                                     MAX_FAILURE_ATTEMPTS));
                    parkMount();
                }
                else
                {
                    emit newLog(i18n("Warning: mount park operation timed out on last attempt."));
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Parking mount in progress...";

            break;

        //case Mount::PARKING_ERROR:
        case ISD::PARK_ERROR:
            if (moduleState()->startupState() == STARTUP_UNPARKING_MOUNT)
            {
                emit newLog(i18n("Mount unparking error."));
                moduleState()->setStartupState(STARTUP_ERROR);
                moduleState()->resetParkingMountFailureCount();
            }
            else if (moduleState()->shutdownState() == SHUTDOWN_PARKING_MOUNT)
            {
                if (moduleState()->increaseParkingMountFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Warning: mount park operation failed on attempt %1/%2. Restarting operation...",
                                     moduleState()->parkingMountFailureCount(),
                                     MAX_FAILURE_ATTEMPTS));
                    parkMount();
                }
                else
                {
                    emit newLog(i18n("Mount parking error."));
                    moduleState()->setShutdownState(SHUTDOWN_ERROR);
                    moduleState()->resetParkingMountFailureCount();
                }

            }
            else if (moduleState()->parkWaitState() == PARKWAIT_PARKING)
            {
                emit newLog(i18n("Mount parking error."));
                moduleState()->setParkWaitState(PARKWAIT_ERROR);
                moduleState()->resetParkingMountFailureCount();
            }
            else if (moduleState()->parkWaitState() == PARKWAIT_UNPARKING)
            {
                emit newLog(i18n("Mount unparking error."));
                moduleState()->setParkWaitState(PARKWAIT_ERROR);
                moduleState()->resetParkingMountFailureCount();
            }
            break;

        //case Mount::PARKING_IDLE:
        // FIXME Does this work as intended? check!
        case ISD::PARK_UNKNOWN:
            // Last parking action did not result in an action, so proceed to next step
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);

            // Last unparking action did not result in an action, so proceed to next step
            if (moduleState()->startupState() == STARTUP_UNPARKING_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);

            // Update parking engine state
            if (moduleState()->parkWaitState() == PARKWAIT_PARKING)
                moduleState()->setParkWaitState(PARKWAIT_PARKED);
            else if (moduleState()->parkWaitState() == PARKWAIT_UNPARKING)
                moduleState()->setParkWaitState(PARKWAIT_UNPARKED);

            moduleState()->resetParkingMountFailureCount();
            break;
    }
}

void SchedulerProcess::checkDomeParkingStatus()
{
    if (domeInterface().isNull())
        return;

    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_DOME)
            {
                emit newLog(i18n("Dome parked."));

                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            moduleState()->resetParkingDomeFailureCount();
            break;

        case ISD::PARK_UNPARKED:
            if (moduleState()->startupState() == STARTUP_UNPARKING_DOME)
            {
                moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
                emit newLog(i18n("Dome unparked."));
            }
            moduleState()->resetParkingDomeFailureCount();
            break;

        case ISD::PARK_PARKING:
        case ISD::PARK_UNPARKING:
            // TODO make the timeouts configurable by the user
            if (moduleState()->getCurrentOperationMsec() > (120 * 1000))
            {
                if (moduleState()->increaseParkingDomeFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Operation timeout. Restarting operation..."));
                    if (status == ISD::PARK_PARKING)
                        parkDome();
                    else
                        unParkDome();
                    break;
                }
            }
            break;

        case ISD::PARK_ERROR:
            if (moduleState()->shutdownState() == SHUTDOWN_PARKING_DOME)
            {
                if (moduleState()->increaseParkingDomeFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Dome parking failed. Restarting operation..."));
                    parkDome();
                }
                else
                {
                    emit newLog(i18n("Dome parking error."));
                    moduleState()->setShutdownState(SHUTDOWN_ERROR);
                    moduleState()->resetParkingDomeFailureCount();
                }
            }
            else if (moduleState()->startupState() == STARTUP_UNPARKING_DOME)
            {
                if (moduleState()->increaseParkingDomeFailureCount() < MAX_FAILURE_ATTEMPTS)
                {
                    emit newLog(i18n("Dome unparking failed. Restarting operation..."));
                    unParkDome();
                }
                else
                {
                    emit newLog(i18n("Dome unparking error."));
                    moduleState()->setStartupState(STARTUP_ERROR);
                    moduleState()->resetParkingDomeFailureCount();
                }
            }
            break;

        default:
            break;
    }
}

bool SchedulerProcess::checkStartupState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Checking Startup State (%1)...").arg(moduleState()->startupState());

    switch (moduleState()->startupState())
    {
        case STARTUP_IDLE:
        {
            KSNotification::event(QLatin1String("ObservatoryStartup"), i18n("Observatory is in the startup process"),
                                  KSNotification::Scheduler);

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Startup Idle. Starting startup process...";

            // If Ekos is already started, we skip the script and move on to dome unpark step
            // unless we do not have light frames, then we skip all
            //QDBusReply<int> isEkosStarted;
            //isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            //if (isEkosStarted.value() == Ekos::Success)
            if (moduleState()->ekosCommunicationStatus() == Ekos::Success)
            {
                if (moduleState()->startupScriptURL().isEmpty() == false)
                    emit newLog(i18n("Ekos is already started, skipping startup script..."));

                if (!activeJob() || activeJob()->getLightFramesRequired())
                    moduleState()->setStartupState(STARTUP_UNPARK_DOME);
                else
                    moduleState()->setStartupState(STARTUP_COMPLETE);
                return true;
            }

            if (moduleState()->currentProfile() != i18n("Default"))
            {
                QList<QVariant> profile;
                profile.append(moduleState()->currentProfile());
                ekosInterface()->callWithArgumentList(QDBus::AutoDetect, "setProfile", profile);
            }

            if (moduleState()->startupScriptURL().isEmpty() == false)
            {
                moduleState()->setStartupState(STARTUP_SCRIPT);
                executeScript(moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile));
                return false;
            }

            moduleState()->setStartupState(STARTUP_UNPARK_DOME);
            return false;
        }

        case STARTUP_SCRIPT:
            return false;

        case STARTUP_UNPARK_DOME:
            // If there is no job in case of manual startup procedure,
            // or if the job requires light frames, let's proceed with
            // unparking the dome, otherwise startup process is complete.
            if (activeJob() == nullptr || activeJob()->getLightFramesRequired())
            {
                if (Options::schedulerUnparkDome())
                    unParkDome();
                else
                    moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
            }
            else
            {
                moduleState()->setStartupState(STARTUP_COMPLETE);
                return true;
            }

            break;

        case STARTUP_UNPARKING_DOME:
            checkDomeParkingStatus();
            break;

        case STARTUP_UNPARK_MOUNT:
            if (Options::schedulerUnparkMount())
                unParkMount();
            else
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);
            break;

        case STARTUP_UNPARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case STARTUP_UNPARK_CAP:
            if (Options::schedulerOpenDustCover())
                unParkCap();
            else
                moduleState()->setStartupState(STARTUP_COMPLETE);
            break;

        case STARTUP_UNPARKING_CAP:
            checkCapParkingStatus();
            break;

        case STARTUP_COMPLETE:
            return true;

        case STARTUP_ERROR:
            emit stopScheduler();
            return true;
    }

    return false;
}

bool SchedulerProcess::checkShutdownState()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking shutdown state...";

    switch (moduleState()->shutdownState())
    {
        case SHUTDOWN_IDLE:

            if (Options::schedulerWarmCCD())
            {
                emit newLog(i18n("Warming up CCD..."));

                // Turn it off
                //QVariant arg(false);
                //captureInterface->call(QDBus::AutoDetect, "setCoolerControl", arg);
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Setting coolerControl=false";
                captureInterface()->setProperty("coolerControl", false);
            }

            // The following steps require a connection to the INDI server
            if (moduleState()->isINDIConnected())
            {
                if (Options::schedulerCloseDustCover())
                {
                    moduleState()->setShutdownState(SHUTDOWN_PARK_CAP);
                    return false;
                }

                if (Options::schedulerParkMount())
                {
                    moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
                    return false;
                }

                if (Options::schedulerParkDome())
                {
                    moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);
                    return false;
                }
            }
            else emit newLog(i18n("Warning: Bypassing parking procedures, no INDI connection."));

            if (moduleState()->shutdownScriptURL().isEmpty() == false)
            {
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
                return false;
            }

            moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
            return true;

        case SHUTDOWN_PARK_CAP:
            if (!moduleState()->isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            else if (Options::schedulerCloseDustCover())
                parkCap();
            else
                moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
            break;

        case SHUTDOWN_PARKING_CAP:
            checkCapParkingStatus();
            break;

        case SHUTDOWN_PARK_MOUNT:
            if (!moduleState()->isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            else if (Options::schedulerParkMount())
                parkMount();
            else
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);
            break;

        case SHUTDOWN_PARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case SHUTDOWN_PARK_DOME:
            if (!moduleState()->isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            }
            else if (Options::schedulerParkDome())
                parkDome();
            else
                moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
            break;

        case SHUTDOWN_PARKING_DOME:
            checkDomeParkingStatus();
            break;

        case SHUTDOWN_SCRIPT:
            if (moduleState()->shutdownScriptURL().isEmpty() == false)
            {
                // Need to stop Ekos now before executing script if it happens to stop INDI
                if (moduleState()->ekosState() != EKOS_IDLE && Options::shutdownScriptTerminatesINDI())
                {
                    stopEkos();
                    return false;
                }

                moduleState()->setShutdownState(SHUTDOWN_SCRIPT_RUNNING);
                executeScript(moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile));
            }
            else
                moduleState()->setShutdownState(SHUTDOWN_COMPLETE);
            break;

        case SHUTDOWN_SCRIPT_RUNNING:
            return false;

        case SHUTDOWN_COMPLETE:
            return completeShutdown();

        case SHUTDOWN_ERROR:
            emit stopScheduler();
            return true;
    }

    return false;
}

bool SchedulerProcess::checkParkWaitState()
{
    if (moduleState()->schedulerState() == SCHEDULER_PAUSED)
        return false;

    if (moduleState()->parkWaitState() == PARKWAIT_IDLE)
        return true;

    // qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking Park Wait State...";

    switch (moduleState()->parkWaitState())
    {
        case PARKWAIT_PARK:
            parkMount();
            break;

        case PARKWAIT_PARKING:
            checkMountParkingStatus();
            break;

        case PARKWAIT_UNPARK:
            unParkMount();
            break;

        case PARKWAIT_UNPARKING:
            checkMountParkingStatus();
            break;

        case PARKWAIT_IDLE:
        case PARKWAIT_PARKED:
        case PARKWAIT_UNPARKED:
            return true;

        case PARKWAIT_ERROR:
            emit newLog(i18n("park/unpark wait procedure failed, aborting..."));
            emit stopScheduler();
            return true;

    }

    return false;
}

void SchedulerProcess::runStartupProcedure()
{
    if (moduleState()->startupState() == STARTUP_IDLE
            || moduleState()->startupState() == STARTUP_ERROR
            || moduleState()->startupState() == STARTUP_COMPLETE)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);

            emit newLog(i18n("Warning: executing startup procedure manually..."));
            moduleState()->setStartupState(STARTUP_IDLE);
            checkStartupState();
            QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));

        });

        KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to execute the startup procedure manually?"));
    }
    else
    {
        switch (moduleState()->startupState())
        {
            case STARTUP_IDLE:
                break;

            case STARTUP_SCRIPT:
                scriptProcess().terminate();
                break;

            case STARTUP_UNPARK_DOME:
                break;

            case STARTUP_UNPARKING_DOME:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting unparking dome...";
                domeInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_MOUNT:
                break;

            case STARTUP_UNPARKING_MOUNT:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting unparking mount...";
                mountInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_CAP:
                break;

            case STARTUP_UNPARKING_CAP:
                break;

            case STARTUP_COMPLETE:
                break;

            case STARTUP_ERROR:
                break;
        }

        moduleState()->setStartupState(STARTUP_IDLE);

        emit newLog(i18n("Startup procedure terminated."));
    }

}

void SchedulerProcess::runShutdownProcedure()
{
    if (moduleState()->shutdownState() == SHUTDOWN_IDLE
            || moduleState()->shutdownState() == SHUTDOWN_ERROR
            || moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
            emit newLog(i18n("Warning: executing shutdown procedure manually..."));
            moduleState()->setShutdownState(SHUTDOWN_IDLE);
            checkShutdownState();
            QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to execute the shutdown procedure manually?"));
    }
    else
    {
        switch (moduleState()->shutdownState())
        {
            case SHUTDOWN_IDLE:
                break;

            case SHUTDOWN_SCRIPT:
                break;

            case SHUTDOWN_SCRIPT_RUNNING:
                scriptProcess().terminate();
                break;

            case SHUTDOWN_PARK_DOME:
                break;

            case SHUTDOWN_PARKING_DOME:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting parking dome...";
                domeInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_MOUNT:
                break;

            case SHUTDOWN_PARKING_MOUNT:
                qCDebug(KSTARS_EKOS_SCHEDULER) << "Aborting parking mount...";
                mountInterface()->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_CAP:
            case SHUTDOWN_PARKING_CAP:
                break;

            case SHUTDOWN_COMPLETE:
                break;

            case SHUTDOWN_ERROR:
                break;
        }

        moduleState()->setShutdownState(SHUTDOWN_IDLE);

        emit newLog(i18n("Shutdown procedure terminated."));
    }
}

bool SchedulerProcess::saveScheduler(const QUrl &fileURL)
{
    QFile file;
    file.setFileName(fileURL.toLocalFile());

    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    QTextStream outstream(&file);

    // We serialize sequence data to XML using the C locale
    QLocale cLocale = QLocale::c();

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << Qt::endl;
    outstream << "<SchedulerList version='1.6'>" << Qt::endl;
    // ensure to escape special XML characters
    outstream << "<Profile>" << QString(entityXML(strdup(moduleState()->currentProfile().toStdString().c_str()))) <<
              "</Profile>" << Qt::endl;

    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
    bool useMosaicInfo = !tiles->sequenceFile().isEmpty();

    if (useMosaicInfo)
    {
        outstream << "<Mosaic>" << Qt::endl;
        outstream << "<Target>" << tiles->targetName() << "</Target>" << Qt::endl;
        outstream << "<Group>" << tiles->group() << "</Group>" << Qt::endl;

        QString ccArg, ccValue = tiles->completionCondition(&ccArg);
        if (ccValue == "FinishSequence")
            outstream << "<FinishSequence/>" << Qt::endl;
        else if (ccValue == "FinishLoop")
            outstream << "<FinishLoop/>" << Qt::endl;
        else if (ccValue == "FinishRepeat")
            outstream << "<FinishRepeat>" << ccArg << "</FinishRepeat>" << Qt::endl;

        outstream << "<Sequence>" << tiles->sequenceFile() << "</Sequence>" << Qt::endl;
        outstream << "<Directory>" << tiles->outputDirectory() << "</Directory>" << Qt::endl;

        outstream << "<FocusEveryN>" << tiles->focusEveryN() << "</FocusEveryN>" << Qt::endl;
        outstream << "<AlignEveryN>" << tiles->alignEveryN() << "</AlignEveryN>" << Qt::endl;
        if (tiles->isTrackChecked())
            outstream << "<TrackChecked/>" << Qt::endl;
        if (tiles->isFocusChecked())
            outstream << "<FocusChecked/>" << Qt::endl;
        if (tiles->isAlignChecked())
            outstream << "<AlignChecked/>" << Qt::endl;
        if (tiles->isGuideChecked())
            outstream << "<GuideChecked/>" << Qt::endl;
        outstream << "<Overlap>" << cLocale.toString(tiles->overlap()) << "</Overlap>" << Qt::endl;
        outstream << "<CenterRA>" << cLocale.toString(tiles->ra0().Hours()) << "</CenterRA>" << Qt::endl;
        outstream << "<CenterDE>" << cLocale.toString(tiles->dec0().Degrees()) << "</CenterDE>" << Qt::endl;
        outstream << "<GridW>" << tiles->gridSize().width() << "</GridW>" << Qt::endl;
        outstream << "<GridH>" << tiles->gridSize().height() << "</GridH>" << Qt::endl;
        outstream << "<FOVW>" << cLocale.toString(tiles->mosaicFOV().width()) << "</FOVW>" << Qt::endl;
        outstream << "<FOVH>" << cLocale.toString(tiles->mosaicFOV().height()) << "</FOVH>" << Qt::endl;
        outstream << "<CameraFOVW>" << cLocale.toString(tiles->cameraFOV().width()) << "</CameraFOVW>" << Qt::endl;
        outstream << "<CameraFOVH>" << cLocale.toString(tiles->cameraFOV().height()) << "</CameraFOVH>" << Qt::endl;
        outstream << "</Mosaic>" << Qt::endl;
    }

    int index = 0;
    for (auto &job : moduleState()->jobs())
    {
        outstream << "<Job>" << Qt::endl;

        // ensure to escape special XML characters
        outstream << "<Name>" << QString(entityXML(strdup(job->getName().toStdString().c_str()))) << "</Name>" << Qt::endl;
        outstream << "<Group>" << QString(entityXML(strdup(job->getGroup().toStdString().c_str()))) << "</Group>" << Qt::endl;
        outstream << "<Coordinates>" << Qt::endl;
        outstream << "<J2000RA>" << cLocale.toString(job->getTargetCoords().ra0().Hours()) << "</J2000RA>" << Qt::endl;
        outstream << "<J2000DE>" << cLocale.toString(job->getTargetCoords().dec0().Degrees()) << "</J2000DE>" << Qt::endl;
        outstream << "</Coordinates>" << Qt::endl;

        if (job->getFITSFile().isValid() && job->getFITSFile().isEmpty() == false)
            outstream << "<FITS>" << job->getFITSFile().toLocalFile() << "</FITS>" << Qt::endl;
        else
            outstream << "<PositionAngle>" << job->getPositionAngle() << "</PositionAngle>" << Qt::endl;

        outstream << "<Sequence>" << job->getSequenceFile().toLocalFile() << "</Sequence>" << Qt::endl;

        if (useMosaicInfo && index < tiles->tiles().size())
        {
            auto oneTile = tiles->tiles().at(index++);
            outstream << "<TileCenter>" << Qt::endl;
            outstream << "<X>" << cLocale.toString(oneTile->center.x()) << "</X>" << Qt::endl;
            outstream << "<Y>" << cLocale.toString(oneTile->center.y()) << "</Y>" << Qt::endl;
            outstream << "<Rotation>" << cLocale.toString(oneTile->rotation) << "</Rotation>" << Qt::endl;
            outstream << "</TileCenter>" << Qt::endl;
        }

        outstream << "<StartupCondition>" << Qt::endl;
        if (job->getFileStartupCondition() == SchedulerJob::START_ASAP)
            outstream << "<Condition>ASAP</Condition>" << Qt::endl;
        else if (job->getFileStartupCondition() == SchedulerJob::START_AT)
            outstream << "<Condition value='" << job->getFileStartupTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << Qt::endl;
        outstream << "</StartupCondition>" << Qt::endl;

        outstream << "<Constraints>" << Qt::endl;
        if (job->hasMinAltitude())
            outstream << "<Constraint value='" << cLocale.toString(job->getMinAltitude()) << "'>MinimumAltitude</Constraint>" <<
                      Qt::endl;
        if (job->getMinMoonSeparation() > 0)
            outstream << "<Constraint value='" << cLocale.toString(job->getMinMoonSeparation()) << "'>MoonSeparation</Constraint>"
                      << Qt::endl;
        if (job->getEnforceWeather())
            outstream << "<Constraint>EnforceWeather</Constraint>" << Qt::endl;
        if (job->getEnforceTwilight())
            outstream << "<Constraint>EnforceTwilight</Constraint>" << Qt::endl;
        if (job->getEnforceArtificialHorizon())
            outstream << "<Constraint>EnforceArtificialHorizon</Constraint>" << Qt::endl;
        outstream << "</Constraints>" << Qt::endl;

        outstream << "<CompletionCondition>" << Qt::endl;
        if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
            outstream << "<Condition>Sequence</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
            outstream << "<Condition value='" << cLocale.toString(job->getRepeatsRequired()) << "'>Repeat</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
            outstream << "<Condition>Loop</Condition>" << Qt::endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
            outstream << "<Condition value='" << job->getCompletionTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << Qt::endl;
        outstream << "</CompletionCondition>" << Qt::endl;

        outstream << "<Steps>" << Qt::endl;
        if (job->getStepPipeline() & SchedulerJob::USE_TRACK)
            outstream << "<Step>Track</Step>" << Qt::endl;
        if (job->getStepPipeline() & SchedulerJob::USE_FOCUS)
            outstream << "<Step>Focus</Step>" << Qt::endl;
        if (job->getStepPipeline() & SchedulerJob::USE_ALIGN)
            outstream << "<Step>Align</Step>" << Qt::endl;
        if (job->getStepPipeline() & SchedulerJob::USE_GUIDE)
            outstream << "<Step>Guide</Step>" << Qt::endl;
        outstream << "</Steps>" << Qt::endl;

        outstream << "</Job>" << Qt::endl;
    }

    outstream << "<SchedulerAlgorithm value='" << ALGORITHM_GREEDY << "'/>" << Qt::endl;
    outstream << "<ErrorHandlingStrategy value='" << Options::errorHandlingStrategy() << "'>" << Qt::endl;
    if (Options::rescheduleErrors())
        outstream << "<RescheduleErrors />" << Qt::endl;
    outstream << "<delay>" << Options::errorHandlingStrategyDelay() << "</delay>" << Qt::endl;
    outstream << "</ErrorHandlingStrategy>" << Qt::endl;

    outstream << "<StartupProcedure>" << Qt::endl;
    if (moduleState()->startupScriptURL().isEmpty() == false)
        outstream << "<Procedure value='" << moduleState()->startupScriptURL().toString(QUrl::PreferLocalFile) <<
                  "'>StartupScript</Procedure>" << Qt::endl;
    if (Options::schedulerUnparkDome())
        outstream << "<Procedure>UnparkDome</Procedure>" << Qt::endl;
    if (Options::schedulerUnparkMount())
        outstream << "<Procedure>UnparkMount</Procedure>" << Qt::endl;
    if (Options::schedulerOpenDustCover())
        outstream << "<Procedure>UnparkCap</Procedure>" << Qt::endl;
    outstream << "</StartupProcedure>" << Qt::endl;

    outstream << "<ShutdownProcedure>" << Qt::endl;
    if (Options::schedulerWarmCCD())
        outstream << "<Procedure>WarmCCD</Procedure>" << Qt::endl;
    if (Options::schedulerCloseDustCover())
        outstream << "<Procedure>ParkCap</Procedure>" << Qt::endl;
    if (Options::schedulerParkMount())
        outstream << "<Procedure>ParkMount</Procedure>" << Qt::endl;
    if (Options::schedulerParkDome())
        outstream << "<Procedure>ParkDome</Procedure>" << Qt::endl;
    if (moduleState()->shutdownScriptURL().isEmpty() == false)
        outstream << "<Procedure value='" << moduleState()->shutdownScriptURL().toString(QUrl::PreferLocalFile) <<
                  "'>ShutdownScript</Procedure>" <<
                  Qt::endl;
    outstream << "</ShutdownProcedure>" << Qt::endl;

    outstream << "</SchedulerList>" << Qt::endl;

    emit newLog(i18n("Scheduler list saved to %1", fileURL.toLocalFile()));
    file.close();
    moduleState()->setDirty(false);
    return true;
}

void SchedulerProcess::checkStartupProcedure()
{
    if (checkStartupState() == false)
        QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));
}

void SchedulerProcess::checkShutdownProcedure()
{
    if (checkShutdownState())
    {
        // shutdown completed
        if (moduleState()->shutdownState() == SHUTDOWN_COMPLETE)
        {
            emit newLog(i18n("Manual shutdown procedure completed successfully."));
            // Stop Ekos
            if (Options::stopEkosAfterShutdown())
                stopEkos();
        }
        else if (moduleState()->shutdownState() == SHUTDOWN_ERROR)
            emit newLog(i18n("Manual shutdown procedure terminated due to errors."));

        moduleState()->setShutdownState(SHUTDOWN_IDLE);
    }
    else
        // If shutdown procedure is not finished yet, let's check again in 1 second.
        QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));

}


void SchedulerProcess::parkCap()
{
    if (capInterface().isNull())
    {
        emit newLog(i18n("Dust cover park requested but no dust covers detected."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    QVariant parkingStatus = capInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    if (status != ISD::PARK_PARKED)
    {
        moduleState()->setShutdownState(SHUTDOWN_PARKING_CAP);
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Parking dust cap...";
        capInterface()->call(QDBus::AutoDetect, "park");
        emit newLog(i18n("Parking Cap..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        emit newLog(i18n("Cap already parked."));
        moduleState()->setShutdownState(SHUTDOWN_PARK_MOUNT);
    }
}

void SchedulerProcess::unParkCap()
{
    if (capInterface().isNull())
    {
        emit newLog(i18n("Dust cover unpark requested but no dust covers detected."));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    QVariant parkingStatus = capInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Cap parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    if (status != ISD::PARK_UNPARKED)
    {
        moduleState()->setStartupState(STARTUP_UNPARKING_CAP);
        capInterface()->call(QDBus::AutoDetect, "unpark");
        emit newLog(i18n("Unparking cap..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        emit newLog(i18n("Cap already unparked."));
        moduleState()->setStartupState(STARTUP_COMPLETE);
    }
}

void SchedulerProcess::parkMount()
{
    if (mountInterface().isNull())
    {
        emit newLog(i18n("Mount park requested but no mounts detected."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    QVariant parkingStatus = mountInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        case ISD::PARK_PARKED:
            if (moduleState()->shutdownState() == SHUTDOWN_PARK_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARK_DOME);

            moduleState()->setParkWaitState(PARKWAIT_PARKED);
            emit newLog(i18n("Mount already parked."));
            break;

        case ISD::PARK_UNPARKING:
        //case Mount::UNPARKING_BUSY:
        /* FIXME: Handle the situation where we request parking but an unparking procedure is running. */

        //        case Mount::PARKING_IDLE:
        //        case Mount::UNPARKING_OK:
        case ISD::PARK_ERROR:
        case ISD::PARK_UNKNOWN:
        case ISD::PARK_UNPARKED:
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Parking mount...";
            QDBusReply<bool> const mountReply = mountInterface()->call(QDBus::AutoDetect, "park");

            if (mountReply.error().type() != QDBusError::NoError)
            {
                qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount park request received DBUS error: %1").arg(
                                                      QDBusError::errorString(mountReply.error().type()));
                if (!manageConnectionLoss())
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
            }
            else moduleState()->startCurrentOperationTimer();
        }

        // Fall through
        case ISD::PARK_PARKING:
            //case Mount::PARKING_BUSY:
            if (moduleState()->shutdownState() == SHUTDOWN_PARK_MOUNT)
                moduleState()->setShutdownState(SHUTDOWN_PARKING_MOUNT);

            moduleState()->setParkWaitState(PARKWAIT_PARKING);
            emit newLog(i18n("Parking mount in progress..."));
            break;

            // All cases covered above so no need for default
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while parking mount.").arg(mountReply.value());
    }

}

void SchedulerProcess::unParkMount()
{
    if (mountInterface().isNull())
    {
        emit newLog(i18n("Mount unpark requested but no mounts detected."));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    QVariant parkingStatus = mountInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            moduleState()->setParkWaitState(PARKWAIT_ERROR);
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    switch (status)
    {
        //case Mount::UNPARKING_OK:
        case ISD::PARK_UNPARKED:
            if (moduleState()->startupState() == STARTUP_UNPARK_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARK_CAP);

            moduleState()->setParkWaitState(PARKWAIT_UNPARKED);
            emit newLog(i18n("Mount already unparked."));
            break;

        //case Mount::PARKING_BUSY:
        case ISD::PARK_PARKING:
        /* FIXME: Handle the situation where we request unparking but a parking procedure is running. */

        //        case Mount::PARKING_IDLE:
        //        case Mount::PARKING_OK:
        //        case Mount::PARKING_ERROR:
        case ISD::PARK_ERROR:
        case ISD::PARK_UNKNOWN:
        case ISD::PARK_PARKED:
        {
            QDBusReply<bool> const mountReply = mountInterface()->call(QDBus::AutoDetect, "unpark");

            if (mountReply.error().type() != QDBusError::NoError)
            {
                qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount unpark request received DBUS error: %1").arg(
                                                      QDBusError::errorString(mountReply.error().type()));
                if (!manageConnectionLoss())
                    moduleState()->setParkWaitState(PARKWAIT_ERROR);
            }
            else moduleState()->startCurrentOperationTimer();
        }

        // Fall through
        //case Mount::UNPARKING_BUSY:
        case ISD::PARK_UNPARKING:
            if (moduleState()->startupState() == STARTUP_UNPARK_MOUNT)
                moduleState()->setStartupState(STARTUP_UNPARKING_MOUNT);

            moduleState()->setParkWaitState(PARKWAIT_UNPARKING);
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";
            break;

            // All cases covered above
            //default:
            //    qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while unparking mount.").arg(mountReply.value());
    }
}

bool SchedulerProcess::isMountParked()
{
    if (mountInterface().isNull())
        return false;
    // First check if the mount is able to park - if it isn't, getParkingStatus will reply PARKING_ERROR and status won't be clear
    //QDBusReply<bool> const parkCapableReply = mountInterface->call(QDBus::AutoDetect, "canPark");
    QVariant canPark = mountInterface()->property("canPark");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount can park:" << (!canPark.isValid() ? "invalid" : (canPark.toBool() ? "T" : "F"));

    if (canPark.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount canPark request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        manageConnectionLoss();
        return false;
    }
    else if (canPark.toBool() == true)
    {
        // If it is able to park, obtain its current status
        //QDBusReply<int> const mountReply  = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
        QVariant parkingStatus = mountInterface()->property("parkStatus");
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Mount parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

        if (parkingStatus.isValid() == false)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount parking status property is invalid %1.").arg(
                                                  mountInterface()->lastError().type());
            manageConnectionLoss();
            return false;
        }

        // Deduce state of mount - see getParkingStatus in mount.cpp
        switch (static_cast<ISD::ParkStatus>(parkingStatus.toInt()))
        {
            //            case Mount::PARKING_OK:     // INDI switch ok, and parked
            //            case Mount::PARKING_IDLE:   // INDI switch idle, and parked
            case ISD::PARK_PARKED:
                return true;

            //            case Mount::UNPARKING_OK:   // INDI switch idle or ok, and unparked
            //            case Mount::PARKING_ERROR:  // INDI switch error
            //            case Mount::PARKING_BUSY:   // INDI switch busy
            //            case Mount::UNPARKING_BUSY: // INDI switch busy
            default:
                return false;
        }
    }
    // If the mount is not able to park, consider it not parked
    return false;
}

void SchedulerProcess::parkDome()
{
    // If there is no dome, mark error
    if (domeInterface().isNull())
    {
        emit newLog(i18n("Dome park requested but no domes detected."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
        return;
    }

    //QDBusReply<int> const domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    //Dome::ParkingStatus status = static_cast<Dome::ParkingStatus>(domeReply.value());
    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());
    if (status != ISD::PARK_PARKED)
    {
        moduleState()->setShutdownState(SHUTDOWN_PARKING_DOME);
        domeInterface()->call(QDBus::AutoDetect, "park");
        emit newLog(i18n("Parking dome..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        emit newLog(i18n("Dome already parked."));
        moduleState()->setShutdownState(SHUTDOWN_SCRIPT);
    }
}

void SchedulerProcess::unParkDome()
{
    // If there is no dome, mark error
    if (domeInterface().isNull())
    {
        emit newLog(i18n("Dome unpark requested but no domes detected."));
        moduleState()->setStartupState(STARTUP_ERROR);
        return;
    }

    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    if (static_cast<ISD::ParkStatus>(parkingStatus.toInt()) != ISD::PARK_UNPARKED)
    {
        moduleState()->setStartupState(STARTUP_UNPARKING_DOME);
        domeInterface()->call(QDBus::AutoDetect, "unpark");
        emit newLog(i18n("Unparking dome..."));

        moduleState()->startCurrentOperationTimer();
    }
    else
    {
        emit newLog(i18n("Dome already unparked."));
        moduleState()->setStartupState(STARTUP_UNPARK_MOUNT);
    }
}

bool SchedulerProcess::isDomeParked()
{
    if (domeInterface().isNull())
        return false;

    QVariant parkingStatus = domeInterface()->property("parkStatus");
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dome parking status" << (!parkingStatus.isValid() ? -1 : parkingStatus.toInt());

    if (parkingStatus.isValid() == false)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome parkStatus request received DBUS error: %1").arg(
                                              mountInterface()->lastError().type());
        if (!manageConnectionLoss())
            parkingStatus = ISD::PARK_ERROR;
    }

    ISD::ParkStatus status = static_cast<ISD::ParkStatus>(parkingStatus.toInt());

    return status == ISD::PARK_PARKED;
}

void SchedulerProcess::checkProcessExit(int exitCode)
{
    scriptProcess().disconnect();

    if (exitCode == 0)
    {
        if (moduleState()->startupState() == STARTUP_SCRIPT)
            moduleState()->setStartupState(STARTUP_UNPARK_DOME);
        else if (moduleState()->shutdownState() == SHUTDOWN_SCRIPT_RUNNING)
            moduleState()->setShutdownState(SHUTDOWN_COMPLETE);

        return;
    }

    if (moduleState()->startupState() == STARTUP_SCRIPT)
    {
        emit newLog(i18n("Startup script failed, aborting..."));
        moduleState()->setStartupState(STARTUP_ERROR);
    }
    else if (moduleState()->shutdownState() == SHUTDOWN_SCRIPT_RUNNING)
    {
        emit newLog(i18n("Shutdown script failed, aborting..."));
        moduleState()->setShutdownState(SHUTDOWN_ERROR);
    }

}

void SchedulerProcess::readProcessOutput()
{
    emit newLog(scriptProcess().readAllStandardOutput().simplified());
}

SchedulerJob *SchedulerProcess::activeJob()
{
    return  moduleState()->activeJob();
}

} // Ekos namespace
