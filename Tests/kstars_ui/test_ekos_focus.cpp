/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "test_ekos_focus.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "test_ekos_mount.h"
#include "Options.h"

class KFocusProcedureSteps: public QObject
{
public:
    QMetaObject::Connection starting;
    QMetaObject::Connection aborting;
    QMetaObject::Connection completing;
    QMetaObject::Connection notguiding;
    QMetaObject::Connection guiding;
    QMetaObject::Connection quantifying;

public:
    bool started { false };
    bool aborted { false };
    bool complete { false };
    bool unguided { false };
    double hfr { -2 };

public:
    KFocusProcedureSteps():
        starting (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() { started = true; }, Qt::UniqueConnection)),
        aborting (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() { started = false; aborted = true; }, Qt::UniqueConnection)),
        completing (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() { started = false; complete = true; }, Qt::UniqueConnection)),
        notguiding (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::suspendGuiding, this, [&]() { unguided = true; }, Qt::UniqueConnection)),
        guiding (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::resumeGuiding, this, [&]() { unguided = false; }, Qt::UniqueConnection)),
        quantifying (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newHFR, this, [&](double _hfr) { hfr = _hfr; }, Qt::UniqueConnection))
    {};
    virtual ~KFocusProcedureSteps() {
        disconnect(starting);
        disconnect(aborting);
        disconnect(completing);
        disconnect(notguiding);
        disconnect(guiding);
        disconnect(quantifying);
    };
};

class KFocusStateList: public QObject, public QList <Ekos::FocusState>
{
public:
    QMetaObject::Connection handler;

public:
    KFocusStateList():
        handler (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newStatus, this, [&](Ekos::FocusState s) { append(s); }, Qt::UniqueConnection))
    {};
    virtual ~KFocusStateList() {};
};

TestEkosFocus::TestEkosFocus(QObject *parent) : QObject(parent)
{
}

void TestEkosFocus::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // We can't use this here because of the meridian flip test
    // HACK: Reset clock to initial conditions
    // KHACK_RESET_EKOS_TIME();

    KTELL_BEGIN();
}

void TestEkosFocus::cleanupTestCase()
{
    KTELL_END();
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosFocus::init()
{
}

void TestEkosFocus::cleanup()
{
    if (Ekos::Manager::Instance())
        if (Ekos::Manager::Instance()->focusModule())
            Ekos::Manager::Instance()->focusModule()->abort();
    KTELL_HIDE();
}

void TestEkosFocus::testCaptureStates()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.");
    KTRY_MOUNT_SYNC(60.0, true, -1);

    // Prepare to detect state change
    KFocusStateList state_list;
    QVERIFY(state_list.handler);

    KTELL("Configure fields.\nCapture a frame.\nExpect PROGRESS, IDLE.");
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(2, 1, 99);
    QTRY_COMPARE_WITH_TIMEOUT(state_list.count(), 2, 5000);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_IDLE);
    state_list.clear();

    KTELL("Move focuser.\nExpect no capture triggered.");
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_MOVETO(43210);
    QTest::qWait(1000);
    QCOMPARE(state_list.count(), 0);

    KTRY_FOCUS_GADGET(QPushButton, startLoopB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);

    KTELL("Loop captures.\nAbort loop.\nExpect FRAMING, PROGRESS, ABORTED, ABORTED.");
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_CLICK(startLoopB);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 1, 5000);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 4, 5000);
    QCOMPARE((int)state_list[0], (int)Ekos::FocusState::FOCUS_FRAMING);
    QCOMPARE((int)state_list[1], (int)Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE((int)state_list[2], (int)Ekos::FocusState::FOCUS_ABORTED);
    QCOMPARE((int)state_list[3], (int)Ekos::FocusState::FOCUS_ABORTED);
    state_list.clear();

    KTRY_FOCUS_GADGET(QCheckBox, useAutoStar);
    KTRY_FOCUS_GADGET(QPushButton, resetFrameB);

    QWARN("This test does not wait for the hardcoded timeout to select a star.");

    KTELL("Use a successful automatic star selection (not full-field).\nCapture a frame.\nExpect PROGRESS, IDLE\nCheck star selection.");
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

    KTELL("Use an unsuccessful automatic star selection (not full-field).\nCapture a frame\nExpect PROGRESS, WAITING.\nCheck star selection.");
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
}

