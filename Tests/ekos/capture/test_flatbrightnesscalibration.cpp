/*
    Tests for the ADU driven light box brightness calibration.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_flatbrightnesscalibration.h"

#include "ekos/capture/flatcalibrationmethod.h"

#include <cmath>

using Ekos::FlatBrightnessCalibration;

namespace
{

/**
 * @brief A simulated dimmable flat panel plus camera.
 *
 * It stands in for any INDI device that implements the generic light box brightness
 * capability. It has no vendor, no driver and no modes - just like the algorithm
 * expects. The exposure is part of the simulation only so that the tests can prove
 * that the algorithm never changes it.
 */
struct SimulatedFlatPanel
{
    // camera
    double exposure { 2.0 };
    double fullWell { 65535.0 };
    double noise { 0.0 };

    // light box, as published through the generic interface
    int minBrightness { 0 };
    int maxBrightness { 255 };
    // The granularity the device really enforces. It is never handed to the
    // algorithm: what a device advertises as its increment is a hint for input
    // widgets, and the algorithm learns the real behaviour from the brightness the
    // device reports back instead.
    int step { 1 };
    int brightness { 0 };

    // response of panel plus camera: ADU at maximum brightness, and the exponent of
    // the brightness to ADU relation (1 = linear)
    double aduAtMaxBrightness { 60000.0 };
    // ADU produced at the lowest brightness the device accepts. Some light sources
    // cannot be dimmed all the way down to darkness.
    double aduAtMinBrightness { 0.0 };
    double gamma { 1.0 };

    int frames { 0 };
    int noiseSign { 1 };

    void setBrightness(int value)
    {
        // A real device clamps and quantizes, so the simulation does too.
        const int steps = static_cast<int>(std::round((value - minBrightness) / static_cast<double>(step)));
        brightness = qBound(minBrightness, minBrightness + steps * step, maxBrightness);
    }

    double measureADU()
    {
        frames++;
        const double span = maxBrightness - minBrightness;
        const double fraction = span > 0 ? (brightness - minBrightness) / span : 0.0;
        double adu = (aduAtMinBrightness + (aduAtMaxBrightness - aduAtMinBrightness) * std::pow(fraction, gamma))
                     * (exposure / 2.0);

        if (noise > 0)
        {
            // Deterministic alternating deviation, no random numbers in a unit test.
            adu += noiseSign * noise;
            noiseSign = -noiseSign;
        }

        return qBound(0.0, adu, fullWell);
    }

    bool isSaturated(double adu) const
    {
        // Mirrors the criterion Ekos applies to a captured frame.
        return adu / fullWell > 0.95;
    }
};

/**
 * @brief Drive the loop against the simulated device until it terminates.
 * @return the result the loop terminated with
 */
FlatBrightnessCalibration::Result runLoop(FlatBrightnessCalibration &calibration, SimulatedFlatPanel &panel,
        int frameLimit = 100)
{
    for (int i = 0; i < frameLimit; i++)
    {
        const double adu = panel.measureADU();
        const auto result = calibration.update(panel.brightness, adu, panel.isSaturated(adu));

        if (result != FlatBrightnessCalibration::RESULT_CONTINUE)
            return result;

        panel.setBrightness(calibration.nextBrightness());
    }

    return FlatBrightnessCalibration::RESULT_CONTINUE;
}

} // namespace

TestFlatBrightnessCalibration::TestFlatBrightnessCalibration() : QObject()
{
}

void TestFlatBrightnessCalibration::testAlreadyWithinTolerance()
{
    SimulatedFlatPanel panel;
    // 128 of 255 on a linear panel that reaches 60000 ADU produces ~30117 ADU
    panel.setBrightness(128);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 30000, 1000));
    QVERIFY(calibration.isActive());

    const double adu = panel.measureADU();
    QCOMPARE(calibration.update(panel.brightness, adu, false), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    // No adjustment must have been calculated, and no further frame is needed.
    QCOMPARE(calibration.iterations(), 1);
    QCOMPARE(panel.frames, 1);
}

