/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <KLocalizedString>

// This header file includes constants used by focus and shared with other modules

namespace Ekos
{

// Reason codes for running Autofocus
typedef enum { FOCUS_NONE,
               FOCUS_MANUAL,
               FOCUS_FILTER,
               FOCUS_TIME,
               FOCUS_TEMPERATURE,
               FOCUS_HFR_CHECK,
               FOCUS_MERIDIAN_FLIP,
               FOCUS_FILTER_OFFSETS,
               FOCUS_ABERRATION_INSPECTOR,
               FOCUS_SCHEDULER,
               FOCUS_USER_REQUEST,
               FOCUS_FOCUS_ADVISOR,
               FOCUS_MAX_REASONS
             } AutofocusReason;

// Associated text for AutofocusReason enum
static const QString AutofocusReasonStr[FOCUS_MAX_REASONS] =
{
    "N/A",
    "User Initiated",
    "Filter Change",
    "Time",
    "Temperature",
    "HFR Check",
    "Meridian Flip",
    "Build Filter Offsets",
    "Aberration Inspector",
    "Scheduler Initiated",
    "User Request (In-Seq)",
    "Focus Advisor"
};

// Reason codes for Autofocus failure
typedef enum { FOCUS_FAIL_NONE,
               FOCUS_FAIL_NO_STARS,
               FOCUS_FAIL_MAX_ITERS,
               FOCUS_FAIL_FOCUSER_NO_MOVE,
               FOCUS_FAIL_R2,
               FOCUS_FAIL_SMALL_HFR,
               FOCUS_FAIL_FOCUSER_OOB,
               FOCUS_FAIL_FLUCTUATIONS,
               FOCUS_FAIL_DEADLOCK,
               FOCUS_FAIL_TOLERANCE,
               FOCUS_FAIL_FOCUSER_ERROR,
               FOCUS_FAIL_FORCE_ABORT,
               FOCUS_FAIL_FILTER_MANAGER,
               FOCUS_FAIL_CAPTURE_TIMEOUT,
               FOCUS_FAIL_CAPTURE_FAILED,
               FOCUS_FAIL_NO_CAMERA,
               FOCUS_FAIL_NO_FOCUSER,
               FOCUS_FAIL_LOW_PULSE,
               FOCUS_FAIL_INTERNAL,
               FOCUS_FAIL_ABORT,
               FOCUS_FAIL_CURVEFIT,
               FOCUS_FAIL_ADVISOR_COMPLETE,
               FOCUS_FAIL_ADVISOR_RERUN,
               FOCUS_FAIL_OPTIMISED_OUT,
               FOCUS_FAIL_MAX_REASONS
             } AutofocusFailReason;

// Associated text for AutofocusFailReason enum
static const QString AutofocusFailReasonStr[FOCUS_FAIL_MAX_REASONS] =
{
    "N/A",
    "No Stars",
    "Hit Max Iterations",
    "Unable to Move Focuser",
    "R2 Check",
    "Delta HFR too small",
    "Focuser Move Out-Of-Bounds",
    "Hit Max Fluctuations",
    "Deadlock",
    "Invalid Tolerance",
    "Focuser Error",
    "Focus Forced Abort",
    "Filter Mnaager Failed",
    "Capture Timeed Out",
    "Capture Failed",
    "No Camera",
    "No Focuser Device",
    "Pulse Value Too Low",
    "Internal Error",
    "Abort Requested",
    "Unable to Fit Curve",
    "Focus Advisor Complete",
    "Focus Advisor Rerun",
    "Focus Optimised Out"
};
}
