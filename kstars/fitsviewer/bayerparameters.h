/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVariant>
#include <QString>

#include "bayer.h"

// bayerparameters.h encapsulates using dc1394 and openCV debayering engines
// Each uses similar but different parameters. This file defines the datatypes
// and helper functions used for debayering.

enum class DebayerEngine
{
    DC1394,
    OpenCV,
    MAX_ITEMS
};

enum class OpenCVAlgo
{
    Bilinear,
    VNG,
    EA,
    MAX_ITEMS
};

enum class BayerPattern
{
    RGGB,
    GRBG,
    GBRG,
    BGGR,
    MAX_ITEMS
};

// Defining a new enum (like dc1394bayer_method_t in bayer.h) but restricting the options.
enum class DC1394DebayerMethod
{
    Nearest,
    Simple,
    Bilinear,
    HQLinear,
    // No Downsample - isn't a debayer method
    EdgeSense,
    VNG,
    // No AHD - crashes
    MAX_ITEMS
};

struct DC1394Params
{
    BayerParams params;
};

struct OpenCVParams
{
    OpenCVAlgo   algo;
    BayerPattern pattern;
    int offsetX { 0 };
    int offsetY { 0 };
};

struct BayerParameters
{
    DebayerEngine engine;
    QVariant params;
};

// Utility helpers
namespace BayerUtils
{
// UI Engine helpers
QString debayerEngineToString(DebayerEngine engine);
QString debayerEngineToolTip();

// UI OpenCV helpers
QString openCVAlgoToString(OpenCVAlgo algo);
QString openCVAlgoToolTip();

// UI DC1394 helpers
QString dc1394MethodToString(DC1394DebayerMethod method);
QString dc1394MethodToolTip();

// Verify if the passed in bayerParams are consistent for dc1394
bool verifyDC1394DebayerParams(const BayerParameters &bayerParams, DC1394Params &dc1394Params);

// Verify if the passed in bayerParams are consistent for openCV
bool verifyCVDebayerParams(const BayerParameters &bayerParams, OpenCVParams &openCVParams);

// Get the BayerPattern enum from the associated string
BayerPattern bayerPatternFromStr(const QString &str);

// Get bayer pattern string from enum
QString bayerPatternToString(BayerPattern pattern);

// Determine whether the passed in string is a valid bayer pattern
bool bayerPatternValid(const QString &str);

// Map UI enum to libdc1394 debayer method
dc1394bayer_method_t convertDC1394Method(DC1394DebayerMethod method);

// Map libdc1394 debayer method to UI string
QString convertDC1394MethodToStr(dc1394bayer_method_t method);

// Map UI enum to libdc1394 color filter
dc1394color_filter_t convertDC1394Filter(BayerPattern filter);

// Convert lib1394 debayer filter to UI enum
BayerPattern convertDC1394Filter_t(dc1394color_filter_t filter);

} // namespace BayerUtils

Q_DECLARE_METATYPE(DebayerEngine)
Q_DECLARE_METATYPE(OpenCVAlgo)
Q_DECLARE_METATYPE(BayerPattern)
Q_DECLARE_METATYPE(DC1394DebayerMethod)
Q_DECLARE_METATYPE(DC1394Params)
Q_DECLARE_METATYPE(OpenCVParams)
Q_DECLARE_METATYPE(dc1394bayer_method_t)
