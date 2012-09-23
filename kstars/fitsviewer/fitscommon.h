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

typedef enum { FITS_NORMAL, FITS_FOCUS, FITS_GUIDE } FITSMode;
typedef enum { FITS_POSITION, FITS_VALUE, FITS_RESOLUTION, FITS_ZOOM, FITS_MESSAGE } FITSBar;
typedef enum { FITS_NONE, FITS_AUTO_STRETCH, FITS_EQUALIZE, FITS_AUTO , FITS_LINEAR, FITS_LOG, FITS_SQRT, FITS_CUSTOM } FITSScale;
typedef enum { ZOOM_FIT_WINDOW, ZOOM_KEEP_LEVEL, ZOOM_FULL } FITSZoom;

#endif // FITSCOMMON_H
