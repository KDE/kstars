/*
    Tests for the choice of how a flat frame reaches its target ADU.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_flatcalibrationmethod.h"

#include "ekos/capture/flatcalibrationmethod.h"

#include <limits>

using Ekos::FlatBrightnessCalibration;
namespace Method = Ekos::FlatCalibrationMethod;

TestFlatCalibrationMethod::TestFlatCalibrationMethod() : QObject()
{
}

void TestFlatCalibrationMethod::testSelectMethod_data()
{
    QTest::addColumn<bool>("skyFlat");
    QTest::addColumn<bool>("lightBoxOn");
    QTest::addColumn<bool>("brightnessControllable");
    QTest::addColumn<int>("expected");

    const int brightness = Method::METHOD_BRIGHTNESS;
    const int exposure = Method::METHOD_EXPOSURE;

    QTest::newRow("light box on, adjustable") << false << true << true << brightness;
    // A light box without brightness control can only be used as it is.
    QTest::newRow("light box on, not adjustable") << false << true << false << exposure;
    // The light comes from somewhere else, e.g. a wall or an external panel, while a
    // light box in the optical train stays off.
    QTest::newRow("light box off, adjustable") << false << false << true << exposure;
    QTest::newRow("no light box") << false << false << false << exposure;
    // Sky flats always adjust the exposure, the light box plays no part in them.
    QTest::newRow("sky flats, light box on") << true << true << true << exposure;
    QTest::newRow("sky flats, light box off") << true << false << true << exposure;
}

void TestFlatCalibrationMethod::testSelectMethod()
{
    QFETCH(bool, skyFlat);
    QFETCH(bool, lightBoxOn);
    QFETCH(bool, brightnessControllable);
    QFETCH(int, expected);

    QCOMPARE(static_cast<int>(Method::select(skyFlat, lightBoxOn, brightnessControllable)), expected);
}

void TestFlatCalibrationMethod::testFallsBackToExposure_data()
{
    QTest::addColumn<int>("result");
    QTest::addColumn<bool>("expected");

    // The brightness range cannot reach the target: the exposure takes over.
    QTest::newRow("too bright at minimum") << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_MINIMUM) << true;
    QTest::newRow("too dark at maximum") << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_MAXIMUM) << true;
    QTest::newRow("between two values") << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_RESOLUTION) << true;
    QTest::newRow("no convergence") << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_MAX_ITERATIONS)
                                    << true;

    // A problem with the light source itself is not something the exposure can fix.
    QTest::newRow("no signal") << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_INVALID_ADU) << false;
    QTest::newRow("not configured") << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_NOT_CONFIGURED)
                                    << false;

    // Not a failure at all.
    QTest::newRow("target reached") << static_cast<int>(FlatBrightnessCalibration::RESULT_TARGET_REACHED) << false;
    QTest::newRow("continue") << static_cast<int>(FlatBrightnessCalibration::RESULT_CONTINUE) << false;
}

void TestFlatCalibrationMethod::testFallsBackToExposure()
{
    QFETCH(int, result);
    QFETCH(bool, expected);

    QCOMPARE(Method::fallsBackToExposure(static_cast<FlatBrightnessCalibration::Result>(result)), expected);
}

void TestFlatCalibrationMethod::testFallbackExposure_data()
{
    QTest::addColumn<double>("exposure");
    QTest::addColumn<double>("measuredADU");
    QTest::addColumn<bool>("saturated");
    QTest::addColumn<double>("targetADU");
    QTest::addColumn<double>("minExposure");
    QTest::addColumn<double>("maxExposure");
    QTest::addColumn<double>("expected");

    QTest::newRow("too dark") << 1.0 << 10000.0 << false << 30000.0 << 0.001 << 3600.0 << 3.0;
    QTest::newRow("too bright") << 1.0 << 60000.0 << false << 30000.0 << 0.001 << 3600.0 << 0.5;
    QTest::newRow("small correction") << 2.0 << 30000.0 << false << 32000.0 << 0.001 << 3600.0 << 2.0 * 32000.0 / 30000.0;
    // A frame that shows little more than the bias would scale the exposure by the ratio
    // of the target to the bias. The first step is capped instead (review by Andreas R.).
    QTest::newRow("no light, only bias") << 2.0 << 500.0 << false << 26214.0 << 0.001 << 3600.0 << 20.0;
    QTest::newRow("just below the cap") << 2.0 << 3000.0 << false << 29000.0 << 0.001 << 3600.0 << 2.0 * 29000.0 / 3000.0;
    // The camera range always wins.
    QTest::newRow("clamped to maximum") << 10.0 << 1000.0 << false << 30000.0 << 0.001 << 60.0 << 60.0;
    QTest::newRow("clamped to minimum") << 0.01 << 60000.0 << false << 100.0 << 0.001 << 3600.0 << 0.001;
    // Without a usable measurement there is no ratio: the exposure stays, within range.
    QTest::newRow("no signal") << 1.0 << 0.0 << false << 30000.0 << 0.001 << 3600.0 << 1.0;
    QTest::newRow("no target") << 1.0 << 10000.0 << false << 0.0 << 0.001 << 3600.0 << 1.0;
    QTest::newRow("invalid measurement") << 1.0 << std::numeric_limits<double>::quiet_NaN() << false << 30000.0 << 0.001
                                         << 3600.0 << 1.0;
    // A saturated frame carries no usable ADU: reduce the exposure like the exposure
    // calibration does after a saturated frame.
    QTest::newRow("saturated") << 1.0 << 65000.0 << true << 20000.0 << 0.001 << 3600.0 << 0.1;
    QTest::newRow("saturated, clamped to minimum") << 0.005 << 65000.0 << true << 20000.0 << 0.001 << 3600.0 << 0.001;
    QTest::newRow("no signal, out of range") << 120.0 << 0.0 << false << 30000.0 << 0.001 << 60.0 << 60.0;
}

void TestFlatCalibrationMethod::testFallbackExposure()
{
    QFETCH(double, exposure);
    QFETCH(double, measuredADU);
    QFETCH(bool, saturated);
    QFETCH(double, targetADU);
    QFETCH(double, minExposure);
    QFETCH(double, maxExposure);
    QFETCH(double, expected);

    QCOMPARE(Method::fallbackExposure(exposure, measuredADU, saturated, targetADU, minExposure, maxExposure), expected);
}

void TestFlatCalibrationMethod::testCheckFrameBrightness_data()
{
    QTest::addColumn<int>("atStart");
    QTest::addColumn<int>("now");
    QTest::addColumn<int>("retakesSoFar");
    QTest::addColumn<int>("expected");

    QTest::newRow("unchanged") << 52 << 52 << 0 << int(Method::FRAME_USE);
    QTest::newRow("unknown start") << -1 << 125 << 0 << int(Method::FRAME_USE);
    // a per-filter preset applied at the filter change right before the exposure
    QTest::newRow("preset applied during the frame") << 5 << 125 << 0 << int(Method::FRAME_RETAKE);
    QTest::newRow("last retake") << 5 << 125 << Method::MAX_BRIGHTNESS_RETAKES - 1 << int(Method::FRAME_RETAKE);
    QTest::newRow("keeps changing") << 5 << 125 << Method::MAX_BRIGHTNESS_RETAKES << int(Method::FRAME_GIVE_UP);
    // a frame at the expected brightness is used, however many frames were taken again before
    QTest::newRow("unchanged after retakes") << 125 << 125 << Method::MAX_BRIGHTNESS_RETAKES << int(Method::FRAME_USE);
}

void TestFlatCalibrationMethod::testCheckFrameBrightness()
{
    QFETCH(int, atStart);
    QFETCH(int, now);
    QFETCH(int, retakesSoFar);
    QFETCH(int, expected);

    QCOMPARE(int(Method::checkFrameBrightness(atStart, now, retakesSoFar)), expected);
}

void TestFlatCalibrationMethod::testExposureLimit_data()
{
    QTest::addColumn<double>("exposure");
    QTest::addColumn<double>("nextExposure");
    QTest::addColumn<int>("limit");

    // camera range 0.001 - 60 seconds
    QTest::newRow("within range") << 1.0 << 2.0 << int(Method::LIMIT_NONE);
    QTest::newRow("too bright at the minimum") << 0.001 << 0.0001 << int(Method::LIMIT_MINIMUM);
    QTest::newRow("too dark at the maximum") << 60.0 << 75.0 << int(Method::LIMIT_MAXIMUM);
    // the first step beyond a limit is clamped and taken, only then it is stuck
    QTest::newRow("reaching the minimum") << 0.005 << 0.0005 << int(Method::LIMIT_NONE);
    QTest::newRow("reaching the maximum") << 50.0 << 75.0 << int(Method::LIMIT_NONE);
    // at a limit, but the next exposure goes back into the range
    QTest::newRow("leaving the minimum") << 0.001 << 0.002 << int(Method::LIMIT_NONE);
    QTest::newRow("leaving the maximum") << 60.0 << 45.0 << int(Method::LIMIT_NONE);
}

void TestFlatCalibrationMethod::testExposureLimit()
{
    QFETCH(double, exposure);
    QFETCH(double, nextExposure);
    QFETCH(int, limit);

    QCOMPARE(int(Method::exposureLimit(exposure, nextExposure, 0.001, 60.0)), limit);
}

void TestFlatCalibrationMethod::testAutoTarget_data()
{
    QTest::addColumn<uint>("bitsPerPixel");
    QTest::addColumn<double>("target");
    QTest::addColumn<double>("tolerance");

    // 40 % of the range, 5 % of that as tolerance
    QTest::newRow("16-bit") << 16u << 26214.0 << 1311.0;
    QTest::newRow("8-bit") << 8u << 102.0 << 5.0;
    // floating point and wider data have no range to derive a target from
    QTest::newRow("32-bit") << 32u << 0.0 << 0.0;
    QTest::newRow("64-bit") << 64u << 0.0 << 0.0;
    QTest::newRow("unknown") << 0u << 0.0 << 0.0;
}

void TestFlatCalibrationMethod::testAutoTarget()
{
    QFETCH(uint, bitsPerPixel);
    QFETCH(double, target);
    QFETCH(double, tolerance);

    QCOMPARE(Method::autoTargetADU(bitsPerPixel), target);
    QCOMPARE(Method::autoTolerance(Method::autoTargetADU(bitsPerPixel)), tolerance);
}

void TestFlatCalibrationMethod::testStallDetector_data()
{
    QTest::addColumn<QList<double>>("exposures");
    QTest::addColumn<QList<double>>("adus");
    QTest::addColumn<double>("target");
    // index of the first frame after which the detector reports a stall, -1 for never
    QTest::addColumn<int>("stallAt");

    QTest::newRow("proportional response")
            << QList<double> {1.0, 1.25, 1.5625, 2.0} << QList<double> {8000, 10000, 12500, 16000} << 20000.0 << -1;
    QTest::newRow("response with bias")
            << QList<double> {0.1, 0.125, 0.15625} << QList<double> {1000, 1125, 1281} << 20000.0 << -1;
    // a target below the bias level, as measured on a real camera with the light box at
    // its minimum: the exposure keeps decreasing and the ADU hardly follows
    QTest::newRow("bias only")
            << QList<double> {0.507, 0.380, 0.285, 0.214, 0.160} << QList<double> {219, 214, 210, 208, 208} << 30.0 << 3;
    // unscaled 14-bit data in a 16-bit frame never reaches a 16-bit target
    QTest::newRow("clipped below the target")
            << QList<double> {1.0, 1.25, 1.5625, 1.953} << QList<double> {16383, 16383, 16383, 16383} << 26214.0 << 3;
    QTest::newRow("ADU against the exposure")
            << QList<double> {1.0, 1.25, 1.5625, 1.953} << QList<double> {10000, 9500, 9000, 8500} << 26214.0 << 3;
    // A dim flat source gives frames that are mostly bias, so even the long exposure that
    // brings it close to the target changes the ADU little in relation. It is no stall.
    QTest::newRow("dim source")
            << QList<double> {0.001, 0.00125, 6.0} << QList<double> {505, 506, 25500} << 26214.0 << -1;
    // Steps of 25 % that hardly change the ADU are what the exposure calibration does when
    // the target would need more than 180 seconds, which it treats as a flat source that is off.
    QTest::newRow("dim source in small steps")
            << QList<double> {0.001, 0.00125, 0.0015625, 0.00195} << QList<double> {505, 506, 507, 508} << 26214.0 << 3;
    // small steps near the target say nothing about the response
    QTest::newRow("small steps")
            << QList<double> {1.0, 1.05, 1.1, 1.15} << QList<double> {20000, 20000, 20000, 20000} << 26214.0 << -1;
    // a stalled step followed by a normal one starts counting again
    QTest::newRow("stalled steps not in a row")
            << QList<double> {1.0, 1.25, 1.5625, 1.953, 2.441, 3.052} << QList<double> {10000, 10010, 10020, 12500, 12510, 12520}
            << 26214.0 << -1;
    QTest::newRow("invalid frame restarts")
            << QList<double> {1.0, 1.25, 1.5625, 1.953, 2.441} << QList<double> {16383, 16383, 0, 16383, 16383} << 26214.0 << -1;
}

void TestFlatCalibrationMethod::testStallDetector()
{
    QFETCH(QList<double>, exposures);
    QFETCH(QList<double>, adus);
    QFETCH(double, target);
    QFETCH(int, stallAt);

    Method::StallDetector detector;
    for (int i = 0; i < exposures.size(); i++)
        QCOMPARE(detector.update(exposures[i], adus[i], target), stallAt >= 0 && i >= stallAt);

    // a reset forgets the frames
    detector.reset();
    QVERIFY(detector.update(exposures.last(), adus.last(), target) == false);
}

QTEST_GUILESS_MAIN(TestFlatCalibrationMethod)