void TestFlatBrightnessCalibration::testBelowTarget()
{
    SimulatedFlatPanel panel;
    panel.setBrightness(10);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 30000, 500));

    // The first correction has to go up.
    const double firstADU = panel.measureADU();
    QVERIFY(firstADU < 30000);
    QCOMPARE(calibration.update(panel.brightness, firstADU, false), FlatBrightnessCalibration::RESULT_CONTINUE);
    QVERIFY(calibration.nextBrightness() > panel.brightness);

    panel.setBrightness(calibration.nextBrightness());
    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QVERIFY(std::fabs(panel.measureADU() - 30000) <= 500);
}

void TestFlatBrightnessCalibration::testAboveTarget()
{
    SimulatedFlatPanel panel;
    panel.setBrightness(250);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 20000, 500));

    // The first correction has to go down.
    const double firstADU = panel.measureADU();
    QVERIFY(firstADU > 20000);
    QCOMPARE(calibration.update(panel.brightness, firstADU, false), FlatBrightnessCalibration::RESULT_CONTINUE);
    QVERIFY(calibration.nextBrightness() < panel.brightness);

    panel.setBrightness(calibration.nextBrightness());
    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QVERIFY(std::fabs(panel.measureADU() - 20000) <= 500);
}

void TestFlatBrightnessCalibration::testMinimumBrightness()
{
    // The panel is far too bright even at its lowest setting.
    SimulatedFlatPanel panel;
    panel.minBrightness = 100;
    panel.maxBrightness = 255;
    panel.aduAtMinBrightness = 5000;
    panel.setBrightness(100);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 100, 10));

    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_FAILED_MINIMUM);
    // The loop must have stopped at the minimum, never below it.
    QCOMPARE(panel.brightness, panel.minBrightness);
}

void TestFlatBrightnessCalibration::testFallbackBrightness_data()
{
    // Response of panel plus camera: the bias of the camera, the highest brightness
    // that still gives no light (-1 if every brightness gives light), and the ADU
    // per brightness step above it for an exposure of one second. A panel that latches
    // off at 0 stays dark at every brightness once 0 was asked for, until its light is
    // turned on again.
    QTest::addColumn<double>("bias");
    QTest::addColumn<int>("darkUpTo");
    QTest::addColumn<double>("aduPerStep");
    QTest::addColumn<bool>("latchesOff");
    QTest::addColumn<double>("exposure");
    QTest::addColumn<int>("startBrightness");
    QTest::addColumn<double>("targetADU");
    QTest::addColumn<int>("fallbackBrightness");

    // A panel that is off at 0 and already too bright at 1: the search stops at 1,
    // which is lit.
    QTest::newRow("off at 0, too bright at 1") << 500.0 << 0 << 30000.0 << false << 2.0 << 52 << 26214.0 << 1;
    // The same panel, but the light box is at 0 when the calibration starts. The dark
    // frame must not be taken for brightness 1 (review by Andreas R.).
    QTest::newRow("at 0 when the calibration starts") << 500.0 << 0 << 30000.0 << false << 2.0 << 0 << 26214.0 << 1;
    // A panel whose driver switches the light off at 0 and then ignores other values,
    // like the Wanderer Cover V4-EC: asking for 0 would leave it dark (review by Andreas R.).
    QTest::newRow("latches off at 0, too bright at 1") << 500.0 << 0 << 30000.0 << true << 2.0 << 52 << 26214.0 << 1;
    // A panel that stays dark up to 10 and is too bright at 11.
    QTest::newRow("dark up to 10") << 500.0 << 10 << 40000.0 << false << 1.0 << 100 << 30000.0 << 11;
    // A panel that still gives light at 0: 0 is never asked for, the search stops at 1.
    QTest::newRow("lit at 0, too bright at 1") << 500.0 << -1 << 20000.0 << false << 1.0 << 100 << 15000.0 << 1;
    // Too dark at full brightness: the brightest frame is kept and the exposure grows.
    QTest::newRow("too dark at maximum") << 500.0 << 0 << 40.0 << false << 1.0 << 10 << 30000.0 << 255;
}

