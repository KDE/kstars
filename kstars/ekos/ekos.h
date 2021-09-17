/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KLocalizedString>

#include <QMetaType>
#include <QDBusArgument>
#include <QStringList>
#include <QString>

#include <vector>

namespace Ekos
{
// Guide States
static const QStringList guideStates = { I18N_NOOP("Idle"),
                                         I18N_NOOP("Aborted"),
                                         I18N_NOOP("Connected"),
                                         I18N_NOOP("Disconnected"),
                                         I18N_NOOP("Capturing"),
                                         I18N_NOOP("Looping"),
                                         I18N_NOOP("Subtracting"),
                                         I18N_NOOP("Subframing"),
                                         I18N_NOOP("Selecting star"),
                                         I18N_NOOP("Calibrating"),
                                         I18N_NOOP("Calibration error"),
                                         I18N_NOOP("Calibrated"),
                                         I18N_NOOP("Guiding"),
                                         I18N_NOOP("Suspended"),
                                         I18N_NOOP("Reacquiring"),
                                         I18N_NOOP("Dithering"),
                                         I18N_NOOP("Manual Dithering"),
                                         I18N_NOOP("Dithering error"),
                                         I18N_NOOP("Dithering successful"),
                                         I18N_NOOP("Settling")
                                       };

typedef enum
{
    GUIDE_IDLE,
    GUIDE_ABORTED,
    GUIDE_CONNECTED,
    GUIDE_DISCONNECTED,
    GUIDE_CAPTURE,
    GUIDE_LOOPING,
    GUIDE_DARK,
    GUIDE_SUBFRAME,
    GUIDE_STAR_SELECT,
    GUIDE_CALIBRATING,
    GUIDE_CALIBRATION_ERROR,
    GUIDE_CALIBRATION_SUCESS,
    GUIDE_GUIDING,
    GUIDE_SUSPENDED,
    GUIDE_REACQUIRE,
    GUIDE_DITHERING,
    GUIDE_MANUAL_DITHERING,
    GUIDE_DITHERING_ERROR,
    GUIDE_DITHERING_SUCCESS,
    GUIDE_DITHERING_SETTLE
} GuideState;

const QString &getGuideStatusString(GuideState state);

// Capture States
static const QStringList captureStates =
{
    I18N_NOOP("Idle"), I18N_NOOP("In Progress"), I18N_NOOP("Capturing"), I18N_NOOP("Pause Planned"), I18N_NOOP("Paused"),
    I18N_NOOP("Suspended"), I18N_NOOP("Aborted"), I18N_NOOP("Waiting"), I18N_NOOP("Image Received"),
    I18N_NOOP("Dithering"), I18N_NOOP("Focusing"), I18N_NOOP("Filter Focus"), I18N_NOOP("Changing Filter"), I18N_NOOP("Guider Settling"),
    I18N_NOOP("Setting Temperature"), I18N_NOOP("Setting Rotator"), I18N_NOOP("Aligning"), I18N_NOOP("Calibrating"),
    I18N_NOOP("Meridian Flip"), I18N_NOOP("Complete")
};

/**
  * @brief Capture states
  *
  * They can be divided into several stages:
  * - No capturing is running (@see CAPTURE_IDLE, @see CAPTURE_COMPLETE or @see CAPTURE_ABORTED)
  * - A capture sequence job is in preparation (@see CAPTURE_PROGRESS as state and @see CAPTURE_SETTING_TEMPERATURE,
  *   @see CAPTURE_SETTING_ROTATOR and @see CAPTURE_CHANGING_FILTER as state events signaled to
  *   @see Capture::updatePrepareState(Ekos::CaptureState))
  * - Calibration activities to initialize the execution of a sequence job (@see CAPTURE_DITHERING, @see CAPTURE_FOCUSING,
  *   @see CAPTURE_ALIGNING and @see CAPTURE_CALIBRATING)
  * - Waiting for start of capturing (@see CAPTURE_PAUSE_PLANNED, @see CAPTURE_PAUSED, @see CAPTURE_SUSPENDED and @see CAPTURE_WAITING)
  * - Capturing (@see CAPTURE_CAPTURING and @see CAPTURE_IMAGE_RECEIVED)
  */
typedef enum
{
    CAPTURE_IDLE,                /*!< no capture job active */
    CAPTURE_PROGRESS,            /*!< capture job sequence in preparation (temperature, filter, rotator) */
    CAPTURE_CAPTURING,           /*!< CCD capture running */
    CAPTURE_PAUSE_PLANNED,       /*!< user has requested to pause the capture sequence */
    CAPTURE_PAUSED,              /*!< paused capture sequence due to a user request */
    CAPTURE_SUSPENDED,           /*!< capture stopped since some limits are not met, but may be continued if all limits are met again */
    CAPTURE_ABORTED,             /*!< capture stopped by the user or aborted due to guiding problems etc. */
    CAPTURE_WAITING,             /*!< waiting for settling of the mount before start of capturing */
    CAPTURE_IMAGE_RECEIVED,      /*!< image received from the CDD device */
    CAPTURE_DITHERING,           /*!< dithering before starting to capture */
    CAPTURE_FOCUSING,            /*!< focusing before starting to capture */
    CAPTURE_FILTER_FOCUS,        /*!< not used */
    CAPTURE_CHANGING_FILTER,     /*!< preparation event changing the filter */
    CAPTURE_GUIDER_DRIFT,        /*!< preparation event waiting for the guider to settle */
    CAPTURE_SETTING_TEMPERATURE, /*!< preparation event setting the camera temperature */
    CAPTURE_SETTING_ROTATOR,     /*!< preparation event setting the camera rotator */
    CAPTURE_ALIGNING,            /*!< aligning before starting to capture */
    CAPTURE_CALIBRATING,         /*!< startup of guiding running before starting to capture */
    CAPTURE_MERIDIAN_FLIP,       /*!< only used as signal that a meridian flip is ongoing */
    CAPTURE_COMPLETE             /*!< capture job sequence completed successfully */
} CaptureState;

const QString &getCaptureStatusString(CaptureState state);

// Focus States
static const QStringList focusStates = { I18N_NOOP("Idle"),    I18N_NOOP("Complete"),       I18N_NOOP("Failed"),
                                         I18N_NOOP("Aborted"), I18N_NOOP("User Input"),     I18N_NOOP("In Progress"),
                                         I18N_NOOP("Framing"), I18N_NOOP("Changing Filter")
                                       };

typedef enum
{
    FOCUS_IDLE,
    FOCUS_COMPLETE,
    FOCUS_FAILED,
    FOCUS_ABORTED,
    FOCUS_WAITING,
    FOCUS_PROGRESS,
    FOCUS_FRAMING,
    FOCUS_CHANGING_FILTER
} FocusState;

const QString &getFocusStatusString(FocusState state);

// Align States
static const QStringList alignStates = { I18N_NOOP("Idle"),    I18N_NOOP("Complete"),  I18N_NOOP("Failed"),
                                         I18N_NOOP("Aborted"), I18N_NOOP("In Progress"), I18N_NOOP("Syncing"),
                                         I18N_NOOP("Slewing"), I18N_NOOP("Suspended")
                                       };

typedef enum
{
    ALIGN_IDLE,
    ALIGN_COMPLETE,
    ALIGN_FAILED,
    ALIGN_ABORTED,
    ALIGN_PROGRESS,
    ALIGN_SYNCING,
    ALIGN_SLEWING,
    ALIGN_SUSPENDED
} AlignState;

const QString &getAlignStatusString(AlignState state);

// Filter Manager States
static const QStringList filterStates = { I18N_NOOP("Idle"), I18N_NOOP("Changing Filter"), I18N_NOOP("Focus Offset"),
                                          I18N_NOOP("Auto Focus")
                                        };
typedef enum
{
    FILTER_IDLE,
    FILTER_CHANGE,
    FILTER_OFFSET,
    FILTER_AUTOFOCUS
} FilterState;

typedef enum
{
    SCRIPT_PRE_JOB,     /**< Script to run before a sequence job is started */
    SCRIPT_PRE_CAPTURE, /**< Script to run before a sequence capture is started */
    SCRIPT_POST_CAPTURE,/**< Script to run after a sequence capture is completed */
    SCRIPT_POST_JOB,    /**< Script to run after a sequence job is completed */
    SCRIPT_N
} ScriptTypes;

const QString &getFilterStatusString(FilterState state);

// Scheduler states

const QString &getSchedulerStatusString(AlignState state);

static const QStringList schedulerStates = { I18N_NOOP("Idle"), I18N_NOOP("Startup"), I18N_NOOP("Running"),
                                             I18N_NOOP("Paused"), I18N_NOOP("Shutdown"), I18N_NOOP("Aborted")
                                           };

typedef enum
{
    SCHEDULER_IDLE,     /*< Scheduler is stopped. */
    SCHEDULER_STARTUP,  /*< Scheduler is starting the observatory up. */
    SCHEDULER_RUNNING,  /*< Scheduler is running. */
    SCHEDULER_PAUSED,   /*< Scheduler is paused by the end-user. */
    SCHEDULER_SHUTDOWN, /*< Scheduler is shutting the observatory down. */
    SCHEDULER_ABORTED,  /*< Scheduler is stopped in error. */
    SCHEDULER_LOADING   /*< Scheduler is loading a schedule. */
} SchedulerState;

typedef enum
{
    Idle,
    Pending,
    Success,
    Error
} CommunicationStatus;

std::vector<double> gsl_polynomial_fit(const double *const data_x, const double *const data_y, const int n,
                                       const int order, double &chisq);

// Invalid value
const int INVALID_VALUE = -1e6;
}

// Communication Status
Q_DECLARE_METATYPE(Ekos::CommunicationStatus)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::CommunicationStatus &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::CommunicationStatus &dest);

// Capture Status
// FIXME is there a way to avoid unnecessary duplicating code? The solution suggested in KDE WiKi is to use Boost
// which we do not have to add as dependency
Q_DECLARE_METATYPE(Ekos::CaptureState)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::CaptureState &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::CaptureState &dest);

// Focus
Q_DECLARE_METATYPE(Ekos::FocusState)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::FocusState &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::FocusState &dest);

// Guide
Q_DECLARE_METATYPE(Ekos::GuideState)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::GuideState &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::GuideState &dest);

// Align
Q_DECLARE_METATYPE(Ekos::AlignState)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::AlignState &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::AlignState &dest);

// Scheduler
Q_DECLARE_METATYPE(Ekos::SchedulerState)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::SchedulerState &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::SchedulerState &dest);
