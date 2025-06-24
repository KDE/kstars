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
    GET_STELLARSOLVER_PROFILES,
    GET_DRIVERS,
    GET_DEVICES,
    NEW_CONNECTION_STATE,
    NEW_INDI_STATE,
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
    NEW_SCHEDULER_STATE,

    NEW_MOSAIC_TILES,

    INVOKE_METHOD,
    SET_PROPERTY,
    GET_PROPERTY,

    SET_CLIENT_STATE,
    LOGOUT,
    SESSION_EXPIRED,

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

    // Trains
    TRAIN_GET_ALL,
    TRAIN_GET_PROFILES,
    TRAIN_UPDATE,
    TRAIN_SET,
    TRAIN_ADD,
    TRAIN_DELETE,
    TRAIN_RESET,
    TRAIN_CONFIGURATION_REQUESTED,
    TRAIN_ACCEPT,

    // Train Settings
    TRAIN_SETTINGS_GET,

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
    CAPTURE_CLEAR_SEQUENCES,
    CAPTURE_SAVE_SEQUENCE_FILE,
    CAPTURE_LOAD_SEQUENCE_FILE,
    CAPTURE_SET_ALL_SETTINGS,
    CAPTURE_GET_ALL_SETTINGS,
    CAPTURE_GET_PREVIEW_LABEL,
    CAPTURE_LOOP,
    CAPTURE_GENERATE_DARK_FLATS,

    // Mount
    MOUNT_PARK,
    MOUNT_UNPARK,
    MOUNT_ABORT,
    MOUNT_SYNC_RADE,
    MOUNT_SYNC_TARGET,
    MOUNT_GOTO_RADE,
    MOUNT_GOTO_TARGET,
    MOUNT_GOTO_PIXEL,
    MOUNT_SET_MOTION,
    MOUNT_SET_TRACKING,
    MOUNT_SET_SLEW_RATE,
    MOUNT_CLEAR,
    MOUNT_GET_ALL_SETTINGS,
    MOUNT_SET_ALL_SETTINGS,
    MOUNT_TOGGLE_AUTOPARK,

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
    FOCUS_SET_ALL_SETTINGS,
    FOCUS_GET_ALL_SETTINGS,
    FOCUS_SET_CROSSHAIR,

    // Guide
    GUIDE_START,
    GUIDE_CAPTURE,
    GUIDE_LOOP,
    GUIDE_STOP,
    GUIDE_CLEAR,
    GUIDE_REPORT,
    GUIDE_SET_ALL_SETTINGS,
    GUIDE_GET_ALL_SETTINGS,
    GUIDE_SET_CALIBRATION_SETTINGS,
    // Align
    ALIGN_SOLVE,
    ALIGN_STOP,
    ALIGN_LOAD_AND_SLEW,
    ALIGN_SET_FILE_EXTENSION,
    ALIGN_SET_ALL_SETTINGS,
    ALIGN_GET_ALL_SETTINGS,
    ALIGN_SET_ASTROMETRY_SETTINGS,
    ALIGN_MANUAL_ROTATOR_STATUS,
    ALIGN_MANUAL_ROTATOR_TOGGLE,

    // Scheduler
    SCHEDULER_GET_ALL_SETTINGS,
    SCHEDULER_SET_ALL_SETTINGS,
    SCHEDULER_SAVE_FILE,
    SCHEDULER_SAVE_SEQUENCE_FILE,
    SCHEDULER_LOAD_FILE,
    SCHEDULER_GET_JOBS,
    SCHEDULER_ADD_JOBS,
    SCHEDULER_REMOVE_JOBS,
    SCHEDULER_START_JOB,
    SCHEDULER_IMPORT_MOSAIC,

    // Polar Assistant Helper
    PAH_START,
    PAH_STOP,
    PAH_REFRESH,
    PAH_SET_CROSSHAIR,
    PAH_SELECT_STAR_DONE,
    PAH_REFRESHING_DONE,
    PAH_RESET_VIEW,
    PAH_SLEW_DONE,
    PAH_PAH_SET_ZOOM,
    PAH_SET_ALGORITHM,

    // Options
    OPTION_SET,
    OPTION_GET,

    // Storage Options
    SET_BLOBS,

    // DSLRs
    DSLR_GET_INFO,
    DSLR_SET_INFO,
    DSLR_SET_MODE,

    //DSLR Lens
    DSLR_ADD_LENS,
    DSLR_DELETE_LENS,
    DSLR_UPDATE_LENS,
    GET_DSLR_LENSES,

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
    DEVICE_MESSAGE,

    // Dialogs
    DIALOG_GET_INFO,
    DIALOG_GET_RESPONSE,

    // Filter Manager
    FM_GET_DATA,
    FM_SET_DATA,

    // Astronomy Library
    ASTRO_GET_ALMANC,
    ASTRO_GET_NAMES,
    ASTRO_GET_DESIGNATIONS,
    ASTRO_GET_LOCATION,
    ASTRO_SEARCH_OBJECTS,
    ASTRO_GET_OBJECT_INFO,
    ASTRO_GET_OBJECTS_INFO,
    ASTRO_GET_OBJECTS_IMAGE,
    ASTRO_GET_SKYPOINT_IMAGE,
    ASTRO_GET_OBJECTS_OBSERVABILITY,
    ASTRO_GET_OBJECTS_RISESET,

    // Dark Library
    DARK_LIBRARY_START,
    DARK_LIBRARY_STOP,
    DARK_LIBRARY_SET_CAMERA_PRESETS,
    DARK_LIBRARY_GET_CAMERA_PRESETS,
    DARK_LIBRARY_SET_ALL_SETTINGS,
    DARK_LIBRARY_GET_ALL_SETTINGS,
    DARK_LIBRARY_GET_DEFECT_SETTINGS,
    DARK_LIBRARY_SET_DEFECT_PIXELS,
    DARK_LIBRARY_SAVE_MAP,
    DARK_LIBRARY_GET_VIEW_MASTERS,
    DARK_LIBRARY_GET_MASTERS_IMAGE,
    DARK_LIBRARY_CLEAR_MASTERS_ROW,
    DARK_LIBRARY_SET_DEFECT_FRAME,

    // File Operations
    FILE_DEFAULT_PATH,
    FILE_DIRECTORY_OPERATION,
};

