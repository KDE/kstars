/*
    KStars UI tests for meridian flip - special cases.

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "test_ekos_meridianflip_specials.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "Options.h"

TestEkosMeridianFlipSpecials::TestEkosMeridianFlipSpecials(QObject *parent) : TestEkosMeridianFlipBase(parent)
{
}

TestEkosMeridianFlipSpecials::TestEkosMeridianFlipSpecials(QString guider, QObject *parent) : TestEkosMeridianFlipBase(guider, parent)
{
}

void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(15, true, true));

    // start guiding
    QVERIFY(startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(25));

    // expected behavior after the meridian flip
    expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);

    // check if guiding continues (re-calibration necessary)
    QTRY_VERIFY_WITH_TIMEOUT(getGuidingStatus() == Ekos::GUIDE_GUIDING, 75000);

    // check refocusing, that should happen immediately after the guiding calibration
    QVERIFY(checkRefocusing(true));

    // After the first capture dithering should take place
    QVERIFY(checkDithering());

    // check if capturing continues
    QTRY_VERIFY_WITH_TIMEOUT(getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 25000);
}


void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, true, true));

    // start alignment
    QVERIFY(startAligning(5.0));

    // start guiding
    QVERIFY(startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // Let capture run a little bit
    QTest::qWait(5000);

    // switch to capture module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000);

    // stop capturing
    expectedCaptureStates.enqueue(Ekos::CAPTURE_PAUSED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), pauseB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 20000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(40));

    // check if capture remains paused (after a meridian flip it is marked as idle - bug or feature?)
    QTRY_VERIFY_WITH_TIMEOUT(getCaptureStatus() == Ekos::CAPTURE_IDLE, 5000);

    // now finish pause
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);

    // expected behavior after the meridian flip
    expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);

    // check if alignment takes place
    expectedAlignStates.enqueue(Ekos::ALIGN_COMPLETE);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedAlignStates, 60000);

    // check if guiding continues (re-calibration necessary)
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedGuidingStates, 75000);

    // check refocusing, that should happen immediately after the guiding calibration
    QVERIFY(checkRefocusing(true));

    // check if capturing continues
    QTRY_VERIFY_WITH_TIMEOUT(getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 25000);

    // After the first capture dithering should take place
    QVERIFY(checkDithering());
}

void TestEkosMeridianFlipSpecials::testSimpleRepeatedMF()
{
    // slew close to the meridian
    QVERIFY(positionMountForMF(7.0));

    // set the HA to delay the meridian flip by 2 min = 360° / 24 / 30 = 0.5°
    QProcess *indi_setprop = new QProcess(this);
    indi_setprop->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(m_MountDevice).arg(0.5)});

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(10));

    // pier side should still be west pointing east, i.e. no meridian flip took place
    KTRY_GADGET(Ekos::Manager::Instance()->mountModule(), QLabel, pierSideLabel);
    QTRY_VERIFY(pierSideLabel->text() == "Pier Side: West (pointing East)");

    qCInfo(KSTARS_EKOS_TEST()) << "Waiting 4 minutes for a second meridian flip...";
    // expected beginning of the meridian flip
    expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_PLANNED);
    expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_RUNNING);

    // but the pier side should not change, so lets wait for 4 minutes for a second meridian flip
    QVERIFY(checkMFExecuted(4*60+10));

    // set back the HA to delay the meridian flip
    indi_setprop = new QProcess(this);
    indi_setprop->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(m_MountDevice).arg(0)});
}

/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF_data()
{
    prepareTestData({"Red,Green,Blue,Red,Green,Blue"}, {false}, {false}, {true});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF_data()
{
    prepareTestData({"Luminance", "Red,Green,Blue,Red,Green,Blue"}, {false, true}, {false, true}, {false, true});
}

void TestEkosMeridianFlipSpecials::testRepeatedMF_data()
{
    prepareTestData({"Luminance"}, {false}, {false}, {false});
}


QTEST_KSTARS_MAIN_GUIDERSELECT(TestEkosMeridianFlipSpecials)

#endif // HAVE_INDI
