/*
    KStars UI tests for meridian flip

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
#include "ekos/capture/capture.h"

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

    // check if meridian flip runs and completes successfully (15 secs buffer for startup)
    QVERIFY(checkMFStarted(7 + 15));
}


void TestEkosMeridianFlip::testSimpleMFDelay()
{
    // switch to mount module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule(), 1000);

    double delay = 4.8;
    // activate meridian flip
    QVERIFY(enableMeridianFlip(delay));

    // slew close to the meridian
    QVERIFY(positionMountForMF(7.0));

    // check that the meridian flip is distance is in the expected corridor
    int distance = secondsToMF();
    qCInfo(KSTARS_EKOS_TEST()) << distance << "seconds to meridian flip...";
    QTRY_VERIFY(delay < distance && distance < delay + 7);

    // check if meridian flip runs and completes successfully (15 secs buffer for startup)
    QVERIFY(checkMFStarted(static_cast<int>(delay) + 7 + 15));
}


void TestEkosMeridianFlip::testGuidingMF()
{
    // prepare the test case and calibrate
    QVERIFY(prepareMFTestcase(false, false));

    // slew close to the meridian
    QVERIFY(positionMountForMF(75.0));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // expected guiding behavior during the meridian flip
    m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_ABORTED);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(80));
    // meridian flip should have been aborted
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedGuidingStates, 0.0);

    // check if guiding has NOT been restarted (since we are not capturing
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuidingStatus() == Ekos::GUIDE_IDLE
                             || m_CaptureHelper->getGuidingStatus() == Ekos::GUIDE_ABORTED, 10000);
}


void TestEkosMeridianFlip::testCaptureMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, false));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);

    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, refocus_checked ? 61000 : 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(15));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}


void TestEkosMeridianFlip::testCaptureMFAbortWaiting()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, false));

    // start capturing
    QVERIFY(startCapturing());

    // Let capture run a little bit
    QTest::qWait(5000);

    // stop capturing
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 2000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(18));

    // wait for settling
    QTest::qWait(2000);

    // check if capturing remains aborted
    QVERIFY(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_IDLE);
}


void TestEkosMeridianFlip::testCaptureMFAbortFlipping()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, false));

    // start capturing
    QVERIFY(startCapturing());

    // check if the meridian flip starts running
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMeridianFlipStates, 22000);

    // Let capture run a little bit
    QTest::qWait(2000);

    // stop capturing
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 2000);

    // check if the meridian flip is completed latest after one minute
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_COMPLETED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMeridianFlipStates, 60000);

    // wait for settling
    QTest::qWait(2000);

    // check if capturing remains aborted
    QVERIFY(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_IDLE);
}


void TestEkosMeridianFlip::testCaptureGuidingMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(20, false, false));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(45));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}


void TestEkosMeridianFlip::testCaptureAlignMF()
{
    if (!astrometry_available)
        QSKIP("No astrometry files available to run test");

    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, false, false));

    // start alignment
    QVERIFY(executeAlignment(5.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(39));

    // Now check if everything continues as it should be
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    QVERIFY(checkPostMFBehavior());

    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 20000);
}


void TestEkosMeridianFlip::testCaptureAlignGuidingMF()
{
    if (!astrometry_available)
        QSKIP("No astrometry files available to run test");

    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, false, false));

    // execute alignment
    QVERIFY(executeAlignment(5.0));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(60));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlip::testRefocusAfterMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(15, false, false));

    // start guiding if configured
    QFETCH(bool, guide);
    if (guide)
        QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(45));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}



/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlip::testSimpleMF_data()
{
    prepareTestData(18.0, {"Greenwich", "Reykjavik", "San Diego", "Hilo", "Hong Kong", "Kuwait City"}, {true, false}, {{"Luminance", 6}}, {0}, {false}, {false});
}

void TestEkosMeridianFlip::testSimpleMFDelay_data()
{
    testSimpleMF_data();
}

void TestEkosMeridianFlip::testGuidingMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {false}, {true}, {false});
}

void TestEkosMeridianFlip::testCaptureMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}, {"Red,Green,Blue,Red,Green,Blue", 1}}, {0}, {false}, {false});
}

void TestEkosMeridianFlip::testCaptureMFAbortWaiting_data()
{
    // no tests for focusing and dithering
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}, {"Red,Green,Blue,Red,Green,Blue", 1}}, {0}, {false}, {false});
}

void TestEkosMeridianFlip::testCaptureMFAbortFlipping_data()
{
    testCaptureMFAbortWaiting_data();
}

void TestEkosMeridianFlip::testCaptureGuidingMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}, {"Red,Green,Blue,Red,Green,Blue", 1}}, {0}, {true}, {false, true});
}

void TestEkosMeridianFlip::testCaptureAlignMF_data()
{
    testCaptureMF_data();
}

void TestEkosMeridianFlip::testCaptureAlignGuidingMF_data()
{
    testCaptureGuidingMF_data();
}

void TestEkosMeridianFlip::testRefocusAfterMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {3}, {false}, {false});
}

QTEST_KSTARS_WITH_GUIDER_MAIN(TestEkosMeridianFlip)

#endif // HAVE_INDI