void TestEkosFocus::testDuplicateFocusRequest()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.\nConfigure a fast autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, -1);
    KTRY_FOCUS_MOVETO(35000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 30);
    KTRY_FOCUS_EXPOSURE(3, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);

    KTELL("Click the autofocus button\nExpect a signal that the procedure starts.\nExpect state change and disabled button.");
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(Ekos::Manager::Instance()->focusModule()->status() != Ekos::FOCUS_IDLE);
    QVERIFY(!startFocusB->isEnabled());
    QVERIFY(stopFocusB->isEnabled());

    KTELL("Issue a few autofocus commands at that point through the d-bus entry point\nExpect no parallel procedure start.");
    for (int i = 0; i < 5; i++)
    {
        autofocus.started = false;
        Ekos::Manager::Instance()->focusModule()->start();
        QTest::qWait(500);
        QVERIFY(!autofocus.started);
    }

    KTELL("Stop the running autofocus.");
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);
}

void TestEkosFocus::testAutofocusSignalEmission()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.\nConfigure fast autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0,true, -1);

    KTRY_FOCUS_MOVETO(35000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 30);
    KTRY_FOCUS_EXPOSURE(3, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);

    KTELL("Configure to restart autofocus when it finishes, like Scheduler does.");
    volatile bool ran_once = false;
    autofocus.completing = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, &autofocus, [&]() {
        autofocus.complete = true;
        autofocus.started = false;
        if (!ran_once)
        {
            Ekos::Manager::Instance()->focusModule()->start();
            ran_once = true;
        }
    }, Qt::UniqueConnection);
    QVERIFY(autofocus.completing);

    KTELL("Run autofocus, wait for completion.\nHandler restarts a second one immediately.");
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);

    KTELL("Wait for the second run to finish.\nNo other autofocus started.");
    autofocus.complete = false;
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testFocusAbort()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.\nConfigure fast autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, -1);
    KTRY_FOCUS_MOVETO(35000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 30);
    KTRY_FOCUS_EXPOSURE(3, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.completing);

    KTELL("Configure to restart autofocus when it finishes, like Scheduler does.");
    volatile bool ran_once = false;
    autofocus.aborting = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() {
        autofocus.aborted = true;
        autofocus.started = false;
        if (!ran_once)
        {
            Ekos::Manager::Instance()->focusModule()->start();
            ran_once = true;
        }
    }, Qt::UniqueConnection);
    QVERIFY(autofocus.aborting);

    KTELL("Run autofocus, don't wait for the completion signal and abort it.\nHandler restarts a second one immediately.");
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 1000);

    KTELL("Wait for the second run to finish.\nNo other autofocus started.");
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testGuidingSuspendWhileFocusing()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator\nConfigure a fast autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, -1);
    KTRY_FOCUS_MOVETO(35000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 30);
    KTRY_FOCUS_EXPOSURE(3, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);
    QVERIFY(autofocus.notguiding);
    QVERIFY(autofocus.guiding);

    KTRY_FOCUS_GADGET(QCheckBox, suspendGuideCheck);

    KTELL("Abort the autofocus with guiding set to suspend\nGuiding required to suspend, then required to resume");
    suspendGuideCheck->setCheckState(Qt::CheckState::Checked);
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    QVERIFY(!autofocus.unguided);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(autofocus.unguided);
    Ekos::Manager::Instance()->focusModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);
    QVERIFY(!autofocus.unguided);

    KTELL("Run the autofocus to completion with guiding set to suspend\nGuiding required to suspend, then required to resume\nNo other autofocus started.");
    autofocus.started = autofocus.aborted = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(autofocus.unguided);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);
    QVERIFY(!autofocus.unguided);
    QVERIFY(!autofocus.started);

    KTELL("Abort the autofocus with guiding set to continue\nNo guiding signal emitted");
    suspendGuideCheck->setCheckState(Qt::CheckState::Unchecked);
    autofocus.started = autofocus.aborted = autofocus.complete = autofocus.unguided = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(!autofocus.unguided);
    Ekos::Manager::Instance()->focusModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);
    QVERIFY(!autofocus.unguided);

    KTELL("Run the autofocus to completion with guiding set to continue\nNo guiding signal emitted\nNo other autofocus started.");
    autofocus.started = autofocus.aborted = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(!autofocus.unguided);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);
    QVERIFY(!autofocus.unguided);
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testFocusWhenMountFlips()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.\nConfigure a fast autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, +10.0/3600);
    KTRY_FOCUS_MOVETO(35000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 5);
    KTRY_FOCUS_EXPOSURE(3, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the states of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);

    KTELL("Ensure flip is enabled on meridian.\n.Start a standard autofocus.");
    Ekos::Manager::Instance()->mountModule()->setMeridianFlipValues(true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->meridianFlipEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->meridianFlipValue() == 0, 1000);
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 5000);

    KTELL("Wait for the meridian flip to occur.\nCheck procedure aborts while flipping.");
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_SLEWING, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);

    KTELL("Wait for the flip to end.");
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 120000);

    KTELL("Start the procedure again.\nExpect the procedure to succeed.");
    autofocus.started = false;
    autofocus.aborted = false;
    autofocus.complete = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 60000);
}