void TestFlatBrightnessCalibration::testFallbackBrightness()
{
    QFETCH(double, bias);
    QFETCH(int, darkUpTo);
    QFETCH(double, aduPerStep);
    QFETCH(bool, latchesOff);
    QFETCH(double, exposure);
    QFETCH(int, startBrightness);
    QFETCH(double, targetADU);
    QFETCH(int, fallbackBrightness);

    bool latched = false;
    auto adu = [&](int brightness, double seconds)
    {
        if (latched || brightness <= darkUpTo)
            return bias;
        return qMin(65535.0, bias + aduPerStep * (brightness - darkUpTo) * seconds);
    };
    const double tolerance = targetADU * 0.05;

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(0, 255, targetADU, tolerance));

    int brightness = startBrightness;
    auto result = FlatBrightnessCalibration::RESULT_CONTINUE;
    for (int i = 0; i < 30 && result == FlatBrightnessCalibration::RESULT_CONTINUE; i++)
    {
        const double measured = adu(brightness, exposure);
        result = calibration.update(brightness, measured, measured >= 65535 * 0.95);
        if (result != FlatBrightnessCalibration::RESULT_CONTINUE)
            break;

        // Brightness 0 is never asked for, since some light boxes switch off there.
        brightness = calibration.nextBrightness();
        QVERIFY2(brightness >= 1, qPrintable(QString("the search asked for brightness %1").arg(brightness)));
        if (latchesOff && brightness == 0)
            latched = true;
    }
    QVERIFY2(Ekos::FlatCalibrationMethod::fallsBackToExposure(result),
             qPrintable(FlatBrightnessCalibration::resultString(result)));

    // The light box keeps a brightness that gives light, and the exposure scaled from
    // the ADU measured there reaches the target.
    QCOMPARE(calibration.fallbackBrightness(), fallbackBrightness);
    QCOMPARE(calibration.fallbackADU(), adu(fallbackBrightness, exposure));
    QVERIFY(calibration.fallbackSaturated() == false);
    const double next = Ekos::FlatCalibrationMethod::fallbackExposure(exposure, calibration.fallbackADU(), false,
                        targetADU, 0.001, 3600);
    QVERIFY2(std::fabs(adu(fallbackBrightness, next) - targetADU) <= tolerance,
             qPrintable(QString("next exposure %1 s gives %2 ADU").arg(next).arg(adu(fallbackBrightness, next))));
}

void TestFlatBrightnessCalibration::testNoSignalAtMaximum_data()
{
    // Response of panel plus camera: the bias of the camera, the highest brightness that
    // still gives no light (255 for a panel without any light), and the ADU at full
    // brightness above the bias.
    QTest::addColumn<double>("bias");
    QTest::addColumn<int>("darkUpTo");
    QTest::addColumn<double>("signalAtMax");
    QTest::addColumn<int>("startBrightness");
    QTest::addColumn<int>("expected");

    const int noSignal = FlatBrightnessCalibration::RESULT_FAILED_INVALID_ADU;
    const int tooDark = FlatBrightnessCalibration::RESULT_FAILED_MAXIMUM;

    // Without light every frame shows the bias: stop instead of lengthening the exposure
    // (review by Andreas R.).
    QTest::newRow("no light") << 500.0 << 255 << 0.0 << 10 << noSignal;
    // A dim panel that does give light falls back to a longer exposure.
    QTest::newRow("dim panel") << 500.0 << -1 << 300.0 << 10 << tooDark;
    // Even a signal of 1.6% of the bias over the brightness range counts as light.
    QTest::newRow("very short exposure") << 500.0 << -1 << 8.0 << 10 << tooDark;
    // A panel that only gives light above 100, measured first in its dark part.
    QTest::newRow("dark up to 100") << 500.0 << 100 << 200.0 << 10 << tooDark;
    // Without a frame at a much lower brightness there is nothing to compare with.
    QTest::newRow("no light, started at 100") << 500.0 << 255 << 0.0 << 100 << tooDark;
    QTest::newRow("no light, started at maximum") << 500.0 << 255 << 0.0 << 255 << tooDark;
}

