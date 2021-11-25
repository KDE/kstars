/*
    KStars UI tests for capture workflows (re-focus, dithering, guiding, ...)

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_workflow.h"

#include "kstars_ui_tests.h"
#include "indi/guimanager.h"
#include "Options.h"
#include "skymapcomposite.h"


TestEkosCaptureWorkflow::TestEkosCaptureWorkflow(QObject *parent) : TestEkosCaptureWorkflow::TestEkosCaptureWorkflow("Internal", parent){}

TestEkosCaptureWorkflow::TestEkosCaptureWorkflow(QString guider, QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper(guider);
    m_CaptureHelper->m_GuiderDevice = "Guide Simulator";
}

bool TestEkosCaptureWorkflow::executeFocusing()
{
    // switch to focus module
    Ekos::Focus *focus = Ekos::Manager::Instance()->focusModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(focus, 1000));

    // run initial focus sequence
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_COMPLETE);
    KTRY_CLICK_SUB(focus, startFocusB);
    KWRAP_SUB(KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 180000));
    // success
    return true;
}

void TestEkosCaptureWorkflow::testCaptureRefocus()
{
    Ekos::Manager *manager = Ekos::Manager::Instance();
    QVERIFY(prepareCapture());
    QVERIFY(executeFocusing());

    // start capturing, expect focus after first captured frame
    Ekos::Capture *capture = manager->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_FOCUSING);
    KTRY_CLICK(capture, startB);
    // focusing must have started 60 secs + exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000 + 10000 + 1000*capture->captureExposureN->value());
}

void TestEkosCaptureWorkflow::testCaptureRefocusAbort()
{
    Ekos::Manager *manager = Ekos::Manager::Instance();
    QVERIFY(prepareCapture());
    QVERIFY(executeFocusing());

    // start capturing, expect focus after first captured frame
    Ekos::Capture *capture = manager->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    KTRY_CLICK(capture, startB);
    // focusing must have started 60 secs + exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 60000 + 10000 + 1000*capture->captureExposureN->value());
    // now abort capturing
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    capture->abort();
    // expect that focusing aborts
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 10000);
}

void TestEkosCaptureWorkflow::testPreCaptureScriptExecution()
{
    // static test with three exposures
    int count = 3;
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";
    KTRY_CAPTURE_CONFIGURE_LIGHT(2.0, count, 0.0, "Luminance", imagepath);

    // create pre-capture script
    KTELL("Destination path : " + destination->path());
    QString precapture_script = destination->path() + "/precapture.sh";
    QString precapture_log    = destination->path() + "/precapture.log";
    // create a script that reads the number from its log file, increases it by 1 and outputs it to the logfile
    // with this script we can count how often it has been executed
    QStringList script_content({"#!/bin/sh",
                                QString("nr=`head -1 %1|| echo 0` 2> /dev/null").arg(precapture_log),
                                QString("nr=$(($nr+1))\necho $nr > %1").arg(precapture_log)});
    // create executable pre-capture script
    QVERIFY2(m_CaptureHelper->writeFile(precapture_script, script_content,
                                        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner),
             "Creating precapture script failed!");

    QMap<Ekos::ScriptTypes, QString> scripts;
    scripts.insert(Ekos::SCRIPT_PRE_CAPTURE, precapture_script);

    // setup scripts - starts as thread since clicking on capture blocks
    bool success = false;
    QTimer::singleShot(1000, capture, [&] {success = m_CaptureHelper->fillScriptManagerDialog(scripts);});
    // open script manager
    KTRY_CLICK(capture, scriptManagerB);
    // verify if script configuration succeeded
    QVERIFY2(success, "Scripts set up failed!");

    // create capture sequence
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    KTRY_CAPTURE_CLICK(addToQueueB);
    // check if row has been added
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 1, 1000);
    // setup the expected number of frames
    for (int i = 0; i < count; i++)
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    // start capture
    KTRY_CLICK(capture, startB);
    // wait that all frames have been taken
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);

    // check the log file if it holds the expected number
    QFile logfile(precapture_log);
    logfile.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&logfile);
    QString countstr = in.readLine();
    logfile.close();
    QVERIFY2(countstr.toInt() == count,
             QString("Pre-capture script not executed as often as expected: %1 expected, %2 detected.")
             .arg(count).arg(countstr).toLocal8Bit());
}

void TestEkosCaptureWorkflow::testGuidingDeviationSuspendingCapture()
{
    const double deviation_limit = 2.0;
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    // set guide deviation guard to < 2"
    KTRY_SET_CHECKBOX(capture, limitGuideDeviationS, true);
    KTRY_SET_DOUBLESPINBOX(capture, limitGuideDeviationN, deviation_limit);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";
    // build a LRGB sequence
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Luminance", imagepath);
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Red", imagepath);
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Green", imagepath);
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Blue", imagepath);

    // set Dubhe as target and slew there
    SkyObject *target = KStars::Instance()->data()->skyComposite()->findByName("Dubhe");
    m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true);

    // start guiding
    m_CaptureHelper->startGuiding(1.0);

    // start capture
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    // wait until capturing starts
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);
    // wait for settling
    QTest::qWait(2000);
    // create a guide drift
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_SUSPENDED);
    Ekos::Manager::Instance()->mountModule()->doPulse(RA_INC_DIR, 2000, DEC_INC_DIR, 2000);
    qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA+DEC guiding pulses.";
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);
    // expect that capturing continues
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 30000);
    // verify that capture starts only once
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PROGRESS);
    QTest::qWait(20000);
    QVERIFY2(m_CaptureHelper->expectedCaptureStates.size() > 0, "Multiple capture starts.");
}

void TestEkosCaptureWorkflow::testGuidingDeviationAbortCapture()
{
    const double deviation_limit = 2.0;
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    // set guide deviation guard to < 2"
    KTRY_SET_CHECKBOX(capture, limitGuideDeviationS, true);
    KTRY_SET_DOUBLESPINBOX(capture, limitGuideDeviationN, deviation_limit);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";
    // build a simple 5xL sequence
    KTRY_CAPTURE_ADD_LIGHT(30.0, 5, 5.0, "Luminance", imagepath);
    // set Dubhe as target and slew there
    SkyObject *target = KStars::Instance()->data()->skyComposite()->findByName("Dubhe");
    m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true);

    // start guiding
    m_CaptureHelper->startGuiding(1.0);

    // start capture
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    // wait until capturing starts
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);
    // wait for settling
    QTest::qWait(2000);
    // create a guide drift
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_SUSPENDED);
    Ekos::Manager::Instance()->mountModule()->doPulse(RA_INC_DIR, 2000, DEC_INC_DIR, 2000);
    qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA+DEC guiding pulses.";
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    // wait that capturing gets suspended
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);
    // abort capturing
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    // check that it has been aborted
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);
    // wait that the guiding deviation is below the limit and
    // verify that capture does not start
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PROGRESS);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() < deviation_limit, 20000);

    QTest::qWait(20000);
    QVERIFY2(m_CaptureHelper->expectedCaptureStates.size() > 0, "Capture has been restarted although aborted.");
}


/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosCaptureWorkflow::testCaptureRefocus_data()
{
    prepareTestData(31.0, "Luminance:3");
}

void TestEkosCaptureWorkflow::testCaptureRefocusAbort_data()
{
    prepareTestData(31.0, "Luminance:3");
}


/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosCaptureWorkflow::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    // set guiding agressiveness (needs to be set before the guiding module starts)
    Options::setRAProportionalGain(0.25);
    Options::setDECProportionalGain(0.25);
    // start the profile
    QVERIFY(m_CaptureHelper->startEkosProfile());
    m_CaptureHelper->init();
    QStandardPaths::setTestModeEnabled(true);

    QFileInfo test_dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation), "test");
    destination = new QTemporaryDir(test_dir.absolutePath());
    QVERIFY(destination->isValid());
    QVERIFY(destination->autoRemove());

    QVERIFY(prepareTestCase());
}

void TestEkosCaptureWorkflow::cleanupTestCase()
{
    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

bool TestEkosCaptureWorkflow::prepareTestCase()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // set logging defaults for alignment
    Options::setVerboseLogging(false);
    Options::setLogToFile(false);

    // set mount defaults
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000));
    // set primary scope to my favorite FSQ-85
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), primaryScopeFocalIN, 450.0);
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), primaryScopeApertureIN, 85.0);
    // set guide scope to a Tak 7x50
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), guideScopeFocalIN, 170.0);
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), guideScopeApertureIN, 50.0);
    // save values
    KTRY_CLICK_SUB(ekos->mountModule(), saveB);
    // disable meridian flip
    KTRY_SET_CHECKBOX_SUB(ekos->mountModule(), meridianFlipCheckBox, false);
    // switch to focus module
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(ekos->focusModule() != nullptr, 5000));
    // set exposure time to 5 sec
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->focusModule(), exposureIN, 5.0);
    // use full field
    KTRY_SET_CHECKBOX_SUB(ekos->focusModule(), useFullField, true);
    // use iterative algorithm
    KTRY_SET_COMBO_SUB(ekos->focusModule(), focusAlgorithmCombo, "Polynomial");
    // step size 5000
    KTRY_SET_SPINBOX_SUB(ekos->focusModule(), stepIN, 5000);
    // set max travel to 100000
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->focusModule(), maxTravelIN, 100000);
    // set low accuracy to 10%
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->focusModule(), toleranceIN, 10.0);
    // ensure that guiding calibration is NOT reused
    Options::setReuseGuideCalibration(false);
    // 4 guide calibration steps are sufficient
    Options::setAutoModeIterations(4);
    // Set the guiding and ST4 device
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), guiderCombo, m_CaptureHelper->m_GuiderDevice);
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), ST4Combo, m_CaptureHelper->m_MountDevice);
    // select primary scope (higher focal length seems better for the guiding simulator)
    KTRY_SET_COMBO_INDEX_SUB(Ekos::Manager::Instance()->guideModule(), FOVScopeCombo, ISD::CCD::TELESCOPE_PRIMARY);


    // close INDI window
    GUIManager::Instance()->close();

    // preparation successful
    return true;
}

void TestEkosCaptureWorkflow::init() {
    // reset counters
    image_count  = 0;
    // clear image directory
    QVERIFY(m_CaptureHelper->getImageLocation()->removeRecursively());
}

void TestEkosCaptureWorkflow::cleanup() {
    Ekos::Manager::Instance()->focusModule()->abort();

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    capture->abort();
    capture->clearSequenceQueue();
    KTRY_SET_CHECKBOX(capture, limitRefocusS, false);
}


bool TestEkosCaptureWorkflow::prepareCapture()
{
    QFETCH(double, exptime);
    QFETCH(QString, sequence);
    // test data
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000));

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";

    // create the destination for images
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << imagepath;

    // create capture sequences
    KTRY_SET_CHECKBOX_SUB(capture, limitRefocusS, true);
    KTRY_SET_SPINBOX_SUB(capture, limitRefocusN, 1);
    KVERIFY_SUB(m_CaptureHelper->fillCaptureSequences(target, sequence, exptime, imagepath));

    // everything successfully completed
    return true;
}

void TestEkosCaptureWorkflow::prepareTestData(double exptime, QString sequence)
{
    QTest::addColumn<double>("exptime");             /*!< exposure time */
    QTest::addColumn<QString>("sequence");           /*!< list of filters */

    QTest::newRow(QString("seq=%1").arg(sequence).toStdString().c_str()) << exptime << sequence;
}

QDir *TestEkosCaptureWorkflow::getImageLocation()
{
    if (imageLocation == nullptr || imageLocation->exists())
        imageLocation = new QDir(destination->path() + "/images");

    return imageLocation;
}

/* *********************************************************************************
 *
 * Main function
 *
 * ********************************************************************************* */

QTEST_KSTARS_WITH_GUIDER_MAIN(TestEkosCaptureWorkflow)
