/*
    Tests for the choice of how a flat frame reaches its target ADU.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

/**
 * @brief Tests for Ekos::FlatCalibrationMethod: whether a flat calibration adjusts the
 * light box brightness or the exposure, when a brightness calibration falls back to
 * the exposure, and the first exposure after doing so. They run without any device
 * and without a display.
 */
class TestFlatCalibrationMethod : public QObject
{
        Q_OBJECT
    public:
        explicit TestFlatCalibrationMethod();

    private Q_SLOTS:
        /** @brief The brightness is only adjusted if the light box is the flat source. */
        void testSelectMethod_data();
        void testSelectMethod();

        /** @brief Only a brightness range that cannot reach the target falls back. */
        void testFallsBackToExposure_data();
        void testFallsBackToExposure();

        /** @brief The first exposure after the fallback is scaled and clamped. */
        void testFallbackExposure_data();
        void testFallbackExposure();

        /** @brief A frame taken while the light box changed its brightness is taken again. */
        void testCheckFrameBrightness_data();
        void testCheckFrameBrightness();

        /** @brief A calibration stuck at an exposure limit of the camera is detected. */
        void testExposureLimit_data();
        void testExposureLimit();

        /** @brief An automatic target follows from the bit depth of the frame. */
        void testAutoTarget_data();
        void testAutoTarget();

        /** @brief An ADU that no longer follows the exposure is detected. */
        void testStallDetector_data();
        void testStallDetector();
};
