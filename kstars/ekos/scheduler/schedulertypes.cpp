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
        case STARTUP_PRE_DEVICES:
            return "Pre-startup phase";
        case STARTUP_PRE_DEVICES_RUNNING:
            return "Starting Ekos and INDI";
        case STARTUP_POST_DEVICES:
            return "Post-startup phase";
        case STARTUP_POST_DEVICES_RUNNING:
            return "Post-startup queue running";
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
            return "SHUTDOWN_IDLE";
        case SHUTDOWN_PRE_QUEUE_RUNNING:
            return "SHUTDOWN_PRE_QUEUE_RUNNING";
        case SHUTDOWN_STOPPING_EKOS:
            return "SHUTDOWN_STOPPING_EKOS";
        case SHUTDOWN_POST_QUEUE_RUNNING:
            return "SHUTDOWN_POST_QUEUE_RUNNING";
        case SHUTDOWN_COMPLETE:
            return "SHUTDOWN_COMPLETE";
        case SHUTDOWN_ERROR:
            return "SHUTDOWN_ERROR";
    }
    return QString("????");
}

QString parkWaitStateString(ParkWaitState state)
{
    switch(state)
    {
        case PARKWAIT_IDLE:
            return "Park idle";
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
