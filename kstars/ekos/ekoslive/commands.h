/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Message Channel

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMap>

namespace EkosLive
{
enum COMMANDS
{
    GET_CONNECTION,
    GET_STATES,
    GET_CAMERAS,
    GET_MOUNTS,
    GET_FILTER_WHEELS,
    GET_DOMES,
    GET_CAPS,
    GET_STELLARSOLVER_PROFILES,
    GET_DRIVERS,
    GET_DEVICES,
    NEW_CONNECTION_STATE,
    NEW_MOUNT_STATE,
    NEW_CAMERA_STATE,
    NEW_CAPTURE_STATE,
    NEW_GUIDE_STATE,
    NEW_FOCUS_STATE,
    NEW_ALIGN_STATE,
    NEW_POLAR_STATE,
    NEW_DOME_STATE,
    NEW_CAP_STATE,
    NEW_PREVIEW_IMAGE,
    NEW_VIDEO_FRAME,
    NEW_ALIGN_FRAME,
    NEW_NOTIFICATION,
    NEW_TEMPERATURE,

    SET_CLIENT_STATE,
    LOGOUT,

    // Profiles
    GET_PROFILES,
    START_PROFILE,
    STOP_PROFILE,
    ADD_PROFILE,
    GET_PROFILE,
    DELETE_PROFILE,
    UPDATE_PROFILE,
    SET_PROFILE_MAPPING,
    SET_PROFILE_PORT_SELECTION,
    GET_PROFILE_PORT_SELECTION,

    // SCOPES
    GET_SCOPES,
    ADD_SCOPE,
    DELETE_SCOPE,
    UPDATE_SCOPE,

    // Capture
    CAPTURE_PREVIEW,
    CAPTURE_TOGGLE_VIDEO,
    CAPTURE_TOGGLE_CAMERA,
    CAPTURE_TOGGLE_FILTER_WHEEL,
    CAPTURE_START,
    CAPTURE_STOP,
    CAPTURE_GET_SEQUENCES,
    CAPTURE_ADD_SEQUENCE,
    CAPTURE_REMOVE_SEQUENCE,
    CAPTURE_SET_SETTINGS,
    CAPTURE_SET_LIMITS,
    CAPTURE_GET_LIMITS,
    CAPTURE_GET_CALIBRATION_SETTINGS,
    CAPTURE_GET_FILE_SETTINGS,
    CAPTURE_LOOP,

    // Mount
    MOUNT_PARK,
    MOUNT_UNPARK,
    MOUNT_ABORT,
    MOUNT_SYNC_RADE,
    MOUNT_SYNC_TARGET,
    MOUNT_GOTO_RADE,
    MOUNT_GOTO_TARGET,
    MOUNT_SET_MOTION,
    MOUNT_SET_TRACKING,
    MOUNT_SET_SLEW_RATE,
    MOUNT_SET_ALTITUDE_LIMITS,
    MOUNT_SET_HA_LIMIT,
    MOUNT_SET_MERIDIAN_FLIP,
    MOUNT_SET_AUTO_PARK,
    MOUNT_SET_EVERYDAY_AUTO_PARK,
    MOUNT_CLEAR,
    MOUNT_GET_SETTINGS,

    // Dome
    DOME_PARK,
    DOME_UNPARK,
    DOME_GOTO,
    DOME_STOP,

    // Cap
    CAP_PARK,
    CAP_UNPARK,
    CAP_SET_LIGHT,

    // Focus
    FOCUS_START,
    FOCUS_CAPTURE,
    FOCUS_STOP,
    FOCUS_RESET,
    FOCUS_IN,
    FOCUS_OUT,
    FOCUS_LOOP,
    FOCUS_SET_SETTINGS,
    FOCUS_SET_PRIMARY_SETTINGS,
    FOCUS_SET_PROCESS_SETTINGS,
    FOCUS_SET_MECHANICS_SETTINGS,
    FOCUS_GET_PRIMARY_SETTINGS,
    FOCUS_GET_PROCESS_SETTINGS,
    FOCUS_GET_MECHANICS_SETTINGS,

