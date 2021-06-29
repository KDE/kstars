/*
    Helper class of KStars UI capture tests

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "test_ekos_capture_helper.h"

#include "test_ekos.h"

TestEkosCaptureHelper::TestEkosCaptureHelper() : TestEkosHelper() {
    m_GuiderDevice  = "Guide Simulator";
    m_FocuserDevice = "Focuser Simulator";
}

void TestEkosCaptureHelper::initTestCase()
{
    TestEkosHelper::initTestCase();
    // connect to the capture process to receive capture status changes
    connect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosCaptureHelper::captureStatusChanged,
            Qt::UniqueConnection);
}

void TestEkosCaptureHelper::cleanupTestCase()
{
    // disconnect to the capture process to receive capture status changes
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosCaptureHelper::captureStatusChanged);
    TestEkosHelper::cleanupTestCase();
}


bool TestEkosCaptureHelper::startCapturing(bool checkCapturing)
{
    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // check if capture is in a stopped state
    KWRAP_SUB(QVERIFY(getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED
                      || getCaptureStatus() == Ekos::CAPTURE_COMPLETE));

    // ensure at least one capture is started if requested
    if (checkCapturing)
        expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    // press start
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedCaptureStates, 5000);
    // all checks succeeded
    return true;
}

bool TestEkosCaptureHelper::stopCapturing()
{
    // check if capture is in a stopped state
    if (getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED || getCaptureStatus() == Ekos::CAPTURE_COMPLETE)
        return true;

    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // else press stop
    expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has stopped
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedCaptureStates, 5000);
    // all checks succeeded
    return true;
}

/* *********************************************************************************
 *
 * Slots for catching state changes
 *
 * ********************************************************************************* */

void TestEkosCaptureHelper::captureStatusChanged(Ekos::CaptureState status) {
    m_CaptureStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedCaptureStates.isEmpty() && expectedCaptureStates.head() == status)
        expectedCaptureStates.dequeue();
}

