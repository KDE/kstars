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
    STARTUP_IDLE,                  /**< Startup procedure not started */
    STARTUP_PRE_DEVICES,           /**< Executing pre-startup tasks (before INDI) */
    STARTUP_PRE_DEVICES_RUNNING,   /**< Waiting for pre-startup tasks to complete */
    STARTUP_POST_DEVICES,          /**< Executing post-startup tasks (after INDI ready) */
    STARTUP_POST_DEVICES_RUNNING,  /**< Waiting for post-startup tasks to complete */
    STARTUP_COMPLETE,              /**< Startup procedure completed successfully */
    STARTUP_ERROR                  /**< Startup procedure failed */
} StartupState;
typedef enum
{
    SHUTDOWN_IDLE,                  ///< Shutdown not in progress
    SHUTDOWN_PRE_QUEUE_RUNNING,     ///< Pre-shutdown queue is executing
    SHUTDOWN_STOPPING_EKOS,         ///< Pre-shutdown done, stopping Ekos/INDI
    SHUTDOWN_POST_QUEUE_RUNNING,    ///< Post-shutdown queue is executing
    SHUTDOWN_COMPLETE,              ///< Shutdown complete
    SHUTDOWN_ERROR                  ///< Shutdown error
} ShutdownState;
typedef enum
{
    PARKWAIT_IDLE,
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

/** @brief States of a SchedulerJob. */
typedef enum
{
    SCHEDJOB_IDLE,       /**< Job was just created, and is not evaluated yet */
    SCHEDJOB_EVALUATION, /**< Job is being evaluated */
    SCHEDJOB_SCHEDULED,  /**< Job was evaluated, and has a schedule */
    SCHEDJOB_BUSY,       /**< Job is being processed */
    SCHEDJOB_ERROR,      /**< Job encountered a fatal issue while processing, and must be reset manually */
    SCHEDJOB_ABORTED,    /**< Job encountered a transitory issue while processing, and will be rescheduled */
    SCHEDJOB_INVALID,    /**< Job has an incorrect configuration, and cannot proceed */
    SCHEDJOB_COMPLETE    /**< Job finished all required captures */
} SchedulerJobStatus;

/** @brief Running stages of a SchedulerJob. */
typedef enum
{
    SCHEDSTAGE_IDLE,
    SCHEDSTAGE_SLEWING,
    SCHEDSTAGE_SLEW_COMPLETE,
    SCHEDSTAGE_FOCUSING,
    SCHEDSTAGE_FOCUS_COMPLETE,
    SCHEDSTAGE_ALIGNING,
    SCHEDSTAGE_ALIGN_COMPLETE,
    SCHEDSTAGE_RESLEWING,
    SCHEDSTAGE_RESLEWING_COMPLETE,
    SCHEDSTAGE_POSTALIGN_FOCUSING,
    SCHEDSTAGE_POSTALIGN_FOCUSING_COMPLETE,
    SCHEDSTAGE_GUIDING,
    SCHEDSTAGE_GUIDING_COMPLETE,
    SCHEDSTAGE_CAPTURING,
    SCHEDSTAGE_COMPLETE
} SchedulerJobStage;

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
