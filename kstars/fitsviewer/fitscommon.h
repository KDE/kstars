/***************************************************************************
                          FITS Common Variables
                             -------------------
    copyright            : (C) 2012 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FITSCOMMON_H
#define FITSCOMMON_H

typedef enum { FITS_NORMAL, FITS_FOCUS, FITS_GUIDE, FITS_CALIBRATE, FITS_WCSM } FITSMode;
typedef enum { FITS_WCS, FITS_VALUE, FITS_POSITION, FITS_ZOOM , FITS_RESOLUTION, FITS_GAMMA, FITS_LED, FITS_MESSAGE } FITSBar;
typedef enum { FITS_NONE, FITS_AUTO_STRETCH, FITS_HIGH_CONTRAST, FITS_EQUALIZE, FITS_HIGH_PASS, FITS_MEDIAN, FITS_ROTATE_CW, FITS_ROTATE_CCW, FITS_FLIP_H, FITS_FLIP_V, FITS_AUTO , FITS_LINEAR, FITS_LOG, FITS_SQRT, FITS_CUSTOM } FITSScale;
typedef enum { ZOOM_FIT_WINDOW, ZOOM_KEEP_LEVEL, ZOOM_FULL } FITSZoom;
typedef enum { HFR_AVERAGE, HFR_MAX } HFRType;

#endif // FITSCOMMON_H
