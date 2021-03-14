/*
    KStars UI tests for meridian flip

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_meridianflip.h"

#if defined(HAVE_INDI)

#include <qdir.h>

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"

#include "auxiliary/dms.h"
#include "ksutils.h"
#include "ekos/guide/internalguide/gmath.h"
#include "Options.h"

/* *****************************************************************************
 *
 * Properly executing a meridian flip and continuing afterwards in exactly the
 * same way as before is a challenge:
 * - If no capturing is running, it should simply take place.
 * - If capturing is running, the meridian flip should wait until the current
 *   frame is captured, capturing is suspended, the meridian flip takes place
 *   and capturing continues where it was suspended. This needs to be tested if
 *   a meridian flip happens in the middle of a capture sequence or between two
 *   of them. Therefore, all capture tests are executed for a LLL sequence and
 *   a RGB sequence. In addition:
 *   - if guiding was active, guiding should be activated again after the meridian
 *     flip and afterwards guiding should continue
 *   - if an alignment has taken place after the last slew, a re-alignment should
 *     be executed after the flip and before guiding and capturing.
 *   All this could be combined with the pre-capturing actions of focusing and
 *   dithering.
 *
 * The tests are available both for the internal guider and for PHD2. For testing
 * with PHD2, invoke the test with the arguments -guider PHD2.
 * ***************************************************************************** */


TestEkosMeridianFlip::TestEkosMeridianFlip(QObject *parent) : TestEkosMeridianFlipBase(parent)
{
}

TestEkosMeridianFlip::TestEkosMeridianFlip(QString guider, QObject *parent) : TestEkosMeridianFlipBase(guider, parent)
{
}


void TestEkosMeridianFlip::testSimpleMF()
{
    // slew close to the meridian
    QVERIFY(positionMountForMF(7.0));

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(10));
}


void TestEkosMeridianFlip::testSimpleMFDelay()
{
    // switch to mount module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule(), 1000);

    double delay = 12.0;
    // activate meridian flip
    QVERIFY(enableMeridianFlip(delay));

    // slew close to the meridian
    QVERIFY(positionMountForMF(7.0));

    // check that the meridian flip is distance is in the expected corridor
    int distance = secondsToMF();
    QTRY_VERIFY(delay < distance && distance < delay + 7);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(static_cast<int>(delay) + 7));
}


void TestEkosMeridianFlip::testGuidingMF()
{
    // slew close to the meridian
    QVERIFY(positionMountForMF(75.0));

    // start guiding
    QVERIFY(startGuiding(2.0));

    // expected guiding behavior during the meridian flip
    expectedGuidingStates.enqueue(Ekos::GUIDE_ABORTED);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(80));
    // meridian flip should have been aborted
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedGuidingStates, 0.0);

    // check if guiding has NOT been restarted (since we are not capturing
    QTRY_VERIFY_WITH_TIMEOUT(getGuidingStatus() == Ekos::GUIDE_IDLE || getGuidingStatus() == Ekos::GUIDE_ABORTED, 10000);
}


void TestEkosMeridianFlip::testCaptureMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, true, false));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    // expect one additional captures to ensure refocusing after the flip
    if (refocus_checked)
        expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);

    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, refocus_checked?61000:21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(15));

    // expect at least one capture
    expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);

    // check refocusing
    QVERIFY(checkRefocusing(false));

    // check if capturing continues
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 5000);
}


void TestEkosMeridianFlip::testCaptureMFAbortWaiting()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, false, false));

    // start capturing
    QVERIFY(startCapturing());

    // Let capture run a little bit
    QTest::qWait(5000);

    // stop capturing
    expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 2000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(18));

    // wait for settling
    QTest::qWait(2000);

    // check if capturing remains aborted
    QVERIFY(getCaptureStatus() == Ekos::CAPTURE_IDLE);
}


void TestEkosMeridianFlip::testCaptureMFAbortFlipping()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, false, false));

    // start capturing
    QVERIFY(startCapturing());

    // check if the meridian flip starts running
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedMeridianFlipStates, 22000);

    // Let capture run a little bit
    QTest::qWait(2000);

    // stop capturing
    expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 2000);

    // check if the meridian flip is completed latest after one minute
    expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_COMPLETED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedMeridianFlipStates, 60000);

    // wait for settling
    QTest::qWait(2000);

    // check if capturing remains aborted
    QVERIFY(getCaptureStatus() == Ekos::CAPTURE_IDLE);
}


void TestEkosMeridianFlip::testCaptureGuidingMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(15, true, true, false));

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

    // check if capturing continues
    QTRY_VERIFY_WITH_TIMEOUT(getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 20000);

    // After the first capture dithering should take place
    QVERIFY(checkDithering());
}


void TestEkosMeridianFlip::testCaptureAlignMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(45, false, true, false));

    // start alignment
    QVERIFY(startAligning(5.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(25));

    // expected behavior after the meridian flip
    expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);

    // check if alignment takes place
    QTRY_VERIFY_WITH_TIMEOUT(getAlignStatus() == Ekos::ALIGN_COMPLETE, 60000);

    // check refocusing
    QVERIFY(checkRefocusing(false));

    // check if capturing continues
    QTRY_VERIFY_WITH_TIMEOUT(getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 20000);
}


void TestEkosMeridianFlip::testCaptureAlignGuidingMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, true, true, false));

    // start alignment
    QVERIFY(startAligning(5.0));

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



/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlip::testSimpleMF_data()
{
    prepareTestData(18.0, {"Luminance"}, {false}, {false}, {false});
}

void TestEkosMeridianFlip::testSimpleMFDelay_data()
{
    testSimpleMF_data();
}

void TestEkosMeridianFlip::testGuidingMF_data()
{
    prepareTestData(18.0, {"Luminance"}, {false}, {false}, {false});
}

void TestEkosMeridianFlip::testCaptureMF_data()
{
    prepareTestData(18.0, {"Luminance", "Red,Green,Blue,Red,Green,Blue"}, {false, true}, {false, true}, {false});
}

void TestEkosMeridianFlip::testCaptureMFAbortWaiting_data()
{
    // no tests for focusing and dithering
    prepareTestData(18.0, {"Luminance", "Red,Green,Blue,Red,Green,Blue"}, {false}, {false}, {false});
}

void TestEkosMeridianFlip::testCaptureMFAbortFlipping_data()
{
    testCaptureMFAbortWaiting_data();
}

void TestEkosMeridianFlip::testCaptureGuidingMF_data()
{
    prepareTestData(18.0, {"Luminance", "Red,Green,Blue,Red,Green,Blue"}, {false, true}, {false, true}, {false, true});
}

void TestEkosMeridianFlip::testCaptureAlignMF_data()
{
    testCaptureMF_data();
}

void TestEkosMeridianFlip::testCaptureAlignGuidingMF_data()
{
    testCaptureGuidingMF_data();
}

QTEST_KSTARS_MAIN_GUIDERSELECT(TestEkosMeridianFlip)

#endif // HAVE_INDI
