/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <KLocalizedString>

typedef enum { FITS_NORMAL, FITS_FOCUS, FITS_GUIDE, FITS_CALIBRATE, FITS_ALIGN } FITSMode;

// Focus States
static const QStringList FITSModes =   { I18N_NOOP("Normal"),  I18N_NOOP("Focus"), I18N_NOOP("Guide"),
                                         I18N_NOOP("Calibrate"), I18N_NOOP("Align")
                                       };

const QString &getFITSModeStringString(FITSMode mode);

typedef enum { FITS_HFR, FITS_WCS, FITS_VALUE, FITS_POSITION, FITS_ZOOM, FITS_RESOLUTION, FITS_LED, FITS_MESSAGE} FITSBar;

typedef enum
{
    FITS_NONE,
    FITS_AUTO_STRETCH,
    FITS_HIGH_CONTRAST,
    FITS_EQUALIZE,
    FITS_HIGH_PASS,
    FITS_MEDIAN,
    FITS_GAUSSIAN,
    FITS_ROTATE_CW,
    FITS_ROTATE_CCW,
    FITS_FLIP_H,
    FITS_FLIP_V,
    FITS_AUTO,
    FITS_LINEAR,
    FITS_LOG,
    FITS_SQRT,
    FITS_CUSTOM
} FITSScale;

typedef enum { ZOOM_FIT_WINDOW, ZOOM_KEEP_LEVEL, ZOOM_FULL } FITSZoom;

typedef enum { HFR_AVERAGE, HFR_MEDIAN, HFR_HIGH, HFR_MAX } HFRType;

typedef enum { ALGORITHM_GRADIENT, ALGORITHM_CENTROID, ALGORITHM_THRESHOLD, ALGORITHM_SEP, ALGORITHM_BAHTINOV } StarAlgorithm;

typedef enum { RED_CHANNEL, GREEN_CHANNEL, BLUE_CHANNEL } ColorChannels;
