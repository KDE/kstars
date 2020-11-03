/*
    KStars UI tests for verifying correct counting of the capture module

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "test_ekos_capture_count.h"

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"

#include "test_ekos_capture_helper.h"

TestEkosCaptureCount::TestEkosCaptureCount(QObject *parent) : QObject(parent) {
    m_CaptureHelper = new TestEkosCaptureHelper();
}


void TestEkosCaptureCount::testCaptureWithCaptureFramesMap()
{
    // clean up capture module
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_GADGET(Ekos::Manager::Instance()->captureModule(), QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);

    // setup capture sequence, fill captured frames map and set expectations
    QVERIFY(prepareCapture());

    // verify if at least one capture is expected
    QVERIFY(executeCapturing());
}

void TestEkosCaptureCount::cleanupScheduler()
{
    Ekos::Manager::Instance()->schedulerModule()->stop();
    QTest::qWait(5000);
    // remove jobs
    Ekos::Manager::Instance()->schedulerModule()->removeAllJobs();
}

void TestEkosCaptureCount::testSchedulerCapture()
{
    // prepare captured frames
    QVERIFY(prepareScheduledCapture());

    // start scheduler job
    KTRY_CLICK(Ekos::Manager::Instance()->schedulerModule(), startB);
    // expect a idle signal at the end
    expectedSchedulerStates.enqueue(Ekos::SCHEDULER_IDLE);

    // ensure that the scheduler has started capturing
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() == 0, 10000);

    // wait for finish capturing
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedSchedulerStates, 120000);

    // verify whether all frames are captured as expected
    QVERIFY2(checkCapturedFrames(), "Capturing did not produce the expected amount of frames.");
}

/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosCaptureCount::testSchedulerCapture_data()
{
    prepareTestData(1.0, "Red:2,Green:2,Blue:2", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(1.0, "Red:1,Red:1,Green:1,Green:1,Blue:1,Blue:1", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(1.0, "Red:1,Green:1,Blue:1,Red:1,Green:1,Blue:1", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(1.0, "Red:1,Green:1,Blue:1", "Red:3,Green:1,Blue:3", "Green:2", 3);
    prepareTestData(1.0, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:5,Green:1,Blue:1", "Red:1");
    prepareTestData(1.0, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "", "Luminance:10,Red:2,Green:2,Blue:2", 2);
    prepareTestData(1.0, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:15,Red:1,Green:2,Blue:2", "Red:1", 2);
    prepareTestData(1.0, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:15,Red:2,Green:3,Blue:3", "Red:1", 3);
    prepareTestData(1.0, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:15,Red:3,Green:3,Blue:2", "Blue:1", 3);
}

void TestEkosCaptureCount::testCaptureWithCaptureFramesMap_data()
{
    // use the same test set
    testSchedulerCapture_data();
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosCaptureCount::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    // start the profile
    QVERIFY(startEkosProfile());
    m_CaptureHelper->initTestCase();
    QStandardPaths::setTestModeEnabled(true);
    QFileInfo test_dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation), "test");
    destination = new QTemporaryDir(test_dir.absolutePath());
    QVERIFY(destination->isValid());
    QVERIFY(destination->autoRemove());
    // do not show images
    Options::setUseFITSViewer(false);
    // disable twilight warning
    KMessageBox::saveDontShowAgainYesNo("astronomical_twilight_warning", KMessageBox::ButtonCode::No);
}

void TestEkosCaptureCount::cleanupTestCase()
{
    m_CaptureHelper->cleanupTestCase();
    QVERIFY(shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();

    // remove destination directory
    destination->remove();
    delete destination;
}

void TestEkosCaptureCount::init()
{
    connect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::captureComplete, this, &TestEkosCaptureCount::captureComplete);
    connect(Ekos::Manager::Instance()->schedulerModule(), &Ekos::Scheduler::newStatus, this, &TestEkosCaptureCount::schedulerStateChanged);
    QStandardPaths::setTestModeEnabled(true);
    // clear image directory
    QVERIFY(getImageLocation()->removeRecursively());
}

void TestEkosCaptureCount::cleanup()
{
    QVERIFY(m_CaptureHelper->stopCapturing());

    disconnect(Ekos::Manager::Instance()->schedulerModule(), &Ekos::Scheduler::newStatus, this, &TestEkosCaptureCount::schedulerStateChanged);
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::captureComplete, this, &TestEkosCaptureCount::captureComplete);

    // clean up capture page
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();

    // clean up expected images
    m_expectedImages.clear();

    // cleanup scheduler
    cleanupScheduler();
}


/* *********************************************************************************
 *
 * Helper functions
 *
 * ********************************************************************************* */

bool TestEkosCaptureCount::checkCapturedFrames()
{
    bool success = true;
    for (QMap<QString, int>::iterator it = m_expectedImages.begin(); it != m_expectedImages.end(); ++it)
        if (it.value() != 0)
        {
            QWARN(QString("Capture count for signature %1 does not match: %2 frames too %3 captured.").arg(it.key()).arg(abs(it.value())).arg(it.value() < 0 ? "much" : "few").toStdString().c_str());
            success = false;
        }

    return success;
}

