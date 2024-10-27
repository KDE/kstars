/*
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMap>

namespace Ekos
{

/** @brief mapping signature --> frames count */
typedef QMap<QString, uint16_t> CapturedFramesMap;

typedef enum
{
    FILENAME_NOT_PREVIEW,
    FILENAME_LOCAL_PREVIEW,
    FILENAME_REMOTE_PREVIEW
} FilenamePreviewType;

// Typedef for HFR Check algorithms.
typedef enum
{
    HFR_CHECK_LAST_AUTOFOCUS, /* Use last Autofocus as reference   */
    HFR_CHECK_FIXED,          /* User supplied fixed reference     */
    HFR_CHECK_MEDIAN_MEASURE, /* Use median algorithm as reference */
    HFR_CHECK_MAX_ALGO        /* Max counter for enum              */
} HFRCheckAlgorithm;

/* Action types to be executed within the capturing workflow. */
typedef enum
{
    CAPTURE_ACTION_NONE,                /* Do nothing.                                                                                    */
    CAPTURE_ACTION_START,               /* Start capturing.                                                                               */
    CAPTURE_ACTION_PAUSE,               /* Pause capturing.                                                                               */
    CAPTURE_ACTION_SUSPEND,             /* Suspend capturing.                                                                             */
    CAPTURE_ACTION_FILTER,              /* Change the filter and wait until the correct filter is set.                                    */
    CAPTURE_ACTION_TEMPERATURE,         /* Set the camera chip target temperature and wait until the target temperature has been reached. */
    CAPTURE_ACTION_ROTATOR,             /* Set the camera rotator target angle and wait until the target angle has been reached.          */
    CAPTURE_ACTION_PREPARE_LIGHTSOURCE, /* Setup the selected flat lights source.                                                         */
    CAPTURE_ACTION_MOUNT_PARK,          /* Park the mount.                                                                                */
    CAPTURE_ACTION_DOME_PARK,           /* Park the dome.                                                                                 */
    CAPTURE_ACTION_FLAT_SYNC_FOCUS,     /* Move the focuser to the focus position for the selected filter.                                */
    CAPTURE_ACTION_SCOPE_COVER,         /* Ensure that the scope cover (if present) is opened.                                            */
    CAPTURE_ACTION_AUTOFOCUS,           /* Execute autofocus (might be triggered due to filter change).                                   */
    CAPTURE_ACTION_DITHER_REQUEST,      /* Request for dither execution.                                                                  */
    CAPTURE_ACTION_DITHER,              /* Execute dithering.                                                                             */
    CAPTURE_ACTION_CHECK_GUIDING,       /* Check if guiding is running.                                                                   */
} CaptureWorkflowActionType;

typedef enum {
    CAPTURE_PREACTION_NONE       = 1 << 0,
    CAPTURE_PREACTION_WALL       = 1 << 1,
    CAPTURE_PREACTION_PARK_MOUNT = 1 << 2,
    CAPTURE_PREACTION_PARK_DOME  = 1 << 3,
} CalibrationPreActions;

typedef enum
{
    SHUTTER_YES,    /* the CCD has a shutter                             */
    SHUTTER_NO,     /* the CCD has no shutter                            */
    SHUTTER_BUSY,   /* determining whether the CCD has a shutter running */
    SHUTTER_UNKNOWN /* unknown whether the CCD has a shutter             */
} ShutterStatus;

typedef enum
{
    CAP_IDLE,
    CAP_PARKING,
    CAP_UNPARKING,
    CAP_PARKED,
    CAP_ERROR,
    CAP_UNKNOWN
} CapState;

typedef enum
{
    CAP_LIGHT_OFF,     /* light is on               */
    CAP_LIGHT_ON,      /* light is off              */
    CAP_LIGHT_UNKNOWN, /* unknown whether on or off */
    CAP_LIGHT_BUSY     /* light state changing      */
} LightState;

typedef enum
{
    CAPTURE_CONTINUE_ACTION_NONE,            /* do nothing              */
    CAPTURE_CONTINUE_ACTION_NEXT_EXPOSURE,   /* start next exposure     */
    CAPTURE_CONTINUE_ACTION_CAPTURE_COMPLETE /* recall capture complete */
} CaptureContinueAction;

/* Result when starting to capture {@see SequenceJob::capture(bool, FITSMode)}. */
typedef enum
{
    CAPTURE_OK,               /* Starting a new capture succeeded.                                          */
    CAPTURE_FRAME_ERROR,      /* Setting frame parameters failed, capture not started.                      */
    CAPTURE_BIN_ERROR,        /* Setting binning parameters failed, capture not started.                    */
    CAPTURE_FOCUS_ERROR,      /* NOT USED.                                                                  */
} CaptureResult;


}; // namespace
