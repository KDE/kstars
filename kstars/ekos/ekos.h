/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KLocalizedString>

#include <QMetaType>
#include <QtDBus/QDBusArgument>
#include <QString>

#include <vector>

namespace Ekos
{
// Guide States
static const QList<KLocalizedString> guideStates = { ki18n("Idle"),
                                                 ki18n("Aborted"),
                                                 ki18n("Connected"),
                                                 ki18n("Disconnected"),
                                                 ki18n("Capturing"),
                                                 ki18n("Looping"),
                                                 ki18n("Subtracting"),
                                                 ki18n("Subframing"),
                                                 ki18n("Selecting star"),
                                                 ki18n("Calibrating"),
                                                 ki18n("Calibration error"),
                                                 ki18n("Calibrated"),
                                                 ki18n("Guiding"),
                                                 ki18n("Suspended"),
                                                 ki18n("Reacquiring"),
                                                 ki18n("Dithering"),
                                                 ki18n("Manual Dithering"),
                                                 ki18n("Dithering error"),
                                                 ki18n("Dithering successful"),
                                                 ki18n("Settling")
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
    GUIDE_CALIBRATION_SUCCESS,
    GUIDE_GUIDING,
    GUIDE_SUSPENDED,
    GUIDE_REACQUIRE,
    GUIDE_DITHERING,
    GUIDE_MANUAL_DITHERING,
    GUIDE_DITHERING_ERROR,
    GUIDE_DITHERING_SUCCESS,
    GUIDE_DITHERING_SETTLE
} GuideState;

const QString getGuideStatusString(GuideState state, bool translated = true);

// Capture States
static const QList<KLocalizedString> captureStates =
{
    ki18n("Idle"), ki18n("In Progress"), ki18n("Capturing"), ki18n("Pause Planned"), ki18n("Paused"),
    ki18n("Suspended"), ki18n("Aborted"), ki18n("Waiting"), ki18n("Image Received"),
    ki18n("Dithering"), ki18n("Focusing"), ki18n("Filter Focus"), ki18n("Changing Filter"), ki18n("Guider Settling"),
    ki18n("Setting Temperature"), ki18n("Setting Rotator"), ki18n("Aligning"), ki18n("Calibrating"),
    ki18n("Meridian Flip"), ki18n("Complete")
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

const QString getCaptureStatusString(CaptureState state, bool translated = true);

// Focus States
static const QList<KLocalizedString> focusStates = { ki18n("Idle"),    ki18n("Complete"),       ki18n("Failed"),
                                                 ki18n("Aborted"), ki18n("User Input"),     ki18n("In Progress"),
                                                 ki18n("Framing"), ki18n("Changing Filter")
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

const QString getFocusStatusString(FocusState state, bool translated = true);

// Align States
static const QList<KLocalizedString> alignStates = { ki18n("Idle"),    ki18n("Complete"),  ki18n("Failed"),
                                                 ki18n("Aborted"), ki18n("In Progress"), ki18n("Successful"),
                                                 ki18n("Syncing"), ki18n("Slewing"), ki18n("Rotating"),
                                                 ki18n("Suspended")
                                               };

typedef enum
{
    ALIGN_IDLE,                 /**< No ongoing operations */
    ALIGN_COMPLETE,             /**< Alignment successfully completed. No operations pending. */
    ALIGN_FAILED,               /**< Alignment failed. No operations pending. */
    ALIGN_ABORTED,              /**< Alignment aborted by user or agent. */
    ALIGN_PROGRESS,             /**< Alignment operation in progress. This include capture and sovling. */
    ALIGN_SUCCESSFUL,           /**< Alignment Astrometry solver successfully solved the image. */
    ALIGN_SYNCING,              /**< Syncing mount to solution coordinates. */
    ALIGN_SLEWING,              /**< Slewing mount to target coordinates.  */
    ALIGN_ROTATING,             /**< Rotating (Automatic or Manual) to target position angle. */
    ALIGN_SUSPENDED             /**< Alignment operations suspended. */
} AlignState;

const QString getAlignStatusString(AlignState state, bool translated = true);

// Filter Manager States
static const QList<KLocalizedString> filterStates = { ki18n("Idle"), ki18n("Changing Filter"), ki18n("Focus Offset"),
                                                  ki18n("Auto Focus")
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

const QString getFilterStatusString(FilterState state, bool translated = true);

// Scheduler states

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

const QString getSchedulerStatusString(SchedulerState state, bool translated = true);

static const QList<KLocalizedString> schedulerStates = { ki18n("Idle"), ki18n("Startup"), ki18n("Running"),
                                                     ki18n("Paused"), ki18n("Shutdown"), ki18n("Aborted"),
                                                     ki18n("Loading")
                                                   };
typedef enum
{
    Idle,
    Pending,
    Success,
    Error
} CommunicationStatus;

typedef enum
{
    EXTENSION_START_REQUESTED,
    EXTENSION_STARTED,
    EXTENSION_STOP_REQUESTED,
    EXTENSION_STOPPED,
} ExtensionState;

const QString getExtensionStatusString(ExtensionState state, bool translated = true);

static const QList<KLocalizedString> extensionStates = { ki18n("Starting"), ki18n("Started"), ki18n("Stopping"), ki18n("Stopped")};

std::vector<double> gsl_polynomial_fit(const double *const data_x, const double *const data_y, const int n,
                                       const int order, double &chisq);

// Invalid value
const int INVALID_VALUE = -1e6;
// Invalid star HFR, FWHM result
static double constexpr INVALID_STAR_MEASURE = -1.0;

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

// Extensions
Q_DECLARE_METATYPE(Ekos::ExtensionState)
QDBusArgument &operator<<(QDBusArgument &argument, const Ekos::ExtensionState &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, Ekos::ExtensionState &dest);
