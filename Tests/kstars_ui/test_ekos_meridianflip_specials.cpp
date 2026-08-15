/*
    KStars UI tests for meridian flip - special cases.

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_meridianflip_specials.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "Options.h"
#include "ekos/capture/capture.h"
#include "ekos/focus/focusmodule.h"

#include <QScopeGuard>

TestEkosMeridianFlipSpecials::TestEkosMeridianFlipSpecials(QObject *parent) : TestEkosMeridianFlipBase(parent)
{
}

TestEkosMeridianFlipSpecials::TestEkosMeridianFlipSpecials(QString guider,
        QObject *parent) : TestEkosMeridianFlipBase(guider, parent)
{
}

void TestEkosMeridianFlipSpecials::testCaptureGuidingDeviationMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(40, true, false));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // wait until a flip is planned
    QVERIFY(QTest::qWaitFor([&]()
    {
        return m_CaptureHelper->expectedMeridianFlipStates.head() != Ekos::MeridianFlipState::MOUNT_FLIP_PLANNED;
    }, 60000));

    qCInfo(KSTARS_EKOS_TEST()) << "Meridian flip planned...";
    // guiding deviation leads to a suspended capture
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_SUSPENDED);

    // now send motion north to create a guiding deviation
    Ekos::Manager::Instance()->mountModule()->doPulse(RA_INC_DIR, 2000, DEC_INC_DIR, 2000);
    qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA+DEC guiding.";
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
    if (refocus_checked)
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
    QVERIFY(prepareCaptureTestcase(15, false, false));

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
    QVERIFY(prepareCaptureTestcase(40, false, false));

    // start alignment
    QVERIFY(executeAlignment(5.0));

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
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_PAUSED, 5000);

    // Lets wait a little bit
    QTest::qWait(5000);

    // now finish pause
    qCInfo(KSTARS_EKOS_TEST) << "Finishing paused capture... ";
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}


void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPauseMFPlanned()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(10, false, false));

    // set a high delay so that it does not start too early
    QVERIFY(enableMeridianFlip(120.0));

    // start alignment
    QVERIFY(executeAlignment(5.0));

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // switch to capture module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000);

    // start capturing
    QVERIFY(startCapturing());

    // reset the MF delay after capturing has started
    QVERIFY(enableMeridianFlip(0.0));

    // Wait until the meridian flip has been planned
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getMeridianFlipStatus() == Ekos::MeridianFlipState::MOUNT_FLIP_PLANNED, 60000);

    // pause capturing
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_PAUSED);
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), pauseB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 40000);

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(40));

    // check if capture remains paused (after a meridian flip it is marked as idle - bug or feature?)
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_PAUSED, 5000);

    // Lets wait a little bit
    QTest::qWait(5000);

    // now finish pause
    qCInfo(KSTARS_EKOS_TEST) << "Finishing paused capture... ";
    KTRY_CLICK(Ekos::Manager::Instance()->captureModule(), startB);

    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testAbortRefocusMF()
{
    // set up the capture sequence
    QVERIFY(prepareCaptureTestcase(20, false, false));
    // refocus every 1min
    KTRY_SET_SPINBOX(Ekos::Manager::Instance()->captureModule(), refocusEveryN, 1);
    // add additional 5 degrees for delay to prevent a meridian flip before focusing starts
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->mountModule(), meridianFlipOffsetDegrees, 5.0);

    // start guiding
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // start capturing
    QVERIFY(startCapturing());

    // expect focusing starts and aborts
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);

    // wait until focusing starts
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_PROGRESS, 90000);
    // trigger the meridian flip by clearing the offset
    meridianFlipOffsetDegrees->setValue(0.0);
    qCInfo(KSTARS_EKOS_TEST) << "Meridian flip offset cleared.";
    // expect focus abort due to started meridian flip
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 90000);
    qCInfo(KSTARS_EKOS_TEST) << "Focusing aborted.";

    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(40));
    // do not expect focusing to restart after the flip
    refocus_checked = false;
    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testSchedulerCaptureMF()
{
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(15, false, Ekos::FINISH_LOOP, 1));
    // start the scheduled procedure
    QVERIFY(startScheduler());
    // check if meridian flip runs and completes successfully
    QVERIFY(checkMFExecuted(120));
    // Now check if everything continues as it should be
    QVERIFY(checkPostMFBehavior());
}

void TestEkosMeridianFlipSpecials::testAbortSchedulerRefocusMF()
{
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(20, false, Ekos::FINISH_LOOP, 1));
    // update the initial focuser position
    KTRY_GADGET(Ekos::Manager::Instance()->focusModule()->mainFocuser().get(), QLineEdit, absTicksLabel);
    initialFocusPosition = absTicksLabel->text().toInt();
    // start the scheduled procedure
    QVERIFY(startScheduler());
    // add additional 5 degrees for delay to prevent a meridian flip before focusing starts
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->mountModule(), meridianFlipOffsetDegrees, 5.0);

    // expect focusing starts and aborts
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);

    // wait until focusing starts
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_PROGRESS, 90000);
    // trigger the meridian flip by clearing the offset after 1 sec
    QTest::qWait(1000.0);
    meridianFlipOffsetDegrees->setValue(0.0);
    qCInfo(KSTARS_EKOS_TEST) << "Meridian flip offset cleared.";
    // expect focus abort due to started meridian flip
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 90000);
    qCInfo(KSTARS_EKOS_TEST) << "Focusing aborted.";
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
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_PLANNED);
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_RUNNING);

    // but the pier side should not change, so lets wait for 4 minutes for a second meridian flip
    QVERIFY(checkMFExecuted(4 * 60 + 10));

    // set back the HA to delay the meridian flip
    indi_setprop = new QProcess(this);
    indi_setprop->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(m_CaptureHelper->m_MountDevice).arg(0)});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignFailedMFRetry()
{
    if (!astrometry_available)
        QSKIP("No astrometry files available to run test");

    // set up the capture sequence - leave enough lead time for the initial alignment
    // cycle below to complete before the mount reaches the meridian
    QVERIFY(prepareCaptureTestcase(40, false, false));

    // run one alignment cycle so that post-flip re-alignment gets triggered automatically
    // (CameraState::setAlignState() sets resumeAlignmentAfterFlip to true on every align
    // state change, so a single completed alignment before the flip is enough)
    QVERIFY(executeAlignment(5.0));

    // widen align's post-slew settle window so it doesn't race ahead to a second solve
    // before the HA reset injected below has had a chance to take effect. Restored via
    // scope guard so it resets even if a QVERIFY below fails and returns early.
    KTRY_GADGET(Ekos::Manager::Instance()->alignModule(), QSpinBox, alignSettlingTime);
    const int originalSettlingTime = alignSettlingTime->value();
    alignSettlingTime->setValue(5000);
    const auto settlingTimeGuard = qScopeGuard([alignSettlingTime, originalSettlingTime]
    {
        alignSettlingTime->setValue(originalSettlingTime);
    });

    // force the simulated mount to not actually change pier side on the upcoming flip slew,
    // so that the flip "fails" and the 4-minute retry logic in MeridianFlipState kicks in.
    // Reset via scope guard for the same reason as above.
    const QString mountDevice = m_CaptureHelper->m_MountDevice;
    QProcess *indi_setprop = new QProcess(this);
    indi_setprop->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(mountDevice).arg(0.5)});
    const auto flipHaGuard = qScopeGuard([this, mountDevice]
    {
        QProcess *reset = new QProcess(this);
        reset->start(QString("indi_setprop"), {QString("-n"), QString("%1.FLIP_HA.FLIP_HA=%2").arg(mountDevice).arg(0)});
        reset->waitForFinished(5000);
    });

    // start capturing
    QVERIFY(startCapturing());

    // check that the (failed) meridian flip runs and "completes" (pier side unchanged)
    // (39s: matches the 40s lead time set in prepareCaptureTestcase() above, minus the
    // time already spent on the initial alignment cycle)
    QVERIFY(checkMFExecuted(39));
    KTRY_GADGET(Ekos::Manager::Instance()->mountModule(), QLabel, pierSideLabel);
    QTRY_VERIFY(pierSideLabel->text() == "Pier Side: West (pointing East)");
    qCInfo(KSTARS_EKOS_TEST()) << "First (failed) meridian flip completed, pier side unchanged as expected.";

    // post-flip re-alignment should start right away despite the failed flip
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ALIGNING, 15000);
    qCInfo(KSTARS_EKOS_TEST()) << "Post-flip re-alignment started after the failed flip.";

    // Between reaching Tracking (above, from the failed flip) and align's own correction
    // slew, re-sync the simulated mount just PAST the (freshly calculated) meridian again
    // (negative secsToMF -> target.ra = LST - |delta| -> ha = +|delta|). On real mounts the
    // hour angle keeps advancing naturally while align captures and solves; the telescope
    // simulator does not reproduce that drift on its own, so it has to be emulated
    // explicitly here to recreate the same situation as on real hardware.
    // Note: fast=false on purpose - the fast=true path adds a fixed +0.002h (~7.2s) pre-
    // meridian pad meant for initial rough positioning, which would swamp the small delta
    // used here and push ha negative again.
    findMFTestTarget(-5, false);
    Ekos::Manager::Instance()->mountModule()->sync(target->ra().Hours(), target->dec().Degrees());
    qCInfo(KSTARS_EKOS_TEST()) << "Re-synced just past the meridian before align's correction slew.";

    // Continuously watch for the actual bug condition from here on: MeridianFlipState
    // re-arming a flip (leaving MOUNT_FLIP_NONE) while Capture is still mid post-flip
    // re-alignment (CAPTURE_ALIGNING). A single point-in-time check after the fact is
    // unreliable - the erroneous re-arm and its consequent (fast, since flipDelayHrs was
    // wiped to 0) flip execution can both finish before any single QVERIFY below gets a
    // chance to observe the intermediate state - so this is recorded live via a direct
    // signal connection instead of polled after the fact.
    bool prematureFlipDetected = false;
    const auto mfState = Ekos::Manager::Instance()->mountModule()->getMeridianFlipState();
    const auto prematureFlipConnection = connect(mfState.get(), &Ekos::MeridianFlipState::newMountMFStatus, this,
                                         [&](Ekos::MeridianFlipState::MeridianFlipMountState status)
    {
        if (m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ALIGNING &&
                status != Ekos::MeridianFlipState::MOUNT_FLIP_NONE)
            prematureFlipDetected = true;
    });
    const auto prematureFlipGuard = qScopeGuard([prematureFlipConnection]
    {
        QObject::disconnect(prematureFlipConnection);
    });

    // Force a real, sizeable correction slew: nudge the mount off the synced position via a
    // manual motion command (TELESCOPE_MOTION_NS), so align's next solve finds a real position
    // error and issues its own genuine coordinate slew to correct it - as opposed to relying
    // on align's solve noise alone, which may leave too small an offset for a reliably
    // observable slew (see the commented-out attempt in startScheduler() above: "slewing
    // detection unsure since the position is close to the target").
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_SLEWING);
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_TRACKING);
    Ekos::Manager::Instance()->mountModule()->motionCommand(ISD::Mount::MOTION_START, ISD::Mount::MOTION_NORTH, -1);
    QTest::qWait(2000);
    Ekos::Manager::Instance()->mountModule()->motionCommand(ISD::Mount::MOTION_STOP, ISD::Mount::MOTION_NORTH, -1);
    qCInfo(KSTARS_EKOS_TEST()) << "Nudged the mount off target to force a real correction slew.";

    // Verify align's correction slew actually passed through MOUNT_SLEWING and MOUNT_TRACKING
    // in that order - otherwise a PASS below would be meaningless (it could just mean the
    // trigger never occurred, not that the bug is fixed).
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMountStates, 60000);
    qCInfo(KSTARS_EKOS_TEST()) << "Confirmed mount passed through MOUNT_SLEWING and MOUNT_TRACKING.";

    // Let alignment run through its solve-and-correct cycle(s), triggered by the nudge above,
    // until post-flip re-alignment is done (capture leaves CAPTURE_ALIGNING). This covers the
    // window in which MeridianFlipState::updateTelescopeCoord() may incorrectly reset the
    // pending 4-minute retry delay (flipDelayHrs) as a side effect of align's own correction
    // slew finishing, because it only checks meridianFlipMountState (which already dropped
    // back to MOUNT_FLIP_NONE) instead of meridianFlipStage / checkMeridianFlipActive()
    // (which is still MF_ALIGNING throughout this window).
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() != Ekos::CAPTURE_ALIGNING, 60000);
    qCInfo(KSTARS_EKOS_TEST()) << "Post-flip re-alignment (including align's correction slew) completed.";

    // Stop watching now that re-alignment is done - the retry delay must not have been wiped
    // out while it was still running: a meridian flip must not have been (re-)armed this
    // early, it is only due after ~4 minutes.
    QObject::disconnect(prematureFlipConnection);
    QVERIFY2(!prematureFlipDetected,
             "Meridian flip was (re-)armed while post-flip alignment was still running - "
             "the pending retry delay was reset too early.");

    // it should still complete correctly after the full ~4 minute delay
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_PLANNED);
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_RUNNING);
    QVERIFY(checkMFExecuted(4 * 60 + 30));

    // FLIP_HA and alignSettlingTime are restored automatically by the scope guards above,
    // regardless of whether this point is reached normally or a QVERIFY above failed.
}

void TestEkosMeridianFlipSpecials::testCaptureRealignMF()
{
    if (!astrometry_available)
        QSKIP("No astrometry files available to run test");

    // prepare for alignment tests
    m_CaptureHelper->prepareAlignmentModule();
    // enforce re-alignment
    Options::setAlignCheckFrequency(1);
    Options::setAlignCheckThreshold(0.0);
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(17, true, Ekos::FINISH_REPEAT, 1));
    // start the scheduled procedure
    QVERIFY(startScheduler());
    // make the alignment exposure so long that the flip happens while capturing the frame for alignment
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 60000);
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->alignModule(), alignExposure, 60.0);
    qCInfo(KSTARS_EKOS_TEST()) << "Setting alignment exposure to 60s.";
    // check if meridian flip has been started
    QVERIFY(checkMFStarted(120));
    // set the alignment exposure time back
    alignExposure->setValue(5.0);
    qCInfo(KSTARS_EKOS_TEST()) << "Setting alignment exposure back to 5s.";
    // check if meridian flip has been completed
    QVERIFY(checkMFExecuted(120));
    // Now check if after the flip everything continues as it should be
    QVERIFY(checkPostMFBehavior());
    // check if capturing starts right now
    QFETCH(double, exptime);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_CAPTURING, (int)(2 * exptime * 1000));
    qCInfo(KSTARS_EKOS_TEST()) << "Capturing started.";
    // check if an image has been captured
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.isEmpty(), 30000);
}

void TestEkosMeridianFlipSpecials::testCapturePostRealignmentFailedHandling()
{
    if (!astrometry_available)
        QSKIP("No astrometry files available to run test");

    // prepare for alignment tests
    m_CaptureHelper->prepareAlignmentModule();
    // setup the scheduler
    QVERIFY(prepareSchedulerTestcase(17, true, Ekos::FINISH_REPEAT, 1));
    // start the scheduled procedure
    QVERIFY(startScheduler());
    // check if meridian flip has been started
    QVERIFY(checkMFStarted(120));
    // Create massive noise such that solving fails
    KTRY_INDI_PROPERTY(m_CaptureHelper->m_CCDDevice, "Simulator Config", "SIMULATOR_SETTINGS", ccd_settings);
    INDI_E *noise_setting = ccd_settings->getElement("SIM_NOISE");
    QVERIFY(ccd_settings != nullptr);
    noise_setting->setValue(100.0);
    ccd_settings->processSetButton();
    // set the alignment exposure so low that alignment fails
    KTRY_SET_DOUBLESPINBOX(Ekos::Manager::Instance()->alignModule(), alignExposure, 0.1);
    // check if meridian flip has been completed
    QVERIFY(checkMFExecuted(120));
    // expect 4 failed alignments (normal, blind solve + 2x retrying)
    for (int i = 0; i < 4; i++)
        m_CaptureHelper->expectedAlignStates.enqueue(Ekos::ALIGN_FAILED);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedAlignStates.isEmpty(), 30000);
}


/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlipSpecials::testCaptureGuidingDeviationMF_data()
{
    prepareTestData(45.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {true}, {false, true});
}

void TestEkosMeridianFlipSpecials::testCaptureGuidingRecalibrationMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}, {"Red,Green,Blue,Red,Green,Blue", 1}}, {0}, {true}, {false});
}

void TestEkosMeridianFlipSpecials::testCaptureDitheringDelayedAfterMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Red,Green,Blue,Red,Green,Blue", 1}}, {0}, {true}, {true});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPausedMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {true}, {false});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignGuidingPauseMFPlanned_data()
{
    prepareTestData(12.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {true}, {false});
}

void TestEkosMeridianFlipSpecials::testAbortRefocusMF_data()
{
    prepareTestData(32.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {1}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testSchedulerCaptureMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 1}}, {0}, {true, false}, {false});
}

void TestEkosMeridianFlipSpecials::testAbortSchedulerRefocusMF_data()
{
    prepareTestData(30.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {1}, {true, false}, {false});
}

void TestEkosMeridianFlipSpecials::testSimpleRepeatedMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testCaptureAlignFailedMFRetry_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testCaptureRealignMF_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {false}, {false});
}

void TestEkosMeridianFlipSpecials::testCapturePostRealignmentFailedHandling_data()
{
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {false}, {false});
}


QTEST_KSTARS_WITH_GUIDER_MAIN(TestEkosMeridianFlipSpecials)

#endif // HAVE_INDI
