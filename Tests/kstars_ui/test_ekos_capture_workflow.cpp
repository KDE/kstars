/*
    KStars UI tests for capture workflows (re-focus, dithering, guiding, ...)

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_workflow.h"

#include "kstars_ui_tests.h"
#include "ui_calibrationoptions.h"
#include "indi/guimanager.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "ekos/capture/sequencejobstate.h"

#define SHUTTER_UNKNOWN -1
#define SHUTTER_NO       0
#define SHUTTER_YES      1


TestEkosCaptureWorkflow::TestEkosCaptureWorkflow(QObject *parent) :
    TestEkosCaptureWorkflow::TestEkosCaptureWorkflow("Internal", parent) {}

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
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

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
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates,
                                     60000 + 10000 + 1000 * capture->captureExposureN->value());
}

void TestEkosCaptureWorkflow::testCaptureRefocusAbort()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    QVERIFY(prepareCapture());
    QVERIFY(executeFocusing());

    // start capturing, expect focus after first captured frame
    Ekos::Capture *capture = manager->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    KTRY_CLICK(capture, startB);
    // focusing must have started 60 secs + exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates,
                                     60000 + 10000 + 1000 * capture->captureExposureN->value());
    // now abort capturing
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    capture->abort();
    // expect that focusing aborts
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 10000);
}

void TestEkosCaptureWorkflow::testPreCaptureScriptExecution()
{
    // default initialization
    QVERIFY(prepareTestCase());

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
    // wait until completed
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
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
    // default initialization
    QVERIFY(prepareTestCase());

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
    // default initialization
    QVERIFY(prepareTestCase());

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

void TestEkosCaptureWorkflow::testInitialGuidingLimitCapture()
{
    // default initialization
    QVERIFY(prepareTestCase());

    const double deviation_limit = 2.0;
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    // set start guide deviation guard to < 2" but disable the other one
    KTRY_SET_CHECKBOX(capture, startGuiderDriftS, true);
    KTRY_SET_DOUBLESPINBOX(capture, startGuiderDriftN, deviation_limit);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";
    // build a simple 5xL sequence
    KTRY_CAPTURE_ADD_LIGHT(30.0, 5, 5.0, "Luminance", imagepath);
    // set Dubhe as target and slew there
    SkyObject *target = KStars::Instance()->data()->skyComposite()->findByName("Dubhe");
    m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true);

    // start guiding
    m_CaptureHelper->startGuiding(2.0);

    // wait 5 seconds
    QTest::qWait(5000);

    // create a guide drift
    Ekos::Manager::Instance()->mountModule()->doPulse(RA_INC_DIR, 2000, DEC_INC_DIR, 2000);
    qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA+DEC guiding pulses.";

    // wait until guide deviation is present
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() > deviation_limit, 15000);

    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // start capture but expect it being suspended first
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    // verify that capturing does not start before the guide deviation is below the limit
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() >= deviation_limit, 60000);
    // wait 3 seconds and then ensure that capture did not start
    QTest::qWait(3000);
    QTRY_VERIFY(m_CaptureHelper->expectedCaptureStates.size() > 0);
    // wait until guiding deviation is below the limit
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() < deviation_limit, 60000);
    // wait until capturing starts
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);
}

void TestEkosCaptureWorkflow::testCaptureWaitingForTemperature()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // initial and target temperature
    QFETCH(double, initTemp);
    QFETCH(double, targetTemp);

    // initialize the CCD temperature and wait until it is reached
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_CCDDevice, "CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", initTemp);
    QTRY_VERIFY_WITH_TIMEOUT(std::abs(capture->temperatureOUT->text().toDouble()) <= Options::maxTemperatureDiff(), 60000);

    // set target temperature
    KTRY_SET_DOUBLESPINBOX(capture, cameraTemperatureN, targetTemp);
    KTRY_SET_CHECKBOX(capture, cameraTemperatureS, true);

    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_LIGHT(10.0, 5, 5.0, "Luminance", getImageLocation()->path() + "/test");
    // expect capturing state
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

    // start capturing
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
    // check if the temperature is at the expected level
    QTRY_VERIFY2(std::abs(capture->temperatureOUT->text().toDouble() - targetTemp) <= Options::maxTemperatureDiff(),
                 QString("Temperature %1°C not at the expected level of %2°C").arg(capture->temperatureOUT->text()).arg(
                     targetTemp).toLocal8Bit());

    // stop capturing
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    // check if capturing has stopped
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);

    // restart again to check whether an already reached temperature is handled properly
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 20000);
    // check if the temperature is at the expected level
    QTRY_VERIFY2(std::abs(capture->temperatureOUT->text().toDouble() - targetTemp) <= Options::maxTemperatureDiff(),
                 QString("Temperature %1°C not at the expected level of %2°C").arg(capture->temperatureOUT->text()).arg(
                     targetTemp).toLocal8Bit());

    // stop capturing
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    // check if capturing has stopped
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);

    // change temperature back to initial value
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_CCDDevice, "CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", initTemp);
    QTRY_VERIFY_WITH_TIMEOUT(std::abs(capture->temperatureOUT->text().toDouble()) <= Options::maxTemperatureDiff(), 60000);

    // start capturing for a second time
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
    // check if the temperature is at the expected level
    QTRY_VERIFY2(std::abs(capture->temperatureOUT->text().toDouble() - targetTemp) <= Options::maxTemperatureDiff(),
                 QString("Temperature %1°C not at the expected level of %2°C").arg(capture->temperatureOUT->text()).arg(
                     targetTemp).toLocal8Bit());

}

void TestEkosCaptureWorkflow::testCaptureWaitingForRotator()
{
    // use the rotator simulator
    m_CaptureHelper->m_RotatorDevice = "Rotator Simulator";

    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // open the rotator dialog
    KTRY_CLICK(capture, rotatorB);

    QWidget *rotatorDialog = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(rotatorDialog = Ekos::Manager::Instance()->findChild<QWidget *>("RotatorDialog"), 2000);

    // target angle rotation
    double targetAngle = 90.0;
    // enforce rotation
    KTRY_SET_CHECKBOX(rotatorDialog, enforceRotationCheck, true);
    // set the target rotation angle
    KTRY_SET_DOUBLESPINBOX(rotatorDialog, targetPASpin, targetAngle);
    // Close with OK
    KTRY_GADGET(capture, QDialogButtonBox, buttonBox);
    QTest::mouseClick(buttonBox->button(QDialogButtonBox::Ok), Qt::LeftButton);

    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Luminance", getImageLocation()->path() + "/test");
    // expect capturing state
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

    // start capturing
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

    KTRY_GADGET(capture, QLineEdit, PAOut);
    QTRY_VERIFY2(fabs(PAOut->text().toDouble() - targetAngle) * 60 <= Options::astrometryRotatorThreshold(),
                 QString("Rotator angle %1° not at the expected value of %2°").arg(PAOut->text()).arg(targetAngle).toLocal8Bit());
}

void TestEkosCaptureWorkflow::testFlatManualSource()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for darks
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    // select manual flat method
    KTRY_SELECT_FLAT_METHOD(manualSourceC, false, false);
    // build a simple 1xL flat sequence
    KTRY_CAPTURE_ADD_FRAME("Flat", 1, 1, 0.0, "Luminance", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", imagepath);
    // click OK or Cancel?
    QFETCH(bool, clickModalOK);
    QFETCH(bool, clickModal2OK);
    if (clickModalOK)
    {
        // start the flat sequence
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
        KTRY_CLICK(capture, startB);
        // click OK in the modal dialog for covering the telescope
        CLOSE_MODAL_DIALOG(0);
        // check if one single flat is captured
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
        if (clickModal2OK)
        {
            // expect the light sequence
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
            // click OK in the modal dialog for uncovering the telescope
            CLOSE_MODAL_DIALOG(0);
            // check if one single light is captured
            KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
        }
        else
        {
            // this must not happen
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
            // click Cancel in the modal dialog for uncovering the telescope
            CLOSE_MODAL_DIALOG(1);
            // check if capturing has not been started
            KTRY_CAPTURE_GADGET(QPushButton, startB);
            // within 5 secs the job must be stopped ...
            QTRY_VERIFY_WITH_TIMEOUT(startB->icon().name() == QString("media-playback-start"), 5000);
            // ... and capturing has not been started
            QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() > 0, 5000);
        }
    }
    else
    {
        // this must not happen
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
        // start flats capturing
        KTRY_CLICK(capture, startB);
        // click Cancel in the modal dialog for covering the telescope
        CLOSE_MODAL_DIALOG(1);
        // check if capturing has not been started
        KTRY_CAPTURE_GADGET(QPushButton, startB);
        // within 5 secs the job mus be stopped ...
        QTRY_VERIFY_WITH_TIMEOUT(startB->icon().name() == QString("media-playback-start"), 5000);
        // ... and capturing has not been started
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() > 0, 5000);
    }
}


void TestEkosCaptureWorkflow::testFlatLightPanelSource()
{
    // use the light panel simulator
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    // select internal or external flat light
    QFETCH(bool, internalLight);
    if (internalLight == true)
        KTRY_SELECT_FLAT_METHOD(flatDeviceSourceC, false, false);
    else
        KTRY_SELECT_FLAT_METHOD(darkDeviceSourceC, false, false);
    // build a simple 1xL flat sequence
    KTRY_CAPTURE_ADD_FRAME("Flat", 1, 1, 0.0, "Luminance", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", imagepath);

    // start the sequence
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    // check if one single flat is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
}


void TestEkosCaptureWorkflow::testDustcapSource()
{
    // use the Flip Flat in simulator mode
    m_CaptureHelper->m_LightPanelDevice = "Flip Flat";

    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->setupTab, 1000);
    QVERIFY(m_CaptureHelper->setupEkosProfile("Simulators", false));
    // start the profile
    KTRY_EKOS_CLICK(processINDIB);

    // wait until the disconnect button is enabled which shows that INDI has started
    KTRY_GADGET(ekos, QAbstractButton, disconnectB);
    QTRY_VERIFY_WITH_TIMEOUT(disconnectB->isEnabled(), 10000);

    // enable simulation
    SET_INDI_VALUE_SWITCH("Flip Flat", "SIMULATION", "ENABLE", true);
    // connect
    SET_INDI_VALUE_SWITCH("Flip Flat", "CONNECTION", "CONNECT", true);
    // park
    SET_INDI_VALUE_SWITCH("Flip Flat", "CAP_PARK", "PARK", true);
    // turn light off
    SET_INDI_VALUE_SWITCH("Flip Flat", "FLAT_LIGHT_CONTROL", "FLAT_LIGHT_OFF",
                          true);

    // Now all devices should be up and running
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 10000);

    // init the helper
    m_CaptureHelper->init();

    // receive status updates from all devices
    m_CaptureHelper->connectModules();

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    // select frame type and internal or external flat light
    QFETCH(bool, internalLight);
    QFETCH(QString, frametype);

    if (internalLight == true)
        KTRY_SELECT_FLAT_METHOD(flatDeviceSourceC, false, false);
    else
        KTRY_SELECT_FLAT_METHOD(darkDeviceSourceC, false, false);

    // build a simple 1xL flat sequence
    KTRY_CAPTURE_ADD_FRAME(frametype, 1, 1, 0.0, "Luminance", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", imagepath);

    // start the sequence
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    // check if one single flat is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
}


void TestEkosCaptureWorkflow::testWallSource()
{
    // use the light panel simulator
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    // select the wall as flat light source (az=90°, alt=0)
    KTRY_SELECT_FLAT_WALL(capture, "90", "0");

    // determine frame type
    QFETCH(QString, frametype);
    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_FRAME(frametype, 2, 1, 2.0, "Luminance", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", imagepath);

    // start the sequence
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_SLEWING);
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_IDLE);
    KTRY_CLICK(capture, startB);
    // check if mount has reached the expected position
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMountStates, 60000);
    // check if one single flat is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
}


void TestEkosCaptureWorkflow::testFlatPreMountAndDomePark()
{
    // use the light panel simulator
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    // use the dome simulator
    m_CaptureHelper->m_DomeDevice = "Dome Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    // select internal flat light, pre-mount and pre-dome park
    KTRY_SELECT_FLAT_METHOD(flatDeviceSourceC, true, true);
    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_FLAT(2, 1, 2.0, "Luminance", imagepath);

    // start the sequence
    m_CaptureHelper->expectedDomeStates.append(ISD::Dome::DOME_PARKED);
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_PARKED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    // check if mount has reached the expected position
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMountStates, 30000);
    // check if dome has reached the expected position
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedDomeStates, 30000);
    // check if one single flat is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
}

void TestEkosCaptureWorkflow::testFlatSyncFocus()
{
    // use the light panel simulator and focuser simulator
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());
    // set flat sync
    Options::setFlatSyncFocus(true);
    // run autofocus
    QVERIFY(executeFocusing());

    Ekos::Focus *focus = Ekos::Manager::Instance()->focusModule();
    // update the initial focuser position
    KTRY_GADGET(Ekos::Manager::Instance()->focusModule(), QLineEdit, absTicksLabel);
    int focusPosition = absTicksLabel->text().toInt();
    // move the focuser 100 steps out
    KTRY_SET_SPINBOX(focus, absTicksSpin, focusPosition + 100);
    // click goto
    KTRY_CLICK(focus, startGotoB);
    // check if new position has been reached
    QTRY_VERIFY_WITH_TIMEOUT(absTicksLabel->text().toInt() == focusPosition + 100, 5000);

    // capture flats with light panel source (simplest way to test)
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    // select internal flat light
    KTRY_SELECT_FLAT_METHOD(flatDeviceSourceC, false, false);
    // build a simple 5xL sequence
    KTRY_CAPTURE_CONFIGURE_FLAT(2, 1, 2.0, "Luminance", imagepath);

    // start the sequence
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    // check if one single flat is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
    // check if the focuser is at the determined focus position
    QTRY_VERIFY_WITH_TIMEOUT(absTicksLabel->text().toInt() == focusPosition, 5000);
}

void TestEkosCaptureWorkflow::testDarkManualCovering()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for darks
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Dark");

    // select manual flat method
    KTRY_SELECT_FLAT_METHOD(manualSourceC, false, false);
    // build a simple 1xBlue dark sequence
    KTRY_CAPTURE_ADD_FRAME("Dark", 1, 1, 0.0, "Blue", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", imagepath);
    // shutter type
    QFETCH(int, shutter);
    // click OK or Cancel?
    QFETCH(bool, clickModalOK);
    QFETCH(bool, clickModal2OK);

    // prepare shutter settings
    QStringList shutterfulCCDs;
    QStringList shutterlessCCDs;
    if (shutter == SHUTTER_NO)
        shutterlessCCDs.append(m_CaptureHelper->m_CCDDevice);
    else if (shutter == SHUTTER_YES)
        shutterfulCCDs.append(m_CaptureHelper->m_CCDDevice);
    // set the option values
    Options::setShutterfulCCDs(shutterfulCCDs);
    Options::setShutterlessCCDs(shutterlessCCDs);

    // if the camera has a shutter, no manual cover is required
    if (clickModalOK)
    {
        // start the flat sequence
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
        KTRY_CLICK(capture, startB);
        // if shutter type unknown, answer the the modal dialog with "No"
        if (shutter == SHUTTER_UNKNOWN)
            CLOSE_MODAL_DIALOG(1);
        // click OK in the modal dialog for covering the telescope if camera has no shutter
        if (shutter != SHUTTER_YES)
            CLOSE_MODAL_DIALOG(0);
        // check if one single flat is captured
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
        if (clickModal2OK)
        {
            // expect the light sequence
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
            // click OK in the modal dialog for uncovering the telescope
            CLOSE_MODAL_DIALOG(0);
            // check if one single light is captured
            KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
        }
        else
        {
            // this must not happen
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
            // click Cancel in the modal dialog for uncovering the telescope
            CLOSE_MODAL_DIALOG(1);
            // check if capturing has not been started
            KTRY_CAPTURE_GADGET(QPushButton, startB);
            // within 5 secs the job must be stopped ...
            QTRY_VERIFY_WITH_TIMEOUT(startB->icon().name() == QString("media-playback-start"), 5000);
            // ... and capturing has not been started
            QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() > 0, 5000);
        }
    }
    else
    {
        // this must not happen
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
        // start flats capturing
        KTRY_CLICK(capture, startB);
        // click Cancel in the modal dialog for covering the telescope
        CLOSE_MODAL_DIALOG(1);
        // check if capturing has not been started
        KTRY_CAPTURE_GADGET(QPushButton, startB);
        // within 5 secs the job mus be stopped ...
        QTRY_VERIFY_WITH_TIMEOUT(startB->icon().name() == QString("media-playback-start"), 5000);
        // ... and capturing has not been started
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() > 0, 5000);
    }
}


void TestEkosCaptureWorkflow::testDarksLibrary()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // ensure that we know that the CCD has a shutter
    m_CaptureHelper->ensureCCDShutter(true);

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // open the darks library dialog
    KTRY_CLICK(capture, darkLibraryB);
    QWidget *darkLibraryDialog = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(darkLibraryDialog = Ekos::Manager::Instance()->findChild<QWidget *>("DarkLibrary"), 2000);

    // select the CCD camera
    KTRY_SET_COMBO(darkLibraryDialog, cameraS, m_CaptureHelper->m_CCDDevice);

    // set dark library values to 3x1s darks
    KTRY_SET_DOUBLESPINBOX(darkLibraryDialog, maxExposureSpin, 1);
    KTRY_SET_SPINBOX(darkLibraryDialog, countSpin, 3);
    // expect exactly 3 frames
    for (int i = 0; i < 3; i++)
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    // start
    KTRY_CLICK(darkLibraryDialog, startB);
    // wait until completion
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.isEmpty(), 10000);
    // check if master frame has been created
    QFileInfo destinationInfo(QStandardPaths::writableLocation(QStandardPaths::DataLocation), "darks");
    QDir destination(destinationInfo.absoluteFilePath());
    QVERIFY(m_CaptureHelper->searchFITS(destination).size() == 1);
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

void TestEkosCaptureWorkflow::testFlatManualSource_data()
{
    QTest::addColumn<bool>("clickModalOK");             /*!< click "OK" on the modal dialog        */
    QTest::addColumn<bool>("clickModal2OK");            /*!< click "OK" on the second modal dialog */

    // both variants: click OK and click Cancel
    QTest::newRow("modal=true") << true << true;
    QTest::newRow("modal=true") << true << false;
    QTest::newRow("modal=false") << false << true;
}