void TestEkosFocus::testFocusWhenHFRChecking()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.\nConfigure a fast autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, -1);
    int initialFocusPosition = 35000;
    KTRY_FOCUS_MOVETO(initialFocusPosition);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 50);
    KTRY_FOCUS_EXPOSURE(3, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);

    KTELL("Run a standard autofocus.\nRun a HFR check.\nExpect no effect on the procedure.");
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);

    KTELL("Wait a little, run a first HFR check while the procedure runs.");
    QTest::qWait(3000);
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    KTELL("Expect procedure to succeed nonetheless.");
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 60000);

    KTELL("Run a second HFR check that would start an autofocus.");
    autofocus.complete = false;
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    KTELL("Expect procedure to start properly.\nAbort the procedure manually.\nRun a third HFR check.");
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 10000);

    KTELL("Expect autofocus to start properly.\nChange settings so that the procedure fails now.\nExpect a failure.");
    autofocus.aborted = autofocus.complete = false;
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.1, 0.1);
    KTRY_FOCUS_EXPOSURE(0.1, 1);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 90000);
    KTRY_FOCUS_CHECK_POSITION_WITH_TIMEOUT(initialFocusPosition, 5000);

    KTELL("Run a fourth HFR check.\nExpect autofocus to complete.");
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 50);
    KTRY_FOCUS_EXPOSURE(3, 99);
    autofocus.aborted = autofocus.complete = false;
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 60000);
}

