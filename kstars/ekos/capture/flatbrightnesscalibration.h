/*
    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace Ekos
{

/**
 * @class FlatBrightnessCalibration
 * @short Bounded feedback loop that drives the brightness of a dimmable light
 * source towards a target ADU while the camera exposure remains fixed.
 *
 * The class is deliberately free of any device, driver, vendor or Qt/INDI
 * dependency: it is fed the measured ADU of the frame that was just captured and
 * it returns the next brightness value that should be requested. Which device is
 * used, which property carries the brightness and how the hardware realizes a
 * given brightness are none of its business - that is entirely up to the INDI
 * driver behind the generic light box interface.
 *
 * Note that there is no exposure time anywhere in this interface. In brightness
 * control mode the camera exposure is a constant chosen by the user and it is
 * structurally impossible for this algorithm to change it.
 *
 * The loop makes no assumption about a linear relation between brightness and
 * ADU. It starts with a proportional estimate and, as soon as the target is
 * bracketed by two measurements, continues with an interpolation that is
 * constrained to stay inside the bracket. That keeps it convergent for any
 * monotonic response, and it degenerates into a plain bisection for responses
 * that are strongly non linear.
 *
 * @author J.D. de Keijzer
 */
class FlatBrightnessCalibration
{
    public:
        /**
         * @brief Outcome of feeding a new measurement into the loop.
         */
        typedef enum
        {
            RESULT_TARGET_REACHED,       /*!< measured ADU is within tolerance, calibration is complete */
            RESULT_CONTINUE,             /*!< a new brightness has been calculated, see @see nextBrightness() */
            RESULT_FAILED_NOT_CONFIGURED,/*!< the loop has not been started or the range is unusable */
            RESULT_FAILED_MINIMUM,       /*!< still too bright at the lowest brightness the device accepts */
            RESULT_FAILED_MAXIMUM,       /*!< still too dark at the highest brightness the device accepts */
            RESULT_FAILED_INVALID_ADU,   /*!< no usable signal although the light source is at its maximum,
                                              @see NO_SIGNAL_ADU_RISE */
            RESULT_FAILED_RESOLUTION,    /*!< the brightness resolution of the device is too coarse */
            RESULT_FAILED_MAX_ITERATIONS /*!< the target could not be reached within the iteration budget */
        } Result;

        /** Default upper limit for the number of test frames. */
        static constexpr int DEFAULT_MAX_ITERATIONS = 20;

        /**
         * A light box at its maximum brightness gives no light if its ADU rose by less than
         * NO_SIGNAL_ADU_RISE compared to a frame at a brightness at least
         * NO_SIGNAL_BRIGHTNESS_FACTOR times lower. Without light, every frame only shows the
         * bias of the camera, and lengthening the exposure would not help either. If the
         * search reaches the maximum without such a frame, it asks for one at the search
         * minimum, @see checksLight().
         */
        static constexpr double NO_SIGNAL_BRIGHTNESS_FACTOR = 4.0;
        static constexpr double NO_SIGNAL_ADU_RISE = 0.01;

        FlatBrightnessCalibration() = default;

        /**
         * @brief start (re-)initialize the loop.
         *
         * The increment a device advertises for its brightness is deliberately not
         * taken as a constraint. In INDI that value is a hint for input widgets, not
         * a promise that other values are refused, and treating it as a grid can put
         * the target out of reach: a device advertising an increment of 10 may well
         * accept 52. The loop therefore considers every integer in range and relies
         * on the brightness the device reports back, which keeps it correct even for
         * a device that really does snap to a coarser grid.
         *
         * The search never asks for less than @see searchMinimum(). A frame the light box
         * reports below that, e.g. because it was at 0 already, is kept at its real
         * brightness.
         * @param minBrightness lowest brightness the device accepts
         * @param maxBrightness highest brightness the device accepts
         * @param targetADU the ADU value that should be reached
         * @param tolerance accepted absolute deviation from @p targetADU
         * @param maxIterations upper limit for the number of measurements
         * @return true if the loop has been started, false if the parameters are unusable
         */
        bool start(int minBrightness, int maxBrightness, double targetADU, double tolerance,
                   int maxIterations = DEFAULT_MAX_ITERATIONS);

        /**
         * @brief reset forget everything, the loop becomes inactive.
         */
        void reset();

        /**
         * @brief isActive true between @see start() and @see reset().
         */
        bool isActive() const
        {
            return m_Active;
        }

        /**
         * @brief update feed the result of the frame that was just measured.
         * @param currentBrightness brightness that was used for that frame
         * @param measuredADU ADU measured in that frame
         * @param saturated true if the frame is saturated and therefore unusable
         * @return what the caller should do next. On RESULT_CONTINUE the value
         *         returned by @see nextBrightness() has to be requested from the
         *         light box, on any other result the loop has terminated.
         */
        Result update(int currentBrightness, double measuredADU, bool saturated);

        /**
         * @brief nextBrightness brightness to be requested next, valid after
         * @see update() returned RESULT_CONTINUE.
         */
        int nextBrightness() const
        {
            return m_NextBrightness;
        }

        /**
         * @brief checksLight whether the frame at @see nextBrightness() checks that the light
         * box gives light, valid after @see update() returned RESULT_CONTINUE.
         *
         * The search was still too dark at the maximum brightness, and it had no frame at a
         * brightness NO_SIGNAL_BRIGHTNESS_FACTOR times lower to compare with. It asks for one
         * at the search minimum, once. The next @see update() then ends the search: with
         * RESULT_FAILED_INVALID_ADU if the ADU at the maximum is less than NO_SIGNAL_ADU_RISE
         * above that frame, with RESULT_FAILED_MAXIMUM otherwise. The fallback keeps the
         * maximum brightness either way.
         */
        bool checksLight() const
        {
            return m_ChecksLight;
        }

