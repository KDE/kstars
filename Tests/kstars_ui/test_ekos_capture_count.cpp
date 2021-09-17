/*
    KStars UI tests for verifying correct counting of the capture module

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_count.h"

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"

#include "test_ekos_capture_helper.h"

/* *****************************************************************************
 * Determining the correct number of frames being already captured and need to be
 * captured in future is surprisingly complex - which comes from the variety of
 * options how capturing can be configured and is being processed.
 *
 * The simplest way is when the Capture module is used standalone. The only thing
 * that needs to be considered is the option setting a captured frames map before
 * starting. This map needs to be considered and the Capture module should capture
 * only the difference between the numbers defined by the capture sequence and
 * these already existing frames.
 *
 * In combination with the Scheduler module, things get complicated, since the
 * Scheduler module
 * - has several options how a capture sequence will be repeated
 *   (single run, fixed number of iterations, infinite looping, termination by date)
 * - has the option "Remember job progress", where if selected existing frames
 *   need to be considered and counted.
 * ***************************************************************************** */

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

void TestEkosCaptureCount::testSchedulerCapture()
{
    KTELL("Expect captures and an idle signal at the end");
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    expectedSchedulerStates.enqueue(Ekos::SCHEDULER_IDLE);

    KTELL("Prepare scheduler captures");
    QVERIFY(prepareScheduledCapture(SchedulerJob::FINISH_REPEAT));

    KTELL("Start scheduler job");
    KTRY_CLICK(Ekos::Manager::Instance()->schedulerModule(), startB);

    KTELL("Ensure that the scheduler has started capturing");
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() == 0, 120000);

    KTELL("Wait for Scheduler to finish capturing");
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedSchedulerStates, 120000);

    KTELL("Verify whether all frames are captured as expected");
    QVERIFY2(checkCapturedFrames(), "Capturing did not produce the expected amount of frames.");
}

void TestEkosCaptureCount::testSchedulerCaptureInfiteLooping()
{
    // prepare captured frames
    QVERIFY(prepareScheduledCapture(SchedulerJob::FINISH_LOOP));
}

