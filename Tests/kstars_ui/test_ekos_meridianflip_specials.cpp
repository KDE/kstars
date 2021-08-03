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

void TestEkosMeridianFlipSpecials::testCaptureGuidingDeviationMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, true, true, true));

    // start guiding
    QVERIFY(startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // wait until a flip is planned
    QVERIFY(QTest::qWaitFor([&](){return expectedMeridianFlipStates.head() != Ekos::Mount::FLIP_PLANNED;}, 60000));

    qCInfo(KSTARS_EKOS_TEST()) << "Meridian flip planned...";
    // guiding deviation leads to a suspended capture
    expectedCaptureStates.enqueue(Ekos::CAPTURE_SUSPENDED);

    // now send motion north to create a guiding deviation
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->doPulse(RA_INC_DIR, 2000, NO_DIR, 0);
    qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA guiding.";
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 10000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(25));

    // set guards for post MF checks
    // autofocus (= insequence focusing) and dithering happen after first capture
    // otherwise it is sufficient to wait for start of capturing
    if (autofocus_checked || dithering_checked)
        expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    else
        expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);

    // check if guiding is running
    if (use_guiding)
    {
        expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedGuidingStates, 30000);
    }

    // check refocusing, that should happen immediately after the guiding calibration
    // exception: in sequence focusing needs one capture, since the guiding deviation
    // has interrupted the first capture
    if (autofocus_checked == false)
        QVERIFY(checkRefocusing());

    // check if capturing has been started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedCaptureStates, 60000);

    // After the first capture dithering should take place
    QVERIFY(checkDithering());

    // in sequence focusing check
    if (autofocus_checked == true)
        QVERIFY(checkRefocusing());
}

void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF()
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

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}


void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, true, true, false));

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

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testAbortRefocusMF()
{
    // select suspend guiding
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->focusModule(), suspendGuideCheck, true);
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(80, true, true, false));

    // start guiding
    QVERIFY(startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // expect focusing starts and aborts within 90 secends
    expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedFocusStates, 90000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(40));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testAbortSchedulerRefocusMF()
{
    // select suspend guiding
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->focusModule(), suspendGuideCheck, true);
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(30, true, true, SchedulerJob::FINISH_LOOP, 1));
    QVERIFY(startScheduler());


    // expect focusing starts and aborts within 90 secends
    expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedFocusStates, 120000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(120));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
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

void TestEkosMeridianFlipSpecials::testCaptureGuidingDeviationMF_data()
{
    prepareTestData(45.0, {"Greenwich"}, {true}, {"Luminance"}, {false, true}, {false, true}, {false, true});
}

void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {"Red,Green,Blue,Red,Green,Blue"}, {false}, {false}, {true});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {"Luminance"}, {false, true}, {false, true}, {false, true});
}

void TestEkosMeridianFlipSpecials::testAbortRefocusMF_data()
{
    prepareTestData(25.0, {"Greenwich"}, {true}, {"Luminance"}, {true}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testAbortSchedulerRefocusMF_data()
{
    prepareTestData(30.0, {"Greenwich"}, {true}, {"Luminance"}, {true}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testRepeatedMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {"Luminance"}, {false}, {false}, {false});
}


QTEST_KSTARS_MAIN_GUIDERSELECT(TestEkosMeridianFlipSpecials)

#endif // HAVE_INDI
