/*
    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flatcalibrationmethod.h"

#include <QtGlobal>

#include <cmath>

namespace Ekos
{

namespace FlatCalibrationMethod
{

Method select(bool skyFlat, bool lightBoxOn, bool brightnessControllable)
{
    if (skyFlat == false && lightBoxOn && brightnessControllable)
        return METHOD_BRIGHTNESS;

    return METHOD_EXPOSURE;
}

bool fallsBackToExposure(FlatBrightnessCalibration::Result result)
{
    switch (result)
    {
        case FlatBrightnessCalibration::RESULT_FAILED_MINIMUM:
        case FlatBrightnessCalibration::RESULT_FAILED_MAXIMUM:
        case FlatBrightnessCalibration::RESULT_FAILED_RESOLUTION:
        case FlatBrightnessCalibration::RESULT_FAILED_MAX_ITERATIONS:
            return true;

        default:
            return false;
    }
}

double fallbackExposure(double exposure, double measuredADU, bool saturated, double targetADU, double minExposure,
                        double maxExposure)
{
    double next = exposure;

    if (saturated)
        next = exposure * SATURATED_EXPOSURE_FACTOR;
    else if (measuredADU > 0 && targetADU > 0 && std::isfinite(measuredADU) && std::isfinite(targetADU))
        next = exposure * qMin(targetADU / measuredADU, MAX_FALLBACK_EXPOSURE_FACTOR);

    return qBound(minExposure, next, maxExposure);
}

FrameCheck checkFrameBrightness(int brightnessAtStart, int brightnessNow, int retakesSoFar)
{
    if (brightnessAtStart < 0 || brightnessNow == brightnessAtStart)
        return FRAME_USE;

    return retakesSoFar < MAX_BRIGHTNESS_RETAKES ? FRAME_RETAKE : FRAME_GIVE_UP;
}

ExposureLimit exposureLimit(double exposure, double nextExposure, double minExposure, double maxExposure)
{
    // The exposure of the job was clamped to the limit before, so it equals the limit
    // apart from rounding.
    constexpr double epsilon = 1e-9;

    if (nextExposure < minExposure && exposure <= minExposure * (1 + epsilon))
        return LIMIT_MINIMUM;
    if (nextExposure > maxExposure && exposure >= maxExposure * (1 - epsilon))
        return LIMIT_MAXIMUM;

    return LIMIT_NONE;
}

double autoTargetADU(uint32_t bitsPerPixel)
{
    switch (bitsPerPixel)
    {
        case 8:
            return std::floor(UINT8_MAX * AUTO_TARGET_FRACTION);
        case 16:
            return std::floor(UINT16_MAX * AUTO_TARGET_FRACTION);
        default:
            return 0;
    }
}

double autoTolerance(double targetADU)
{
    return std::round(targetADU * AUTO_TOLERANCE_FRACTION);
}

void StallDetector::reset()
{
    m_Exposure = -1;
    m_ADU = -1;
    m_StalledSteps = 0;
}

bool StallDetector::update(double exposure, double measuredADU, double targetADU)
{
    if (exposure <= 0 || measuredADU <= 0 || targetADU <= 0 || std::isfinite(exposure) == false
            || std::isfinite(measuredADU) == false || std::isfinite(targetADU) == false)
    {
        reset();
        return false;
    }

    if (m_Exposure > 0)
    {
        const double exposureChange = (exposure - m_Exposure) / m_Exposure;
        const double aduChange = (measuredADU - m_ADU) / m_ADU;
        const double previousDistance = std::fabs(m_ADU - targetADU);
        const double progress = previousDistance > 0 ?
                                (previousDistance - std::fabs(measuredADU - targetADU)) / previousDistance : 0;

        // Small steps near the target are left alone, only a considerable change of
        // the exposure says something about the response. An ADU that moves against
        // the exposure does not follow it either.
        if (std::fabs(exposureChange) >= MIN_EXPOSURE_CHANGE)
        {
            if (aduChange / exposureChange < MIN_RESPONSE && progress < MIN_PROGRESS)
                m_StalledSteps++;
            else
                m_StalledSteps = 0;
        }
    }

    m_Exposure = exposure;
    m_ADU = measuredADU;

    return m_StalledSteps >= STALLED_STEPS;
}

} // namespace FlatCalibrationMethod

} // namespace Ekos