    // Guide
    GUIDE_START,
    GUIDE_CAPTURE,
    GUIDE_LOOP,
    GUIDE_STOP,
    GUIDE_CLEAR,
    GUIDE_REPORT,
    GUIDE_SET_SETTINGS,

    // Align
    ALIGN_SOLVE,
    ALIGN_STOP,
    ALIGN_LOAD_AND_SLEW,
    ALIGN_SET_FILE_EXTENSION,
    ALIGN_SET_SETTINGS,

    // Polar Assistant Helper
    PAH_START,
    PAH_STOP,
    PAH_REFRESH,
    PAH_SET_CROSSHAIR,
    PAH_SELECT_STAR_DONE,
    PAH_REFRESHING_DONE,
    PAH_SET_SETTINGS,
    PAH_RESET_VIEW,
    PAH_SLEW_DONE,
    PAH_PAH_SET_ZOOM,

    // Options
    OPTION_SET_HIGH_BANDWIDTH,
    OPTION_SET_IMAGE_TRANSFER,
    OPTION_SET_NOTIFICATIONS,
    OPTION_SET_CLOUD_STORAGE,

    // Storage Options
    SET_BLOBS,

    // DSLRs
    DSLR_GET_INFO,
    DSLR_SET_INFO,
    DSLR_SET_MODE,

    // Low-level device Access
    DEVICE_GET,
    DEVICE_RESTART,
    DEVICE_BLOB_GET,
    DEVICE_PROPERTY_GET,
    DEVICE_PROPERTY_SET,
    DEVICE_PROPERTY_ADD,
    DEVICE_PROPERTY_REMOVE,
    DEVICE_PROPERTY_SUBSCRIBE,
    DEVICE_PROPERTY_UNSUBSCRIBE,

    // Dialogs
    DIALOG_GET_INFO,
    DIALOG_GET_RESPONSE,

    // Filter Manager
    FM_GET_DATA,
    FM_SET_DATA,