void TestFlatBrightnessCalibration::testNoSignalAtMaximum()
{
    QFETCH(double, bias);
    QFETCH(int, darkUpTo);
    QFETCH(double, signalAtMax);
    QFETCH(int, startBrightness);
    QFETCH(int, expected);

    auto adu = [&](int brightness)
    {
        if (brightness <= darkUpTo)
            return bias;
        const int from = qMax(0, darkUpTo);
        return bias + signalAtMax * (brightness - from) / (255 - from);
    };

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(0, 255, 30000, 1500));

    int brightness = startBrightness;
    auto result = FlatBrightnessCalibration::RESULT_CONTINUE;
    for (int i = 0; i < 30 && result == FlatBrightnessCalibration::RESULT_CONTINUE; i++)
    {
        result = calibration.update(brightness, adu(brightness), false);
        brightness = calibration.nextBrightness();
    }

    QCOMPARE(int(result), expected);
    // Without light the calibration stops, it does not fall back to the exposure.
    QCOMPARE(Ekos::FlatCalibrationMethod::fallsBackToExposure(result),
             expected == FlatBrightnessCalibration::RESULT_FAILED_MAXIMUM);
}

void TestFlatBrightnessCalibration::testMaximumBrightness()
{
    // The panel cannot produce the requested ADU, not even at full power.
    SimulatedFlatPanel panel;
    panel.aduAtMaxBrightness = 10000;
    panel.setBrightness(10);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 40000, 500));

    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_FAILED_MAXIMUM);
    // The loop must have stopped at the maximum, never above it.
    QCOMPARE(panel.brightness, panel.maxBrightness);
}

void TestFlatBrightnessCalibration::testZeroADU()
{
    SimulatedFlatPanel panel;
    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(0, 255, 30000, 500));

    // A frame without any signal cannot be scaled proportionally, but as long as
    // there is headroom the loop has to keep trying rather than give up.
    auto result = calibration.update(0, 0.0, false);
    QCOMPARE(result, FlatBrightnessCalibration::RESULT_CONTINUE);
    QVERIFY(calibration.nextBrightness() > 0);

    // Without any signal at full brightness there is nothing left to do.
    calibration.reset();
    QVERIFY(calibration.start(0, 255, 30000, 500));
    QCOMPARE(calibration.update(255, 0.0, false), FlatBrightnessCalibration::RESULT_FAILED_INVALID_ADU);

    // A dead light source is reported instead of looping forever.
    calibration.reset();
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 30000, 500));
    panel.aduAtMaxBrightness = 0;
    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_FAILED_INVALID_ADU);
}

void TestFlatBrightnessCalibration::testSaturation()
{
    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(0, 255, 30000, 500));

    // A saturated frame must never be accepted, even if its clipped ADU happens to
    // land inside the tolerance band, and the answer is always less brightness.
    const auto result = calibration.update(200, 30000, true);
    QCOMPARE(result, FlatBrightnessCalibration::RESULT_CONTINUE);
    QVERIFY(calibration.nextBrightness() < 200);

    // A panel that saturates the sensor still converges downwards.
    SimulatedFlatPanel panel;
    panel.aduAtMaxBrightness = 200000;
    panel.setBrightness(255);
    calibration.reset();
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 30000, 500));
    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    const double finalADU = panel.measureADU();
    QVERIFY(!panel.isSaturated(finalADU));
    QVERIFY(std::fabs(finalADU - 30000) <= 500);
}

void TestFlatBrightnessCalibration::testTargetCannotBeReached()
{
    // A coarse device: the target falls between two adjacent brightness values and no
    // amount of iterating can do better.
    SimulatedFlatPanel panel;
    panel.minBrightness = 0;
    panel.maxBrightness = 10;
    panel.step = 1;
    panel.aduAtMaxBrightness = 60000;
    panel.setBrightness(0);

    FlatBrightnessCalibration calibration;
    // 30000 ADU would require brightness 5, ask for something in between with a
    // tolerance far tighter than one step is worth.
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 33000, 100));

    const auto result = runLoop(calibration, panel);
    QCOMPARE(result, FlatBrightnessCalibration::RESULT_FAILED_RESOLUTION);
    // The two adjacent values around the target are reported so that the user learns
    // what the device can actually do.
    QCOMPARE(calibration.belowBrightness(), 5);
    QCOMPARE(calibration.belowADU(), 30000.0);
    QCOMPARE(calibration.aboveBrightness(), 6);
    QCOMPARE(calibration.aboveADU(), 36000.0);
}

