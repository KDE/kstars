/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bayerparameters.h"

#include <KLocalizedString>

namespace BayerUtils
{
// Engine helpers
QString debayerEngineToString(DebayerEngine engine)
{
    switch (engine)
    {
        case DebayerEngine::DC1394:
            return i18nc("@item:inlistbox Debayer engine", "DC1394");
        case DebayerEngine::OpenCV:
            return i18nc("@item:inlistbox Debayer engine", "OpenCV");
        default:
            break;
    }
    return i18nc("@item:inlistbox Debayer engine", "Unknown");
}

QString debayerEngineToolTip()
{
    return i18nc("@info:tooltip",
                 "<b>DC1394</b>: Uses the legacy libdc1394 debayer engine<br/>"
                 "• Slower but very reliable<br/><br/>"
                 "<b>OpenCV</b>: Experimental OpenCV-based debayer engine<br/>"
                 "• Faster than libdc1394<br/>"
                 "• Better edge handling");
}


// OpenCV helpers
QString openCVAlgoToString(OpenCVAlgo algo)
{
    switch (algo)
    {
        case OpenCVAlgo::Bilinear:
            return i18nc("@item:inlistbox OpenCV debayer algorithm", "Bilinear");
        case OpenCVAlgo::VNG:
            return i18nc("@item:inlistbox OpenCV debayer algorithm", "VNG");
        case OpenCVAlgo::EA:
            return i18nc("@item:inlistbox OpenCV debayer algorithm", "Edge-Aware");
        default:
            break;
    }
    return i18nc("@item:inlistbox OpenCV debayer algorithm", "Unknown");
}

QString openCVAlgoToolTip()
{
    return i18nc("@info:tooltip",
                 "<b>Bilinear</b>: Simple bilinear interpolation<br/>"
                 "• Very fast, soft edges<br/><br/>"
                 "<b>VNG</b>: Variable Number of Gradients<br/>"
                 "• Improved color reconstruction<br/>"
                 "• 8-bit images only<br/><br/>"
                 "<b>Edge-Aware</b>: Edge-aware interpolation<br/>"
                 "• High quality and very fast");
}

// DC1394 helpers
QString dc1394MethodToString(DC1394DebayerMethod method)
{
    switch (method)
    {
        case DC1394DebayerMethod::Nearest:
            return i18nc("@item:inlistbox DC1394 debayer method", "Nearest");
        case DC1394DebayerMethod::Simple:
            return i18nc("@item:inlistbox DC1394 debayer method", "Simple");
        case DC1394DebayerMethod::Bilinear:
            return i18nc("@item:inlistbox DC1394 debayer method", "Bilinear");
        case DC1394DebayerMethod::HQLinear:
            return i18nc("@item:inlistbox DC1394 debayer method", "HQ Linear");
        case DC1394DebayerMethod::EdgeSense:
            return i18nc("@item:inlistbox DC1394 debayer method", "Edge Sense");
        case DC1394DebayerMethod::VNG:
            return i18nc("@item:inlistbox DC1394 debayer method", "VNG");
        default:
            break;
    }
    return i18nc("@item:inlistbox DC1394 debayer method", "Unknown");
}

QString dc1394MethodToolTip()
{
    return i18nc("@info:tooltip",
                 "<b>Nearest</b>: Fastest method, lowest quality<br/><br/>"
                 "<b>Simple</b>: Basic interpolation, slightly smoothed<br/><br/>"
                 "<b>Bilinear</b>: Good balance of speed and quality<br/><br/>"
                 "<b>HQ Linear</b>: Higher quality, slower<br/><br/>"
                 "<b>Edge Sense</b>: Preserves edges, high quality<br/><br/>"
                 "<b>VNG</b>: Highest quality, slowest");
}

// Bayer pattern helpers
QString bayerPatternToString(BayerPattern pattern)
{
    switch (pattern)
    {
        case BayerPattern::RGGB:
            return QLatin1String("RGGB");
        case BayerPattern::GRBG:
            return QLatin1String("GRBG");
        case BayerPattern::GBRG:
            return QLatin1String("GBRG");
        case BayerPattern::BGGR:
            return QLatin1String("BGGR");
        default:
            break;
    }
    return i18n("RGGB");
}

BayerPattern bayerPatternFromStr(const QString &str)
{
    if (str == QLatin1String("RGGB")) return BayerPattern::RGGB;
    if (str == QLatin1String("GRBG")) return BayerPattern::GRBG;
    if (str == QLatin1String("GBRG")) return BayerPattern::GBRG;
    if (str == QLatin1String("BGGR")) return BayerPattern::BGGR;
    return BayerPattern::RGGB;
}

bool bayerPatternValid(const QString &str)
{
    return str == QLatin1String("RGGB") ||
           str == QLatin1String("GRBG") ||
           str == QLatin1String("GBRG") ||
           str == QLatin1String("BGGR");
}

bool verifyDC1394DebayerParams(const BayerParameters &bayerParams, DC1394Params &dc1394Params)
{
    if (bayerParams.engine != DebayerEngine::DC1394)
        return false;

    if (!bayerParams.params.canConvert<DC1394Params>())
        return false;

    dc1394Params = bayerParams.params.value<DC1394Params>();
    return true;
}

bool verifyCVDebayerParams(const BayerParameters &bayerParams, OpenCVParams &openCVParams)
{
    if (bayerParams.engine != DebayerEngine::OpenCV)
        return false;

    if (!bayerParams.params.canConvert<OpenCVParams>())
        return false;

    openCVParams = bayerParams.params.value<OpenCVParams>();
    return true;
}

dc1394bayer_method_t convertDC1394Method(DC1394DebayerMethod method)
{
    switch (method)
    {
        case DC1394DebayerMethod::Nearest:
            return DC1394_BAYER_METHOD_NEAREST;
        case DC1394DebayerMethod::Simple:
            return DC1394_BAYER_METHOD_SIMPLE;
        case DC1394DebayerMethod::Bilinear:
            return DC1394_BAYER_METHOD_BILINEAR;
        case DC1394DebayerMethod::HQLinear:
            return DC1394_BAYER_METHOD_HQLINEAR;
        case DC1394DebayerMethod::EdgeSense:
            return DC1394_BAYER_METHOD_EDGESENSE;
        case DC1394DebayerMethod::VNG:
            return DC1394_BAYER_METHOD_VNG;
        default:
            break;
    }
    return DC1394_BAYER_METHOD_NEAREST;
}

QString convertDC1394MethodToStr(dc1394bayer_method_t method)
{
    switch (method)
    {
        case DC1394_BAYER_METHOD_NEAREST:
            return BayerUtils::dc1394MethodToString(DC1394DebayerMethod::Nearest);
        case DC1394_BAYER_METHOD_SIMPLE:
            return BayerUtils::dc1394MethodToString(DC1394DebayerMethod::Simple);
        case DC1394_BAYER_METHOD_BILINEAR:
            return BayerUtils::dc1394MethodToString(DC1394DebayerMethod::Bilinear);
        case DC1394_BAYER_METHOD_HQLINEAR:
            return BayerUtils::dc1394MethodToString(DC1394DebayerMethod::HQLinear);
        case DC1394_BAYER_METHOD_EDGESENSE:
            return BayerUtils::dc1394MethodToString(DC1394DebayerMethod::EdgeSense);
        case DC1394_BAYER_METHOD_VNG:
            return BayerUtils::dc1394MethodToString(DC1394DebayerMethod::VNG);
        default:
            return i18nc("@item:inlistbox DC1394 debayer method", "Unknown");
    }
}

dc1394color_filter_t convertDC1394Filter(BayerPattern filter)
{
    switch (filter)
    {
        case BayerPattern::RGGB:
            return DC1394_COLOR_FILTER_RGGB;
        case BayerPattern::GRBG:
            return DC1394_COLOR_FILTER_GRBG;
        case BayerPattern::GBRG:
            return DC1394_COLOR_FILTER_GBRG;
        case BayerPattern::BGGR:
            return DC1394_COLOR_FILTER_BGGR;
        default:
            break;
    }
    return DC1394_COLOR_FILTER_RGGB;
}

BayerPattern convertDC1394Filter_t(dc1394color_filter_t filter)
{
    switch (filter)
    {
        case DC1394_COLOR_FILTER_RGGB:
            return BayerPattern::RGGB;
        case DC1394_COLOR_FILTER_GRBG:
            return BayerPattern::GRBG;
        case DC1394_COLOR_FILTER_GBRG:
            return BayerPattern::GBRG;
        case DC1394_COLOR_FILTER_BGGR:
            return BayerPattern::BGGR;
        default:
            break;
    }
    return BayerPattern::RGGB;
}

} // namespace BayerUtils
