/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulertypes.h"

namespace Ekos
{
// Functions to make human-readable debug messages for the various enums.

QString ekosStateString(EkosState state)
{
    switch(state)
    {
        case EKOS_IDLE:
            return "Ekos is idle";
        case EKOS_STARTING:
            return "Starting Ekos";
        case EKOS_STOPPING:
            return "Stopping Ekos";
        case EKOS_READY:
            return "Ekos is ready";
    }
    return QString("????");
}

QString indiStateString(INDIState state)
{
    switch(state)
    {
        case INDI_IDLE:
            return "INDI is idle";
        case INDI_PROPERTY_CHECK:
            return "Checking INDI properties";
        case INDI_CONNECTING:
            return "Connecting to INDI";
        case INDI_DISCONNECTING:
            return "Disconnecting to INDI";
        case INDI_READY:
            return "INDI is ready";
    }
    return QString("????");
}

QString startupStateString(StartupState state)
{
    switch(state)
    {
        case STARTUP_IDLE:
            return "Startup is idle";
        case STARTUP_SCRIPT:
            return "Startup running script";
        case STARTUP_UNPARK_DOME:
            return "Startup unpark dome";
        case STARTUP_UNPARKING_DOME:
            return "Startup unparking dome";
        case STARTUP_UNPARK_MOUNT:
            return "Startup unpark mount";
        case STARTUP_UNPARKING_MOUNT:
            return "Startup unparking mount";
        case STARTUP_UNPARK_CAP:
            return "Startup remove cap";
        case STARTUP_UNPARKING_CAP:
            return "Starup removing cap";
        case STARTUP_ERROR:
            return "Startup error";
        case STARTUP_COMPLETE:
            return "Startup is complete";
    }
    return QString("????");
}

QString shutdownStateString(ShutdownState state)
{
    switch(state)
    {
        case SHUTDOWN_IDLE:
            return "Shutdown is idle";
        case SHUTDOWN_PARK_CAP:
            return "Shutdown remove cap";
        case SHUTDOWN_PARKING_CAP:
            return "Shutdown removing cap";
        case SHUTDOWN_PARK_MOUNT:
            return "Shutdown park mount";
        case SHUTDOWN_PARKING_MOUNT:
            return "Shutdown parking mount";
        case SHUTDOWN_PARK_DOME:
            return "Shutdown park dome";
        case SHUTDOWN_PARKING_DOME:
            return "Shutdown parking dome";
        case SHUTDOWN_SCRIPT:
            return "Shutdown script";
        case SHUTDOWN_SCRIPT_RUNNING:
            return "Shutdown script running";
        case SHUTDOWN_ERROR:
            return "Shutdown error";
        case SHUTDOWN_COMPLETE:
            return "Shutdown complete";
    }
    return QString("????");
}

QString parkWaitStateString(ParkWaitState state)
{
    switch(state)
    {
        case PARKWAIT_IDLE:
            return "Park idle";
        case PARKWAIT_PARK:
            return "Park";
        case PARKWAIT_PARKING:
            return "Parking";
        case PARKWAIT_PARKED:
            return "Parked";
        case PARKWAIT_UNPARK:
            return "Unpark";
        case PARKWAIT_UNPARKING:
            return "Unparking";
        case PARKWAIT_UNPARKED:
            return "Unparked";
        case PARKWAIT_ERROR:
            return "Park error";
    }
    return QString("????");
}

QString timerStr(SchedulerTimerState state)
{
    switch (state)
    {
        case RUN_WAKEUP:
            return QString("RUN_WAKEUP");
        case RUN_SCHEDULER:
            return QString("RUN_SCHEDULER");
        case RUN_JOBCHECK:
            return QString("RUN_JOBCHECK");
        case RUN_SHUTDOWN:
            return QString("RUN_SHUTDOWN");
        case RUN_NOTHING:
            return QString("RUN_NOTHING");
    }
    return QString("????");
}

QString startupConditionString(StartupCondition condition)
{
    switch(condition)
    {
        case START_ASAP:
            return "ASAP";
        case START_AT:
            return "AT";
    }
    return QString("????");
}

QString completionConditionString(CompletionCondition condition)
{
    switch(condition)
    {
        case FINISH_SEQUENCE:
            return "FINISH";
        case FINISH_REPEAT:
            return "REPEAT";
        case FINISH_LOOP:
            return "LOOP";
        case FINISH_AT:
            return "AT";
    }
    return QString("????");
}


} // Ekos namespace