void TestFlatBrightnessCalibration::testMaximumIterations()
{
    FlatBrightnessCalibration calibration;
    // A budget of three attempts, and a device that never gets anywhere near the
    // target because its response does not react at all.
    QVERIFY(calibration.start(0, 65535, 30000, 10, 3));

    QCOMPARE(calibration.update(100, 1000, false), FlatBrightnessCalibration::RESULT_CONTINUE);
    QCOMPARE(calibration.update(200, 1000, false), FlatBrightnessCalibration::RESULT_CONTINUE);
    QCOMPARE(calibration.update(300, 1000, false), FlatBrightnessCalibration::RESULT_FAILED_MAX_ITERATIONS);
    QCOMPARE(calibration.iterations(), 3);

    // Frames that are on target do not consume the budget: verifying every frame of a
    // long flat sequence must not run the calibration out of attempts.
    calibration.reset();
    QVERIFY(calibration.start(0, 255, 30000, 500, 3));
    for (int i = 0; i < 20; i++)
        QCOMPARE(calibration.update(128, 30100, false), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QCOMPARE(calibration.attempts(), 0);
    // ... and a single deviating frame afterwards is still corrected.
    QCOMPARE(calibration.update(128, 20000, false), FlatBrightnessCalibration::RESULT_CONTINUE);
}

void TestFlatBrightnessCalibration::testExposureRemainsFixed()
{
    SimulatedFlatPanel panel;
    panel.exposure = 2.0;
    panel.setBrightness(5);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 30000, 500));
    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);

    // The exposure is the control variable of the *other* ADU mode. Here it is a
    // constant chosen by the user, and the loop has no way of touching it: there is
    // no exposure anywhere in the interface of the algorithm.
    QCOMPARE(panel.exposure, 2.0);
    // The target has been reached by brightness alone.
    QVERIFY(panel.brightness > 5);
}

void TestFlatBrightnessCalibration::testNonLinearResponse()
{
    // A device whose brightness value is nowhere near proportional to the resulting
    // ADU. The proportional first estimate is wrong here, so the loop has to recover
    // from it.
    const QList<double> gammas { 0.4, 2.5 };
    for (const double gamma : gammas)
    {
        SimulatedFlatPanel panel;
        panel.gamma = gamma;
        panel.setBrightness(20);

        FlatBrightnessCalibration calibration;
        QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 25000, 500));

        QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
        QVERIFY(std::fabs(panel.measureADU() - 25000) <= 500);
    }
}

void TestFlatBrightnessCalibration::testArbitraryDeviceRanges()
{
    // Whatever range, offset and step a device publishes, the loop has to work with
    // it. Nothing here is specific to any manufacturer, model or driver.
    struct Range
    {
        int min, max, step;
    };
    const QVector<Range> ranges =
    {
        {0, 100, 1}, {0, 255, 1}, {1, 1000, 1}, {0, 65535, 1}, {0, 1000, 25}, {50, 150, 2}
    };

    for (const auto &range : ranges)
    {
        SimulatedFlatPanel panel;
        panel.minBrightness = range.min;
        panel.maxBrightness = range.max;
        panel.step = range.step;
        panel.setBrightness(range.min);

        FlatBrightnessCalibration calibration;
        QVERIFY(calibration.start(range.min, range.max, 30000, 1000));
        QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);

        // The requested values always stayed inside what the device published.
        QVERIFY(panel.brightness >= range.min);
        QVERIFY(panel.brightness <= range.max);
    }
}

