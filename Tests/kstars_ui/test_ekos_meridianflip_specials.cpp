/*
    KStars UI tests for meridian flip - special cases.

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    QVERIFY(prepareCaptureTestcase(40, true, true));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // wait until a flip is planned
    QVERIFY(QTest::qWaitFor([&](){return m_CaptureHelper->expectedMeridianFlipStates.head() != Ekos::Mount::FLIP_PLANNED;}, 60000));

    qCInfo(KSTARS_EKOS_TEST()) << "Meridian flip planned...";
    // guiding deviation leads to a suspended capture
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_SUSPENDED);

    // now send motion north to create a guiding deviation
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->doPulse(RA_INC_DIR, 2000, NO_DIR, 0);
    qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA guiding.";
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 20000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(25));

    // set guards for post MF checks
    // 1. dithering happen after first capture otherwise it is sufficient to wait for start of capturing
    if (dithering_checked)
        m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    else
        m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);

    // 2. ensure that focusing starts
    if (autofocus_checked || refocus_checked)
        m_CaptureHelper->expectedFocusStates.enqueue(Ekos::FOCUS_PROGRESS);

    // check if guiding is running
    if (m_CaptureHelper->use_guiding)
    {
        m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedGuidingStates, 30000);
    }

    // check refocusing, that should happen immediately after the guiding calibration
    // both for in sequence and time based re-focusing
    QVERIFY(checkRefocusing());

    // check if capturing has been started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

    // After the first capture dithering should take place
    QVERIFY(checkDithering());
}

void TestEkosMeridianFlipSpecials::testCaptureGuidingRecalibrationMF()
{
    // use three steps in each direction for calibration
    Options::setAutoModeIterations(3);

    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(30, false, false));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // now enable resetting guiding calibration
    Options::setResetGuideCalibration(true);
    Options::setReuseGuideCalibration(false);

    // start capturing
    QVERIFY(startCapturing());

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(45));

    // check if guiding calibration is executed
    m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_CALIBRATING);
    m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_CALIBRATION_SUCCESS);
    m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    // check if capturing starts right now
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.isEmpty(), 120000);
    // check if calibration was finished
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedGuidingStates.isEmpty(), 30000);
}


void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(15, true, false));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // check if single capture completes correctly
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 21000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(25));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}


void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, true, false));

    // start alignment
    QVERIFY(startAligning(5.0));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // Let capture run a little bit
    QTest::qWait(5000);

    // switch to capture module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000);

    // stop capturing
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_PAUSED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), pauseB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 20000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(40));

    // check if capture remains paused (after a meridian flip it is marked as idle - bug or feature?)
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_IDLE, 5000);

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
    QVERIFY(prepareCaptureTestcase(80, true, false));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // expect focusing starts and aborts within 90 secends
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 90000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(40));

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testSchedulerCaptureMF()
{
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(15, false, Ekos::Scheduler::ALGORITHM_GREEDY, SchedulerJob::FINISH_LOOP, 1));
    // start the scheduled procedure
    QVERIFY(startScheduler());
    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(120));
    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testAbortSchedulerRefocusMF()
{
    // select suspend guiding
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->focusModule(), suspendGuideCheck, true);
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(30, true, Ekos::Scheduler::ALGORITHM_CLASSIC, SchedulerJob::FINISH_LOOP, 1));
    // update the initial focuser position
    KTRY_GADGET(Ekos::Manager::Instance()->focusModule(), QLineEdit, absTicksLabel);
    initialFocusPosition = absTicksLabel->text().toInt();
    // start the scheduled procedure
    QVERIFY(startScheduler());

    // expect focusing starts and aborts within 90 secends
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 120000);
    // check if the focuser moved back to the last known focus position
    // moving back should be finished 5 secs after focusing aborted
    QTRY_VERIFY2_WITH_TIMEOUT(initialFocusPosition == absTicksLabel->text().toInt(),
                              QString("Focuser is at position %1 instead of initial focus position %2")
                              .arg(absTicksLabel->text()).arg(initialFocusPosition).toLocal8Bit(), 5000);

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
    indi_setprop->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(m_CaptureHelper->m_MountDevice).arg(0.5)});

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(10));

    // pier side should still be west pointing east, i.e. no meridian flip took place
    KTRY_GADGET(Ekos::Manager::Instance()->mountModule(), QLabel, pierSideLabel);
    QTRY_VERIFY(pierSideLabel->text() == "Pier Side: West (pointing East)");

    qCInfo(KSTARS_EKOS_TEST()) << "Waiting 4 minutes for a second meridian flip...";
    // expected beginning of the meridian flip
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_PLANNED);
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_RUNNING);

    // but the pier side should not change, so lets wait for 4 minutes for a second meridian flip
    QVERIFY(checkMFExecuted(4*60+10));

    // set back the HA to delay the meridian flip
    indi_setprop = new QProcess(this);
    indi_setprop->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(m_CaptureHelper->m_MountDevice).arg(0)});
}


/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlipSpecials::testCaptureGuidingDeviationMF_data()
{
    prepareTestData(45.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {false, true}, {false, true}, {true}, {false, true});
}

void TestEkosMeridianFlipSpecials::testCaptureGuidingRecalibrationMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}, {"Red,Green,Blue,Red,Green,Blue", 1}}, {false}, {false}, {true}, {false});
}

void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Red,Green,Blue,Red,Green,Blue", 1}}, {false}, {false}, {true}, {true});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {false, true}, {false, true}, {true}, {false, true});
}

void TestEkosMeridianFlipSpecials::testAbortRefocusMF_data()
{
    prepareTestData(25.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {true}, {false}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testSchedulerCaptureMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 1}}, {false}, {false}, {true, false}, {false});
}

void TestEkosMeridianFlipSpecials::testAbortSchedulerRefocusMF_data()
{
    prepareTestData(30.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {true}, {false}, {true, false}, {false});
}

void TestEkosMeridianFlipSpecials::testSimpleRepeatedMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {false}, {false}, {false}, {false});
}


QTEST_KSTARS_WITH_GUIDER_MAIN(TestEkosMeridianFlipSpecials)

#endif // HAVE_INDI