void TestEkosCaptureWorkflow::testFlatLightPanelSource_data()
{
    QTest::addColumn<bool>("internalLight");             /*!< use internal or external flat light */

    QTest::newRow("light=internal") << true;   // light source integrated into the light panel
    QTest::newRow("light=external") << false;  // external light source used
}

void TestEkosCaptureWorkflow::testDustcapSource_data()
{
    QTest::addColumn<QString>("frametype");              /*!< frame type (Dark or Flat)           */
    QTest::addColumn<bool>("internalLight");             /*!< use internal or external flat light */

    QTest::newRow("Flat, light=internal") << "Flat" << true;   // flat + light source integrated into the light panel
    QTest::newRow("Flat, light=external") << "Flat" << false;  // flat + external light source used
    QTest::newRow("Dark") << "Dark" << false;  // dark
}

void TestEkosCaptureWorkflow::testWallSource_data()
{
    QTest::addColumn<QString>("frametype");              /*!< frame type (Dark or Flat)           */

    QTest::newRow("Flat") << "Flat";
    QTest::newRow("Dark") << "Dark";
}

void TestEkosCaptureWorkflow::testDarkManualCovering_data()
{
    QTest::addColumn<int>("shutter");                   /*!< does the CCD have a shutter?          */
    QTest::addColumn<bool>("clickModalOK");             /*!< click "OK" on the modal dialog        */
    QTest::addColumn<bool>("clickModal2OK");            /*!< click "OK" on the second modal dialog */

    // all shutter types plus both variants: click OK and click Cancel
    QTest::newRow("shutter=? modal=true") << SHUTTER_UNKNOWN << true << true;
    QTest::newRow("shutter=yes modal=true") << SHUTTER_YES << true << true;
    QTest::newRow("shutter=no modal=true") << SHUTTER_NO << true << true;
    QTest::newRow("modal=true") << SHUTTER_NO << true << false;
    QTest::newRow("modal=false") << SHUTTER_NO << false << true;
}