void TestFlatBrightnessCalibration::testInvalidConfiguration()
{
    FlatBrightnessCalibration calibration;

    // A device that publishes no usable range.
    QVERIFY(!calibration.start(100, 100, 30000, 500));
    QVERIFY(!calibration.isActive());
    // An unusable target.
    QVERIFY(!calibration.start(0, 255, 0, 500));
    // Feeding an inactive loop is reported instead of silently doing something.
    QCOMPARE(calibration.update(10, 1000, false), FlatBrightnessCalibration::RESULT_FAILED_NOT_CONFIGURED);
}

void TestFlatBrightnessCalibration::testNoisyMeasurements()
{
    // Measurement noise larger than the step effect must not make the loop oscillate
    // forever: it either lands inside the tolerance or reports that it cannot.
    SimulatedFlatPanel panel;
    panel.noise = 300;
    panel.setBrightness(10);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 30000, 1000));

    const auto result = runLoop(calibration, panel);
    QVERIFY(result == FlatBrightnessCalibration::RESULT_TARGET_REACHED ||
            result == FlatBrightnessCalibration::RESULT_FAILED_RESOLUTION ||
            result == FlatBrightnessCalibration::RESULT_FAILED_MAX_ITERATIONS);
    // In particular, it terminated.
    QVERIFY(result != FlatBrightnessCalibration::RESULT_CONTINUE);
}

void TestFlatBrightnessCalibration::testAdvertisedStepIsNotAConstraint()
{
    // A device may advertise a coarse increment - 10 is a common choice - while
    // happily accepting any value in its range. Snapping the search onto such an
    // increment would put this target permanently out of reach: it is only met at
    // brightness 52, with 50 too dark and 60 too bright.
    SimulatedFlatPanel panel;
    panel.step = 1;

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 12235, 150));

    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QCOMPARE(panel.brightness, 52);
}

void TestFlatBrightnessCalibration::testDeviceSnappingToCoarseGrid()
{
    // The same target on a device that really does refuse anything but multiples of
    // ten. The target cannot be met, and the loop has to notice that the device
    // stopped moving instead of requesting the same value until the iteration budget
    // runs out.
    SimulatedFlatPanel panel;
    panel.step = 10;

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, 12235, 150));

    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_FAILED_RESOLUTION);
    QVERIFY(calibration.iterations() < FlatBrightnessCalibration::DEFAULT_MAX_ITERATIONS);
}

void TestFlatBrightnessCalibration::testPerFilterPresetIsTheStartingPoint_data()
{
    QTest::addColumn<double>("aduAtMaxBrightness");
    QTest::addColumn<int>("preset");
    QTest::addColumn<double>("targetADU");
    QTest::addColumn<bool>("presetOnTarget");

    // An INDI light box can store a brightness per filter and applies it when the
    // filter wheel changes, e.g. 10 for a bright luminance filter and 100 for a dim
    // narrowband filter. The response of panel plus filter differs accordingly.
    QTest::newRow("luminance, preset on target") << 150000.0 << 10 << 5900.0 << true;
    QTest::newRow("luminance, preset close") << 150000.0 << 10 << 7000.0 << false;
    QTest::newRow("h-alpha, preset on target") << 15000.0 << 100 << 5900.0 << true;
    QTest::newRow("h-alpha, preset close") << 15000.0 << 100 << 7000.0 << false;
}