    // Astronomy Library
    ASTRO_GET_ALMANC,
    ASTRO_SEARCH_OBJECTS,
    ASTRO_GET_OBJECTS_INFO,
    ASTRO_GET_OBJECTS_IMAGE,
    ASTRO_GET_OBJECTS_OBSERVABILITY,
    ASTRO_GET_OBJECTS_RISESET,
};

static QMap<COMMANDS, QString> const commands =
{
    {GET_CONNECTION, "get_connection"},
    {GET_STATES, "get_states"},
    {GET_CAMERAS, "get_cameras"},
    {GET_MOUNTS, "get_mounts"},
    {GET_FILTER_WHEELS, "get_filter_wheels"},
    {GET_DOMES, "get_domes"},
    {GET_CAPS, "get_caps"},
    {GET_STELLARSOLVER_PROFILES, "get_stellarsolver_profiles"},
    {GET_DRIVERS, "get_drivers"},
    {GET_DEVICES, "get_devices"},
    {NEW_CONNECTION_STATE, "new_connection_state"},
    {NEW_MOUNT_STATE, "new_mount_state"},
    {NEW_CAMERA_STATE, "new_camera_state"},
    {NEW_CAPTURE_STATE, "new_capture_state"},
    {NEW_GUIDE_STATE, "new_guide_state"},
    {NEW_FOCUS_STATE, "new_focus_state"},
    {NEW_ALIGN_STATE, "new_align_state"},
    {NEW_POLAR_STATE, "new_polar_state"},
    {NEW_DOME_STATE, "new_dome_state"},
    {NEW_CAP_STATE, "new_cap_state"},
    {NEW_PREVIEW_IMAGE, "new_preview_image"},
    {NEW_VIDEO_FRAME, "new_video_frame"},
    {NEW_ALIGN_FRAME, "new_align_frame"},
    {NEW_NOTIFICATION, "new_notification"},
    {NEW_TEMPERATURE, "new_temperature"},

    {SET_CLIENT_STATE, "set_client_state"},
    {LOGOUT, "logout"},

    {GET_PROFILES, "get_profiles"},
    {START_PROFILE, "profile_start"},
    {STOP_PROFILE, "profile_stop"},
    {ADD_PROFILE, "profile_add"},
    {GET_PROFILE, "profile_get"},
    {DELETE_PROFILE, "profile_delete"},
    {UPDATE_PROFILE, "profile_update"},
    {SET_PROFILE_MAPPING, "profile_set_mapping"},
    {SET_PROFILE_PORT_SELECTION, "profile_set_port_selection"},
    {GET_PROFILE_PORT_SELECTION, "profile_get_port_selection"},

    {GET_SCOPES, "get_scopes"},
    {ADD_SCOPE, "scope_add"},
    {DELETE_SCOPE, "scope_delete"},
    {UPDATE_SCOPE, "scope_update"},

    {CAPTURE_PREVIEW, "capture_preview"},
    {CAPTURE_TOGGLE_VIDEO, "capture_toggle_video"},
    {CAPTURE_TOGGLE_CAMERA, "capture_toggle_camera"},
    {CAPTURE_TOGGLE_FILTER_WHEEL, "capture_toggle_filter_wheel"},
    {CAPTURE_START, "capture_start"},
    {CAPTURE_STOP, "capture_stop"},
    {CAPTURE_GET_SEQUENCES, "capture_get_sequences"},
    {CAPTURE_ADD_SEQUENCE, "capture_add_sequence"},
    {CAPTURE_REMOVE_SEQUENCE, "capture_remove_sequence"},
    {CAPTURE_SET_SETTINGS, "capture_set_settings"},
    {CAPTURE_SET_LIMITS, "capture_set_limits"},
    {CAPTURE_GET_LIMITS, "capture_get_limits"},
    {CAPTURE_GET_CALIBRATION_SETTINGS, "capture_get_calibration_settings"},
    {CAPTURE_GET_FILE_SETTINGS, "capture_get_file_settings"},
    {CAPTURE_LOOP, "capture_loop"},

    {MOUNT_PARK, "mount_park"},
    {MOUNT_UNPARK, "mount_unpark"},
    {MOUNT_ABORT, "mount_abort"},
    {MOUNT_SYNC_RADE, "mount_sync_rade"},
    {MOUNT_SYNC_TARGET, "mount_sync_target"},
    {MOUNT_GOTO_RADE, "mount_goto_rade"},
    {MOUNT_GOTO_TARGET, "mount_goto_target"},
    {MOUNT_SET_MOTION, "mount_set_motion"},
    {MOUNT_SET_TRACKING, "mount_set_tracking"},
    {MOUNT_SET_SLEW_RATE, "mount_set_slew_rate"},
    {MOUNT_SET_ALTITUDE_LIMITS, "mount_set_altitude_limits"},
    {MOUNT_SET_HA_LIMIT, "mount_set_ha_limit"},
    {MOUNT_SET_MERIDIAN_FLIP, "mount_set_meridian_flip"},
    {MOUNT_SET_AUTO_PARK, "mount_set_auto_park"},
    {MOUNT_SET_EVERYDAY_AUTO_PARK, "mount_set_everyday_auto_park"},
    {MOUNT_CLEAR, "mount_clear"},
    {MOUNT_GET_SETTINGS, "mount_get_settings"},

    {DOME_PARK, "dome_park"},
    {DOME_UNPARK, "dome_unpark"},
    {DOME_GOTO, "dome_goto"},
    {DOME_STOP, "dome_stop"},

    {CAP_PARK, "cap_park"},
    {CAP_UNPARK, "cap_unpark"},
    {CAP_SET_LIGHT, "cap_set_light"},

    {FOCUS_START, "focus_start"},
    {FOCUS_CAPTURE, "focus_capture"},
    {FOCUS_STOP, "focus_stop"},
    {FOCUS_RESET, "focus_reset"},
    {FOCUS_IN, "focus_in"},
    {FOCUS_OUT, "focus_out"},
    {FOCUS_LOOP, "focus_loop"},
    {FOCUS_SET_SETTINGS, "focus_set_settings"},
    {FOCUS_SET_PRIMARY_SETTINGS, "focus_set_primary_settings"},
    {FOCUS_SET_PROCESS_SETTINGS, "focus_set_process_settings"},
    {FOCUS_SET_MECHANICS_SETTINGS, "focus_set_mechanics_settings"},
    {FOCUS_GET_PRIMARY_SETTINGS, "focus_get_primary_settings"},
    {FOCUS_GET_PROCESS_SETTINGS, "focus_get_process_settings"},
    {FOCUS_GET_MECHANICS_SETTINGS, "focus_get_mechanics_settings"},

    {GUIDE_START, "guide_start"},
    {GUIDE_CAPTURE, "guide_capture"},
    {GUIDE_LOOP, "guide_loop"},
    {GUIDE_STOP, "guide_stop"},
    {GUIDE_CLEAR, "guide_clear"},
    {GUIDE_REPORT, "guide_report"},
    {GUIDE_SET_SETTINGS, "guide_set_settings"},

    {ALIGN_SOLVE, "align_solve"},
    {ALIGN_STOP, "align_stop"},
    {ALIGN_LOAD_AND_SLEW, "align_load_and_slew"},
    {ALIGN_SET_FILE_EXTENSION, "align_set_file_extension"},
    {ALIGN_SET_SETTINGS, "align_set_settings"},

    {PAH_START, "polar_start"},
    {PAH_STOP, "polar_stop"},
    {PAH_SET_SETTINGS, "polar_set_settings"},
    {PAH_REFRESH, "polar_refresh"},
    {PAH_SET_CROSSHAIR, "polar_set_crosshair"},
    {PAH_SELECT_STAR_DONE, "polar_star_select_done"},
    {PAH_REFRESHING_DONE, "polar_refreshing_done"},
    {PAH_RESET_VIEW, "polar_reset_view"},
    {PAH_SLEW_DONE, "polar_slew_done"},
    {PAH_PAH_SET_ZOOM, "polar_set_zoom"},

    {OPTION_SET_HIGH_BANDWIDTH, "option_set_high_bandwidth"},
    {OPTION_SET_IMAGE_TRANSFER, "option_set_image_transfer"},
    {OPTION_SET_NOTIFICATIONS, "option_set_notifications"},
    {OPTION_SET_CLOUD_STORAGE, "option_set_cloud_storage"},

    {SET_BLOBS, "set_blobs"},

    {DSLR_GET_INFO, "dslr_get_info"},
    {DSLR_SET_INFO, "dslr_set_info"},
    {DSLR_SET_MODE, "dslr_set_mode"},

    {DEVICE_GET, "device_get"},
    {DEVICE_RESTART, "device_restart"},
    {DEVICE_BLOB_GET, "device_blob_get"},
    {DEVICE_PROPERTY_GET, "device_property_get"},
    {DEVICE_PROPERTY_SET, "device_property_set"},
    {DEVICE_PROPERTY_ADD, "device_property_add"},
    {DEVICE_PROPERTY_REMOVE, "device_property_remove"},
    {DEVICE_PROPERTY_SUBSCRIBE, "device_property_subscribe"},
    {DEVICE_PROPERTY_UNSUBSCRIBE, "device_property_unsubscribe"},

    {DIALOG_GET_INFO, "dialog_get_info"},
    {DIALOG_GET_RESPONSE, "dialog_get_response"},

    {FM_GET_DATA, "fm_get_data"},
    {FM_SET_DATA, "fm_set_data"},

    {ASTRO_GET_ALMANC, "astro_get_almanac"},
    {ASTRO_SEARCH_OBJECTS, "astro_search_objects"},
    {ASTRO_GET_OBJECTS_INFO, "astro_get_objects_info"},
    {ASTRO_GET_OBJECTS_IMAGE, "astro_get_objects_image"},
    {ASTRO_GET_OBJECTS_OBSERVABILITY, "astro_get_objects_observability"},
    {ASTRO_GET_OBJECTS_RISESET, "astro_get_objects_riseset"}
};

}
