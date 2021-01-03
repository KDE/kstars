/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_focus.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "Options.h"

TestEkosFocus::TestEkosFocus(QObject *parent) : QObject(parent)
{
}

void TestEkosFocus::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosFocus::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosFocus::init()
{

}

void TestEkosFocus::cleanup()
{

}

void TestEkosFocus::testCaptureStates()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Pre-configure steps
    KTRY_FOCUS_MOVETO(40000);

    // Prepare to detect state change
    QList <Ekos::FocusState> state_list;
    auto state_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newStatus, this, [&](Ekos::FocusState s) {
        state_list.append(s);
    });
    QVERIFY(state_handler);

    // Configure some fields, capture, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(2, 1, 99);
    QTRY_COMPARE_WITH_TIMEOUT(state_list.count(), 2, 5000);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_IDLE);
    state_list.clear();

    // Move step value, expect no capture
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_MOVETO(43210);
    QTest::qWait(1000);
    QCOMPARE(state_list.count(), 0);

    KTRY_FOCUS_GADGET(QPushButton, startLoopB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);

    // Loop captures, abort, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_CLICK(startLoopB);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 1, 5000);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 4, 5000);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_FRAMING);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[2], Ekos::FocusState::FOCUS_ABORTED);
    QCOMPARE(state_list[3], Ekos::FocusState::FOCUS_IDLE);
    state_list.clear();

    KTRY_FOCUS_GADGET(QCheckBox, useAutoStar);
    KTRY_FOCUS_GADGET(QPushButton, resetFrameB);

    QWARN("This test does not wait for the hardcoded timeout to select a star.");

    // Use successful automatic star selection (not full-field), capture, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 3.0);
    useAutoStar->setCheckState(Qt::CheckState::Checked);
    KTRY_FOCUS_DETECT(2, 1, 99);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);
    KTRY_FOCUS_CLICK(resetFrameB);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_IDLE);
    useAutoStar->setCheckState(Qt::CheckState::Unchecked);
    state_list.clear();

    // Use unsuccessful automatic star selection, capture, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 3.0);
    useAutoStar->setCheckState(Qt::CheckState::Checked);
    KTRY_FOCUS_DETECT(0.01, 1, 1);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);
    KTRY_FOCUS_CLICK(resetFrameB);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_WAITING);
    useAutoStar->setCheckState(Qt::CheckState::Unchecked);
    state_list.clear();

    disconnect(state_handler);
}

void TestEkosFocus::testDuplicateFocusRequest()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus, pre-set to near-optimal 38500 steps
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    auto startup_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() {
        autofocus_started = true;
    });
    QVERIFY(startup_handler);

    // If we click the autofocus button, we receive a signal that the procedure starts, the state change and the button is disabled
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QVERIFY(Ekos::Manager::Instance()->focusModule()->status() != Ekos::FOCUS_IDLE);
    QVERIFY(!startFocusB->isEnabled());
    QVERIFY(stopFocusB->isEnabled());

    // If we issue an autofocus command at that point (bypassing d-bus), no procedure should start
    autofocus_started = false;
    Ekos::Manager::Instance()->focusModule()->start();
    QTest::qWait(500);
    QVERIFY(!autofocus_started);

    // Stop the running autofocus
    KTRY_FOCUS_CLICK(stopFocusB);
    QTest::qWait(500);

    disconnect(startup_handler);
}

void TestEkosFocus::testAutofocusSignalEmission()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus, pre-set to near-optimal 38500 steps
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    auto startup_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() {
        autofocus_started = true;
    });
    QVERIFY(startup_handler);

    // Prepare to restart the autofocus procedure immediately when it finishes
    volatile bool ran_once = false;
    volatile bool autofocus_complete = false;
    auto completion_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() {
        autofocus_complete = true;
        autofocus_started = false;
        if (!ran_once)
        {
            Ekos::Manager::Instance()->focusModule()->start();
            ran_once = true;
        }
    });
    QVERIFY(completion_handler);

    // Run the autofocus, wait for the completion signal and restart a second one immediately
    QVERIFY(!autofocus_started);
    QVERIFY(!autofocus_complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_complete, 30000);

    // Wait for the second run to finish
    autofocus_complete = false;
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_complete, 30000);

    // No other autofocus started after that
    QVERIFY(!autofocus_started);

    // Disconnect signals
    disconnect(startup_handler);
    disconnect(completion_handler);
}