void TestFlatBrightnessCalibration::testPerFilterPresetIsTheStartingPoint()
{
    QFETCH(double, aduAtMaxBrightness);
    QFETCH(int, preset);
    QFETCH(double, targetADU);
    QFETCH(bool, presetOnTarget);

    const double tolerance = 200;

    // The calibration starts from the brightness the light box reports, so the first
    // frame is taken at the preset and nothing resets the panel to a fixed value.
    SimulatedFlatPanel fromPreset;
    fromPreset.aduAtMaxBrightness = aduAtMaxBrightness;
    fromPreset.setBrightness(preset);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(fromPreset.minBrightness, fromPreset.maxBrightness, targetADU, tolerance));

    if (presetOnTarget)
    {
        // A preset that already produces the target ADU is used as is.
        const double adu = fromPreset.measureADU();
        QCOMPARE(calibration.update(fromPreset.brightness, adu, fromPreset.isSaturated(adu)),
                 FlatBrightnessCalibration::RESULT_TARGET_REACHED);
        QCOMPARE(fromPreset.brightness, preset);
        QCOMPARE(fromPreset.frames, 1);
        return;
    }

    QCOMPARE(runLoop(calibration, fromPreset), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QVERIFY(std::fabs(fromPreset.measureADU() - targetADU) <= tolerance);
    const int framesFromPreset = calibration.iterations();

    // The same panel without a preset, starting at its lowest brightness, needs more
    // test frames: the preset is a head start, not something the loop overrides.
    SimulatedFlatPanel fromMinimum;
    fromMinimum.aduAtMaxBrightness = aduAtMaxBrightness;
    fromMinimum.setBrightness(fromMinimum.minBrightness);

    FlatBrightnessCalibration withoutPreset;
    QVERIFY(withoutPreset.start(fromMinimum.minBrightness, fromMinimum.maxBrightness, targetADU, tolerance));
    QCOMPARE(runLoop(withoutPreset, fromMinimum), FlatBrightnessCalibration::RESULT_TARGET_REACHED);

    QVERIFY2(framesFromPreset < withoutPreset.iterations(),
             qPrintable(QString("from preset: %1 frames, from minimum: %2 frames")
                        .arg(framesFromPreset).arg(withoutPreset.iterations())));
}

void TestFlatBrightnessCalibration::testDriftAfterTightBracket()
{
    // A linear panel reaching 60000 ADU at 255. The target is met at brightness 148
    // only, with 147 just too dark and 149 just too bright.
    const auto adu = [](int brightness, double scale)
    {
        return scale * 60000.0 * brightness / 255.0;
    };
    const double target = adu(148, 1.0);
    const double tolerance = 150;

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(0, 255, target, tolerance));

    QCOMPARE(calibration.update(147, adu(147, 1.0), false), FlatBrightnessCalibration::RESULT_CONTINUE);
    QCOMPARE(calibration.update(149, adu(149, 1.0), false), FlatBrightnessCalibration::RESULT_CONTINUE);
    QCOMPARE(calibration.update(148, adu(148, 1.0), false), FlatBrightnessCalibration::RESULT_TARGET_REACHED);

    // The light source becomes 5 % brighter, so 148 is now too bright. The bracket
    // 147 - 149 belongs to the light before the drift: combined with this frame it
    // would leave no value between 147 and 148 and abort the run, although the
    // target can easily be reached again further down.
    const auto result = calibration.update(148, adu(148, 1.05), false);
    QCOMPARE(result, FlatBrightnessCalibration::RESULT_CONTINUE);
    QVERIFY(calibration.nextBrightness() < 148);

    // Follow it through: the search reaches the target on the drifted light source.
    int brightness = calibration.nextBrightness();
    FlatBrightnessCalibration::Result next = FlatBrightnessCalibration::RESULT_CONTINUE;
    for (int i = 0; i < 20 && next == FlatBrightnessCalibration::RESULT_CONTINUE; i++)
    {
        next = calibration.update(brightness, adu(brightness, 1.05), false);
        if (next == FlatBrightnessCalibration::RESULT_CONTINUE)
            brightness = calibration.nextBrightness();
    }
    QCOMPARE(next, FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QVERIFY(std::fabs(adu(brightness, 1.05) - target) <= tolerance);
}

void TestFlatBrightnessCalibration::testDriftAfterTargetReached_data()
{
    QTest::addColumn<double>("drift");

    QTest::newRow("5 % brighter") << 1.05;
    QTest::newRow("5 % darker") << 0.95;
    QTest::newRow("20 % brighter") << 1.20;
    QTest::newRow("20 % darker") << 0.80;
}

