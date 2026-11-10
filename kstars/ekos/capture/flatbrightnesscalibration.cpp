/*
    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flatbrightnesscalibration.h"

#include <QtGlobal>

#include <cmath>

namespace Ekos
{

int FlatBrightnessCalibration::searchMinimum(int minBrightness, int maxBrightness)
{
    // Brightness 0 switches some light boxes off, and some only switch on again when
    // their light is turned on, so the search never asks for it.
    return (minBrightness == 0 && maxBrightness > 1) ? 1 : minBrightness;
}

bool FlatBrightnessCalibration::start(int minBrightness, int maxBrightness, double targetADU,
                                      double tolerance, int maxIterations)
{
    reset();

    // Nothing can be achieved without a usable range or a usable target.
    if (maxBrightness <= minBrightness || targetADU <= 0 || maxIterations < 1)
        return false;

    m_DeviceMin     = minBrightness;
    m_Min           = searchMinimum(minBrightness, maxBrightness);
    m_Max           = maxBrightness;
    m_Target        = targetADU;
    m_Tolerance     = qMax(0.0, tolerance);
    m_MaxIterations = maxIterations;
    m_Active        = true;

    return true;
}

void FlatBrightnessCalibration::reset()
{
    m_Active         = false;
    m_Iterations     = 0;
    m_Attempts       = 0;
    m_NextBrightness = 0;
    m_HasRequested   = false;
    m_LastMeasuredBrightness = -1;
    m_HasLow         = false;
    m_HasHigh        = false;
    m_LowBrightness  = 0;
    m_HighBrightness = 0;
    m_LowADU         = 0;
    m_HighADU        = 0;
    m_HighSaturated  = false;
    m_DimmestBrightness = -1;
    m_DimmestADU     = 0;
    m_Reached        = false;
    m_ChecksLight    = false;
    m_LightCheckMeasured   = false;
    m_LightChecked   = false;
    m_MaxADU         = 0;
}

int FlatBrightnessCalibration::belowBrightness() const
{
    return m_HasLow ? m_LowBrightness : -1;
}

double FlatBrightnessCalibration::belowADU() const
{
    return m_HasLow ? m_LowADU : 0;
}

int FlatBrightnessCalibration::aboveBrightness() const
{
    return m_HasHigh ? m_HighBrightness : -1;
}

double FlatBrightnessCalibration::aboveADU() const
{
    return m_HasHigh ? m_HighADU : 0;
}

int FlatBrightnessCalibration::fallbackBrightness() const
{
    return m_HasHigh ? aboveBrightness() : belowBrightness();
}

double FlatBrightnessCalibration::fallbackADU() const
{
    return m_HasHigh ? aboveADU() : belowADU();
}

bool FlatBrightnessCalibration::fallbackSaturated() const
{
    return m_HasHigh && m_HighSaturated;
}

void FlatBrightnessCalibration::restartAfterDrift()
{
    // Keep the configuration and the iteration budget of the current search, which
    // was reset when the target was reached, but forget every measurement.
    m_HasRequested   = false;
    m_LastMeasuredBrightness = -1;
    m_HasLow         = false;
    m_HasHigh        = false;
    m_DimmestBrightness = -1;
    m_Reached        = false;
    m_ChecksLight    = false;
    m_LightChecked   = false;
}

int FlatBrightnessCalibration::quantize(double value) const
{
    if (!std::isfinite(value))
        return m_Min;

    // Brightness is an integral quantity, so round to the nearest value the
    // interface can carry. It is deliberately not snapped onto the increment the
    // device advertises: see start() for why.
    const double rounded = std::round(value);

    if (rounded <= m_Min)
        return m_Min;
    if (rounded >= m_Max)
        return m_Max;

    return static_cast<int>(rounded);
}

int FlatBrightnessCalibration::stepDown(int from) const
{
    return qMax(m_Min, from - 1);
}

int FlatBrightnessCalibration::stepUp(int from) const
{
    return qMin(m_Max, from + 1);
}

FlatBrightnessCalibration::Result FlatBrightnessCalibration::endAtMaximum(double measuredADU)
{
    // Still too dark at the maximum. A light box without light only shows the bias of the
    // camera, which a frame at a much lower brightness reveals. Without one, e.g. when the
    // search started at a high preset, take it at the search minimum before the exposure
    // is lengthened in vain.
    if (!m_LightChecked && m_Max >= NO_SIGNAL_BRIGHTNESS_FACTOR * m_Min
            && (m_DimmestBrightness < 0 || NO_SIGNAL_BRIGHTNESS_FACTOR * m_DimmestBrightness > m_Max))
    {
        m_ChecksLight    = true;
        m_LightChecked   = true;
        m_MaxADU         = measuredADU;
        m_NextBrightness = m_Min;
        m_HasRequested   = true;
        return RESULT_CONTINUE;
    }

    return RESULT_FAILED_MAXIMUM;
}

FlatBrightnessCalibration::Result FlatBrightnessCalibration::checkLight(int currentBrightness, double measuredADU,
        bool saturated)
{
    m_ChecksLight = false;
    m_LightCheckMeasured = true;

    // The frame is only compared with the one at the maximum, the search itself ended
    // there. A light box that did not go low enough says nothing about the light, nor
    // does a frame that is brighter than the target although the one at the maximum was
    // too dark. The exposure is then adjusted as it would have been without this frame.
    // Without light, noise can make this frame a little brighter than the one at the
    // maximum, which still counts as no rise.
    const bool usable = !saturated && std::isfinite(measuredADU) && measuredADU > 0
                        && measuredADU <= m_Target
                        && NO_SIGNAL_BRIGHTNESS_FACTOR * currentBrightness <= m_Max;
    if (usable && m_MaxADU < measuredADU * (1 + NO_SIGNAL_ADU_RISE))
        return RESULT_FAILED_INVALID_ADU;

    return RESULT_FAILED_MAXIMUM;
}

FlatBrightnessCalibration::Result FlatBrightnessCalibration::update(int currentBrightness, double measuredADU,
        bool saturated)
{
    if (!m_Active)
        return RESULT_FAILED_NOT_CONFIGURED;

    // The device may report anything, so never trust the incoming value blindly. A frame
    // below the search minimum, e.g. at 0 when the light box was there already, is kept
    // at its real brightness: it describes that brightness, not the minimum.
    currentBrightness = qBound(m_DeviceMin, currentBrightness, m_Max);

    m_Iterations++;
    m_Attempts++;
    m_LightCheckMeasured = false;

    // The frame at the search minimum that checks for light after the search ended at the
    // maximum, @see endAtMaximum().
    if (m_ChecksLight)
        return checkLight(currentBrightness, measuredADU, saturated);

    // A frame without any signal cannot be classified by its ADU. A saturated
    // frame is by definition too bright, whatever its ADU happens to read.
    const bool noSignal  = !saturated && (!std::isfinite(measuredADU) || measuredADU <= 0.0);
    const bool tooBright = saturated || (!noSignal && measuredADU > m_Target);
    const bool onTarget  = !saturated && !noSignal && std::fabs(measuredADU - m_Target) <= m_Tolerance;

    // The target was reached before and this frame is off target: the light source
    // has drifted. Everything measured so far describes the light before the drift,
    // and combining it with this frame can leave no brightness to try, so search
    // again starting from this frame.
    if (m_Reached && !onTarget)
        restartAfterDrift();

    // 1. Are we done? A saturated frame is never acceptable, not even when its
    //    clipped ADU accidentally falls inside the tolerance band.
    if (onTarget)
    {
        // Frames that are on target do not consume the iteration budget.
        m_Attempts = 0;
        m_LastMeasuredBrightness = currentBrightness;
        m_Reached = true;
        return RESULT_TARGET_REACHED;
    }

    // A device that ignored the last request and stayed exactly where it was cannot
    // be driven any further, so stop instead of asking for the same value forever.
    // This is what a device that really only accepts a coarse grid looks like from
    // the outside.
    if (m_HasRequested && currentBrightness != m_NextBrightness
            && currentBrightness == m_LastMeasuredBrightness)
        return RESULT_FAILED_RESOLUTION;

    m_LastMeasuredBrightness = currentBrightness;

    // 2. Update the bracket. A frame without signal counts as too dark.
    if (tooBright)
    {
        if (!m_HasHigh || currentBrightness < m_HighBrightness)
        {
            m_HighBrightness = currentBrightness;
            m_HighADU        = measuredADU;
            m_HighSaturated  = saturated;
            m_HasHigh        = true;
        }
    }
    else
    {
        if (!m_HasLow || currentBrightness > m_LowBrightness)
        {
            m_LowBrightness = currentBrightness;
            m_LowADU        = noSignal ? 0.0 : measuredADU;
            m_HasLow        = true;
        }
    }

    // Measurement noise or a changing light source can produce a bracket that
    // contradicts itself. Trust the most recent measurement and rebuild the
    // bracket from there instead of interpolating over nonsense.
    if (m_HasLow && m_HasHigh && m_LowBrightness >= m_HighBrightness)
    {
        if (tooBright)
            m_HasLow = false;
        else
            m_HasHigh = false;
    }

    // 3. No signal at all while the light source is already at its maximum means
    //    there is nothing this loop can do. With a real camera the bias keeps the ADU
    //    above 0, so a light box without light shows as an ADU that hardly rose although
    //    the brightness went up considerably.
    if (noSignal && currentBrightness >= m_Max)
        return RESULT_FAILED_INVALID_ADU;
    if (!tooBright && !noSignal && currentBrightness >= m_Max && m_DimmestBrightness >= 0
            && currentBrightness >= NO_SIGNAL_BRIGHTNESS_FACTOR * m_DimmestBrightness
            && measuredADU < m_DimmestADU * (1 + NO_SIGNAL_ADU_RISE))
        return RESULT_FAILED_INVALID_ADU;

    // Remember the frame at the lowest brightness for that comparison.
    if (!saturated && !noSignal && (m_DimmestBrightness < 0 || currentBrightness < m_DimmestBrightness))
    {
        m_DimmestBrightness = currentBrightness;
        m_DimmestADU        = measuredADU;
    }

    // 4. Do not iterate forever.
    if (m_Attempts >= m_MaxIterations)
        return RESULT_FAILED_MAX_ITERATIONS;

    int next = currentBrightness;

    if (m_HasLow && m_HasHigh)
    {
        // The target is bracketed. Interpolate between the two bounds, but never
        // leave the open bracket: that is what guarantees progress in the presence
        // of a non linear response, of overshoot and of noise.
        double candidate = 0.5 * (m_LowBrightness + m_HighBrightness);

        const double aduSpan = m_HighADU - m_LowADU;
        if (aduSpan > 0)
        {
            const double interpolated = m_LowBrightness +
                                        (m_Target - m_LowADU) * (m_HighBrightness - m_LowBrightness) / aduSpan;
            // Only trust the interpolation while it stays inside the bracket.
            if (interpolated > m_LowBrightness && interpolated < m_HighBrightness)
                candidate = interpolated;
        }

        next = quantize(candidate);
        if (next <= m_LowBrightness)
            next = stepUp(m_LowBrightness);
        if (next >= m_HighBrightness)
            next = stepDown(m_HighBrightness);

        // Nothing left between the two bounds: the device simply cannot be set any
        // more precisely than this.
        if (next <= m_LowBrightness || next >= m_HighBrightness)
            return RESULT_FAILED_RESOLUTION;
    }
    else if (tooBright)
    {
        // Every measurement so far is above the target, we have to go down.
        if (currentBrightness <= m_Min)
            return RESULT_FAILED_MINIMUM;

        double candidate;
        if (saturated || measuredADU <= 0)
            // A saturated reading carries no scale information, so halve the
            // distance to the minimum instead of trusting the ADU.
            candidate = m_Min + 0.5 * (currentBrightness - m_Min);
        else
            // Proportional first estimate.
            candidate = currentBrightness * m_Target / measuredADU;

        next = quantize(candidate);
        if (next >= currentBrightness)
            next = stepDown(currentBrightness);
        if (next >= currentBrightness)
            return RESULT_FAILED_MINIMUM;
    }
    else
    {
        // Every measurement so far is below the target, we have to go up.
        if (currentBrightness >= m_Max)
            return endAtMaximum(measuredADU);

        double candidate;
        if (noSignal)
            // Without signal there is nothing to scale, so bisect towards the
            // maximum until the frame shows something.
            candidate = currentBrightness + 0.5 * (m_Max - currentBrightness);
        else
            // Proportional first estimate.
            candidate = currentBrightness * m_Target / measuredADU;

        next = quantize(candidate);
        if (next <= currentBrightness)
            next = stepUp(currentBrightness);
        if (next <= currentBrightness)
            return RESULT_FAILED_MAXIMUM;
    }

    m_NextBrightness = next;
    m_HasRequested   = true;
    return RESULT_CONTINUE;
}

QString FlatBrightnessCalibration::resultString(Result result)
{
    switch (result)
    {
        case RESULT_TARGET_REACHED:
            return "target reached";
        case RESULT_CONTINUE:
            return "continue";
        case RESULT_FAILED_NOT_CONFIGURED:
            return "not configured";
        case RESULT_FAILED_MINIMUM:
            return "minimum brightness";
        case RESULT_FAILED_MAXIMUM:
            return "maximum brightness";
        case RESULT_FAILED_INVALID_ADU:
            return "invalid ADU";
        case RESULT_FAILED_RESOLUTION:
            return "brightness resolution exhausted";
        case RESULT_FAILED_MAX_ITERATIONS:
            return "maximum iterations";
    }

    return "unknown";
}

} // namespace Ekos
