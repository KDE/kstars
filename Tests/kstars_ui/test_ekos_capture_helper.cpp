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

    QStandardPaths::setTestModeEnabled(true);
    QFileInfo test_dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation), "test");
    destination = new QTemporaryDir(test_dir.absolutePath());
    QVERIFY(destination->isValid());
    QVERIFY(destination->autoRemove());
}

void TestEkosCaptureHelper::cleanupTestCase()
{
    // disconnect to the capture process to receive capture status changes
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosCaptureHelper::captureStatusChanged);
    TestEkosHelper::cleanupTestCase();

    // remove destination directory
    destination->remove();
    delete destination;
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

QString TestEkosCaptureHelper::calculateSignature(QString target, QString filter)
{
    if (target == "")
        return getImageLocation()->path() + "/Light/" + filter + "/Light";
    else
        return getImageLocation()->path() + "/" + target + "/Light/" + filter + "/" + target + "_Light";
}

QDir *TestEkosCaptureHelper::getImageLocation()
{
    if (imageLocation == nullptr || imageLocation->exists())
        imageLocation = new QDir(destination->path() + "/images");

    return imageLocation;
}

bool TestEkosCaptureHelper::fillCaptureSequences(QString target, QString sequence, double exptime, QString fitsDirectory)
{
    if (sequence == "")
        return true;

    for (QString value : sequence.split(","))
    {
        KVERIFY_SUB(value.indexOf(":") > -1);
        QString filter = value.left(value.indexOf(":"));
        int count      = value.right(value.length()-value.indexOf(":")-1).toInt();
        KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), fileTimestampS, true);
        KTRY_SET_LINEEDIT_SUB(Ekos::Manager::Instance()->captureModule(), filePrefixT, target);
        if (count > 0)
            KWRAP_SUB(KTRY_CAPTURE_ADD_LIGHT(exptime, count, 0, filter, fitsDirectory));
        // ensure that no old values are present
        Ekos::Manager::Instance()->captureModule()->setCapturedFramesMap(calculateSignature(target, filter), 0);
    }

    return true;
}

void TestEkosCaptureHelper::cleanupScheduler()
{
    Ekos::Manager::Instance()->schedulerModule()->stop();
    QTest::qWait(5000);
    // remove jobs
    Ekos::Manager::Instance()->schedulerModule()->removeAllJobs();
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