bool TestEkosCaptureCount::executeCapturing()
{

    // calculate frame counts
    int framesCount = 0;
    for(int value: m_expectedImages.values())
            framesCount += value;

    // capture
    KWRAP_SUB(QVERIFY(m_CaptureHelper->startCapturing(framesCount > 0)));

    // expect receiving a new CAPTURE_COMPLETE signal
    if (framesCount > 0)
        m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_COMPLETE);

    // wait for finish capturing
    // ensure that the scheduler has started capturing
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() == 0, 120000));

    // verify whether all frames are captured as expected
    KWRAP_SUB(QVERIFY2(checkCapturedFrames(), "Capturing did not produce the expected amount of frames."));

    // wait for shutdown
    QTest::qWait(5000);
    return true;
}

bool TestEkosCaptureCount::startEkosProfile()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->setupTab, 1000));

    KWRAP_SUB(QVERIFY(m_CaptureHelper->setupEkosProfile("Simulators", false)));
    // start the profile
    KTRY_EKOS_CLICK(processINDIB);
    // wait for the devices to come up
    QTest::qWait(10000);

    // Everything completed successfully
    return true;
}

bool TestEkosCaptureCount::shutdownEkosProfile()
{
    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ...";
    KWRAP_SUB(KTRY_EKOS_STOP_SIMULATORS());
    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ... (done)";
    // Everything completed successfully
    return true;
}

bool TestEkosCaptureCount::prepareCapture()
{
    QFETCH(double, exptime);
    QFETCH(QString, sequence);
    QFETCH(QString, capturedFramesMap);
    QFETCH(QString, expectedFrames);
    QFETCH(int, iterations);

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000));

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/" + target;

    // create the destination for images
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << imagepath;

    // create capture sequences
    for (int i = 0; i < iterations; i++)
        KVERIFY_SUB(fillCaptureSequences(sequence, exptime, imagepath));

    // fill the captured frames map that hold the numbers of already taken frames
    KVERIFY_SUB(fillCapturedFramesMap(capturedFramesMap));

    // fill the map of expected frames
    KVERIFY_SUB(setExpectedFrames(expectedFrames));

    // everything successfully completed
    return true;
}

bool TestEkosCaptureCount::prepareScheduledCapture()
{
    QFETCH(double, exptime);
    QFETCH(QString, sequence);
    QFETCH(QString, capturedFramesMap);
    QFETCH(QString, expectedFrames);
    QFETCH(int, iterations);

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000));

    // create the destination for images
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << getImageLocation()->path();

    // step 1: create the frames due to the captured frames map
    if (capturedFramesMap != "")
    {
        KVERIFY_SUB(fillCaptureSequences(capturedFramesMap, exptime, getImageLocation()->filePath(target)));
        KVERIFY_SUB(fillCapturedFramesMap(""));
        KVERIFY_SUB(setExpectedFrames(capturedFramesMap));

        // create the expected frames
        KVERIFY_SUB(executeCapturing());

        // clean up
        capture->clearSequenceQueue();
    }

    // step 2: create the frames due to the captured frames map
    KVERIFY_SUB(fillCaptureSequences(sequence, exptime, getImageLocation()->path()));
    KVERIFY_SUB(fillCapturedFramesMap(""));
    KVERIFY_SUB(setExpectedFrames(expectedFrames));

    // save current capture sequence to Ekos sequence file
    QString sequenceFile = destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file" << sequenceFile << "created.";
    KVERIFY_SUB(Ekos::Manager::Instance()->captureModule()->saveSequenceQueue(sequenceFile));

    // setup scheduler
    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    // set sequence file
    scheduler->setSequence(sequenceFile);
    // set Kocab as target
    KTRY_SET_LINEEDIT_SUB(scheduler, nameEdit, target);
    KTRY_SET_LINEEDIT_SUB(scheduler, raBox, "14 50 42");
    KTRY_SET_LINEEDIT_SUB(scheduler, decBox, "74 09 20");
    // disable all step checks
    KTRY_SET_CHECKBOX_SUB(scheduler, trackStepCheck, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, focusStepCheck, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, alignStepCheck, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, guideStepCheck, false);
    // ignore twilight
    KTRY_SET_CHECKBOX_SUB(scheduler, twilightCheck, false);
    // set remember job progress
    Options::setRememberJobProgress(true);
    // disable INDI stopping after scheduler finished
    Options::setStopEkosAfterShutdown(false);
    // repeat the job for a fixed amount
    KTRY_SET_RADIOBUTTON_SUB(scheduler, repeatCompletionR, true);
    KTRY_SET_SPINBOX_SUB(scheduler, repeatsSpin, iterations);
    // add scheduler job
    KTRY_CLICK_SUB(scheduler, addToQueueB);
    KTRY_GADGET_SUB(scheduler, QTableWidget, queueTable);
    KVERIFY_SUB(queueTable->rowCount() == 1);
    KVERIFY_SUB(queueTable->columnCount() > 3);

    // verify the displayed capture counts
    QString displayedCounts = queueTable->item(0, 2)->text();
    KVERIFY2_SUB(displayedCounts.indexOf("/") > 0, "Scheduler job table does not display in style captured/total.");
    // check display of already captured
    int captured          = displayedCounts.left(displayedCounts.indexOf("/")).toInt();
    int captured_expected = totalCount(capturedFramesMap);
    KVERIFY2_SUB(captured == captured_expected,
                 QString("Scheduler job table shows %1 captured frames instead of %2.").arg(captured).arg(captured_expected).toStdString().c_str());
    // check display of expected frames
    int total          = displayedCounts.right(displayedCounts.length() - displayedCounts.indexOf("/") - 1).toInt();
    int total_expected = totalCount(sequence) * iterations;
    KVERIFY2_SUB(total == total_expected,
                 QString("Scheduler job table shows %1 expected frames instead of %2.").arg(total).arg(total_expected).toStdString().c_str());

    // everything successfully completed
    return true;
}

