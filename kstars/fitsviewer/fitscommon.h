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

typedef enum { FITS_NORMAL, FITS_FOCUS } FITSMode;
typedef enum { FITS_POSITION, FITS_VALUE, FITS_RESOLUTION, FITS_ZOOM, FITS_MESSAGE } FITSBar;
typedef enum { FITSAuto = 0 , FITSLinear, FITSLog, FITSSqrt, FITSLowPass, FITSEqualize, FITSCustom } FITSScale;
typedef enum { ZOOM_FIT_WINDOW, ZOOM_KEEP_LEVEL, ZOOM_FULL } FITSZoom;

#endif // FITSCOMMON_H