/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosCaptureCount::testSchedulerCapture_data()
{
#ifdef LONG_TEST
    prepareTestData(0.1, "Red:2,Green:2,Blue:2", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(0.1, "Red:2,Green:2,Blue:2", "Red:3,Green:1,Blue:2", "Green:1");
    prepareTestData(0.1, "Red:1,Red:1,Green:1,Green:1,Blue:1,Blue:1", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(0.1, "Red:1,Green:1,Blue:1,Red:1,Green:1,Blue:1", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(0.1, "Red:1,Green:1,Blue:1", "Red:3,Green:1,Blue:3", "Green:2", 3);
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:4,Green:1,Blue:1", "Luminance:1,Red:1");
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "", "Luminance:10,Red:2,Green:2,Blue:2", 2);
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:15,Red:1,Green:2,Blue:2", "Red:1", 2);
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:15,Red:2,Green:3,Blue:3", "Red:1", 3);
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:15,Red:3,Green:3,Blue:2", "Blue:1", 3);
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:5,Red:1,Green:1,Blue:1", "Luminance:10,Red:2,Green:2,Blue:2", 3);
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:2,Red:1,Green:1,Blue:1", "Luminance:13,Red:2,Green:2,Blue:2", 3);
#else
    prepareTestData(0.1, "Red:2,Green:2,Blue:2", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(0.1, "Red:1,Green:1,Blue:1,Red:1,Green:1,Blue:1", "Red:2,Green:1,Blue:2", "Green:1");
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:4,Green:1,Blue:1", "Luminance:1,Red:1");
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:5,Red:1,Green:1,Blue:1", "Luminance:10,Red:2,Green:2,Blue:2", 3);
#endif
}

void TestEkosCaptureCount::testCaptureWithCaptureFramesMap_data()
{
    // use the same test set
    testSchedulerCapture_data();
}

void TestEkosCaptureCount::testSchedulerCaptureInfiteLooping_data()
{
    prepareTestData(0.1, "Luminance:2", "Luminance:2", "");
    prepareTestData(0.1, "Luminance:3,Red:1,Green:1,Blue:1,Luminance:2", "Luminance:5,Green:1,Blue:1", "");
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
    QVERIFY(m_CaptureHelper->startEkosProfile());
    m_CaptureHelper->init();
    // do not show images
    Options::setUseFITSViewer(false);
    // disable twilight warning
    KMessageBox::saveDontShowAgainYesNo("astronomical_twilight_warning", KMessageBox::ButtonCode::No);
}

void TestEkosCaptureCount::cleanupTestCase()
{
    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosCaptureCount::init()
{
    connect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::captureComplete, this, &TestEkosCaptureCount::captureComplete);
    connect(Ekos::Manager::Instance()->schedulerModule(), &Ekos::Scheduler::newStatus, this, &TestEkosCaptureCount::schedulerStateChanged);
    QStandardPaths::setTestModeEnabled(true);
    // clear image directory
    QVERIFY(m_CaptureHelper->getImageLocation()->removeRecursively());
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
    m_CaptureHelper->cleanupScheduler();
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
    QTest::qWait(500);
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
    QString imagepath = m_CaptureHelper->getImageLocation()->path() + "/" + target;

    // create the destination for images
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << imagepath;

    // create capture sequences
    for (int i = 0; i < iterations; i++)
        KVERIFY_SUB(m_CaptureHelper->fillCaptureSequences(target, sequence, exptime, imagepath));

    // fill the captured frames map that hold the numbers of already taken frames
    KVERIFY_SUB(fillCapturedFramesMap(capturedFramesMap));

    // fill the map of expected frames
    KVERIFY_SUB(setExpectedFrames(expectedFrames));

    // everything successfully completed
    return true;
}

bool TestEkosCaptureCount::prepareScheduledCapture(SchedulerJob::CompletionCondition completionCondition)
{
    QFETCH(double, exptime);
    QFETCH(QString, sequence);
    QFETCH(QString, capturedFramesMap);
    QFETCH(QString, expectedFrames);
    QFETCH(int, iterations);
    QFETCH(bool, rememberJobProgress);

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000));

    // create the destination for images
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << m_CaptureHelper->getImageLocation()->path();

    // step 1: create the frames due to the captured frames map
    if (capturedFramesMap != "")
    {
        KVERIFY_SUB(m_CaptureHelper->fillCaptureSequences(target, capturedFramesMap, exptime, m_CaptureHelper->getImageLocation()->filePath(target)));
        KVERIFY_SUB(fillCapturedFramesMap(""));
        KVERIFY_SUB(setExpectedFrames(capturedFramesMap));

        // create the expected frames
        KVERIFY_SUB(executeCapturing());

        // clean up
        capture->clearSequenceQueue();
    }

    // step 2: create the sequence for the test
    KVERIFY_SUB(m_CaptureHelper->fillCaptureSequences(target, sequence, exptime, m_CaptureHelper->getImageLocation()->path()));
    KVERIFY_SUB(fillCapturedFramesMap(""));
    if (rememberJobProgress)
        KVERIFY_SUB(setExpectedFrames(expectedFrames));
    else
        for (int i = 0; i < iterations; i++)
            KVERIFY_SUB(setExpectedFrames(sequence));

    // save current capture sequence to Ekos sequence file
    QString sequenceFile = m_CaptureHelper->destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file" << sequenceFile << "created.";
    KVERIFY_SUB(Ekos::Manager::Instance()->captureModule()->saveSequenceQueue(sequenceFile));

    // setup scheduler
    setupScheduler(sequenceFile, sequence, capturedFramesMap, completionCondition, iterations, rememberJobProgress, exptime);

    // everything successfully completed
    return true;
}

bool TestEkosCaptureCount::setupScheduler(QString sequenceFile, QString sequence, QString capturedFramesMap, SchedulerJob::CompletionCondition completionCondition,
                                          int iterations, bool rememberJobProgress, double exptime)
{
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
    Options::setRememberJobProgress(rememberJobProgress);
    // disable INDI stopping after scheduler finished
    Options::setStopEkosAfterShutdown(false);

    // set the completion condition
    switch (completionCondition) {
    case SchedulerJob::FINISH_REPEAT:
        // repeat the job for a fixed amount
        KTRY_SET_RADIOBUTTON_SUB(scheduler, repeatCompletionR, true);
        KTRY_SET_SPINBOX_SUB(scheduler, repeatsSpin, iterations);
        break;
    case SchedulerJob::FINISH_LOOP:
        KTRY_SET_RADIOBUTTON_SUB(scheduler, loopCompletionR, true);
        break;
    default:
        QWARN(QString("Unsupported completion condition %1!").arg(completionCondition).toStdString().c_str());
        return false;
        break;
    }
    // add scheduler job
    KTRY_CLICK_SUB(scheduler, addToQueueB);

    // verify the displayed capture counts
    KVERIFY_SUB(verifySchedulerCounting(sequence, capturedFramesMap, completionCondition, iterations, rememberJobProgress, exptime));

    // everything worked as expected
    return true;
}

bool TestEkosCaptureCount::verifySchedulerCounting(QString sequence, QString capturedFramesMap, SchedulerJob::CompletionCondition completionCondition,
                                                   int iterations, bool rememberJobProgress, double exptime)
{
    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    KTRY_GADGET_SUB(scheduler, QTableWidget, queueTable);
    KVERIFY_SUB(queueTable->rowCount() == 1);
    KVERIFY_SUB(queueTable->columnCount() > 3);
    QString displayedCounts = queueTable->item(0, 2)->text();
    KVERIFY2_SUB(displayedCounts.indexOf("/") > 0, "Scheduler job table does not display in style captured/total.");

    int total = -1, captured = -1, total_repeat_expected = 0;

    // check display of expected frames
    if (completionCondition == SchedulerJob::FINISH_REPEAT)
    {
        total = displayedCounts.right(displayedCounts.length() - displayedCounts.indexOf("/") - 1).toInt();
        total_repeat_expected = totalCount(sequence) * iterations;
        KVERIFY2_SUB(total == total_repeat_expected,
                     QString("Scheduler job table shows %1 expected frames instead of %2.").arg(total).arg(total_repeat_expected).toStdString().c_str());
    }

    // check display of already captured
    captured = displayedCounts.left(displayedCounts.indexOf("/")).toInt();
    // determine expected captures
    int captured_expected = 0;
    // the captured frames map is only relevant if the "Remember job progress" option is selected
    if (rememberJobProgress)
    {
        QMap<QString, uint16_t> capturedMap = framesMap(capturedFramesMap);
        QMap<QString, uint16_t> sequenceMap = framesMap(sequence);
        // for each filter, the displayed total is limited by the capture sequence multiplied by the number of iterations
        for (QString key : sequenceMap.keys())
            captured_expected += std::min(capturedMap[key], static_cast<uint16_t>(sequenceMap[key] * iterations));
    }
    // execute the check
    KVERIFY2_SUB(captured == captured_expected,
                 QString("Scheduler job table shows %1 captured frames instead of %2.").arg(captured).arg(captured_expected).toStdString().c_str());

    // check estimated duration time (only relevant for repeats
    if (completionCondition == SchedulerJob::FINISH_REPEAT)
    {
        QString estimation = queueTable->item(0, 7)->text();
        QTime estimatedDuration = QTime::fromString(estimation, "HH:mm:ss");
        int duration = estimatedDuration.second() + 60*estimatedDuration.minute() + 3600*estimatedDuration.hour();
        KVERIFY2_SUB(std::fabs((total_repeat_expected - captured_expected)*exptime - duration) <= 1,
                     QString("Scheduler job table shows %1 seconds expected instead of %2.").arg(duration).arg((total_repeat_expected - captured_expected)*exptime).toStdString().c_str());
    }
    // everything worked as expected
    return true;
}

void TestEkosCaptureCount::prepareTestData(double exptime, QString sequence, QString capturedFramesMap, QString expectedFrames, int iterations)
{

    QTest::addColumn<double>("exptime");             /*!< exposure time */
    QTest::addColumn<QString>("sequence");           /*!< list of filters */
    QTest::addColumn<QString>("capturedFramesMap");  /*!< list of frame counts */
    QTest::addColumn<QString>("expectedFrames");     /*!< list of frames per filter that are expected */
    QTest::addColumn<int>("iterations");             /*!< how often should the sequence be repeated */
    QTest::addColumn<bool>("rememberJobProgress");   /*!< "Remember job progress" option selected */

    for (bool remember : {false, true})
        QTest::newRow(QString("seq=%1, captured=%2, it=%3, remember=%4").arg(sequence).arg(capturedFramesMap).arg(iterations).arg(remember ? "true":"false").toStdString().c_str())
                << exptime << sequence << capturedFramesMap << expectedFrames << iterations << remember;
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
            Ekos::Manager::Instance()->captureModule()->setCapturedFramesMap(m_CaptureHelper->calculateSignature(target, filter), count);
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
            if (m_expectedImages.contains(filter))
                m_expectedImages[filter] += count;
            else
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


QMap<QString, uint16_t> TestEkosCaptureCount::framesMap(QString sequence)
{
    QMap<QString, uint16_t> result;

    if (sequence == "")
        return result;

    for (QString value : sequence.split(","))
    {
        QString filter = value.left(value.indexOf(":"));
        int count      = value.right(value.length()-value.indexOf(":")-1).toInt();
        if (result.contains(filter))
            result[filter] += count;
        else
            result[filter] = count;
    }

    return result;
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