void TestEkosCaptureWorkflow::testCaptureWaitingForTemperature_data()
{
    QTest::addColumn<double>("initTemp");             /*!< Initial temperature value */
    QTest::addColumn<double>("targetTemp");           /*!< Target temperature value  */

    QTest::newRow("init=0 target=-5") << 0.0 << -5.0;
    QTest::newRow("init=0 target=0")  << 0.0 <<  0.0;
}


/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosCaptureWorkflow::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    // set guiding agressiveness (needs to be set before the guiding module starts)
    Options::setRAProportionalGain(0.25);
    Options::setDECProportionalGain(0.25);

    QStandardPaths::setTestModeEnabled(true);

    QFileInfo test_dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation), "test");
    destination = new QTemporaryDir(test_dir.absolutePath());
    QVERIFY(destination->isValid());
    QVERIFY(destination->autoRemove());
}

void TestEkosCaptureWorkflow::cleanupTestCase()
{
    // nothing to do since we start the INDI service for each test case
}

bool TestEkosCaptureWorkflow::prepareTestCase()
{
    // use the helper to start the profile
    KVERIFY_SUB(m_CaptureHelper->startEkosProfile());
    // prepare optical trains for testing
    m_CaptureHelper->prepareOpticalTrains();
    // prepare the mount module for testing
    m_CaptureHelper->prepareMountModule();
    // prepare for focusing tests
    m_CaptureHelper->prepareFocusModule();
    // prepare for alignment tests
    m_CaptureHelper->prepareAlignmentModule();
    // prepare for guiding tests
    m_CaptureHelper->prepareGuidingModule();

    m_CaptureHelper->init();

    // clear image directory
    KVERIFY_SUB(m_CaptureHelper->getImageLocation()->removeRecursively());

    // set logging defaults for alignment
    Options::setVerboseLogging(false);
    Options::setLogToFile(false);


    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    if (mount->parkStatus() == ISD::PARK_PARKED)
        mount->unpark();
    KTRY_VERIFY_WITH_TIMEOUT_SUB(mount->parkStatus() == ISD::PARK_UNPARKED, 30000);

    // ensure that the dome is unparked
    //    if (m_CaptureHelper->m_DomeDevice != nullptr)
    //    {
    //        Ekos::Dome *dome = Ekos::Manager::Instance()->domeModule();
    //        KVERIFY_SUB(dome != nullptr);
    //        if (dome->parkStatus() == ISD::PARK_PARKED)
    //            dome->unpark();
    //        KTRY_VERIFY_WITH_TIMEOUT_SUB(dome->parkStatus() == ISD::PARK_UNPARKED, 30000);
    //    }

    // close INDI window
    GUIManager::Instance()->close();

    // preparation successful
    return true;
}

void TestEkosCaptureWorkflow::init()
{
    // reset counters
    image_count  = 0;

    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    // clear light panel
    m_CaptureHelper->m_LightPanelDevice = nullptr;
    // clear rotator
    m_CaptureHelper->m_RotatorDevice = nullptr;

}

void TestEkosCaptureWorkflow::cleanup()
{
    if (Ekos::Manager::Instance()->focusModule() != nullptr)
        Ekos::Manager::Instance()->focusModule()->abort();

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    if (capture != nullptr)
    {
        capture->abort();
        capture->clearSequenceQueue();
        KTRY_SET_CHECKBOX(capture, limitRefocusS, false);
    }

    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
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