void TestEkosFocus::testFocusAbort()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus, pre-set to near-optimal 38500 steps
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    auto startup_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() {
        autofocus_started = true;
    });
    QVERIFY(startup_handler);

    // Prepare to detect the end of the autofocus_procedure
    volatile bool autofocus_completed = false;
    auto completion_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() {
        autofocus_completed = true;
        autofocus_started = false;
    });
    QVERIFY(completion_handler);

    // Prepare to restart the autofocus procedure immediately when it finishes
    volatile bool ran_once = false;
    volatile bool autofocus_aborted = false;
    auto abort_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() {
        autofocus_aborted = true;
        autofocus_started = false;
        if (!ran_once)
        {
            Ekos::Manager::Instance()->focusModule()->start();
            ran_once = true;
        }
    });
    QVERIFY(abort_handler);

    // Run the autofocus, don't wait for the completion signal, abort it and restart a second one immediately
    QVERIFY(!autofocus_started);
    QVERIFY(!autofocus_aborted);
    QVERIFY(!autofocus_completed);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_aborted, 1000);

    // Wait for the second run to finish
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_completed, 30000);

    // No other autofocus started after that
    QVERIFY(!autofocus_started);

    // Disconnect signals
    disconnect(startup_handler);
    disconnect(completion_handler);
    disconnect(abort_handler);
}

void TestEkosFocus::testGuidingSuspend()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    auto startup_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() {
        autofocus_started = true;
    });
    QVERIFY(startup_handler);

    // Prepare to detect the failure of the autofocus procedure
    volatile bool autofocus_aborted = false;
    auto abort_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() {
        autofocus_aborted = true;
        autofocus_started = false;
    });
    QVERIFY(abort_handler);

    // Prepare to detect the end of the autofocus_procedure
    volatile bool autofocus_completed = false;
    auto completion_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() {
        autofocus_completed = true;
        autofocus_started = false;
    });
    QVERIFY(completion_handler);

    // Prepare to detect guiding suspending
    volatile bool guiding_suspended = false;
    auto suspend_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::suspendGuiding, this, [&]() {
        guiding_suspended = true;
    });
    QVERIFY(suspend_handler);

    // Prepare to detect guiding suspending
    auto resume_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::resumeGuiding, this, [&]() {
        guiding_suspended = false;
    });
    QVERIFY(resume_handler);

    KTRY_FOCUS_GADGET(QCheckBox, suspendGuideCheck);

    // Abort the autofocus with guiding set to suspend, guiding will be required to suspend, then required to resume
    suspendGuideCheck->setCheckState(Qt::CheckState::Checked);
    QVERIFY(!autofocus_started);
    QVERIFY(!autofocus_aborted);
    QVERIFY(!autofocus_completed);
    QVERIFY(!guiding_suspended);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QVERIFY(guiding_suspended);
    Ekos::Manager::Instance()->focusModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_aborted, 5000);
    QVERIFY(!guiding_suspended);

    // Run the autofocus to completion with guiding set to suspend, guiding will be required to suspend, then required to resume
    autofocus_started = autofocus_aborted = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QVERIFY(guiding_suspended);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_completed, 30000);
    QVERIFY(!guiding_suspended);

    // No other autofocus started after that
    QVERIFY(!autofocus_started);

    // Abort the autofocus with guiding set to continue, no guiding signal will be emitted
    suspendGuideCheck->setCheckState(Qt::CheckState::Unchecked);
    autofocus_started = autofocus_aborted = autofocus_completed = guiding_suspended = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QVERIFY(!guiding_suspended);
    Ekos::Manager::Instance()->focusModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_aborted, 5000);
    QVERIFY(!guiding_suspended);

    // Run the autofocus to completion with guiding set to continue, no guiding signal will be emitted
    autofocus_started = autofocus_aborted = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QVERIFY(!guiding_suspended);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_completed, 30000);
    QVERIFY(!guiding_suspended);

    // No other autofocus started after that
    QVERIFY(!autofocus_started);

    // Disconnect signals
    disconnect(startup_handler);
    disconnect(completion_handler);
    disconnect(abort_handler);
    disconnect(suspend_handler);
    disconnect(resume_handler);
}