        /**
         * @brief lightCheckMeasured whether the last @see update() measured the frame that checks
         * that the light box gives light, @see checksLight(). The ADU at the maximum it was
         * compared with is @see belowADU().
         */
        bool lightCheckMeasured() const
        {
            return m_LightCheckMeasured;
        }

        /**
         * @brief iterations number of measurements fed into the loop so far.
         */
        int iterations() const
        {
            return m_Iterations;
        }

        /**
         * @brief attempts number of measurements since the target was last reached.
         *
         * This, and not @see iterations(), is what the iteration budget applies to:
         * once the target has been reached the loop keeps verifying every frame of
         * the sequence, and those frames must not consume the budget that is there
         * to bound a single calibration.
         */
        int attempts() const
        {
            return m_Attempts;
        }

        /**
         * @brief belowBrightness brightest brightness measured below the target in the
         * current search, -1 if there is none.
         */
        int belowBrightness() const;

        /**
         * @brief belowADU ADU measured at @see belowBrightness(), 0 for a frame without signal.
         */
        double belowADU() const;

        /**
         * @brief aboveBrightness dimmest brightness measured above the target in the
         * current search, -1 if there is none.
         */
        int aboveBrightness() const;

        /**
         * @brief aboveADU ADU measured at @see aboveBrightness().
         */
        double aboveADU() const;

        /**
         * @brief fallbackBrightness brightness to keep when the target turns out to be
         * unreachable and the exposure has to be adjusted instead, -1 if nothing has
         * been measured yet.
         *
         * That is the dimmest brightness that was measured too bright, if there is
         * one. A frame above the target is lit by definition, so shortening the
         * exposure from there reaches the target, whatever the panel does at lower
         * values. A frame below the target could be one without light, which still
         * shows the bias of the camera: some panels switch off at their lowest
         * brightness or stay dark below a threshold. Only if every frame was too dark,
         * the brightest one is kept and the exposure has to grow.
         */
        int fallbackBrightness() const;

        /**
         * @brief fallbackADU ADU measured at @see fallbackBrightness().
         */
        double fallbackADU() const;

        /**
         * @brief fallbackSaturated whether the frame at @see fallbackBrightness() was
         * saturated, in which case @see fallbackADU() carries no scale information.
         */
        bool fallbackSaturated() const;

        /**
         * @brief searchMinimum lowest brightness the search asks for in a device range.
         *
         * A brightness of 0 switches some light boxes off, and some only switch on again
         * when their light is turned on. A device range that starts at 0 is therefore
         * searched from 1, unless 1 is also its maximum.
         * @param minBrightness lowest brightness the device accepts
         * @param maxBrightness highest brightness the device accepts
         */
        static int searchMinimum(int minBrightness, int maxBrightness);

        /**
         * @brief minBrightness lowest brightness the search asks for, @see searchMinimum().
         */
        int minBrightness() const
        {
            return m_Min;
        }
        int maxBrightness() const
        {
            return m_Max;
        }

        /**
         * @brief resultString untranslated description of a result, for logging.
         */
        static QString resultString(Result result);

    private:
        /**
         * @brief quantize round a raw value to the nearest brightness the interface
         * can carry and clamp it into the valid range.
         */
        int quantize(double value) const;
        /** @brief one brightness value below @p from, clamped to the minimum */
        int stepDown(int from) const;
        /** @brief one brightness value above @p from, clamped to the maximum */
        int stepUp(int from) const;
        /**
         * @brief restartAfterDrift forget all measurements once a frame falls outside
         * the tolerance after the target had been reached, so that the light source is
         * searched again as it is now instead of as it was before it drifted.
         */
        void restartAfterDrift();
        /**
         * @brief endAtMaximum end a search that is still too dark at the maximum, or ask for
         * a frame at the search minimum first, @see checksLight().
         */
        Result endAtMaximum(double measuredADU);
        /** @brief compare the frame at the search minimum with the one at the maximum */
        Result checkLight(int currentBrightness, double measuredADU, bool saturated);

        // configuration
        bool m_Active { false };
        // lowest brightness the search asks for, and lowest one the device accepts
        int m_Min { 0 };
        int m_DeviceMin { 0 };
        int m_Max { 0 };
        double m_Target { 0 };
        double m_Tolerance { 0 };
        int m_MaxIterations { DEFAULT_MAX_ITERATIONS };

        // loop state
        int m_Iterations { 0 };
        int m_Attempts { 0 };
        int m_NextBrightness { 0 };
        // Detection of a device that ignores a request and stays where it is
        bool m_HasRequested { false };
        int m_LastMeasuredBrightness { -1 };
        // The target has been reached and the following frames are being verified
        bool m_Reached { false };

        // brackets: the brightest measurement that stayed below the target and the
        // darkest measurement that ended up above it
        bool m_HasLow { false };
        bool m_HasHigh { false };
        int m_LowBrightness { 0 };
        int m_HighBrightness { 0 };
        double m_LowADU { 0 };
        double m_HighADU { 0 };
        bool m_HighSaturated { false };

        // the frame at the lowest brightness measured, to tell a light box without light
        int m_DimmestBrightness { -1 };
        double m_DimmestADU { 0 };

        // the frame at the search minimum that checks for light, @see checksLight()
        bool m_ChecksLight { false };
        bool m_LightCheckMeasured { false };
        bool m_LightChecked { false };
        double m_MaxADU { 0 };
};

} // namespace Ekos
