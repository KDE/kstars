/*
    Tests for the ADU driven light box brightness calibration.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/capture/flatbrightnesscalibration.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

/**
 * @brief Tests for Ekos::FlatBrightnessCalibration, the feedback loop that reaches a
 * target ADU by changing the brightness of a light box while the camera exposure
 * stays fixed.
 *
 * The device is simulated, so no hardware and no INDI driver is involved. Note that
 * the simulation deliberately covers several unrelated brightness ranges, gammas and
 * step sizes: the algorithm may not assume anything about a specific device.
 */
class TestFlatBrightnessCalibration : public QObject
{
        Q_OBJECT
    public:
        explicit TestFlatBrightnessCalibration();

    private Q_SLOTS:
        /** @brief 1. The very first frame is already on target. */
        void testAlreadyWithinTolerance();
        /** @brief 2. The frame is too dark, brightness has to increase. */
        void testBelowTarget();
        /** @brief 3. The frame is too bright, brightness has to decrease. */
        void testAboveTarget();
        /** @brief 4. Still too bright at the lowest brightness the device accepts. */
        void testMinimumBrightness();
        /**
         * @brief When the target cannot be reached, the light box keeps the dimmest brightness
         * that was too bright, which gives light, also on panels that are dark at low values.
         * Brightness 0 is never asked for, since some light boxes switch off there for good.
         */
        void testFallbackBrightness_data();
        void testFallbackBrightness();
        /**
         * @brief A light box without light stops at maximum brightness instead of falling back
         * to a longer exposure, while a dim one that does give light falls back.
         */
        void testNoSignalAtMaximum_data();
        void testNoSignalAtMaximum();
        /**
         * @brief A search that reaches the maximum brightness without a frame at a quarter of it
         * or lower takes one at its minimum, and stops if the light box gives no light.
         */
        void testLightCheckAtSearchMinimum_data();
        void testLightCheckAtSearchMinimum();
        /** @brief 5. Still too dark at the highest brightness the device accepts. */
        void testMaximumBrightness();
        /** @brief 6. A frame without any signal. */
        void testZeroADU();
        /** @brief 7. A saturated frame is never accepted and leads downwards. */
        void testSaturation();
        /** @brief 8. The target sits between two adjacent brightness values. */
        void testTargetCannotBeReached();
        /** @brief 9. The iteration budget is respected. */
        void testMaximumIterations();
        /** @brief 10. The camera exposure is never touched. */
        void testExposureRemainsFixed();
        /** @brief 11. Convergence for a strongly non linear response. */
        void testNonLinearResponse();
        /** @brief 12. No assumption about any particular device or brightness range. */
        void testArbitraryDeviceRanges();
        /** @brief An unusable configuration is rejected instead of misbehaving. */
        void testInvalidConfiguration();
        /** @brief A device range that starts at 0 is searched from 1. */
        void testSearchMinimum_data();
        void testSearchMinimum();
        /** @brief Noisy measurements still converge. */
        void testNoisyMeasurements();
        /** @brief The increment a device advertises must not constrain the search. */
        void testAdvertisedStepIsNotAConstraint();
        /** @brief A device that really only accepts a coarse grid terminates cleanly. */
        void testDeviceSnappingToCoarseGrid();
        /** @brief The brightness a per-filter preset applied is the starting point. */
        void testPerFilterPresetIsTheStartingPoint_data();
        void testPerFilterPresetIsTheStartingPoint();
        /** @brief A frame that drifts out of tolerance after a tight bracket does not abort. */
        void testDriftAfterTightBracket();
        /** @brief After a drift of the light source the target is reached again. */
        void testDriftAfterTargetReached_data();
        void testDriftAfterTargetReached();
        /**
         * @brief When the brightness range cannot reach the target, the light box keeps
         * the closest brightness and the first exposure step reaches the target.
         */
        void testFallbackToExposure_data();
        void testFallbackToExposure();
};