void TestEkosFocus::testFocusWhenGuidingResumes()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a successful pre-focus capture
    KTRY_FOCUS_MOVETO(50000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 50);
    KTRY_FOCUS_EXPOSURE(2, 1);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    auto startup_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() {
        autofocus_started = true;
    });
    QVERIFY(startup_handler);

    // Prepare to detect the end of the autofocus_procedure
    volatile bool autofocus_completed = false;
    auto completion_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() {
        autofocus_completed = true;
        autofocus_started = false;
    });
    QVERIFY(completion_handler);

    // Prepare to detect the failure of the autofocus procedure
    volatile bool autofocus_aborted = false;
    auto abort_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() {
        autofocus_aborted = true;
        autofocus_started = false;
    });
    QVERIFY(abort_handler);

    // Run a standard autofocus
    QVERIFY(!autofocus_started);
    QVERIFY(!autofocus_aborted);
    QVERIFY(!autofocus_completed);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 10000);

    // Wait a little, then run a capture check with an unachievable HFR
    QTest::qWait(3000);
    QWARN("Requesting a first in-sequence HFR check, letting it complete...");
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure succeeds
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_completed, 60000);

    // Run again a capture check
    autofocus_completed = false;
    QWARN("Requesting a second in-sequence HFR check, aborting it...");
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure starts properly
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 10000);

    // Abort the procedure manually, and run again a capture check that will fail
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_aborted, 10000);
    QWARN("Requesting a third in-sequence HFR check, making it fail during autofocus...");
    autofocus_aborted = autofocus_completed = false;
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure starts properly, after acknowledging there is an autofocus to run
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 10000);

    // Change settings so that the procedure fails now
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.1, 0.1);
    KTRY_FOCUS_EXPOSURE(0.1, 1);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_aborted, 60000);

    // Run again a capture check
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 20);
    KTRY_FOCUS_EXPOSURE(2, 1);
    autofocus_aborted = autofocus_completed = false;
    QWARN("Requesting a fourth in-sequence HFR check, making it succeed...");
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure succeeds
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_completed, 60000);

    // Disconnect signals
    disconnect(startup_handler);
    disconnect(completion_handler);
    disconnect(abort_handler);
}

void TestEkosFocus::testFocusFailure()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a failing autofocus - small gain, small exposure, small frame filter
    KTRY_FOCUS_MOVETO(10000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 1.0, 0.1);
    KTRY_FOCUS_EXPOSURE(0.01, 1);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    auto startup_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() {
        autofocus_started = true;
    });
    QVERIFY(startup_handler);

    // Prepare to detect the end of the autofocus_procedure
    volatile bool autofocus_completed = false;
    auto completion_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() {
        autofocus_completed = true;
        autofocus_started = false;
    });
    QVERIFY(completion_handler);

    // Prepare to detect the failure of the autofocus procedure
    volatile bool autofocus_aborted = false;
    auto abort_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() {
        autofocus_aborted = true;
        autofocus_started = false;
    });
    QVERIFY(abort_handler);

    // Run the autofocus, wait for the completion signal
    QVERIFY(!autofocus_started);
    QVERIFY(!autofocus_aborted);
    QVERIFY(!autofocus_completed);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_aborted, 30000);

    // No other autofocus started after that, we are not running a sequence focus
    QVERIFY(!autofocus_started);

    // Disconnect signals
    disconnect(startup_handler);
    disconnect(completion_handler);
    disconnect(abort_handler);
}