void TestEkosFocus::testFocusFailure()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, -1);

    KTELL("Configure an autofocus that cannot see any star, so that the initial setup fails.");
    KTRY_FOCUS_MOVETO(10000);
    KTRY_FOCUS_CONFIGURE("SEP", "Polynomial", 0.0, 1.0, 0.1);
    KTRY_FOCUS_EXPOSURE(0.01, 1);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);

    KTELL("Run the autofocus, wait for the completion signal.\nExpect no further autofocus started as we are not running a sequence.");
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 30000);
    QVERIFY(!autofocus.started);

    QSKIP("Skipping abort test for device limits, focus algorithms are too sensitive to CCD Sim noise.");

    KTELL("Configure an autofocus that can see stars but is too far off and cannot achieve focus, so that the procedure fails.");
    KTRY_FOCUS_MOVETO(25000);
    QWARN("Iterative and Polynomial are too easily successful for this test.");
    KTRY_FOCUS_CONFIGURE("SEP", "Linear", 0.0, 100.0, 1.0);
    KTRY_FOCUS_EXPOSURE(5, 99);
    KTRY_FOCUS_GADGET(QDoubleSpinBox, maxTravelIN);
    maxTravelIN->setValue(2000);

    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);
    autofocus.started = false;
    autofocus.aborted = false;
    autofocus.complete = false;

    KTELL("Run the autofocus, wait for the completion signal.\nNo further autofocus started as we are not running a sequence.");
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 240000);
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testFocusOptions()
{
    KTELL("Sync high on meridian to avoid jitter in CCD Simulator.\nConfigure a standard autofocus.");
    KTRY_FOCUS_SHOW();
    KTRY_MOUNT_SYNC(60.0, true, -1);
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3);
    KTRY_FOCUS_EXPOSURE(1, 99);

    // Prepare to detect a new HFR
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.quantifying);

    KTRY_FOCUS_GADGET(QPushButton, captureB);

    KTELL("Validate filter to apply to frame after capture.");
    // This option is tricky: it follows the FITSViewer::filterTypes filter list, but
    // is used as a list of filter that may be applied in the FITS view.
    // In the Focus view, it also requires the "--" item as no-op filter, present in the
    // combobox stating which filter to apply to frames before analysing them.
    {
        // Validate the default Ekos option is recognised as a filter
        int const fe = Options::focusEffect();
        QVERIFY(0 <= fe && fe < FITSViewer::filterTypes.count() + 1);
        QWARN(qPrintable(QString("Default filtering option is %1/%2").arg(fe).arg(FITSViewer::filterTypes.value(fe - 1, "--"))));

        // Validate the UI changes the Ekos option
        for (int i = 0; i < FITSViewer::filterTypes.count(); i++)
        {
            QTRY_VERIFY_WITH_TIMEOUT(captureB->isEnabled(), 5000);

            QString const & filterType = FITSViewer::filterTypes.value(i, "Unknown image filter");
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
            autofocus.hfr = -2;
            KTRY_FOCUS_CLICK(captureB);
            QTRY_VERIFY_WITH_TIMEOUT(-1 <= autofocus.hfr, 5000);
        }

        // Set no-op filter to apply in the UI, verify impact on Ekos option
        Options::setFocusEffect(0);
        KTRY_FOCUS_COMBO_SET(filterCombo, "--");
        QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) 0, 1000);

        // Set no-op filter to apply with the d-bus entry point, verify impact on Ekos option
        Options::setFocusEffect(0);
        Ekos::Manager::Instance()->focusModule()->setImageFilter("--");
        QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) 0, 1000);

        // Restore the original Ekos option
        KTRY_FOCUS_COMBO_SET(filterCombo, FITSViewer::filterTypes.value(fe - 1, "--"));
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
    QVERIFY(ekos);

    QFETCH(QString, NAME);
    QFETCH(QString, RA);
    QFETCH(QString, DEC);

    KTELL(QString(NAME+"\nSync to %1/%2 to make the mount teleport to the object.").arg(qPrintable(RA)).arg(qPrintable(DEC)));
    QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000);
    ekos->mountModule()->setMeridianFlipValues(false, 0);
    QVERIFY(ekos->mountModule()->sync(RA, DEC));
    ekos->mountModule()->setTrackEnabled(true);

    KTELL(NAME+"\nWait for Focus to come up\nSwitch to Focus tab.");
    KTRY_FOCUS_SHOW();

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    KTRY_FOCUS_GADGET(QLineEdit, starsOut);

    KTELL(NAME+"\nMove focuser to see stars.");
    KTRY_FOCUS_MOVETO(35000);

    KTELL(NAME+"\nRun the detection with SEP.");
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    KTELL(NAME+"\nRun the detection with Centroid.");
    KTRY_FOCUS_CONFIGURE("Centroid", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    KTELL(NAME+"\nRun the detection with Threshold (no full-field).");
    KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    KTELL(NAME+"\nRun the detection with Gradient (no full-field).");
    KTRY_FOCUS_CONFIGURE("Gradient", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    KTELL(NAME+"\nRun the detection with SEP (8s capture).");
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(8, 1, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    KTELL(NAME+"\nRun the detection with SEP\nFull-field with various values\nHFR averaged on 3 frames.");
    for (double inner = 0.0; inner < 100.0; inner += 43.0)
    {
        for (double outer = 100.0; inner < outer; outer -= 42.0)
        {
            KTRY_FOCUS_CONFIGURE("SEP", "Iterative", inner, outer, 3.0);
            KTRY_FOCUS_DETECT(1, 2, 99);
        }
    }

    KTELL(NAME+"\nRun the detection with Threshold, full-field.");
    for (double threshold = 80.0; threshold < 99.0; threshold += 13.3)
    {
        KTRY_FOCUS_GADGET(QDoubleSpinBox, thresholdSpin);
        thresholdSpin->setValue(threshold);
        KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0, 0.0, 3.0);
        KTRY_FOCUS_DETECT(1, 1, 99);
    }
#endif
}

QTEST_KSTARS_MAIN(TestEkosFocus)

#endif // HAVE_INDI