void TestEkosCaptureCount::prepareTestData(double exptime, QString sequence, QString capturedFramesMap, QString expectedFrames, int iterations)
{

    QTest::addColumn<double>("exptime");             /*!< exposure time */
    QTest::addColumn<QString>("sequence");           /*!< list of filters */
    QTest::addColumn<QString>("capturedFramesMap");  /*!< list of frame counts */
    QTest::addColumn<QString>("expectedFrames");     /*!< list of frames per filter that are expected */
    QTest::addColumn<int>("iterations");                 /*!< how often should the sequence be repeated */

    QTest::newRow(QString("seq=%1x, ex=%2, it=%3").arg(sequence).arg(capturedFramesMap).arg(iterations).toStdString().c_str())
            << exptime << sequence << capturedFramesMap << expectedFrames << iterations;
 }

QDir *TestEkosCaptureCount::getImageLocation()
{
    if (imageLocation == nullptr || imageLocation->exists())
        imageLocation = new QDir(destination->path() + "/images");

    return imageLocation;
}

QString TestEkosCaptureCount::calculateSignature(QString filter)
{
    if (target == "")
        return getImageLocation()->path() + "/Light/" + filter + "/Light";
    else
        return getImageLocation()->path() + "/" + target + "/Light/" + filter + "/" + target + "_Light";
}

bool TestEkosCaptureCount::fillCaptureSequences(QString sequence, double exptime, QString fitsDirectory)
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
        Ekos::Manager::Instance()->captureModule()->setCapturedFramesMap(calculateSignature(filter), 0);
    }

    return true;
}

bool TestEkosCaptureCount::fillCapturedFramesMap(QString capturedFramesMap)
{
    if (capturedFramesMap != "")
    {
        for (QString value : capturedFramesMap.split(","))
        {
            KVERIFY_SUB(value.indexOf(":") > -1);
            QString filter = value.left(value.indexOf(":"));
            int count      = value.right(value.length()-value.indexOf(":")-1).toInt();
            Ekos::Manager::Instance()->captureModule()->setCapturedFramesMap(calculateSignature(filter), count);
        }
    }

    return true;
}

bool TestEkosCaptureCount::setExpectedFrames(QString expectedFrames)
{
    if (expectedFrames != "")
    {
        for (QString value : expectedFrames.split(","))
        {
            KVERIFY_SUB(value.indexOf(":") > -1);
            QString filter = value.left(value.indexOf(":"));
            int count      = value.right(value.length()-value.indexOf(":")-1).toInt();
            m_expectedImages.insert(filter, count);
        }
    }
    else
        m_expectedImages.clear();

    return true;
}

int TestEkosCaptureCount::totalCount(QString sequence)
{
    if (sequence == "")
        return 0;

    int total = 0;
    for (QString value : sequence.split(","))
        total += value.right(value.length()-value.indexOf(":")-1).toInt();

    return total;
}
/* *********************************************************************************
 *
 * Slots
 *
 * ********************************************************************************* */

void TestEkosCaptureCount::captureComplete(const QString &filename, double exposureSeconds, const QString &filter, double hfr)
{
    Q_UNUSED(filename);
    Q_UNUSED(exposureSeconds);
    Q_UNUSED(hfr);

    // reduce the for the job's signature the number of expected images
    m_expectedImages.insert(filter, m_expectedImages.value(filter, 0) - 1);
}

void TestEkosCaptureCount::schedulerStateChanged(Ekos::SchedulerState status)
{
    m_SchedulerStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedSchedulerStates.isEmpty() && expectedSchedulerStates.head() == status)
        expectedSchedulerStates.dequeue();

}

/* *********************************************************************************
 *
 * Main function
 *
 * ********************************************************************************* */

QTEST_KSTARS_MAIN(TestEkosCaptureCount)