static QMap<COMMANDS, QString> const commands =
{
    {GET_CONNECTION, "get_connection"},
    {GET_STATES, "get_states"},
    {GET_STELLARSOLVER_PROFILES, "get_stellarsolver_profiles"},
    {GET_DRIVERS, "get_drivers"},
    {GET_DEVICES, "get_devices"},
    {NEW_CONNECTION_STATE, "new_connection_state"},
    {NEW_INDI_STATE, "new_indi_state"},
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
    {NEW_SCHEDULER_STATE, "new_scheduler_state"},

    {NEW_MOSAIC_TILES, "new_mosaic_tiles"},

    {INVOKE_METHOD, "invoke_method"},
    {SET_PROPERTY, "set_property"},
    {GET_PROPERTY, "get_property"},

    {SET_CLIENT_STATE, "set_client_state"},
    {LOGOUT, "logout"},
    {SESSION_EXPIRED, "session_expired"},

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

    {TRAIN_GET_ALL, "train_get_all"},
    {TRAIN_GET_PROFILES, "train_get_profiles"},
    {TRAIN_UPDATE, "train_update"},
    {TRAIN_SET, "train_set"},
    {TRAIN_ADD, "train_add"},
    {TRAIN_DELETE, "train_delete"},
    {TRAIN_RESET, "train_reset"},
    {TRAIN_CONFIGURATION_REQUESTED, "train_configuration_requested"},
    {TRAIN_ACCEPT, "train_accept"},
    {TRAIN_SETTINGS_GET, "train_settings_get"},

    {CAPTURE_PREVIEW, "capture_preview"},
    {CAPTURE_TOGGLE_VIDEO, "capture_toggle_video"},
    {CAPTURE_TOGGLE_CAMERA, "capture_toggle_camera"},
    {CAPTURE_TOGGLE_FILTER_WHEEL, "capture_toggle_filter_wheel"},
    {CAPTURE_START, "capture_start"},
    {CAPTURE_STOP, "capture_stop"},
    {CAPTURE_GET_SEQUENCES, "capture_get_sequences"},
    {CAPTURE_ADD_SEQUENCE, "capture_add_sequence"},
    {CAPTURE_REMOVE_SEQUENCE, "capture_remove_sequence"},
    {CAPTURE_CLEAR_SEQUENCES, "capture_clear_sequences"},
    {CAPTURE_SAVE_SEQUENCE_FILE, "capture_save_sequence_file"},
    {CAPTURE_LOAD_SEQUENCE_FILE, "capture_load_sequence_file"},
    {CAPTURE_SET_ALL_SETTINGS, "capture_set_all_settings"},
    {CAPTURE_GET_ALL_SETTINGS, "capture_get_all_settings"},
    {CAPTURE_GET_PREVIEW_LABEL, "capture_get_preview_label"},
    {CAPTURE_LOOP, "capture_loop"},
    {CAPTURE_GENERATE_DARK_FLATS, "capture_generate_dark_flats"},

    {MOUNT_PARK, "mount_park"},
    {MOUNT_UNPARK, "mount_unpark"},
    {MOUNT_ABORT, "mount_abort"},
    {MOUNT_SYNC_RADE, "mount_sync_rade"},
    {MOUNT_SYNC_TARGET, "mount_sync_target"},
    {MOUNT_GOTO_RADE, "mount_goto_rade"},
    {MOUNT_GOTO_TARGET, "mount_goto_target"},
    {MOUNT_GOTO_PIXEL, "mount_goto_pixel"},
    {MOUNT_SET_MOTION, "mount_set_motion"},
    {MOUNT_SET_TRACKING, "mount_set_tracking"},
    {MOUNT_SET_SLEW_RATE, "mount_set_slew_rate"},
    {MOUNT_CLEAR, "mount_clear"},
    {MOUNT_GET_ALL_SETTINGS, "mount_get_all_settings"},
    {MOUNT_SET_ALL_SETTINGS, "mount_set_all_settings"},
    {MOUNT_TOGGLE_AUTOPARK, "mount_toggle_autopark"},

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
    {FOCUS_SET_ALL_SETTINGS, "focus_set_all_settings"},
    {FOCUS_GET_ALL_SETTINGS, "focus_get_all_settings"},
    {FOCUS_SET_CROSSHAIR, "focus_set_crosshair"},

    {SCHEDULER_GET_ALL_SETTINGS, "scheduler_get_all_settings"},
    {SCHEDULER_SET_ALL_SETTINGS, "scheduler_set_all_settings"},
    {SCHEDULER_SAVE_FILE, "scheduler_save_file"},
    {SCHEDULER_SAVE_SEQUENCE_FILE, "scheduler_save_sequence_file"},
    {SCHEDULER_LOAD_FILE, "scheduler_load_file"},
    {SCHEDULER_GET_JOBS, "scheduler_get_jobs"},
    {SCHEDULER_ADD_JOBS, "scheduler_add_jobs"},
    {SCHEDULER_REMOVE_JOBS, "scheduler_remove_jobs"},
    {SCHEDULER_START_JOB, "scheduler_start_job"},
    {SCHEDULER_IMPORT_MOSAIC, "scheduler_import_mosaic"},

    {GUIDE_START, "guide_start"},
    {GUIDE_CAPTURE, "guide_capture"},
    {GUIDE_LOOP, "guide_loop"},
    {GUIDE_STOP, "guide_stop"},
    {GUIDE_CLEAR, "guide_clear"},
    {GUIDE_REPORT, "guide_report"},
    {GUIDE_SET_ALL_SETTINGS, "guide_set_all_settings"},
    {GUIDE_GET_ALL_SETTINGS, "guide_get_all_settings"},
    {GUIDE_SET_CALIBRATION_SETTINGS, "guide_set_calibration_settings"},

    {ALIGN_SOLVE, "align_solve"},
    {ALIGN_STOP, "align_stop"},
    {ALIGN_LOAD_AND_SLEW, "align_load_and_slew"},
    {ALIGN_SET_FILE_EXTENSION, "align_set_file_extension"},
    {ALIGN_SET_ALL_SETTINGS, "align_set_all_settings"},
    {ALIGN_GET_ALL_SETTINGS, "align_get_all_settings"},
    {ALIGN_SET_ASTROMETRY_SETTINGS, "align_set_astrometry_settings"},
    {ALIGN_MANUAL_ROTATOR_STATUS, "align_manual_rotator_status"},
    {ALIGN_MANUAL_ROTATOR_TOGGLE, "align_manual_rotator_toggle"},

    {PAH_START, "polar_start"},
    {PAH_STOP, "polar_stop"},
    {PAH_REFRESH, "polar_refresh"},
    {PAH_SET_CROSSHAIR, "polar_set_crosshair"},
    {PAH_SELECT_STAR_DONE, "polar_star_select_done"},
    {PAH_REFRESHING_DONE, "polar_refreshing_done"},
    {PAH_RESET_VIEW, "polar_reset_view"},
    {PAH_SLEW_DONE, "polar_slew_done"},
    {PAH_PAH_SET_ZOOM, "polar_set_zoom"},
    {PAH_SET_ALGORITHM, "polar_set_algorithm"},

    {OPTION_SET, "option_set"},
    {OPTION_GET, "option_get"},

    {SET_BLOBS, "set_blobs"},

    {DSLR_GET_INFO, "dslr_get_info"},
    {DSLR_SET_INFO, "dslr_set_info"},
    {DSLR_SET_MODE, "dslr_set_mode"},

    {GET_DSLR_LENSES, "get_dslr_lenses"},
    {DSLR_ADD_LENS, "dslr_add_lens"},
    {DSLR_DELETE_LENS, "dslr_delete_lens"},
    {DSLR_UPDATE_LENS, "dslr_update_lens"},

    {DEVICE_GET, "device_get"},
    {DEVICE_RESTART, "device_restart"},
    {DEVICE_BLOB_GET, "device_blob_get"},
    {DEVICE_PROPERTY_GET, "device_property_get"},
    {DEVICE_PROPERTY_SET, "device_property_set"},
    {DEVICE_PROPERTY_ADD, "device_property_add"},
    {DEVICE_PROPERTY_REMOVE, "device_property_remove"},
    {DEVICE_PROPERTY_SUBSCRIBE, "device_property_subscribe"},
    {DEVICE_PROPERTY_UNSUBSCRIBE, "device_property_unsubscribe"},
    {DEVICE_MESSAGE, "device_message"},

    {DIALOG_GET_INFO, "dialog_get_info"},
    {DIALOG_GET_RESPONSE, "dialog_get_response"},

    {FM_GET_DATA, "fm_get_data"},
    {FM_SET_DATA, "fm_set_data"},

    {ASTRO_GET_ALMANC, "astro_get_almanac"},
    {ASTRO_GET_NAMES, "astro_get_names"},
    {ASTRO_GET_DESIGNATIONS, "astro_get_designations"},
    {ASTRO_GET_LOCATION, "astro_get_location"},
    {ASTRO_SEARCH_OBJECTS, "astro_search_objects"},
    {ASTRO_GET_OBJECT_INFO, "astro_get_object_info"},
    {ASTRO_GET_OBJECTS_INFO, "astro_get_objects_info"},
    {ASTRO_GET_OBJECTS_IMAGE, "astro_get_objects_image"},
    {ASTRO_GET_SKYPOINT_IMAGE, "astro_get_skypoint_image"},
    {ASTRO_GET_OBJECTS_OBSERVABILITY, "astro_get_objects_observability"},
    {ASTRO_GET_OBJECTS_RISESET, "astro_get_objects_riseset"},

    {DARK_LIBRARY_START, "dark_library_start"},
    {DARK_LIBRARY_STOP, "dark_library_stop"},
    {DARK_LIBRARY_SET_CAMERA_PRESETS, "dark_library_set_camera_presets"},
    {DARK_LIBRARY_GET_CAMERA_PRESETS, "dark_library_get_camera_presets"},
    {DARK_LIBRARY_SET_ALL_SETTINGS, "dark_library_set_all_settings"},
    {DARK_LIBRARY_GET_ALL_SETTINGS, "dark_library_get_all_settings"},
    {DARK_LIBRARY_GET_DEFECT_SETTINGS, "dark_library_get_defect_settings"},
    {DARK_LIBRARY_SET_DEFECT_PIXELS, "dark_library_set_defect_pixels"},
    {DARK_LIBRARY_SAVE_MAP, "dark_library_save_map"},
    {DARK_LIBRARY_GET_VIEW_MASTERS, "dark_library_get_view_masters"},
    {DARK_LIBRARY_GET_MASTERS_IMAGE, "dark_library_get_masters_image"},
    {DARK_LIBRARY_CLEAR_MASTERS_ROW, "dark_library_clear_masters_row"},
    {DARK_LIBRARY_SET_DEFECT_FRAME, "dark_library_set_defect_frame"},

    {FILE_DEFAULT_PATH, "file_default_path"},
    {FILE_DIRECTORY_OPERATION, "file_directory_operation"},
};

}
