/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "QString"

namespace Ekos
{
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
} ParkWaitState;
// overall states of EKOS
typedef enum { EKOS_IDLE, EKOS_STARTING, EKOS_STOPPING, EKOS_READY } EkosState;
// overall states of INDI
typedef enum { INDI_IDLE, INDI_CONNECTING, INDI_DISCONNECTING, INDI_PROPERTY_CHECK, INDI_READY } INDIState;

/** @brief options what should happen if an error or abort occurs */
typedef enum
{
    ERROR_DONT_RESTART,
    ERROR_RESTART_AFTER_TERMINATION,
    ERROR_RESTART_IMMEDIATELY
} ErrorHandlingStrategy;

/** @brief Algorithms, in the same order as UI. */
typedef enum
{
    ALGORITHM_GREEDY = 1
} SchedulerAlgorithm;


/** @brief Conditions under which a SchedulerJob may start. */
typedef enum
{
    START_ASAP = 0,
    START_AT   = 2
} StartupCondition;

/** @brief Conditions under which a SchedulerJob may complete. */
typedef enum
{
    FINISH_SEQUENCE,
    FINISH_REPEAT,
    FINISH_LOOP,
    FINISH_AT
} CompletionCondition;

/** @brief IterationTypes, the different types of scheduler iterations that are run. */
typedef enum
{
    RUN_WAKEUP = 0,
    RUN_SCHEDULER,
    RUN_JOBCHECK,
    RUN_SHUTDOWN,
    RUN_NOTHING
} SchedulerTimerState;

/** @brief mapping signature --> frames count */
typedef QMap<QString, uint16_t> CapturedFramesMap;

// Functions to make human-readable debug messages for the various enums.

QString ekosStateString(EkosState state);
QString indiStateString(INDIState state);
QString startupStateString(StartupState state);
QString shutdownStateString(ShutdownState state);
QString parkWaitStateString(ParkWaitState state);
QString timerStr(SchedulerTimerState state);
QString startupConditionString(StartupCondition condition);
QString completionConditionString(CompletionCondition condition);

} // Ekos namespace