void TestEkosFocus::testFocusOptions()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a proper autofocus
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3);
    KTRY_FOCUS_EXPOSURE(3, 99);

    // Prepare to detect a new HFR
    volatile double hfr = -2;
    auto hfr_handler = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newHFR, this, [&](double _hfr) {
        hfr = _hfr;
    });
    QVERIFY(hfr_handler);

    KTRY_FOCUS_GADGET(QPushButton, captureB);

    // Filter to apply to frame after capture
    // This option is tricky: it follows the FITSViewer::filterTypes filter list, but
    // is used as a list of filter that may be applied in the FITS view.
    // In the Focus view, it also requires the "--" item as no-op filter, present in the
    // combobox stating which filter to apply to frames before analysing them.
    {
        // Validate the default Ekos option is recognised as a filter
        int const fe = Options::focusEffect();
        QVERIFY(0 <= fe && fe < FITSViewer::filterTypes.count() + 1);
        QWARN(qPrintable(QString("Default filtering option is %1/%2").arg(fe).arg(FITSViewer::filterTypes.value(fe - 1, "(none)"))));

        // Validate the UI changes the Ekos option
        for (int i = 0; i < FITSViewer::filterTypes.count(); i++)
        {
            QTRY_VERIFY_WITH_TIMEOUT(captureB->isEnabled(), 5000);

            QString const & filterType = FITSViewer::filterTypes.value(i, "???");
            QWARN(qPrintable(QString("Testing filtering option %1/%2").arg(i + 1).arg(filterType)));

            // Set filter to apply in the UI, verify impact on Ekos option
            Options::setFocusEffect(fe);
            KTRY_FOCUS_COMBO_SET(filterCombo, filterType);
            QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) i + 1, 1000);

            // Set filter to apply with the d-bus entry point, verify impact on Ekos option
            Options::setFocusEffect(fe);
            Ekos::Manager::Instance()->focusModule()->setImageFilter(filterType);
            QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) i + 1, 1000);

            // Run a capture with detection for coverage
            hfr = -2;
            KTRY_FOCUS_CLICK(captureB);
            QTRY_VERIFY_WITH_TIMEOUT(-1 <= hfr, 5000);
        }

        // Restore the original Ekos option
        KTRY_FOCUS_COMBO_SET(filterCombo, FITSViewer::filterTypes[fe - 1]);
        QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) fe, 1000);
    }
}

void TestEkosFocus::testStarDetection_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("RA");
    QTest::addColumn<QString>("DEC");

    // Altitude computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    std::list<char const *> Objects = { "Polaris", "Mizar", "M 51", "M 13", "M 47", "Vega", "NGC 2238", "M 81" };
    size_t count = 0;

    foreach (char const *name, Objects)
    {
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (10.0 < o.alt().Degrees())
            {
                QTest::addRow("%s", name)
                        << name
                        << o.ra().toHMSString()
                        << o.dec().toDMSString();
                count++;
            }
            else QWARN(QString("Fixture '%1' altitude is '%2' degrees, discarding.").arg(name).arg(so->alt().Degrees()).toStdString().c_str());
        }
    }

    if (!count)
        QSKIP("No usable fixture objects, bypassing test.");
#endif
}

void TestEkosFocus::testStarDetection()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    QFETCH(QString, NAME);
    QFETCH(QString, RA);
    QFETCH(QString, DEC);
    qDebug("Test focusing on '%s' RA '%s' DEC '%s'",
           NAME.toStdString().c_str(),
           RA.toStdString().c_str(),
           DEC.toStdString().c_str());

    // Just sync to RA/DEC to make the mount teleport to the object
    QWARN("During this test, the mount is not tracking - we leave it as is for the feature in the CCD simulator to trigger a failure.");
    QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000);
    QVERIFY(ekos->mountModule()->sync(RA, DEC));

    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    KTRY_FOCUS_GADGET(QLineEdit, starsOut);

    // Locate somewhere we do see stars with the CCD Simulator
    KTRY_FOCUS_MOVETO(60000);

    // Run the focus procedure for SEP
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Centroid
    KTRY_FOCUS_CONFIGURE("Centroid", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Threshold - disable full-field
    KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Gradient - disable full-field
    KTRY_FOCUS_CONFIGURE("Gradient", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Longer exposure
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(8, 1, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure again to cover more code
    // Filtering annulus is independent of the detection method
    // Run the HFR average over three frames with SEP to avoid
    for (double inner = 0.0; inner < 100.0; inner += 43.0)
    {
        for (double outer = 100.0; inner < outer; outer -= 42.0)
        {
            KTRY_FOCUS_CONFIGURE("SEP", "Iterative", inner, outer, 3.0);
            KTRY_FOCUS_DETECT(0.1, 2, 99);
        }
    }

    // Test threshold - disable full-field
    for (double threshold = 80.0; threshold < 99.0; threshold += 13.3)
    {
        KTRY_FOCUS_GADGET(QDoubleSpinBox, thresholdSpin);
        thresholdSpin->setValue(threshold);
        KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0, 0.0, 3.0);
        KTRY_FOCUS_DETECT(0.1, 1, 99);
    }
#endif
}

QTEST_KSTARS_MAIN(TestEkosFocus)

#endif // HAVE_INDI