void TestFlatBrightnessCalibration::testDriftAfterTargetReached()
{
    QFETCH(double, drift);

    const double target = 30000;
    const double tolerance = 300;

    SimulatedFlatPanel panel;
    panel.setBrightness(10);

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, target, tolerance));
    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);

    // A few frames of the sequence are taken on target.
    for (int i = 0; i < 3; i++)
    {
        const double adu = panel.measureADU();
        QCOMPARE(calibration.update(panel.brightness, adu, panel.isSaturated(adu)),
                 FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    }

    // Then the light source drifts, e.g. while an LED panel warms up.
    panel.aduAtMaxBrightness *= drift;

    QCOMPARE(runLoop(calibration, panel), FlatBrightnessCalibration::RESULT_TARGET_REACHED);
    QVERIFY(std::fabs(panel.measureADU() - target) <= tolerance);
}

void TestFlatBrightnessCalibration::testFallbackToExposure_data()
{
    QTest::addColumn<int>("minBrightness");
    QTest::addColumn<int>("maxBrightness");
    QTest::addColumn<double>("aduAtMinBrightness");
    QTest::addColumn<double>("aduAtMaxBrightness");
    QTest::addColumn<int>("startBrightness");
    QTest::addColumn<double>("targetADU");
    QTest::addColumn<double>("tolerance");
    QTest::addColumn<int>("expectedResult");
    QTest::addColumn<int>("expectedBrightness");

    // 10000 ADU at full brightness, 40000 wanted: four times the exposure.
    QTest::newRow("too dark at maximum") << 0 << 255 << 0.0 << 10000.0 << 10 << 40000.0 << 500.0
                                         << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_MAXIMUM) << 255;
    // 5000 ADU at the lowest brightness, 1000 wanted: a fifth of the exposure.
    QTest::newRow("too bright at minimum") << 100 << 255 << 5000.0 << 60000.0 << 100 << 1000.0 << 50.0
                                           << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_MINIMUM) << 100;
    // 6000 ADU per step, 32000 wanted: brightness 6 (36000) is the dimmest one that is
    // too bright, the exposure shrinks by a ninth.
    QTest::newRow("between two values") << 0 << 10 << 0.0 << 60000.0 << 0 << 32000.0 << 100.0
                                        << static_cast<int>(FlatBrightnessCalibration::RESULT_FAILED_RESOLUTION) << 6;
}

void TestFlatBrightnessCalibration::testFallbackToExposure()
{
    QFETCH(int, minBrightness);
    QFETCH(int, maxBrightness);
    QFETCH(double, aduAtMinBrightness);
    QFETCH(double, aduAtMaxBrightness);
    QFETCH(int, startBrightness);
    QFETCH(double, targetADU);
    QFETCH(double, tolerance);
    QFETCH(int, expectedResult);
    QFETCH(int, expectedBrightness);

    SimulatedFlatPanel panel;
    panel.minBrightness = minBrightness;
    panel.maxBrightness = maxBrightness;
    panel.aduAtMinBrightness = aduAtMinBrightness;
    panel.aduAtMaxBrightness = aduAtMaxBrightness;
    panel.setBrightness(startBrightness);
    const double sequenceExposure = panel.exposure;

    FlatBrightnessCalibration calibration;
    QVERIFY(calibration.start(panel.minBrightness, panel.maxBrightness, targetADU, tolerance));

    const auto result = runLoop(calibration, panel);
    QCOMPARE(static_cast<int>(result), expectedResult);
    QVERIFY(Ekos::FlatCalibrationMethod::fallsBackToExposure(result));
    // The brightness search itself never touches the exposure.
    QCOMPARE(panel.exposure, sequenceExposure);

    // The light box keeps the dimmest brightness that was too bright, or the brightest
    // one if every frame was too dark, and the exposure is scaled from the ADU measured
    // there.
    QCOMPARE(calibration.fallbackBrightness(), expectedBrightness);
    panel.setBrightness(calibration.fallbackBrightness());
    panel.exposure = Ekos::FlatCalibrationMethod::fallbackExposure(sequenceExposure, calibration.fallbackADU(),
                     calibration.fallbackSaturated(), targetADU, 0.001, 3600);

    QVERIFY(std::fabs(panel.measureADU() - targetADU) <= tolerance);
}

QTEST_GUILESS_MAIN(TestFlatBrightnessCalibration)
