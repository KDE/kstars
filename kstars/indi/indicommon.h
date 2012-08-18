/*  INDI Common Defs
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDICOMMON_H
#define INDICOMMON_H

#define	INDIVERSION	1.7	/* we support this or less */

typedef enum { PRIMARY_XML, THIRD_PARTY_XML, EM_XML, HOST_SOURCE } DriverSource;

typedef enum { SERVER_CLIENT, SERVER_ONLY} ServerMode;

typedef enum { DATA_FITS, DATA_VIDEO, DATA_CCDPREVIEW, DATA_ASCII, DATA_OTHER } INDIDataTypes ;


/* GUI layout */
#define PROPERTY_LABEL_WIDTH	100
#define ELEMENT_LABEL_WIDTH	175
#define ELEMENT_READ_WIDTH	175
#define ELEMENT_WRITE_WIDTH	175
#define ELEMENT_FULL_WIDTH	340
#define MIN_SET_WIDTH		50
#define MAX_SET_WIDTH		110
#define MAXINDINAME		32
#define MED_INDI_FONT		2
#define MAX_LABEL_LENGTH	20

// Pulse tracking
#define INDI_PULSE_TRACKING   15000

typedef enum {PG_NONE = 0, PG_TEXT, PG_NUMERIC, PG_BUTTONS,
              PG_RADIO, PG_MENU, PG_LIGHTS, PG_BLOB} PGui;

/* new versions of glibc define TIME_UTC as a macro */
#undef TIME_UTC

/* INDI std properties */
/* N.B. Need to modify corresponding entry in indidevice.cpp when changed */
enum stdProperties { CONNECTION, DEVICE_PORT, TIME_UTC, TIME_LST, TIME_UTC_OFFSET, GEOGRAPHIC_COORD,   /* General */
                     EQUATORIAL_COORD, EQUATORIAL_EOD_COORD, EQUATORIAL_EOD_COORD_REQUEST, HORIZONTAL_COORD,  /* Telescope */
                     TELESCOPE_ABORT_MOTION, ON_COORD_SET, SOLAR_SYSTEM, TELESCOPE_MOTION_NS, /* Telescope */
                     TELESCOPE_MOTION_WE, TELESCOPE_PARK,  /* Telescope */
                     CCD_EXPOSURE_REQUEST, CCD_TEMPERATURE_REQUEST, CCD_FRAME,           /* CCD */
                     CCD_FRAME_TYPE, CCD_BINNING, CCD_INFO,
                     VIDEO_STREAM,						/* Video */
                     FOCUS_SPEED, FOCUS_MOTION, FOCUS_TIMER,			/* Focuser */
                     FILTER_SLOT};						/* Filter */

/* Devices families that we explicitly support (i.e. with std properties) */
typedef enum { KSTARS_TELESCOPE, KSTARS_CCD, KSTARS_FILTER, KSTARS_VIDEO, KSTARS_FOCUSER, KSTARS_DOME, KSTARS_RECEIVERS, KSTARS_GPS, KSTARS_UNKNOWN } DeviceFamily;

typedef enum  { FRAME_LIGHT,FRAME_BIAS, FRAME_DARK,FRAME_FLAT} CCDFrameType;

typedef enum  { SINGLE_BIN, DOUBLE_BIN, TRIPLE_BIN,QUADRAPLE_BIN} CCDBinType;

typedef enum { INDI_SEND_COORDS, INDI_ENGAGE_TRACKING, INDI_SET_PORT, INDI_CONNECT, INDI_DISCONNECT, INDI_CAPTURE, INDI_SET_FILTER,
               INDI_FOCUS_IN, INDI_FOCUS_OUT, INDI_FOCUS_MOVE, INDI_CCD_FRAME, INDI_CCD_BINNING } DeviceCommand;


#endif // INDICOMMON_H
