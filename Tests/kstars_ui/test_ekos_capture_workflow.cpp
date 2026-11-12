/*
    KStars UI tests for capture workflows (re-focus, dithering, guiding, ...)

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_workflow.h"

#include "kstars_ui_tests.h"
#include "ui_calibrationoptions.h"
#include "indicom.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "ekos/capture/sequencejobstate.h"
#include "ekos/capture/capturetypes.h"
#include "ekos/capture/capture.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "indi/indifilterwheel.h"
#include "indi/indilightbox.h"
#include "fitsviewer/fitsdata.h"

#include <QRegularExpression>
#include <QScopeGuard>

#include <algorithm>

#define SHUTTER_UNKNOWN -1
#define SHUTTER_NO       0
#define SHUTTER_YES      1

namespace
{
template <typename T>
void appendPixels(const QSharedPointer<FITSData> &data, QVector<double> &values)
{
    const T *buffer = reinterpret_cast<const T *>(data->getImageBuffer());
    const uint32_t count = data->width() * data->height();
    for (uint32_t i = 0; i < count; i++)
        values.append(static_cast<double>(buffer[i]));
}

// The values of the first channel, row by row, whatever the data type of the image.
QVector<double> pixelValues(const QSharedPointer<FITSData> &data)
{
    QVector<double> values;
    values.reserve(data->width() * data->height());
    switch (data->dataType())
    {
        case TBYTE:
            appendPixels<uint8_t>(data, values);
            break;
        case TSHORT:
            appendPixels<int16_t>(data, values);
            break;
        case TUSHORT:
            appendPixels<uint16_t>(data, values);
            break;
        case TLONG:
            appendPixels<int32_t>(data, values);
            break;
        case TULONG:
            appendPixels<uint32_t>(data, values);
            break;
        case TFLOAT:
            appendPixels<float>(data, values);
            break;
        case TDOUBLE:
            appendPixels<double>(data, values);
            break;
        default:
            break;
    }
    return values;
}

double medianOf(QVector<double> values)
{
    if (values.isEmpty())
        return 0;
    auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

QSharedPointer<FITSData> loadFITS(const QString &filename)
{
    QSharedPointer<FITSData> data(new FITSData());
    QFuture<bool> result = data->loadFromFile(filename);
    result.waitForFinished();
    if (!result.result())
        data.clear();
    return data;
}

// The newest master dark recorded for the camera, empty if there is none
QString newestMasterDark(const QString &camera)
{
    QList<QVariantMap> darkFrames;
    if (!KStarsData::Instance()->userdb()->GetAllDarkFrames(darkFrames))
        return QString();

    QVariantMap newest;
    for (const auto &oneFrame : darkFrames)
    {
        if (oneFrame["ccd"].toString() == camera && (newest.isEmpty() || oneFrame["id"].toInt() > newest["id"].toInt()))
            newest = oneFrame;
    }
    return newest["filename"].toString();
}
}


TestEkosCaptureWorkflow::TestEkosCaptureWorkflow(QObject *parent) :
    TestEkosCaptureWorkflow::TestEkosCaptureWorkflow("Internal", parent) {}

TestEkosCaptureWorkflow::TestEkosCaptureWorkflow(QString guider, QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper(guider);
    m_CaptureHelper->m_GuiderDevice = "CCD Simulator";
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
}

void TestEkosCaptureWorkflow::testCaptureRefocusDelay()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    QVERIFY(prepareCapture(1));
    QVERIFY(m_CaptureHelper->executeFocusing());

    // start capturing, expect focus after first captured frame
    Ekos::Capture *capture = manager->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_FOCUSING);
    KTRY_CLICK(capture, startB);
    // focusing must have started 60 secs + exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates,
                                     60000 + 10000 + 1000 * capture->mainCamera()->captureExposureN->value());
}

void TestEkosCaptureWorkflow::testCaptureRefocusHFR()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    QVERIFY(prepareCapture(0, 0.1));
    QVERIFY(m_CaptureHelper->executeFocusing());

    // start capturing
    Ekos::Capture *capture = manager->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    KTRY_CLICK(capture, startB);
    // wait for one frame has been captured:   exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates,
                                     10000 +  1000 * capture->mainCamera()->captureExposureN->value());
    // now move the focuser twice to increase the HFR
    KTRY_GADGET(manager->focusModule()->mainFocuser().get(), QPushButton, focusOutB);
    QTRY_VERIFY_WITH_TIMEOUT(focusOutB->isEnabled(), 5000);
    KTRY_CLICK(manager->focusModule(), focusOutB);
    QTRY_VERIFY_WITH_TIMEOUT(focusOutB->isEnabled(), 5000);
    KTRY_CLICK(manager->focusModule(), focusOutB);
    QTRY_VERIFY_WITH_TIMEOUT(focusOutB->isEnabled(), 5000);
    KTRY_CLICK(manager->focusModule()->mainFocuser().get(), focusOutB);
    // check if focusing has started, latest after two more frames
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates,
                                     10000 + 2 * 1000 * capture->mainCamera()->captureExposureN->value());
}

void TestEkosCaptureWorkflow::testCaptureHFRCheckFilterRestoration()
{
    // Regression test: the in-sequence HFR check switches the physical filter wheel
    // from the capture filter (e.g. Red) to the Focus module's lock filter (Luminance)
    // but never restores it — every frame after the check is exposed through Luminance
    // while the FITS header says "Red".
    //
    // ── Test flow ─────────────────────────────────────────────────────────────
    //   1. Enable in-sequence HFR with a small threshold (0.1 %) so the check fires
    //      after the first captured frame.
    //   2. Run autofocus with the Luminance filter (prepareFocusModule already set
    //      focusFilter = "Luminance" and locked every filter to "Luminance").
    //   3. Add a two-frame Red capture sequence (5 s each).
    //   4. Start the capture sequence and wait for CAPTURE_COMPLETE.
    //   5. Assert that the physical filter wheel is on Red, not Luminance.
    //
    // ── Pass/fail criterion ───────────────────────────────────────────────────
    //   WITHOUT fix  → filter stays on Luminance after the HFR check → TEST FAILS
    //   WITH fix     → filter is restored to Red after the HFR check  → TEST PASSES

    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization; prepareFocusModule() sets focusFilter = "Luminance"
    // and locks every filter (including "Red") to "Luminance".
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    Ekos::Capture *capture = manager->captureModule();

    // ── Step 1: enable in-sequence HFR ───────────────────────────────────────
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KTRY_SET_CHECKBOX(capture, enforceAutofocusHFR, true);
    // 0.1 % threshold: any tiny HFR deviation between the Luminance-baseline and
    // the Red-filter frames triggers the in-sequence check.
    KTRY_SET_DOUBLESPINBOX(capture, hFRThresholdPercentage, 0.1);

    // ── Step 2: run autofocus with Luminance filter ───────────────────────────
    QVERIFY(m_CaptureHelper->executeFocusing());

    // ── Step 3: add a capture sequence — Red filter, 5 s × 2 ─────────────────
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    QString imagepath = getImageLocation()->path() + "/test";
    KTRY_CAPTURE_ADD_LIGHT(5.0, 2, 0.0, "Red", "test", imagepath);

    // ── Step 4: run the capture sequence ─────────────────────────────────────
    auto filtermanager = manager->focusModule()->mainFocuser()->filterManager();
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    // Allow up to 2 min: 2 × 5 s exposures + HFR check overhead + focus overhead
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 120000);

    // ── Step 5: assert the filter is Red, not Luminance ──────────────────────
    // If the in-sequence HFR check fired (filter → Luminance) but the fix is
    // missing, the wheel stays on Luminance and this assertion fails.
    const int redFilterPos = filtermanager->getFilterLabels().indexOf("Red") + 1;
    QVERIFY2(redFilterPos > 0, "Red filter not found in the CCD Simulator filter wheel");
    QTRY_COMPARE_WITH_TIMEOUT(filtermanager->getFilterPosition(), redFilterPos, 10000);
}

void TestEkosCaptureWorkflow::testCaptureRefocusWithJobFilter()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization; prepareFocusModule() sets focusFilter = "Luminance"
    // and locks every filter to "Luminance".
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    Ekos::Capture *capture = manager->captureModule();
    auto focuser = manager->focusModule()->mainFocuser();
    auto filtermanager = focuser->filterManager();
    const int lumFilterPos = filtermanager->getFilterLabels().indexOf("Luminance") + 1;
    const int redFilterPos = filtermanager->getFilterLabels().indexOf("Red") + 1;
    QVERIFY2(lumFilterPos > 0 && redFilterPos > 0, "Luminance or Red filter missing in the CCD Simulator filter wheel");

    // Focusing for Red has to use Red itself
    QVERIFY(filtermanager->setFilterLock(redFilterPos - 1, NULL_FILTER));

    // Autofocus with Luminance leaves Luminance in place, like an alignment with the Luminance filter
    QVERIFY(m_CaptureHelper->executeFocusing());
    QTRY_COMPARE_WITH_TIMEOUT(filtermanager->getFilterPosition(), lumFilterPos, 10000);

    // Red capture sequence with an in-sequence autofocus due before its first frame
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    QString imagepath = getImageLocation()->path() + "/test";
    KTRY_CAPTURE_ADD_LIGHT(5.0, 1, 0.0, "Red", "test", imagepath);
    capture->inSequenceAFRequested(true, "");

    // Remember the filter in place when the in-sequence autofocus completes
    int focusFilterPos = -1;
    connect(focuser.get(), &Ekos::Focus::newStatus, this, [&](Ekos::FocusState state)
    {
        if (state == Ekos::FOCUS_COMPLETE && focusFilterPos < 0)
            focusFilterPos = filtermanager->getFilterPosition();
    });
    auto disconnectFocus = qScopeGuard([&]()
    {
        disconnect(focuser.get(), &Ekos::Focus::newStatus, this, nullptr);
    });

    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_COMPLETE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 120000);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

    QCOMPARE(focusFilterPos, redFilterPos);
}


void TestEkosCaptureWorkflow::testCaptureRefocusTemperature()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    Ekos::Capture *capture = manager->captureModule();
    // select temperature threshold
    double deltaT = 2.0;
    QVERIFY(prepareCapture(0, 0, deltaT));
    // set the focuser temperature
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_FocuserDevice, "FOCUS_TEMPERATURE", "TEMPERATURE", 0);
    // select the focuser as temperature source
    KTRY_SET_COMBO(manager->focusModule(), defaultFocusTemperatureSource, m_CaptureHelper->m_FocuserDevice);

    QVERIFY(m_CaptureHelper->executeFocusing());

    // start capturing
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    KTRY_CLICK(capture, startB);
    // wait for one frame has been captured:   exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates,
                                     10000 +  1000 * capture->mainCamera()->captureExposureN->value());
    // now change the temperature on the focuser
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_FocuserDevice, "FOCUS_TEMPERATURE", "TEMPERATURE", -2 * deltaT);

    KTRY_GADGET(manager->focusModule(), QPushButton, focusOutB);
    QTRY_VERIFY_WITH_TIMEOUT(focusOutB->isEnabled(), 5000);
    KTRY_CLICK(manager->focusModule(), focusOutB);
    QTRY_VERIFY_WITH_TIMEOUT(focusOutB->isEnabled(), 5000);
    KTRY_CLICK(manager->focusModule(), focusOutB);
    // check if focusing has started, latest after two more frames
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates,
                                     10000 + 2 * 1000 * capture->mainCamera()->captureExposureN->value());

}


void TestEkosCaptureWorkflow::testCaptureRefocusAfterPause()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    Ekos::Capture *capture = manager->captureModule();
    // select temperature threshold
    double deltaT = 2.0;
    QVERIFY(prepareCapture(0, 0, deltaT));
    // set the focuser temperature
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_FocuserDevice, "FOCUS_TEMPERATURE", "TEMPERATURE", 0);
    // select the focuser as temperature source
    KTRY_SET_COMBO(manager->focusModule(), defaultFocusTemperatureSource, m_CaptureHelper->m_FocuserDevice);

    QVERIFY(m_CaptureHelper->executeFocusing());

    // start capturing
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 15000);

    // pause and suspend the sequence, like it happens for a follower camera while
    // the lead camera is dithering
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PAUSED);
    KTRY_CLICK(capture, pauseB);
    capture->suspend();
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 15000);

    // change the temperature so that re-focusing is due when the sequence continues
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_FocuserDevice, "FOCUS_TEMPERATURE", "TEMPERATURE", -2 * deltaT);

    // resume the sequence, re-focusing is expected to be triggered
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_COMPLETE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 60000);
    // capturing must continue after re-focusing has completed
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates,
                                     30000 + 1000 * capture->mainCamera()->captureExposureN->value());
}

void TestEkosCaptureWorkflow::testCaptureRefocusAbort()
{
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Manager *manager = Ekos::Manager::Instance();
    QVERIFY(prepareCapture(1));
    QVERIFY(m_CaptureHelper->executeFocusing());

    // start capturing, expect focus after first captured frame
    Ekos::Capture *capture = manager->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_PROGRESS);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_FOCUSING);
    KTRY_CLICK(capture, startB);
    // focusing must have started 60 secs + exposure time + 10 secs delay
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates,
                                     60000 + 10000 + 1000 * capture->mainCamera()->captureExposureN->value());
    // the capture module must change to focusing subsequently
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 5000);
    // now abort capturing
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_ABORTED);
    capture->abort();
    // expect that focusing aborts
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedFocusStates, 10000);
}

void TestEkosCaptureWorkflow::testCaptureScriptsExecution()
{
    // default initialization
    QVERIFY(prepareTestCase());

    QFETCH(bool, pausing);
    // static test with three exposures
    int count = 3;
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";

    // create executable scripts
    m_CaptureHelper->createAllCaptureScripts(destination);

    // setup scripts - starts as thread since clicking on capture blocks
    fillCaptureScripts();

    // create capture sequences
    KTRY_CAPTURE_CONFIGURE_LIGHT(2.0, count, 0.0, "Luminance", "test", imagepath);
    KTRY_CAPTURE_CLICK(addToQueueB);
    KTRY_CAPTURE_CONFIGURE_LIGHT(2.0, count, 0.0, "Red", "test", imagepath);
    KTRY_CAPTURE_CLICK(addToQueueB);
    // check if row has been added
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 2, 1000);
    // wait to pause when second frame is running
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    // start capture
    KTRY_CLICK(capture, startB);
    if (pausing)
    {
        // wait for first frame to be completed
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 15000);
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PAUSED);
        QTest::qWait(1500);
        // press pause
        KTRY_CLICK(capture, pauseB);
        // wait until paused
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 15000);
        // prepare next stage
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PAUSED);
        QTest::qWait(3000);
        // press continue
        KTRY_CLICK(capture, startB);
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.size() <= 1, 15000);
        QTest::qWait(500);
        // press pause
        KTRY_CLICK(capture, pauseB);
        // wait until paused
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 15000);
        // press continue
        QTest::qWait(3000);
        KTRY_CLICK(capture, startB);
    }
    // wait until completed
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    // wait that all frames have been taken
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);

    // check the log file if it holds the expected number
    QVERIFY(m_CaptureHelper->checkScriptRuns(count, 2));
    // the scripts are cleared in cleanup(), also if this test fails
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
    KTRY_SET_CHECKBOX(capture, enforceGuideDeviation, true);
    KTRY_SET_DOUBLESPINBOX(capture, guideDeviation, deviation_limit);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";
    // build a LRGB sequence
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Luminance", "test", imagepath);
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Red", "test", imagepath);
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Green", "test", imagepath);
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Blue", "test", imagepath);

    // set a position in the west
    SkyPoint *target = new SkyPoint();
    target->setAz(270.0);
    target->setAlt(KStarsData::Instance()->geo()->lat()->Degrees() / 2.0);
    // translate to equatorial coordinates
    const dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    const dms *lat = KStarsData::Instance()->geo()->lat();
    target->HorizontalToEquatorial(&lst, lat);

    m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true);

    // clear calibration to ensure proper guiding
    KTRY_CLICK(Ekos::Manager::Instance()->guideModule(), clearCalibrationB);

    // start guiding
    m_CaptureHelper->startGuiding(2.0);

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
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 60000);
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
    KTRY_SET_CHECKBOX(capture, enforceGuideDeviation, true);
    KTRY_SET_DOUBLESPINBOX(capture, guideDeviation, deviation_limit);

    // add target to path to emulate the behavior of the scheduler
    QString imagepath = getImageLocation()->path() + "/test";
    // build a simple 5xL sequence
    KTRY_CAPTURE_ADD_LIGHT(45.0, 5, 5.0, "Luminance", "", imagepath);
    // set Dubhe as target and slew there
    SkyObject *target = KStars::Instance()->data()->skyComposite()->findByName("Dubhe");
    m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true);

    // clear calibration to ensure proper guiding
    KTRY_CLICK(Ekos::Manager::Instance()->guideModule(), clearCalibrationB);

    // start guiding
    m_CaptureHelper->startGuiding(2.0);

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
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 15000);
    // abort capturing
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    // check that it has been aborted
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);
    // wait that the guiding deviation is below the limit and
    // verify that capture does not start
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PROGRESS);
    // the simulators add guiding errors per axis, so recovering from the drift may take longer than a minute
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() < deviation_limit, 120000);

    QTest::qWait(20000);
    QVERIFY2(m_CaptureHelper->expectedCaptureStates.size() > 0, "Capture has been restarted although aborted.");
}

void TestEkosCaptureWorkflow::testAbortedSequenceNotRestartedAfterDithering()
{
    // Regression test: CaptureModuleState::setGuideStatus() restarts every camera whose
    // capture is stopped as soon as dithering completes - not only those that it stopped
    // for dithering itself. A sequence that has been aborted while the dithering was
    // still running is therefore resurrected, and keeps exposing after the mount has
    // already been slewed away or parked. The resulting frames are useless.
    //
    // ── Test flow ─────────────────────────────────────────────────────────────
    //   1. Start guiding and a 3 x 5s sequence that dithers after every frame. The
    //      dithering settle time is stretched so that the dithering is still running
    //      when the sequence gets aborted.
    //   2. Wait until dithering starts after the first frame.
    //   3. Abort the sequence, like the Scheduler does when it bumps a running job.
    //   4. Wait for the dithering to complete successfully.
    //   5. Assert that capturing does not resume.
    //
    // ── Pass/fail criterion ───────────────────────────────────────────────────
    //   WITHOUT fix → the dithering result restarts the aborted sequence → TEST FAILS
    //   WITH fix    → the sequence stays aborted                         → TEST PASSES

    // default initialization
    QVERIFY(prepareTestCase());

    // dither after every frame, with a settle time long enough to abort in between.
    // Must be set after prepareTestCase(), the capture helper disables dithering in init().
    Options::setDitherEnabled(true);
    Options::setDitherNoGuiding(false);
    Options::setDitherFrames(1);
    Options::setDitherPixels(3.0);
    Options::setDitherSettle(20);

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // the sequence job only dithers if the per job dithering is enabled
    KTRY_SET_CHECKBOX(capture, enableDitherPerJob, true);

    const QString imagepath = getImageLocation()->path() + "/test";
    KTRY_CAPTURE_ADD_LIGHT(5.0, 3, 0.0, "Luminance", "test", imagepath);

    // ── Step 1: point the mount at the target and start guiding ──────────────
    SkyObject *target = KStars::Instance()->data()->skyComposite()->findByName("Dubhe");
    QVERIFY(target != nullptr);
    QVERIFY(m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true));

    // clear calibration to ensure proper guiding
    KTRY_CLICK(Ekos::Manager::Instance()->guideModule(), clearCalibrationB);
    QVERIFY(m_CaptureHelper->startGuiding(2.0));

    // ── Step 2: capture until dithering starts after the first frame ─────────
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_DITHERING);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

    // ── Step 3: abort while the dithering is still in progress ───────────────
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);

    // from here on, capturing must not start again - watch out for it before
    // the dithering reports its result
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PROGRESS);
    const QString framePath = imagepath + "/test/Light/Luminance";
    const int framesWhenAborted = QDir(framePath).entryList(QDir::Files).count();

    // ── Step 4: wait for the dithering to succeed ────────────────────────────
    m_CaptureHelper->expectedGuidingStates.append(Ekos::GUIDE_DITHERING_SUCCESS);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedGuidingStates, 60000);

    // ── Step 5: the aborted sequence must not be restarted ───────────────────
    QTest::qWait(20000);
    QVERIFY2(m_CaptureHelper->expectedCaptureStates.size() > 0,
             "Capture has been restarted by the dithering result although aborted.");
    QCOMPARE(QDir(framePath).entryList(QDir::Files).count(), framesWhenAborted);
}

void TestEkosCaptureWorkflow::testAbortedSequenceNotRestartedAfterDitheringError()
{
    // Companion to testAbortedSequenceNotRestartedAfterDithering() for the failure
    // branch: CaptureModuleState::setGuideStatus() handles GUIDE_DITHERING_SUCCESS and
    // GUIDE_DITHERING_ERROR alike, so an aborted sequence has to stay aborted for both.
    //
    // The dithering result is reported directly instead of being provoked in the guider.
    // InternalGuider::abortDither() emits GUIDE_DITHERING_ERROR only if
    // ditherFailAbortsAutoGuide is set, and then immediately, without the settle delay
    // that the success path has - there would be no window left to abort the sequence in.
    // Without that option it settles and emits GUIDE_DITHERING_SUCCESS instead, which is
    // what the companion test already covers. Ekos::Manager connects Guide::newStatus to
    // Capture::setGuideStatus(), so calling it directly delivers to the capture module
    // exactly what a failed dithering run would deliver.
    //
    // ── Test flow ─────────────────────────────────────────────────────────────
    //   1. Capture the first frame of a 3 x 5s sequence.
    //   2. Abort the sequence, like the Scheduler does when it bumps a running job.
    //   3. Report a failed dithering run to the capture module.
    //   4. Assert that capturing does not resume.
    //
    // ── Pass/fail criterion ───────────────────────────────────────────────────
    //   WITHOUT fix → the dithering result restarts the aborted sequence → TEST FAILS
    //   WITH fix    → the sequence stays aborted                         → TEST PASSES

    // default initialization
    QVERIFY(prepareTestCase());

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    const QString imagepath = getImageLocation()->path() + "/test";
    KTRY_CAPTURE_ADD_LIGHT(5.0, 3, 0.0, "Luminance", "test", imagepath);

    // ── Step 1: capture the first frame ──────────────────────────────────────
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);

    // ── Step 2: abort the sequence ───────────────────────────────────────────
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);

    // from here on, capturing must not start again
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_PROGRESS);
    const QString framePath = imagepath + "/test/Light/Luminance";
    const int framesWhenAborted = QDir(framePath).entryList(QDir::Files).count();

    // ── Step 3: report a failed dithering run ────────────────────────────────
    capture->setGuideStatus(Ekos::GUIDE_DITHERING_ERROR);

    // ── Step 4: the aborted sequence must not be restarted ───────────────────
    QTest::qWait(20000);
    QVERIFY2(m_CaptureHelper->expectedCaptureStates.size() > 0,
             "Capture has been restarted by the dithering result although aborted.");
    QCOMPARE(QDir(framePath).entryList(QDir::Files).count(), framesWhenAborted);
}

void TestEkosCaptureWorkflow::testInitialGuidingLimitCapture()
{
    // default initialization
    QVERIFY(prepareTestCase());

    const double deviation_limit = 2.0;
    QFETCH(double, exptime);
    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    // set start guide deviation guard to < 2" but disable the other one
    KTRY_SET_CHECKBOX(capture, enforceStartGuiderDrift, true);
    KTRY_SET_DOUBLESPINBOX(capture, startGuideDeviation, deviation_limit);
    // create sequence with 10 sec delay
    QVERIFY(prepareCapture(0, 0, 0, 10));
    // set Dubhe as target and slew there
    SkyObject *target = KStars::Instance()->data()->skyComposite()->findByName("Dubhe");
    m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true);

    // start guiding
    m_CaptureHelper->startGuiding(2.0);

    for (int i = 1; i <= 2; i++)
    {
        // wait initially 5 seconds
        if (i == 1)
            QTest::qWait(5000);

        // prepare to expect that capturing will start
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

        // ensure that guiding is running
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuidingStatus() == Ekos::GUIDE_GUIDING, 10000);

        // create a guide drift
        Ekos::Manager::Instance()->mountModule()->doPulse(RA_INC_DIR, 2000, DEC_INC_DIR, 2000);
        qCInfo(KSTARS_EKOS_TEST()) << "Sent 2000ms RA+DEC guiding pulses.";

        // wait until guide deviation is present
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() > deviation_limit, 15000);

        if (i == 1)
        {
            // start capture but expect it being suspended first
            KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
            KTRY_CLICK(capture, startB);
        }
        // verify that capturing does not start before the guide deviation is below the limit
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() >= deviation_limit, 60000);
        // wait 3 seconds and then ensure that capture did not start
        QTest::qWait(3000);
        QTRY_VERIFY(m_CaptureHelper->expectedCaptureStates.size() > 0);
        // wait until guiding deviation is below the limit
        QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getGuideDeviation() < deviation_limit, 60000);
        // wait until capturing starts
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 30000);
        if (i < 2)
        {
            // in the first iteration wait until the capture completes
            m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
            KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 1500 * exptime);
        }
    }
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
    QFETCH(bool, scripts);

    // capture scripts, cleared again in cleanup()
    if (scripts)
    {
        QVERIFY(m_CaptureHelper->createAllCaptureScripts(destination));
        fillCaptureScripts();
    }

    // initialize the CCD temperature and wait until it is reached
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_CCDDevice, "CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", initTemp);
    QTRY_VERIFY_WITH_TIMEOUT(std::abs(capture->mainCamera()->temperatureOUT->text().toDouble()) <=
                             Options::maxTemperatureDiff(),
                             60000);

    // set target temperature (a real, non-"--" value enforces it - there is no separate checkbox anymore)
    KTRY_SET_DOUBLESPINBOX(capture, cameraTemperatureN, targetTemp);

    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_LIGHT(10.0, 5, 5.0, "Luminance", "test", getImageLocation()->path() + " / test");
    // expect capturing state
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

    // start capturing
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
    // Read the current temperature from the UI
    double currentTemp = m_CaptureHelper->readCurrentTemperature(capture);
    // check if the temperature is at the expected level
    QTRY_VERIFY2(std::abs(currentTemp - targetTemp) <= Options::maxTemperatureDiff(),
                 QString("Temperature %1°C not at the expected level of %2°C").arg(currentTemp).arg(targetTemp).toLocal8Bit());

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
    // Read the current temperature from the UI
    currentTemp = m_CaptureHelper->readCurrentTemperature(capture);
    // check if the temperature is at the expected level
    QTRY_VERIFY2(std::abs(currentTemp - targetTemp) <= Options::maxTemperatureDiff(),
                 QString("Temperature %1°C not at the expected level of %2C").arg(currentTemp).arg(targetTemp).toLocal8Bit());

    // stop capturing
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(capture, startB);
    // check if capturing has stopped
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 10000);

    // change temperature back to initial value
    SET_INDI_VALUE_DOUBLE(m_CaptureHelper->m_CCDDevice, "CCD_TEMPERATURE", "CCD_TEMPERATURE_VALUE", initTemp);
    QTRY_VERIFY_WITH_TIMEOUT(std::abs(m_CaptureHelper->readCurrentTemperature(capture) - initTemp) <=
                             Options::maxTemperatureDiff(),
                             60000);

    // start capturing for a second time
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
    // Read the current temperature from the UI
    currentTemp = m_CaptureHelper->readCurrentTemperature(capture);
    // check if the temperature is at the expected level
    QTRY_VERIFY2(std::abs(currentTemp - targetTemp) <= Options::maxTemperatureDiff(),
                 QString("Temperature %1°C not at the expected level of %2°C").arg(currentTemp).arg(targetTemp).toLocal8Bit());
}

void TestEkosCaptureWorkflow::testCaptureWaitingForRotator()
{
    // use the rotator simulator
    m_CaptureHelper->m_RotatorDevice = "Rotator Simulator";

    // default initialization
    QVERIFY(prepareTestCase());
    QSKIP("Skipping test after UI rework");

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // open the rotator dialog
    KTRY_CLICK(capture, rotatorB);

    // ensure that the rotator dialog appears
    QWidget *rotatorDialog = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((rotatorDialog = Ekos::Manager::Instance()->findChild<QWidget *>("RotatorDialog")) != nullptr,
                             10000);

    // target angle rotation
    double targetAngle = 90.0;
    // set the target rotation angle
    KTRY_SET_DOUBLESPINBOX(rotatorDialog, CameraPA, targetAngle);
    // save it to the sequence job
    KTRY_SET_CHECKBOX(rotatorDialog, enforceJobPA, true);

    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_LIGHT(30.0, 1, 5.0, "Luminance", "test", getImageLocation()->path() + "/test");
    // expect capturing state
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

    // set the rotation angle back
    CameraPA->setValue(0);
    // let the rotator rotate back for at least 3sec
    QTest::qWait(3000);

    // start capturing
    KTRY_CLICK(capture, startB);
    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

    // KTRY_GADGET(rotatorDialog, QLineEdit, rawAngleOut);
    // QTRY_VERIFY2(std::abs(rawAngleOut->text().toDouble() - targetAngle) * 60 <= Options::astrometryRotatorThreshold(),
    //              QString("Rotator angle %1° not at the expected value of %2°").arg(rawAngleOut->text()).arg(targetAngle).toLocal8Bit());
    QWARN("Since the rotator interface has changed, the correct rotator angle cannot be checked from the test case.");
}

void TestEkosCaptureWorkflow::testFlatManualSource()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_MANUAL(capture);
    // ensure that the filter wheel is configured
    KTRY_CAPTURE_GADGET(QComboBox, FilterPosCombo);
    // wait until filter combo box is filled
    QTRY_VERIFY_WITH_TIMEOUT(FilterPosCombo->count() > 0, 60000);
    // build a simple 1xL flat sequence
    KTRY_CAPTURE_ADD_FRAME("Flat", 1, 1, 0.0, "Luminance", "test", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", "test", imagepath);
    // click OK or Cancel?
    QFETCH(bool, clickModalOK);
    QFETCH(bool, clickModal2OK);
    if (clickModalOK)
    {
        // Expect a capture sequence
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
        KTRY_CLICK(capture, startB);
        // click OK in the modal dialog for covering the telescope
        CLOSE_MODAL_DIALOG(0);
        // check if one single frame is captured
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


void TestEkosCaptureWorkflow::testLightPanelSource()
{
    // use the light panel simulator
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    // ensure that we know that the CCD has a shutter
    m_CaptureHelper->ensureCCDShutter(true);
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to the selected type so that we can set the calibration
    QFETCH(QString, frametype);
    KTRY_SET_COMBO(capture, captureTypeS, frametype);

    // select internal or external flat light
    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_FRAME(frametype, 1, 1, 0.0, "Luminance", "test", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", "test", imagepath);

    // start the sequence
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK(capture, startB);
    // check if one single flat is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
}


namespace
{
/**
 * @brief Records the light panel state for each frame received by Capture, and the light states
 * reported by the device. A light command sets the light box to busy until the driver confirms it,
 * so lightStatus() is LIGHT_ON only if the light is on and no command to change it is pending.
 */
class LightPanelRecorder
{
    public:
        LightPanelRecorder(ISD::LightBox *lightBox, Ekos::Capture *capture) : m_LightBox(lightBox)
        {
            m_Clock.start();
            QObject::connect(lightBox, &ISD::LightBox::newLightStatus, &m_Context,
                             [this](ISD::LightBox::LightStatus status)
            {
                m_Events << QString("%1ms light=%2").arg(m_Clock.elapsed()).arg(ISD::LightBox::getStatusString(status, false));
                if (status == ISD::LightBox::LIGHT_OFF)
                    lightOffEvents++;
            });
            QObject::connect(capture, &Ekos::Capture::newStatus, &m_Context,
                             [this](Ekos::CaptureState status, const QString &, int)
            {
                m_Events << QString("%1ms capture=%2 (light=%3)").arg(m_Clock.elapsed())
                         .arg(Ekos::getCaptureStatusString(status, false))
                         .arg(ISD::LightBox::getStatusString(m_LightBox->lightStatus(), false));
                captureState = status;
                if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
                    frames << m_LightBox->lightStatus();
            });
        }

        ~LightPanelRecorder()
        {
            qInfo() << "Light panel event log:";
            for (const auto &event : m_Events)
                qInfo().noquote() << "   " << event;
        }

        /** @brief frames from position from on received with a light state other than the expected one */
        QString unexpectedFrames(ISD::LightBox::LightStatus expected, int from = 0, int to = -1) const
        {
            QStringList result;
            for (int i = from; i < (to < 0 ? frames.count() : to); i++)
                if (frames[i] != expected)
                    result << QString("#%1 (light=%2)").arg(i + 1).arg(ISD::LightBox::getStatusString(frames[i], false));
            return result.join(", ");
        }

        QList<ISD::LightBox::LightStatus> frames;
        Ekos::CaptureState captureState { Ekos::CAPTURE_IDLE };
        int lightOffEvents { 0 };

    private:
        ISD::LightBox *m_LightBox { nullptr };
        QElapsedTimer m_Clock;
        QStringList m_Events;
        QObject m_Context;
};
}

// verify that the light is on and that no command to change it has been sent: sending a command
// changes the local FLAT_LIGHT_CONTROL switch right away
#define KVERIFY_LIGHT_ON_UNCHANGED(lightBox) do { \
        QCOMPARE(lightBox->lightStatus(), ISD::LightBox::LIGHT_ON); \
        auto lightSP = lightBox->getSwitch("FLAT_LIGHT_CONTROL"); \
        QVERIFY(lightSP && lightSP.findWidgetByName("FLAT_LIGHT_ON")); \
        QCOMPARE(lightSP.findWidgetByName("FLAT_LIGHT_ON")->getState(), ISS_ON); \
    } while (false)

#define KTRY_GET_LIGHTBOX(lightBox) \
    ISD::LightBox *lightBox = Ekos::OpticalTrainManager::Instance()->getLightBox(m_CaptureHelper->m_primaryTrain); \
    QVERIFY(lightBox != nullptr); \
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_OFF, 5000)

void TestEkosCaptureWorkflow::testLightPanelConsecutiveFlats()
{
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    m_CaptureHelper->ensureCCDShutter(true);
    QVERIFY(prepareTestCase());

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KTRY_GET_LIGHTBOX(lightBox);

    // two flats followed by two frames of the given type
    QString imagepath = getImageLocation()->path() + "/test";
    QFETCH(QString, secondType);
    QFETCH(QString, secondFilter);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_MANUAL(capture);
    KTRY_CAPTURE_ADD_FRAME("Flat", 1, 2, 0.0, "Luminance", "test", imagepath);
    KTRY_CAPTURE_ADD_FRAME(secondType, 1, 2, 0.0, secondFilter, "test", imagepath);

    LightPanelRecorder recorder(lightBox, capture);
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY_WITH_TIMEOUT(recorder.captureState == Ekos::CAPTURE_COMPLETE, 90000);
    QCOMPARE(recorder.frames.count(), 4);

    // all flats are taken with the light on, other frames with the light off
    QVERIFY2(recorder.unexpectedFrames(ISD::LightBox::LIGHT_ON, 0, 2).isEmpty(),
             qPrintable(recorder.unexpectedFrames(ISD::LightBox::LIGHT_ON, 0, 2)));
    const auto expected = secondType == "Flat" ? ISD::LightBox::LIGHT_ON : ISD::LightBox::LIGHT_OFF;
    QVERIFY2(recorder.unexpectedFrames(expected, 2).isEmpty(), qPrintable(recorder.unexpectedFrames(expected, 2)));
    // the light is not switched off between consecutive flat jobs
    if (secondType == "Flat")
        QCOMPARE(recorder.lightOffEvents, 0);
    // the light is off once the sequence is complete
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_OFF, 10000);
}

void TestEkosCaptureWorkflow::testLightPanelConsecutiveFlats_data()
{
    QTest::addColumn<QString>("secondType");
    QTest::addColumn<QString>("secondFilter");
    QTest::newRow("flats, same filter") << "Flat" << "Luminance";
    QTest::newRow("flats, filter change") << "Flat" << "Red";
    QTest::newRow("flats, then lights") << "Light" << "Red";
}

void TestEkosCaptureWorkflow::testLightPanelStopFlats()
{
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    m_CaptureHelper->ensureCCDShutter(true);
    QVERIFY(prepareTestCase());

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KTRY_GET_LIGHTBOX(lightBox);
    auto camera = capture->mainCamera();
    QVERIFY(camera != nullptr);

    QString imagepath = getImageLocation()->path() + "/test";
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_MANUAL(capture);
    KTRY_CAPTURE_ADD_FRAME("Flat", 1, 4, 0.0, "Luminance", "test", imagepath);

    LightPanelRecorder recorder(lightBox, capture);
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY_WITH_TIMEOUT(recorder.frames.count() >= 1, 30000);
    QCOMPARE(lightBox->lightStatus(), ISD::LightBox::LIGHT_ON);

    QFETCH(QString, action);
    if (action == "abort")
        camera->abort();
    else if (action == "stop")
        camera->stop();
    else if (action == "suspend")
    {
        camera->suspend();
        KVERIFY_LIGHT_ON_UNCHANGED(lightBox);
        // aborting the suspended capture ends capturing
        camera->abort();
    }
    else if (action == "pause")
    {
        // pause during an exposure
        QTRY_VERIFY_WITH_TIMEOUT(recorder.captureState == Ekos::CAPTURE_CAPTURING, 10000);
        camera->pause();
        QTRY_VERIFY_WITH_TIMEOUT(recorder.captureState == Ekos::CAPTURE_PAUSED, 30000);
        // the light stays on while paused
        KVERIFY_LIGHT_ON_UNCHANGED(lightBox);
        QCOMPARE(recorder.lightOffEvents, 0);
        // resume and complete the sequence
        camera->toggleSequence();
        QTRY_VERIFY_WITH_TIMEOUT(recorder.captureState == Ekos::CAPTURE_COMPLETE, 60000);
        // resuming reports image received once more
        QVERIFY(recorder.frames.count() >= 4);
        QVERIFY2(recorder.unexpectedFrames(ISD::LightBox::LIGHT_ON).isEmpty(),
                 qPrintable(recorder.unexpectedFrames(ISD::LightBox::LIGHT_ON)));
    }
    else
        QFAIL(qPrintable("Unknown action " + action));

    // in all cases, the light is off once capturing ends
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_OFF, 10000);
}

void TestEkosCaptureWorkflow::testLightPanelStopFlats_data()
{
    QTest::addColumn<QString>("action");
    for (auto action :
            {"abort", "stop", "suspend", "pause"
            })
        QTest::newRow(action) << action;
}

void TestEkosCaptureWorkflow::testLightPanelRestartFlatsAfterAbort()
{
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    m_CaptureHelper->ensureCCDShutter(true);
    QVERIFY(prepareTestCase());

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KTRY_GET_LIGHTBOX(lightBox);
    auto camera = capture->mainCamera();
    QVERIFY(camera != nullptr);

    QString imagepath = getImageLocation()->path() + "/test";
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_MANUAL(capture);
    KTRY_CAPTURE_ADD_FRAME("Flat", 1, 3, 0.0, "Luminance", "test", imagepath);

    LightPanelRecorder recorder(lightBox, capture);
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY_WITH_TIMEOUT(recorder.frames.count() >= 1, 30000);
    QCOMPARE(lightBox->lightStatus(), ISD::LightBox::LIGHT_ON);

    // abort, turning the light off, and restart before the device confirmed it
    camera->abort();
    const int restartFrame = recorder.frames.count();
    camera->start();
    QTRY_VERIFY_WITH_TIMEOUT(recorder.captureState == Ekos::CAPTURE_COMPLETE, 60000);
    QVERIFY(recorder.frames.count() > restartFrame);
    // all flats after restarting are taken with the light on
    QVERIFY2(recorder.unexpectedFrames(ISD::LightBox::LIGHT_ON, restartFrame).isEmpty(),
             qPrintable(recorder.unexpectedFrames(ISD::LightBox::LIGHT_ON, restartFrame)));
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_OFF, 10000);
}

void TestEkosCaptureWorkflow::testLightPanelManualLightUntouched()
{
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
    m_CaptureHelper->ensureCCDShutter(true);
    QVERIFY(prepareTestCase());

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);
    KTRY_GET_LIGHTBOX(lightBox);
    auto camera = capture->mainCamera();
    QVERIFY(camera != nullptr);

    // turn the light on outside of Capture
    QVERIFY(lightBox->setLightEnabled(true));
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_ON, 5000);

    // stopping capture does not touch a light that Capture did not turn on
    capture->abort();
    KVERIFY_LIGHT_ON_UNCHANGED(lightBox);
    camera->stop();
    KVERIFY_LIGHT_ON_UNCHANGED(lightBox);
    camera->suspend();
    KVERIFY_LIGHT_ON_UNCHANGED(lightBox);
}

void TestEkosCaptureWorkflow::testDustcapSource()
{
    // use the Flip Flat in simulator mode both as light source and as dust cap
    m_CaptureHelper->m_DustCapDevice    = "Flip Flat";
    m_CaptureHelper->m_LightPanelDevice = "Flip Flat";
    // darks and biases would otherwise ask whether the camera has a shutter, and wait for an answer
    m_CaptureHelper->ensureCCDShutter(true);

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
    SET_INDI_VALUE_SWITCH("Flip Flat", "FLAT_LIGHT_CONTROL", "FLAT_LIGHT_OFF", true);

    // Now all devices should be up and running
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->indiStatus() == Ekos::Success, 10000);

    // init the helper
    m_CaptureHelper->init();

    // prepare optical trains for testing
    m_CaptureHelper->prepareOpticalTrains();

    // wait for all modules to be configured
    m_CaptureHelper->checkModuleConfigurationsCompleted();

    // receive status updates from all devices
    m_CaptureHelper->connectModules();

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // clear calibration settings
    KTRY_SET_CHECKBOX(capture, captureCalibrationWall, false);
    KTRY_SET_CHECKBOX(capture, captureCalibrationParkMount, false);
    KTRY_SET_CHECKBOX(capture, captureCalibrationParkDome, false);

    // use a test directory for flats
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    QTRY_VERIFY_WITH_TIMEOUT(captureTypeS->findText("Flat", Qt::MatchExactly) >= 0, 5000);
    KTRY_SELECT_FLAT_MANUAL(capture);

    // select frame type and internal or external flat light
    QFETCH(QString, frametype);

    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_FRAME(frametype, 1, 1, 0.0, "Luminance", "test", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", "test", imagepath);

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
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");

    // select the wall as flat light source (az=90°, alt=0)
    KTRY_SELECT_FLAT_WALL(capture, "90", "0");

    // determine frame type
    QFETCH(QString, frametype);
    // build a simple 1xL sequence
    KTRY_CAPTURE_ADD_FRAME(frametype, 2, 1, 2.0, "Luminance", "test", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", "test", imagepath);
    // switch capture type to flat so that we can set the calibration
    captureTypeS->setCurrentText("Flat");
    // add another sequence to check if wall source may be used twice
    // select another wall position as flat light source (az=80°, alt=0)
    KTRY_SELECT_FLAT_WALL(capture, "80", "0");
    KTRY_CAPTURE_ADD_FRAME(frametype, 2, 2, 2.0, "Luminance", "test", imagepath);

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
    // check if one single flat and one red frame is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

    // reset the sequence
    KTRY_CLICK(capture, resetB);
    // restart the sequence
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IDLE);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_SLEWING);
    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_IDLE);
    KTRY_CLICK(capture, startB);
    // check if mount has reached the expected position
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMountStates, 60000);
    // check if one single flat and one red frame is captured
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);

}


namespace
{
/**
 * @brief Records the log of the Capture module.
 */
class CaptureLogRecorder
{
    public:
        explicit CaptureLogRecorder(Ekos::Capture *capture)
        {
            QObject::connect(capture, &Ekos::Capture::newLog, &m_Context, [this](const QString & text)
            {
                lines << text;
            });
        }

        /** @brief The number of log lines that contain @p text. */
        int count(const QString &text) const
        {
            return lines.filter(text).size();
        }

        /** @brief The complete log, for failure messages. */
        const char *message()
        {
            m_Message = lines.join(QLatin1Char('\n')).toLocal8Bit();
            return m_Message.constData();
        }

        QStringList lines;

    private:
        QObject m_Context;
        QByteArray m_Message;
};
} // namespace

// Flat ADU tests with the CCD and light panel simulators. The flats of the CCD simulator only
// depend on the exposure, not on the light panel brightness, so the brightness search can never
// reach the target there: these tests run into the fallback to the exposure on purpose. A short
// exposure that stays dark at every brightness looks like a light box without light.
#define KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox) \
    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator"; \
    m_CaptureHelper->ensureCCDShutter(true); \
    QVERIFY(prepareTestCase(true)); \
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule(); \
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000); \
    KTRY_GET_LIGHTBOX(lightBox); \
    QVERIFY(lightBox->isBrightnessControllable())

// Turn the light on outside of Capture and set its brightness, as a user would before taking flats.
#define KTRY_LIGHTBOX_ON_AT(lightBox, value) do { \
    lightBox->setLightEnabled(true); \
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_ON, 5000); \
    QVERIFY(lightBox->setBrightness(value)); \
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(lightBox->brightness()), value, 5000); } while (false)

void TestEkosCaptureWorkflow::testFlatADUPreview_data()
{
    QTest::addColumn<int>("target");

    QTest::newRow("manual target") << 20000;
    QTest::newRow("Auto") << 0;
}

void TestEkosCaptureWorkflow::testFlatADUPreview()
{
    QFETCH(int, target);

    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, 100);

    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SET_COMBO(capture, FilterPosCombo, "Luminance");
    KTRY_SET_DOUBLESPINBOX(capture, captureExposureN, 1.0);
    KTRY_SELECT_FLAT_ADU(capture, target, 1000, false);

    // Count the frames and any other brightness the light box reports. The preview status is
    // recorded here, since the status of the capture helper may still be that of an earlier row.
    int frames = 0;
    bool completed = false;
    QList<int> otherBrightness;
    QObject context;
    connect(capture, &Ekos::Capture::newStatus, &context, [&frames, &completed](Ekos::CaptureState status)
    {
        if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
            frames++;
        else if (status == Ekos::CAPTURE_COMPLETE)
            completed = true;
    });
    connect(lightBox, &ISD::LightBox::newBrightnessStatus, &context, [&otherBrightness](uint16_t value)
    {
        if (value != 100)
            otherBrightness << value;
    });

    // The simulated flat of one second is saturated, so a calibration would need more frames. A preview
    // takes a single frame as it is instead, whatever the flat duration and target ADU are.
    KTRY_CLICK(capture, previewB);
    QTRY_VERIFY2_WITH_TIMEOUT(completed, log.message(), 120000);

    QCOMPARE(frames, 1);
    QVERIFY2(log.count("Flat calibration") == 0, log.message());
    QVERIFY2(log.count("Current ADU") == 0, log.message());
    QVERIFY2(log.count("Current image is saturated") == 0, log.message());
    QCOMPARE(captureExposureN->value(), 1.0);
    QVERIFY2(otherBrightness.isEmpty(), qPrintable(QString("light box brightness changed to %1")
             .arg(otherBrightness.isEmpty() ? 0 : otherBrightness.first())));
    QCOMPARE(static_cast<int>(lightBox->brightness()), 100);
}

void TestEkosCaptureWorkflow::testFlatADUNoLightStops()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, 100);

    // A short exposure that stays far below the target at any brightness. The simulated flats
    // do not depend on the brightness, which is what a light box without light looks like.
    // The search starts above a quarter of the maximum, so it has no frame to compare with
    // when it reaches the maximum, and takes one at its minimum.
    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20000, 1000, false);
    KTRY_CAPTURE_ADD_FRAME("Flat", 0.02, 1, 0.0, "Luminance", "test", imagepath);

    // Wait for the job to end either way: a job that falls back to the exposure completes, and
    // waiting for the abort alone would run into the test function timeout.
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED
                              || m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE, log.message(), 90000);
    QVERIFY2(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED, log.message());

    QVERIFY2(log.count("Flat calibration adjusts the light box brightness") == 1, log.message());
    QVERIFY2(log.count("Checking at brightness 1 that the light box gives light") == 1, log.message());
    // the log shows what the frame at brightness 1 was compared with
    const QRegularExpression checkFrame("Current ADU is \\d+ at light box brightness 1, and \\d+ at the maximum "
                                        "brightness \\(255\\)");
    QVERIFY2(log.lines.filter(checkFrame).size() == 1, log.message());
    QVERIFY2(log.count("No signal measured at maximum light box brightness (255)") == 1, log.message());
    // the exposure is not lengthened in vain
    QVERIFY2(log.count("adjusting the exposure instead") == 0, log.message());
    // the job ends with the light box back at the brightness it started with
    QVERIFY2(log.count("Light box brightness restored to 100") == 1, log.message());
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(lightBox->brightness()), 100, 5000);
}

void TestEkosCaptureWorkflow::testFlatADURetakeAfterPresetChange()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);

    // The filter wheel stands on Red, the light box at 5, and Luminance has a preset of 125.
    // The light box applies that preset when the filter changes, which Capture does right
    // before the exposure of the first frame.
    ISD::FilterWheel *filterWheel = Ekos::OpticalTrainManager::Instance()->getFilterWheel(m_CaptureHelper->m_primaryTrain);
    QVERIFY(filterWheel != nullptr);
    QVERIFY(filterWheel->setPosition(1));
    QTRY_VERIFY_WITH_TIMEOUT(filterWheel->getNumber("FILTER_SLOT") &&
                             filterWheel->getNumber("FILTER_SLOT")[0].getValue() == 1, 10000);
    KTRY_LIGHTBOX_ON_AT(lightBox, 5);

    // The light box offers presets per filter once it follows the filter wheel of the train.
    auto activeDevices = lightBox->getText("ACTIVE_DEVICES");
    QVERIFY(activeDevices);
    auto activeFilter = activeDevices.findWidgetByName("ACTIVE_FILTER");
    QVERIFY(activeFilter != nullptr);
    qInfo() << "Light box follows filter wheel" << activeFilter->getText() << ", train filter wheel"
            << filterWheel->getDeviceName();
    if (QString(activeFilter->getText()) != filterWheel->getDeviceName())
    {
        activeFilter->setText(filterWheel->getDeviceName().toLatin1().constData());
        lightBox->sendNewProperty(activeDevices);
    }

    QTRY_VERIFY_WITH_TIMEOUT(lightBox->getNumber("FLAT_LIGHT_FILTER_INTENSITY") &&
                             lightBox->getNumber("FLAT_LIGHT_FILTER_INTENSITY").count() > 0, 10000);
    auto presets = lightBox->getNumber("FLAT_LIGHT_FILTER_INTENSITY");
    bool presetSet = false;
    for (auto &preset : presets)
    {
        if (QString(preset.getName()) == "Luminance" || QString(preset.getLabel()) == "Luminance")
        {
            preset.setValue(125);
            presetSet = true;
        }
    }
    QVERIFY2(presetSet, "no brightness preset for Luminance");
    lightBox->sendNewProperty(presets);
    // The simulator keeps its presets in its configuration, so remove them again when this
    // test ends, also when it fails: the following test cases expect a light box without them.
    auto removePresets = qScopeGuard([lightBox]()
    {
        auto presets = lightBox->getNumber("FLAT_LIGHT_FILTER_INTENSITY");
        if (!presets)
            return;
        for (auto &preset : presets)
            preset.setValue(0);
        lightBox->sendNewProperty(presets);
        const bool removed = QTest::qWaitFor([lightBox]()
        {
            for (const auto &preset : lightBox->getNumber("FLAT_LIGHT_FILTER_INTENSITY"))
                if (preset.getValue() != 0)
                    return false;
            return true;
        }, 5000);
        if (!removed)
            qCWarning(KSTARS_EKOS_TEST) << "The light box did not confirm that its presets were removed.";
    });
    auto luminancePreset = [lightBox]()
    {
        for (const auto &preset : lightBox->getNumber("FLAT_LIGHT_FILTER_INTENSITY"))
            if (QString(preset.getName()) == "Luminance" || QString(preset.getLabel()) == "Luminance")
                return static_cast<int>(preset.getValue());
        return -1;
    };
    QTRY_COMPARE_WITH_TIMEOUT(luminancePreset(), 125, 5000);
    // setting a preset does not change the brightness while another filter is in place
    QCOMPARE(static_cast<int>(lightBox->brightness()), 5);

    // A short flat that the simulator brings within the tolerance at any brightness, so that the
    // first frame taken at the preset completes the calibration.
    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20000, 1000, false);
    KTRY_CAPTURE_ADD_FRAME("Flat", 0.075, 1, 0.0, "Luminance", "test", imagepath);

    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE, log.message(), 120000);

    // the frame taken while the preset was applied is taken again, and the search starts at the preset
    QVERIFY2(log.count("The light box brightness changed from 5 to 125 during the frame. Taking it again.") == 1,
             log.message());
    QVERIFY2(log.count("Light box brightness calibration starts at brightness 125") == 1, log.message());
    QVERIFY2(log.count("within target ADU tolerance range") == 1, log.message());
    // the job ends with the light box back at the brightness it had before the job
    QVERIFY2(log.count("Light box brightness restored to 5") == 1, log.message());
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(lightBox->brightness()), 5, 5000);
}

void TestEkosCaptureWorkflow::testFlatADUAutoTarget()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, 100);

    // target 0 is Auto, which brings its own tolerance
    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 0, 1000, false);
    QDialog *calibrationOptions = Ekos::Manager::Instance()->findChild<QDialog*>("Calibration");
    QVERIFY(calibrationOptions != nullptr);
    KTRY_GADGET(calibrationOptions, QSpinBox, captureCalibrationADUValue);
    QCOMPARE(captureCalibrationADUValue->text(), QString("Auto"));
    KTRY_GADGET(calibrationOptions, QSpinBox, captureCalibrationADUTolerance);
    QVERIFY(captureCalibrationADUTolerance->isEnabled() == false);

    // The simulated flats ignore the brightness, so the long exposure is too bright at any
    // brightness and falls back to the exposure, which reaches the automatic target of the
    // 16-bit simulator. A short exposure would look like a light box without light.
    KTRY_CAPTURE_ADD_FRAME("Flat", 1.0, 1, 0.0, "Luminance", "test", imagepath);
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE, log.message(), 120000);

    QVERIFY2(log.count("Target ADU: Auto.") == 1, log.message());
    QVERIFY2(log.count("Automatic target ADU for 16-bit images: 26214 ± 1311.") == 1, log.message());
    QVERIFY2(log.count("within target ADU tolerance range") == 1, log.message());

    // a target set by hand enables the tolerance again
    captureCalibrationADUValue->setValue(20000);
    QVERIFY(captureCalibrationADUTolerance->isEnabled());
}

void TestEkosCaptureWorkflow::testFlatADUStopsAtExposureLimit()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, 100);

    // A target that even the shortest exposure of the simulator overshoots: the brightness
    // falls back to the exposure, which stops at the camera limit instead of looping.
    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20, 5, false);
    KTRY_CAPTURE_ADD_FRAME("Flat", 0.01, 1, 0.0, "Luminance", "test", imagepath);

    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED, log.message(), 120000);

    QVERIFY2(log.count("The image is still too bright at the shortest exposure") == 1, log.message());
    // the light box returns to the brightness it started with
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(lightBox->brightness()), 100, 5000);
}

void TestEkosCaptureWorkflow::testFlatADUFallbackTooBright()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, 100);

    // An exposure that saturates at any brightness: the search goes down to 1, never to 0, which
    // switches some light boxes off, and falls back to the exposure there.
    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20000, 1000, false);
    KTRY_CAPTURE_ADD_FRAME("Flat", 1.0, 1, 0.0, "Luminance", "test", imagepath);

    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE, log.message(), 180000);

    QVERIFY2(log.count("Light box brightness calibration starts at brightness 100") == 1, log.message());
    QVERIFY2(log.count("brightness: minimum (1)") == 1, log.message());
    QVERIFY2(log.count("Continuing with light box brightness 1 and adjusting the exposure instead") == 1,
             log.message());
    // the saturated frame reduces the exposure right away, without another saturated frame
    QVERIFY2(log.count("Current image is saturated") == 0, log.message());
    QVERIFY2(log.count("Flats are taken at an exposure of") == 1, log.message());
    // the job ends with the light box back at the brightness it started with
    QVERIFY2(log.count("Light box brightness restored to 100") == 1, log.message());
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(lightBox->brightness()), 100, 5000);
}

void TestEkosCaptureWorkflow::testFlatADUAbortRestoresBrightness_data()
{
    QTest::addColumn<int>("start");
    QTest::addColumn<double>("exposure");

    QTest::newRow("restored") << 100 << 1.0;
    // 0 switches some light boxes off, so the brightness the job ended at is kept. The
    // simulated frames do not depend on the brightness, and the short exposure keeps them
    // below the target, so that the search moves away from 0 instead of falling back there.
    QTest::newRow("not restored to 0") << 0 << 0.02;
}

void TestEkosCaptureWorkflow::testFlatADUAbortRestoresBrightness()
{
    QFETCH(int, start);
    QFETCH(double, exposure);

    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, start);
    auto camera = capture->mainCamera();
    QVERIFY(camera != nullptr);

    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20000, 1000, false);
    KTRY_CAPTURE_ADD_FRAME("Flat", exposure, 1, 0.0, "Luminance", "test", imagepath);

    // abort as soon as the search has changed the brightness
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(log.count("Next light box brightness is") > 0, log.message(), 60000);
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->brightness() != start, 10000);
    camera->abort();

    if (start > 0)
    {
        QTRY_VERIFY2_WITH_TIMEOUT(log.count(QString("Light box brightness restored to %1").arg(start)) == 1, log.message(),
                                  10000);
        QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(lightBox->brightness()), start, 5000);
    }
    else
    {
        QTRY_VERIFY2_WITH_TIMEOUT(log.count("Light box brightness left at") == 1, log.message(), 10000);
        QVERIFY2(log.count("Light box brightness restored") == 0, log.message());
        QVERIFY(lightBox->brightness() > 0);
    }
}

void TestEkosCaptureWorkflow::testFlatADUNativeEncodingStops()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);
    KTRY_LIGHTBOX_ON_AT(lightBox, 100);

    // the transfer format is kept in the settings, so it is set back for the following test cases
    KTRY_GADGET(capture, QComboBox, captureEncodingS);
    QVERIFY2(captureEncodingS->findText("Native") >= 0, "the CCD simulator does not offer the Native transfer format");
    auto restoreEncoding = qScopeGuard([captureEncodingS]()
    {
        captureEncodingS->setCurrentText("FITS");
    });
    captureEncodingS->setCurrentText("Native");
    QTRY_COMPARE_WITH_TIMEOUT(captureEncodingS->currentText(), QString("Native"), 1000);

    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    int frames = 0;
    QObject context;
    connect(capture, &Ekos::Capture::newStatus, &context, [&frames](Ekos::CaptureState status)
    {
        if (status == Ekos::CAPTURE_IMAGE_RECEIVED)
            frames++;
    });
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20000, 1000, false);
    KTRY_CAPTURE_ADD_FRAME("Flat", 1.0, 1, 0.0, "Luminance", "test", imagepath);

    // the job stops before its first frame and says how to go on
    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(log.count("Cannot calculate the ADU of flat frames in Native images") == 1, log.message(),
                              60000);
    QVERIFY2(log.count("Select Manual flat duration in the Calibration options, "
                       "or set the image transfer format to FITS or XISF") == 1, log.message());
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED, 10000);
    QCOMPARE(frames, 0);
    QVERIFY2(log.count("Flat calibration") == 0, log.message());
    QCOMPARE(static_cast<int>(lightBox->brightness()), 100);
}

void TestEkosCaptureWorkflow::testFlatADUWallUsesLightBox()
{
    KTRY_PREPARE_FLAT_ADU_TEST(capture, lightBox);

    QString imagepath = getImageLocation()->path() + "/test";
    CaptureLogRecorder log(capture);
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    KTRY_SELECT_FLAT_ADU(capture, 20000, 1000, true);
    KTRY_CAPTURE_ADD_FRAME("Flat", 1.0, 1, 0.0, "Luminance", "test", imagepath);

    KTRY_CLICK(capture, startB);
    QTRY_VERIFY2_WITH_TIMEOUT(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE, log.message(), 180000);

    // at the wall position Capture turns the light box on, which makes it the flat source
    QVERIFY2(log.count("Slew to wall position complete") > 0, log.message());
    QVERIFY2(log.count("Turn light box light on") == 1, log.message());
    QVERIFY2(log.count("Flat calibration adjusts the light box brightness") == 1, log.message());
    QVERIFY2(log.count("within target ADU tolerance range") == 1, log.message());
    QTRY_VERIFY_WITH_TIMEOUT(lightBox->lightStatus() == ISD::LightBox::LIGHT_OFF, 10000);
}

//void TestEkosCaptureWorkflow::testPreMountAndDomePark()
//{
//    // use the light panel simulator
//    m_CaptureHelper->m_LightPanelDevice = "Light Panel Simulator";
//    // use the dome simulator
//    m_CaptureHelper->m_DomeDevice = "Dome Simulator";
//    // default initialization
//    QVERIFY(prepareTestCase());

//    // QSKIP("Observatory refactoring needs to be completed until this test can be activated.");

//    // switch to capture module
//    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
//    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

//    // use a test directory for flats
//    QString imagepath = getImageLocation()->path() + "/test";

//    // switch capture type to flat so that we can set the calibration
//    KTRY_SET_COMBO(capture, captureTypeS, "Flat");

//    // select internal flat light, pre-mount and but not pre-dome park
//    KTRY_SELECT_FLAT_METHOD(flatDeviceSourceC, true, false);
//    // determine frame type
//    QFETCH(QString, frametype);
//    // build a simple 1xL sequence
//    KTRY_CAPTURE_ADD_FRAME(frametype, 2, 1, 2.0, "Luminance", imagepath);

//    // start the sequence
//    // m_CaptureHelper->expectedDomeStates.append(ISD::Dome::DOME_PARKED);
//    m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_PARKED);
//    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);
//    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
//    KTRY_CLICK(capture, startB);
//    // check if mount has reached the expected position
//    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedMountStates, 30000);
//    // check if dome has reached the expected position
//    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedDomeStates, 30000);
//    // check if one single flat is captured
//    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates, 60000);
//}

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
    QVERIFY(m_CaptureHelper->executeFocusing());

    QSharedPointer<Ekos::Focus> focus = Ekos::Manager::Instance()->focusModule()->mainFocuser();
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
    KTRY_SET_COMBO(capture, captureTypeS, "Flat");
    // this test is about the focuser, so take the flat at its exposure instead of calibrating it
    KTRY_SELECT_FLAT_MANUAL(capture);

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

    // clear calibration settings
    KTRY_SET_CHECKBOX(capture, captureCalibrationWall, false);
    KTRY_SET_CHECKBOX(capture, captureCalibrationParkMount, false);
    KTRY_SET_CHECKBOX(capture, captureCalibrationParkDome, false);

    // use a test directory for darks
    QString imagepath = getImageLocation()->path() + "/test";

    // switch capture type to flat so that we can set the calibration
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Dark");

    // build a simple 1xBlue dark sequence
    KTRY_CAPTURE_ADD_FRAME("Dark", 1, 1, 0.0, "Blue", "test", imagepath);
    // build a simple 1xL light sequence
    KTRY_CAPTURE_ADD_LIGHT(1, 1, 0.0, "Red", "test", imagepath);
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
        // if shutter type unknown, answer the modal dialog with "No"
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

    // select the primary train for darks capturing
    KTRY_SET_COMBO(darkLibraryDialog, opticalTrainCombo, m_CaptureHelper->m_primaryTrain);

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
    QFileInfo destinationInfo(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation), "darks");
    QDir destination(destinationInfo.absoluteFilePath());
    QVERIFY(m_CaptureHelper->searchFITS(destination).size() == 1);

    // The metadata of the generated master dark must survive the database round trip unchanged.
    auto camera = capture->mainCamera()->activeCamera();
    QVERIFY(camera != nullptr);
    auto chip = camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    QVERIFY(chip != nullptr);

    QList<QVariantMap> darkFrames;
    QVERIFY(KStarsData::Instance()->userdb()->GetAllDarkFrames(darkFrames));
    QVariantMap masterDark;
    for (const auto &oneFrame : darkFrames)
    {
        if (oneFrame["ccd"].toString() == camera->getDeviceName() &&
                (masterDark.isEmpty() || oneFrame["id"].toInt() > masterDark["id"].toInt()))
            masterDark = oneFrame;
    }
    QVERIFY2(!masterDark.isEmpty(), "The generated master dark was not recorded in the darkframe table");

    // Binning, duration and temperature must be stored as values, not left to the column defaults.
    QVERIFY(!masterDark["binX"].isNull());
    QVERIFY(!masterDark["binY"].isNull());
    QVERIFY(masterDark["binX"].toInt() > 0);
    QCOMPARE(masterDark["binX"].toInt(), masterDark["binY"].toInt());
    QVERIFY(qAbs(masterDark["duration"].toDouble() - 1.0) < 0.001);
    QVERIFY(!masterDark["temperature"].isNull());
    // Gain is only recorded for cameras that report it, but it must never be NULL: the sentinel
    // is what tells findDarkFrame() that the gain is unknown instead of a mismatch.
    QVERIFY(!masterDark["gain"].isNull());

    // And the recorded dark must be found again for the same chip, duration and binning.
    QSharedPointer<FITSData> darkData;
    QVERIFY(Ekos::DarkLibrary::Instance()->findDarkFrame(chip, 1.0, darkData));
    QVERIFY(!darkData.isNull());

    // An expired dark frame must be rejected with an explanation, not by a generic message.
    const uint validityDays = Options::darkLibraryDuration();
    QVERIFY(validityDays > 1);
    QVariantMap expiredDark = masterDark;
    expiredDark["timestamp"] = QDateTime::currentDateTime().addDays(-static_cast<int>(validityDays) - 1).toString(Qt::ISODate);
    QVERIFY(KStarsData::Instance()->userdb()->UpdateDarkFrame(expiredDark));
    Ekos::DarkLibrary::Instance()->refreshFromDB();

    QString expiryReason;
    QSharedPointer<FITSData> expiredData;
    QVERIFY(!Ekos::DarkLibrary::Instance()->findDarkFrame(chip, 1.0, expiredData, &expiryReason));
    QVERIFY2(!expiryReason.isEmpty(), "An expired dark frame must be rejected with an explanation");

    // restore the recorded timestamp, so the library is left as it was generated
    QVERIFY(KStarsData::Instance()->userdb()->UpdateDarkFrame(masterDark));
    Ekos::DarkLibrary::Instance()->refreshFromDB();

    // A lookup that cannot match must also fail *and* explain why, rather than silently returning nothing.
    // The generated master dark is 1x1, so a 2x2 request must not be satisfied by it.
    chip->setBinning(2, 2);
    int chipBinX = 0, chipBinY = 0;
    for (int i = 0; i < 20; i++)
    {
        chip->getBinning(&chipBinX, &chipBinY);
        if (chipBinX == 2 && chipBinY == 2)
            break;
        QTest::qWait(100);
    }
    QCOMPARE(chipBinX, 2);
    QCOMPARE(chipBinY, 2);

    QString failureReason;
    QSharedPointer<FITSData> unmatchedDark;
    QVERIFY(!Ekos::DarkLibrary::Instance()->findDarkFrame(chip, 1.0, unmatchedDark, &failureReason));
    QVERIFY2(!failureReason.isEmpty(), "A failed dark frame lookup must explain why it failed");

    // restore the binning for the tests that run after this one
    chip->setBinning(1, 1);
}

void TestEkosCaptureWorkflow::testDarksLibraryAfterAbortedRun()
{
    // only capture is needed
    QVERIFY(prepareTestCase(true));
    m_CaptureHelper->ensureCCDShutter(true);

    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    QVERIFY(camera != nullptr);

    // A clear bias level, so that a wrongly scaled master dark stands out. The simulator only adds the
    // bias together with the readout noise, which other test cases switch off.
    QVERIFY(setCameraNumber("SIMULATOR_SETTINGS", "SIM_NOISE", 10));
    QVERIFY(setCameraNumber("CCD_OFFSET", "OFFSET", 500));

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    QWidget *darkLibraryDialog = nullptr;
    QVERIFY(openDarkLibrary(4, darkLibraryDialog));

    // Stop the first run after two frames
    m_CaptureHelper->expectedCaptureStates.clear();
    for (int i = 0; i < 2; i++)
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    KTRY_CLICK(darkLibraryDialog, startB);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.isEmpty(), 30000);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_ABORTED);
    KTRY_CLICK(darkLibraryDialog, stopB);
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->expectedCaptureStates.isEmpty(), 10000);
    KTRY_GADGET(darkLibraryDialog, QPushButton, startB);
    QTRY_VERIFY_WITH_TIMEOUT(startB->isEnabled(), 5000);

    // Then run it again to completion
    QString master;
    QVERIFY(runDarkLibrary(darkLibraryDialog, 4, master));

    // Every simulated dark pixel is bias + uniform noise in [0, SIM_NOISE)
    const double bias = camera->getNumber("CCD_OFFSET").findWidgetByName("OFFSET")->getValue();
    const double noise = camera->getNumber("SIMULATOR_SETTINGS").findWidgetByName("SIM_NOISE")->getValue();
    const double expected = bias + std::max(0.0, noise - 1) / 2;

    auto masterData = loadFITS(master);
    QVERIFY2(masterData, qPrintable(QString("Failed to load the master dark %1").arg(master)));
    const double median = medianOf(pixelValues(masterData));

    QVERIFY2(std::abs(median - expected) <= std::max(3.0, 0.03 * expected),
             qPrintable(QString("The master dark median is %1 ADU, but a single dark frame has %2 ADU: frames of the "
                       "stopped run were averaged into the next master dark.").arg(median).arg(expected)));
}

void TestEkosCaptureWorkflow::testDarksLibraryDefectMap()
{
    // only capture is needed
    QVERIFY(prepareTestCase(true));
    m_CaptureHelper->ensureCCDShutter(true);

    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    QVERIFY(camera != nullptr);
    if (!camera->getNumber("SIM_DEFECTS").isValid())
        QSKIP("The CCD simulator cannot simulate hot and cold pixels (SIM_DEFECTS), INDI is too old.");

    // Hot pixels must stand out from a flat, noisy bias, and cold pixels read half the bias.
    // Gain 0 keeps the simulated dark current at the configured rate. The simulator only adds the
    // bias together with the readout noise, which other test cases switch off.
    QVERIFY(setCameraNumber("SIMULATOR_SETTINGS", "SIM_NOISE", 10));
    QVERIFY(setCameraNumber("CCD_OFFSET", "OFFSET", 500));
    QVERIFY(setCameraNumber("CCD_GAIN", "GAIN", 0));
    QVERIFY(setCameraNumber("SIM_DEFECTS", "SIM_HOT_RATE", 2000));
    QVERIFY(setCameraNumber("SIM_DEFECTS", "SIM_HOT_PIXELS", 300));
    QVERIFY(setCameraNumber("SIM_DEFECTS", "SIM_COLD_PIXELS", 100));
    auto restoreDefects = qScopeGuard([this]()
    {
        setCameraNumber("SIM_DEFECTS", "SIM_HOT_PIXELS", 0);
        setCameraNumber("SIM_DEFECTS", "SIM_COLD_PIXELS", 0);
    });

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    QWidget *darkLibraryDialog = nullptr;
    QVERIFY(openDarkLibrary(3, darkLibraryDialog));
    QString master;
    QVERIFY(runDarkLibrary(darkLibraryDialog, 3, master));

    // Find the defects in the master dark independently from the defect map code.
    // KStars leaves a 4 pixel border alone, since the median filter needs the neighbours.
    auto masterData = loadFITS(master);
    QVERIFY2(masterData, qPrintable(QString("Failed to load the master dark %1").arg(master)));
    const int width = masterData->width();
    const int height = masterData->height();
    const QVector<double> masterValues = pixelValues(masterData);
    const double median = medianOf(masterValues);
    QList<int> hot, cold;
    for (int y = 4; y < height - 4; y++)
        for (int x = 4; x < width - 4; x++)
        {
            const double value = masterValues[x + y * width];
            if (value - median > 300)
                hot << x + y * width;
            else if (median - value > 150)
                cold << x + y * width;
        }
    QVERIFY2(hot.size() > 250 && cold.size() > 80,
             qPrintable(QString("Expected about 300 hot and 100 cold pixels in the master dark, found %1 and %2")
                        .arg(hot.size()).arg(cold.size())));

    // Generate and save the defect map of the new master dark in the Dark Library, as a user does
    KTRY_GADGET(darkLibraryDialog, QTabWidget, darkTabsWidget);
    darkTabsWidget->setCurrentIndex(1);
    KTRY_GADGET(darkLibraryDialog, QComboBox, masterDarksCombo);
    QVERIFY(masterDarksCombo->count() > 0);
    bool masterLoaded = false;
    auto loadedConnection = connect(Ekos::DarkLibrary::Instance(), &Ekos::DarkLibrary::newImage, this,
                                    [&masterLoaded, &master](const QSharedPointer<FITSData> &data)
    {
        if (data && data->filename() == master)
            masterLoaded = true;
    });
    auto disconnectLoaded = qScopeGuard([loadedConnection]()
    {
        disconnect(loadedConnection);
    });
    // The newest master is the last entry. Select another one first so that selecting it loads it.
    if (masterDarksCombo->count() > 1)
        masterDarksCombo->setCurrentIndex(0);
    masterDarksCombo->setCurrentIndex(masterDarksCombo->count() - 1);
    QTRY_VERIFY_WITH_TIMEOUT(masterLoaded, 10000);
    KTRY_CLICK(darkLibraryDialog, saveMapB);

    auto chip = camera->getChip(ISD::CameraChip::PRIMARY_CCD);
    QVERIFY(chip != nullptr);
    QSharedPointer<DefectMap> defectMap;
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::DarkLibrary::Instance()->findDefectMap(chip, 1.0, defectMap), 5000);

    // Prefer the defect map, so the correction below cannot fall back to subtracting the master dark
    KTRY_GADGET(darkLibraryDialog, QRadioButton, preferDefectsRadio);
    QTest::mouseClick(preferDefectsRadio, Qt::LeftButton);
    const int trainID = Ekos::OpticalTrainManager::Instance()->id(m_CaptureHelper->m_primaryTrain);
    QTRY_VERIFY_WITH_TIMEOUT([trainID]()
    {
        Ekos::OpticalTrainSettings::Instance()->setOpticalTrainID(trainID);
        return Ekos::OpticalTrainSettings::Instance()->getOneSetting(Ekos::OpticalTrainSettings::DarkLibrary)
               .toMap().contains("preferDefectsRadio");
    }
    (), 5000);
    auto restorePreference = qScopeGuard([darkLibraryDialog]()
    {
        if (auto darks = darkLibraryDialog->findChild<QRadioButton *>("preferDarksRadio"))
            darks->setChecked(true);
    });

    // Correct a copy of the master dark through the same path as Capture, Focus and Guide
    auto frame = loadFITS(master);
    QVERIFY(frame);
    Ekos::DarkProcessor processor;
    QVERIFY(processor.denoiseSynchronous(trainID, chip, frame, 1.0, 0, 0));
    const QVector<double> corrected = pixelValues(frame);
    QVERIFY2(medianOf(corrected) > median / 2, "The defect map was not used: the master dark was subtracted instead");

    int survivingHot = 0, survivingCold = 0;
    for (const int index : std::as_const(hot))
        if (corrected[index] - median > 300)
            survivingHot++;
    for (const int index : std::as_const(cold))
        if (median - corrected[index] > 150)
            survivingCold++;

    QVERIFY2(survivingHot == 0 && survivingCold == 0,
             qPrintable(QString("The defect map of a %1x%2 master dark left %3 of %4 hot and %5 of %6 cold pixels")
                        .arg(width).arg(height).arg(survivingHot).arg(hot.size()).arg(survivingCold).arg(cold.size())));
}

void TestEkosCaptureWorkflow::testLoadEsqFileGeneral()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // initialize the capture settings
    QFETCH(uint, esqVersion);
    QFETCH(QString, observer);
    QFETCH(bool, guideDeviationCheck);
    QFETCH(bool, startGuideDeviationCheck);
    QFETCH(bool, inSequenceFocusCheck);
    QFETCH(bool, autofocusOnTemperatureCheck);
    QFETCH(bool, refocusEveryNTimesCheck);
    QFETCH(bool, focusAfterMeridianFlipCheck);
    TestEkosCaptureHelper::CaptureSettings settings;
    settings.observer = observer;
    settings.guideDeviation = {guideDeviationCheck, 2.0};
    settings.startGuideDeviation = {startGuideDeviationCheck, 1.0};
    settings.inSequenceFocus = {inSequenceFocusCheck, 1.5};
    settings.autofocusOnTemperature = {autofocusOnTemperatureCheck, 3.3};
    settings.refocusEveryN = {refocusEveryNTimesCheck, 5};
    settings.refocusAfterMeridianFlip = focusAfterMeridianFlipCheck;

    // clear current values to ensure that we observe a change
    QString oldObserver("unknown");
    capture->setObserverName(oldObserver);
    KTRY_SET_DOUBLESPINBOX(capture, guideDeviation, 0.0);
    KTRY_SET_CHECKBOX(capture, enforceGuideDeviation, !settings.guideDeviation.enabled);
    KTRY_SET_DOUBLESPINBOX(capture, startGuideDeviation, 0.0);
    KTRY_SET_CHECKBOX(capture, enforceStartGuiderDrift, !settings.startGuideDeviation.enabled);
    KTRY_SET_DOUBLESPINBOX(capture, hFRDeviation, 0.1);
    KTRY_SET_CHECKBOX(capture, enforceAutofocusHFR, !settings.inSequenceFocus.enabled);
    KTRY_SET_DOUBLESPINBOX(capture, maxFocusTemperatureDelta, 0.2);
    KTRY_SET_CHECKBOX(capture, enforceAutofocusOnTemperature, !settings.autofocusOnTemperature.enabled);
    KTRY_SET_SPINBOX(capture, refocusEveryN, 100);
    KTRY_SET_CHECKBOX(capture, enforceRefocusEveryN, !settings.refocusEveryN.enabled);
    KTRY_SET_CHECKBOX(capture, refocusAfterMeridianFlip, !settings.refocusAfterMeridianFlip);

    // create capture sequence file
    TestEkosCaptureHelper::SimpleCaptureLightsJob job;
    QVector<TestEkosCaptureHelper::SimpleCaptureLightsJob> jobs;
    jobs.append(job);
    QStringList content = m_CaptureHelper->getSimpleEsqContent(settings, jobs,
                          static_cast<TestEkosCaptureHelper::ESQVersion>(esqVersion));
    QString esqFilename = destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file name: " << esqFilename;
    m_CaptureHelper->writeFile(esqFilename, content);

    // load the capture sequence file
    QVERIFY2(capture->loadSequenceQueue(esqFilename), "Loading capture sequence file failed!");

    // Verify the results
    QCOMPARE(capture->getObserverName(), settings.observer);
    QCOMPARE(enforceGuideDeviation->isChecked(), settings.guideDeviation.enabled);
    QCOMPARE(guideDeviation->value(), settings.guideDeviation.value);
    QCOMPARE(enforceStartGuiderDrift->isChecked(), settings.startGuideDeviation.enabled);
    QCOMPARE(startGuideDeviation->value(), settings.startGuideDeviation.value);
    QCOMPARE(enforceAutofocusHFR->isChecked(), settings.inSequenceFocus.enabled);
    QCOMPARE(hFRDeviation->value(), settings.inSequenceFocus.value);
    QCOMPARE(enforceAutofocusOnTemperature->isChecked(), settings.autofocusOnTemperature.enabled);
    QCOMPARE(maxFocusTemperatureDelta->value(), settings.autofocusOnTemperature.value);
    QCOMPARE(enforceRefocusEveryN->isChecked(), settings.refocusEveryN.enabled);
    QCOMPARE(refocusEveryN->value(), settings.refocusEveryN.value);
    QCOMPARE(refocusAfterMeridianFlip->isChecked(), settings.refocusAfterMeridianFlip);
}

void TestEkosCaptureWorkflow::testLoadEsqFileBasicJobSettings()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // initialize default settings
    TestEkosCaptureHelper::CaptureSettings settings = {"Test Observer", {true, 2.0},  {true, 1.0},  {false, 1.5}, {true, 1.0}, {false, 5}, false};

    // clear the UI settings
    KTRY_SET_DOUBLESPINBOX(capture, captureExposureN, 1.0);
    KTRY_SET_SPINBOX(capture, captureCountN, 1);
    KTRY_SET_SPINBOX(capture, captureDelayN, 0);
    KTRY_SET_SPINBOX(capture, captureBinHN, 1);
    KTRY_SET_SPINBOX(capture, captureBinVN, 1);
    KTRY_SET_SPINBOX(capture, captureFrameXN, 0);
    KTRY_SET_SPINBOX(capture, captureFrameYN, 0);
    KTRY_SET_SPINBOX(capture, captureFrameWN, 500);
    KTRY_SET_SPINBOX(capture, captureFrameHN, 500);
    KTRY_SET_COMBO(capture, FilterPosCombo, "Blue");
    KTRY_SET_COMBO(capture, captureEncodingS, "FITS");
    KTRY_SET_COMBO(capture, captureTypeS, "Dark");
    KTRY_SET_LINEEDIT(capture, targetNameT, "nothing");
    KTRY_SET_LINEEDIT(capture, fileDirectoryT, "/home/pi");
    KTRY_SET_LINEEDIT(capture, placeholderFormatT, "/%T");
    KTRY_SET_SPINBOX(capture, formatSuffixN, 1);
    KTRY_SET_DOUBLESPINBOX(capture, cameraTemperatureN, 10.0);
    KTRY_SET_COMBO_INDEX(capture, fileUploadModeS, 1);

    // create the job
    TestEkosCaptureHelper::SimpleCaptureLightsJob job;
    QFETCH(uint, esqVersion);
    job.version = static_cast<TestEkosCaptureHelper::ESQVersion>(esqVersion);
    QFETCH(double, exposureTime);
    job.exposureTime = exposureTime;
    QFETCH(QString, targetName);
    job.targetName = targetName;
    QFETCH(int, count);
    job.count = count;
    QFETCH(int, delay);
    job.delayMS = delay;
    QFETCH(int, binX);
    job.binX = binX;
    QFETCH(int, binY);
    job.binY = binY;
    QFETCH(int, x);
    job.x = x;
    QFETCH(int, y);
    job.y = y;
    QFETCH(int, w);
    job.w = w;
    QFETCH(int, h);
    job.h = h;
    QFETCH(QString, filter);
    job.filterName = filter;
    QFETCH(QString, type);
    job.type = type;
    QFETCH(QString, encoding);
    job.encoding = encoding;
    QFETCH(QString, fitsDirectory);
    job.fitsDirectory = fitsDirectory;
    QFETCH(int, formatSuffix);
    job.formatSuffix = formatSuffix;
    QFETCH(QString, placeholderFormat);
    job.placeholderFormat = placeholderFormat;
    QFETCH(double, cameraTemperature);
    job.cameraTemperature.value = cameraTemperature;
    QFETCH(bool, cameraCooling);
    job.cameraTemperature.enabled = cameraCooling;
    QFETCH(int, fileUploadMode);
    job.uploadMode = fileUploadMode;

    QVector<TestEkosCaptureHelper::SimpleCaptureLightsJob> jobs;
    jobs.append(job);
    // create capture sequence file
    QStringList content = m_CaptureHelper->getSimpleEsqContent(settings, jobs);
    QString esqFilename = destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file name: " << esqFilename;
    m_CaptureHelper->writeFile(esqFilename, content);

    // load the capture sequence file
    QVERIFY2(capture->loadSequenceQueue(esqFilename), "Loading capture sequence file failed!");

    // Verify the results
    QTRY_COMPARE(captureExposureN->value(), exposureTime);
    QTRY_COMPARE(captureCountN->value(), count);
    QTRY_COMPARE(captureDelayN->value(), delay / 1000);
    QTRY_COMPARE(captureBinHN->value(), binX);
    QTRY_COMPARE(captureBinVN->value(), binY);
    QTRY_COMPARE(captureFrameXN->value(), x);
    QTRY_COMPARE(captureFrameYN->value(), y);
    QTRY_COMPARE(captureFrameWN->value(), w);
    QTRY_COMPARE(captureFrameHN->value(), h);
    QTRY_COMPARE(FilterPosCombo->currentText(), filter);
    QTRY_COMPARE(captureTypeS->currentText(), type);
    QTRY_COMPARE(captureEncodingS->currentText(), encoding);
    QTRY_COMPARE(targetNameT->text(), esqVersion == TestEkosCaptureHelper::ESQ_VERSION_2_5 ? "Test Target" : targetName);
    QTRY_COMPARE(fileDirectoryT->text(), fitsDirectory);
    QTRY_COMPARE(placeholderFormatT->text(), placeholderFormat);
    QTRY_COMPARE(formatSuffixN->value(), formatSuffix);
    // A disabled target temperature no longer round-trips its specific value - the field simply
    // holds the "--" special value (its minimum), exactly like Gain/Offset already behave.
    if (cameraCooling)
        QTRY_COMPARE(cameraTemperatureN->value(), cameraTemperature);
    else
        QTRY_COMPARE(cameraTemperatureN->value(), cameraTemperatureN->minimum());
    QTRY_COMPARE(fileUploadModeS->currentIndex(), fileUploadMode);
}


void TestEkosCaptureWorkflow::testLoadEsqFileCalibrationSettings()
{
    // default initialization
    QVERIFY(prepareTestCase());

    // switch to capture module
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(capture, 1000);

    // initialize default settings
    TestEkosCaptureHelper::CaptureSettings settings = {"Test Observer", {true, 2.0},  {true, 1.0},  {false, 1.5}, {true, 1.0}, {false, 5}, false};

    // retrieve test data
    QFETCH(uint, esqVersion);
    QFETCH(double, exposureTime);
    QFETCH(int, count);
    QFETCH(QString, type);
    QFETCH(uint, pre_action);
    QFETCH(double, wall_az);
    QFETCH(double, wall_alt);
    QFETCH(bool, duration_manual);
    QFETCH(bool, duration_adu);
    QFETCH(int, adu);
    QFETCH(int, tolerance);

    // clear the UI settings
    KTRY_SET_DOUBLESPINBOX(capture, captureExposureN, 5.4);
    KTRY_SET_COMBO(capture, captureTypeS, (type == "Flat" ? "Dark" : "Flat"));
    KTRY_SET_SPINBOX(capture, captureCountN, 2 + count);

    // create the job
    TestEkosCaptureHelper::SimpleCaptureCalibratingJob job;
    job.version = static_cast<TestEkosCaptureHelper::ESQVersion>(esqVersion);
    job.exposureTime = exposureTime;
    job.type = type;
    job.count = count;
    job.preAction = pre_action;
    job.wall_az = wall_az;
    job.wall_alt = wall_alt;
    job.duration_manual = duration_manual;
    job.duration_adu = duration_adu;
    job.adu = adu;
    job.tolerance = tolerance;

    QVector<TestEkosCaptureHelper::SimpleCaptureCalibratingJob> jobs;
    jobs.append(job);
    // create capture sequence file
    QStringList content = m_CaptureHelper->getSimpleEsqContent(settings, jobs,
                          static_cast<TestEkosCaptureHelper::ESQVersion>(esqVersion));
    QString esqFilename = destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file name: " << esqFilename;
    m_CaptureHelper->writeFile(esqFilename, content);

    // load the capture sequence file
    QVERIFY2(capture->loadSequenceQueue(esqFilename), "Loading capture sequence file failed!");

    // Verify the results on the main tab
    QTRY_COMPARE(captureExposureN->value(), exposureTime);
    QTRY_COMPARE(captureCountN->value(), count);
    QTRY_COMPARE(captureTypeS->currentText(), type);

    // Verify the results on the calibration dialog
    bool success = false;
    QTimer::singleShot(1000, capture, [&] { success = verifyCalibrationSettings(); });
    KTRY_CLICK(capture, calibrationB);

    QTRY_VERIFY_WITH_TIMEOUT(success, 10000);
}

bool TestEkosCaptureWorkflow::verifyCalibrationSettings()
{
    bool passed = false;
    QDialog *calibrationDialog = nullptr;
    KTRY_VERIFY_WITH_TIMEOUT_SUB(calibrationDialog = Ekos::Manager::Instance()->findChild<QDialog *>("Calibration"),
                                 2000);
    // ensure that the cancel button is pressed in any case
    [&]()
    {
        QFETCH(uint, pre_action);
        QFETCH(double, wall_az);
        QFETCH(double, wall_alt);
        QFETCH(bool, duration_manual);
        QFETCH(bool, duration_adu);
        QFETCH(int, adu);
        QFETCH(int, tolerance);
        KTRY_GADGET(calibrationDialog, QCheckBox, captureCalibrationWall);
        KTRY_GADGET(calibrationDialog, QCheckBox, captureCalibrationParkMount);
        KTRY_GADGET(calibrationDialog, QCheckBox, captureCalibrationParkDome);
        KTRY_GADGET(calibrationDialog, dmsBox, azBox);
        KTRY_GADGET(calibrationDialog, dmsBox, altBox);
        KTRY_GADGET(calibrationDialog, QRadioButton, captureCalibrationDurationManual);
        KTRY_GADGET(calibrationDialog, QRadioButton, captureCalibrationUseADU);
        KTRY_GADGET(calibrationDialog, QSpinBox, captureCalibrationADUValue);
        KTRY_GADGET(calibrationDialog, QSpinBox, captureCalibrationADUTolerance);
        QTRY_COMPARE(captureCalibrationWall->isChecked(), (pre_action & Ekos::CAPTURE_PREACTION_WALL) > 0);
        QTRY_COMPARE(captureCalibrationParkMount->isChecked(), (pre_action & Ekos::CAPTURE_PREACTION_PARK_MOUNT) > 0);
        QTRY_COMPARE(captureCalibrationParkDome->isChecked(), (pre_action & Ekos::CAPTURE_PREACTION_PARK_DOME) > 0);

        if (pre_action & Ekos::CAPTURE_PREACTION_WALL)
        {
            dms wallAz, wallAlt;
            bool azOk = false, altOk = false;

            wallAz  = azBox->createDms(&azOk);
            wallAlt = altBox->createDms(&altOk);

            QTRY_COMPARE(wallAz.Degrees(), wall_az);
            QTRY_COMPARE(wallAlt.Degrees(), wall_alt);
        }
        QTRY_COMPARE(captureCalibrationDurationManual->isChecked(), duration_manual);
        QTRY_COMPARE(captureCalibrationUseADU->isChecked(), duration_adu);
        if (duration_adu)
        {
            QTRY_COMPARE(captureCalibrationADUValue->value(), adu);
            QTRY_COMPARE(captureCalibrationADUTolerance->value(), tolerance);
        }
        passed = true;
    }
    ();
    // cancel the dialog
    calibrationDialog->done(QDialog::Rejected);

    return passed;
}


/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosCaptureWorkflow::testCaptureRefocusDelay_data()
{
    prepareTestData(31.0, {"Luminance:3"});
}

void TestEkosCaptureWorkflow::testCaptureRefocusHFR_data()
{
    prepareTestData(10.0, {"Luminance:6"});
}

void TestEkosCaptureWorkflow::testCaptureRefocusTemperature_data()
{
    prepareTestData(10.0, {"Luminance:6"});
}

void TestEkosCaptureWorkflow::testCaptureRefocusAfterPause_data()
{
    prepareTestData(10.0, {"Luminance:6"});
}

void TestEkosCaptureWorkflow::testCaptureRefocusAbort_data()
{
    prepareTestData(31.0, {"Luminance:3"});
}

void TestEkosCaptureWorkflow::testCaptureScriptsExecution_data()
{
    QTest::addColumn<bool>("pausing");             /*!< pause between capturing */
    QTest::newRow("pausing=false") << false;
    QTest::newRow("pausing=true") << true;
}

void TestEkosCaptureWorkflow::testInitialGuidingLimitCapture_data()
{
    prepareTestData(20.0, {"Luminance:5"});
}

void TestEkosCaptureWorkflow::testFlatManualSource_data()
{
    QTest::addColumn<bool>("clickModalOK");             /*!< click "OK" on the modal dialog        */
    QTest::addColumn<bool>("clickModal2OK");            /*!< click "OK" on the second modal dialog */

    // both variants for second click: click OK and click Cancel
    QTest::newRow("Flat, modal=true/true")  << true << true;
    QTest::newRow("Flat, modal=true/false")  << true << false;
    // first click Cancel
    QTest::newRow("Flat, modal=false") << false << true;
}

void TestEkosCaptureWorkflow::testLightPanelSource_data()
{
    QTest::addColumn<QString>("frametype");              /*!< frame type (Bias, Dark, Flat)       */
    QTest::addColumn<bool>("internalLight");             /*!< use internal or external flat light */

    for (auto frametype :
            {
                "Flat", "Dark", "Bias"
            })
        for (auto internalLight : // light source integrated into the light panel?
                {
                    true, false
                })
            QTest::newRow(QString("%1, light=%2").arg(frametype).arg(internalLight ? "internal" : "external").toLatin1())
                    << frametype << internalLight;
}

void TestEkosCaptureWorkflow::testDustcapSource_data()
{
    QTest::addColumn<QString>("frametype");              /*!< frame type (Dark or Flat)           */

    //    QTest::newRow("Flat, light=internal") << "Flat" << true;   // flat + light source integrated into the light panel
    QTest::newRow("Flat, light=internal") << "Flat";  // flat + internal light source used
    QTest::newRow("Dark, light=internal") << "Dark";  // dark + external light source turned off
    QTest::newRow("Bias, light=internal") << "Bias";  // dark + external light source turned off
    //    QTest::newRow("Dark") << "Dark" << false;  // dark
}

void TestEkosCaptureWorkflow::testWallSource_data()
{
    QTest::addColumn<QString>("frametype");              /*!< frame type (Dark, Flat or bias)           */

    QTest::newRow("Flat") << "Flat";
    QTest::newRow("Dark") << "Dark";
    QTest::newRow("Bias") << "Bias";
}


//void TestEkosCaptureWorkflow::testPreMountAndDomePark_data()
//{
//    testWallSource_data();
//}

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
    QTest::addColumn<bool>("scripts");                /*!< Run capture scripts       */

    QTest::newRow("init=0 target=-5") << 0.0 << -5.0 << false;
    QTest::newRow("init=0 target=0")  << 0.0 <<  0.0 << false;
    // a pre-capture script must not keep a restarted job from waiting for its temperature
    QTest::newRow("init=0 target=-5 scripts") << 0.0 << -5.0 << true;
}

void TestEkosCaptureWorkflow::testLoadEsqFileGeneral_data()
{
    QTest::addColumn<uint>("esqVersion");                  /*!< ESQ XML version                         */
    QTest::addColumn<QString>("observer");                 /*!< Set the observer value                  */
    QTest::addColumn<bool>("guideDeviationCheck");         /*!< Enable guide deviation                  */
    QTest::addColumn<bool>("startGuideDeviationCheck");    /*!< Enable starting guide deviation         */
    QTest::addColumn<bool>("inSequenceFocusCheck");        /*!< Enable in sequence focusing (HFR based) */
    QTest::addColumn<bool>("autofocusOnTemperatureCheck"); /*!< Enable temperature based autofocus      */
    QTest::addColumn<bool>("refocusEveryNTimesCheck");     /*!< Enable focusing after every n capture   */
    QTest::addColumn<bool>("focusAfterMeridianFlipCheck"); /*!< Enable refocus after a meridian flip    */

    uint version = TestEkosCaptureHelper::ESQ_VERSION_2_6;
    QTest::newRow(QString("observer v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit())
            << version << "KStars Freak" << false << false << false << false << false << false;
    QTest::newRow(QString("guideDeviationLimit v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) << version
            <<
    "KStars Freak" << true << false << false << false << false << false;
    QTest::newRow(QString("startGuideDeviation v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) << version
            << "KStars Freak" << false << true << false << false << false << false;
    QTest::newRow(QString("inSequenceFocus v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) << version <<
                          "KStars Freak" << false << false << true << false << false << false;
    QTest::newRow(QString("autofocusOnTemperature v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) <<
            version << "KStars Freak" << false << false << false << true << false << false;
    QTest::newRow(QString("refocusEveryN v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) << version <<
                          "KStars Freak" << false << false << false << false << true << false;
    QTest::newRow(QString("refocusAfterMeridianFlip v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) <<
            version << "KStars Freak" << false << false << false << false << false << true;
}

void TestEkosCaptureWorkflow::testLoadEsqFileBasicJobSettings_data()
{
    QTest::addColumn<uint>("esqVersion");             /*!< ESQ XML version             */
    QTest::addColumn<double>("exposureTime");         /*!< Exposure time               */
    QTest::addColumn<QString>("targetName");          /*!< Capture target              */
    QTest::addColumn<int>("count");                   /*!< Number of frames            */
    QTest::addColumn<int>("delay");                   /*!< Delay between captures      */
    QTest::addColumn<QString>("filter");              /*!< Filter name                 */
    QTest::addColumn<QString>("type");                /*!< Frame type (Light etc.)     */
    QTest::addColumn<QString>("encoding");            /*!< Encoding (FITS etc.)        */
    QTest::addColumn<int>("binX");                    /*!< Binning (X value)           */
    QTest::addColumn<int>("binY");                    /*!< Binning (Y value)           */
    QTest::addColumn<int>("x");                       /*!< ROI left                    */
    QTest::addColumn<int>("y");                       /*!< ROI top                     */
    QTest::addColumn<int>("w");                       /*!< ROI width                   */
    QTest::addColumn<int>("h");                       /*!< ROI height                  */
    QTest::addColumn<QString>("fitsDirectory");       /*!< Base directory for frames   */
    QTest::addColumn<QString>("placeholderFormat");   /*!< Placeholder format string   */
    QTest::addColumn<int>("formatSuffix");            /*!< Digits of the number suffix */
    QTest::addColumn<double>("cameraTemperature");    /*!< Cooling temperature         */
    QTest::addColumn<bool>("cameraCooling");          /*!< Cooling necessary           */
    QTest::addColumn<int>("fileUploadMode");          /*!< Upload mode (local/remote)  */

    // within the cooling range of the CCD simulator (-15°C to 25°C)
    double exposureTime = 2.0, cameraTemperature = -10.0;
    int count = 2, delay = 5000, binX = 2, binY = 2;
    int x = 10, y = 10, w = 480, h = 360;
    int formatSuffix = 4, fileUploadMode = 2;
    bool cameraCooling = true;
    QString target("Test Target");
    QString filter("Green");
    QString type("Light");
    QString encoding("Native");
    QString fitsDirectory("/home/astro");
    QString placeholderFormat("/%t/%T/%T_%t_%e");

    for (uint version :
            {
                TestEkosCaptureHelper::ESQ_VERSION_2_4, TestEkosCaptureHelper::ESQ_VERSION_2_5, TestEkosCaptureHelper::ESQ_VERSION_2_6
            })
    {
        QTest::newRow(QString("%2x %5 %3 %1s bin=%6x%7 dir=%4 v=%8").arg(exposureTime).arg(count).arg(filter)
                      .arg(fitsDirectory).arg(type).arg(binX).arg(binY).arg(m_CaptureHelper->esqVersionNames[version]).toLatin1())
                << version << exposureTime << target << count << delay << filter << type << encoding << binX << binY << x << y << w << h <<
                fitsDirectory << placeholderFormat << formatSuffix << cameraTemperature << cameraCooling << fileUploadMode;
    }
}

void TestEkosCaptureWorkflow::testLoadEsqFileCalibrationSettings_data()
{
    QTest::addColumn<uint>("esqVersion");             /*!< ESQ XML version              */
    QTest::addColumn<double>("exposureTime");         /*!< Exposure time                */
    QTest::addColumn<int>("count");                   /*!< Number of frames             */
    QTest::addColumn<QString>("type");                /*!< Frame type (Flat etc.)       */
    QTest::addColumn<uint>("pre_action");             /*!< Calibration Pre-Actions      */
    QTest::addColumn<double>("wall_az");              /*!< Az position for wall source  */
    QTest::addColumn<double>("wall_alt");             /*!< Alt position for wall source */
    QTest::addColumn<bool>("duration_manual");        /*!< Manual exposure time         */
    QTest::addColumn<bool>("duration_adu");           /*!< ADU based exposure           */
    QTest::addColumn<int>("adu");                     /*!< Target ADU                   */
    QTest::addColumn<int>("tolerance");               /*!< ADU tolerance                */

    for (uint version :
            {
                TestEkosCaptureHelper::ESQ_VERSION_2_5, TestEkosCaptureHelper::ESQ_VERSION_2_6
            })
    {
        QTest::newRow(QString("Flat pre_action=wall adu=manual v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit())
                << version << 1.0 << 2 <<
        "Flat" << static_cast<uint>(Ekos::CAPTURE_PREACTION_WALL) << 180.0 << 85.0 << true << false << 12345 << 1234;
        QTest::newRow(QString("Dark pre_action=none adu=automatic v=%1").arg(
                          m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) << version  << 1.0 << 2 << "Dark" <<
                                  static_cast<uint>(Ekos::CAPTURE_PREACTION_NONE) << 180.0 << 85.0 << false << true <<  12345 << 1234;
        QTest::newRow(QString("Bias pre_action=park_mount v=%1").arg(m_CaptureHelper->esqVersionNames[version]).toLocal8Bit()) <<
                version  << 1.0 << 2 << "Bias" << static_cast<uint>(Ekos::CAPTURE_PREACTION_PARK_MOUNT) << 180.0 << 85.0 << false << true <<
                12345 << 1234;
    }
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosCaptureWorkflow::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    // limit guiding pulses to ensure that guiding deviations lead to aborted capture
    Options::setRAMaximumPulseArcSec(5.0);
    Options::setDECMaximumPulseArcSec(5.0);

    QStandardPaths::setTestModeEnabled(true);

    QFileInfo test_dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation), "test");
    destination = new QTemporaryDir(test_dir.absolutePath());
    QVERIFY(destination->isValid());
    QVERIFY(destination->autoRemove());
    // wait for all module settings updated
    m_CaptureHelper->waitForSettingsUpdated = true;
}

void TestEkosCaptureWorkflow::cleanupTestCase()
{
    // nothing to do since we start the INDI service for each test case
}

bool TestEkosCaptureWorkflow::setCameraNumber(const QString &property, const QString &element, double value)
{
    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(camera != nullptr);

    auto number = camera->getNumber(property.toLatin1().constData());
    KVERIFY2_SUB(number.isValid(), qPrintable(QString("The camera has no %1 property").arg(property)));
    auto widget = number.findWidgetByName(element.toLatin1().constData());
    KVERIFY2_SUB(widget != nullptr, qPrintable(QString("%1 has no element %2").arg(property, element)));
    widget->setValue(value);
    camera->sendNewProperty(number);

    // wait until the driver confirms the new value
    KTRY_VERIFY_WITH_TIMEOUT_SUB(std::abs(camera->getNumber(property.toLatin1().constData())
                                          .findWidgetByName(element.toLatin1().constData())->getValue() - value) < 1e-6, 5000);
    return true;
}

bool TestEkosCaptureWorkflow::openDarkLibrary(int count, QWidget *&darkLibraryDialog)
{
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    KVERIFY_SUB(capture != nullptr);
    KTRY_CLICK_SUB(capture, darkLibraryB);
    darkLibraryDialog = nullptr;
    KTRY_VERIFY_WITH_TIMEOUT_SUB(darkLibraryDialog = Ekos::Manager::Instance()->findChild<QWidget *>("DarkLibrary"), 2000);

    KTRY_SET_COMBO_SUB(darkLibraryDialog, opticalTrainCombo, m_CaptureHelper->m_primaryTrain);
    KTRY_SET_DOUBLESPINBOX_SUB(darkLibraryDialog, minExposureSpin, 1);
    KTRY_SET_DOUBLESPINBOX_SUB(darkLibraryDialog, maxExposureSpin, 1);
    KTRY_SET_SPINBOX_SUB(darkLibraryDialog, countSpin, count);
    return true;
}

bool TestEkosCaptureWorkflow::runDarkLibrary(QWidget *darkLibraryDialog, int count, QString &master)
{
    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(camera != nullptr);
    const QString previous = newestMasterDark(camera->getDeviceName());

    m_CaptureHelper->expectedCaptureStates.clear();
    for (int i = 0; i < count; i++)
        m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_IMAGE_RECEIVED);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_COMPLETE);
    KTRY_CLICK_SUB(darkLibraryDialog, startB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates.isEmpty(), 30000 + count * 5000);

    KTRY_VERIFY_WITH_TIMEOUT_SUB(!(master = newestMasterDark(camera->getDeviceName())).isEmpty() && master != previous,
                                 5000);
    return true;
}

bool TestEkosCaptureWorkflow::prepareTestCase(bool flatsOnly)
{
    // set logging defaults for alignment
    Options::setVerboseLogging(false);
    Options::setLogToFile(false);

    // turn off altitude limits
    Options::setEnableAltitudeLimits(false);

    // use the helper to start the profile
    KVERIFY_SUB(m_CaptureHelper->startEkosProfile());
    // prepare optical trains for testing
    m_CaptureHelper->prepareOpticalTrains();
    // prepare the mount module for testing with OAG guiding
    m_CaptureHelper->prepareMountModule(TestEkosCaptureHelper::SCOPE_FSQ85, TestEkosCaptureHelper::SCOPE_FSQ85);
    if (flatsOnly == false)
    {
        // prepare for focusing tests
        m_CaptureHelper->prepareFocusModule();
        // prepare for alignment tests
        m_CaptureHelper->prepareAlignmentModule();
        // prepare for guiding tests
        m_CaptureHelper->prepareGuidingModule();
    }
    // prepare for capturing tests
    m_CaptureHelper->prepareCaptureModule();

    m_CaptureHelper->init();

    // check if all module settings are updated
    m_CaptureHelper->checkModuleConfigurationsCompleted();

    // clear image directory
    KVERIFY_SUB(m_CaptureHelper->getImageLocation()->removeRecursively());

    // ensure that the scope is unparked
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

    // preparation successful
    return true;
}

void TestEkosCaptureWorkflow::init()
{
    // reset counters
    image_count  = 0;
    // reset calibration
    Options::setCalibrationPreActionIndex(Ekos::CAPTURE_PREACTION_NONE);

    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    // clear light panel
    m_CaptureHelper->m_LightPanelDevice = nullptr;
    // clear rotator
    m_CaptureHelper->m_RotatorDevice = nullptr;
    // disable reset jobs warning
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    KMessageBox::saveDontShowAgainTwoActions("reset_job_status_warning", KMessageBox::ButtonCode::Cancel);
#else
    KMessageBox::saveDontShowAgainYesNo("reset_job_status_warning", KMessageBox::ButtonCode::No);
#endif
}

void TestEkosCaptureWorkflow::cleanup()
{
    if (Ekos::Manager::Instance()->focusModule() != nullptr)
        Ekos::Manager::Instance()->focusModule()->mainFocuser()->abort();

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    if (capture != nullptr)
    {
        capture->abort();
        capture->clearSequenceQueue();
        // Turn off the target temperature, which is kept in the settings as well: left
        // behind, it makes the jobs of all following test cases wait for the cooler.
        auto temperature = capture->findChild<QDoubleSpinBox *>("cameraTemperatureN");
        if (temperature != nullptr)
            temperature->setValue(temperature->minimum());
    }
    // Clear the capture scripts, which are kept in the settings and would otherwise
    // run in all following test cases as well.
    if (capture != nullptr && m_CaptureHelper->getScripts().size() > 0)
    {
        m_CaptureHelper->clearScripts();
        fillCaptureScripts();
        // wait for a sync
        QTest::qWait(5000);
        qCInfo(KSTARS_EKOS_TEST) << "Scripts cleared.";
    }
    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}


bool TestEkosCaptureWorkflow::prepareCapture(int refocusLimitTime, double refocusHFRThreshold, double refocusTemp,
        int delay)
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

    // set refocusing limits
    KTRY_SET_CHECKBOX_SUB(capture, enforceRefocusEveryN, (refocusLimitTime > 0));
    if (refocusLimitTime > 0)
        KTRY_SET_SPINBOX_SUB(capture, refocusEveryN, refocusLimitTime);

    KTRY_SET_CHECKBOX_SUB(capture, enforceAutofocusHFR, (refocusHFRThreshold > 0));
    if (refocusHFRThreshold > 0)
        KTRY_SET_DOUBLESPINBOX_SUB(capture, hFRThresholdPercentage, refocusHFRThreshold);

    KTRY_SET_CHECKBOX_SUB(capture, enforceAutofocusOnTemperature, (refocusTemp > 0));
    if (refocusTemp > 0)
        KTRY_SET_DOUBLESPINBOX_SUB(capture, maxFocusTemperatureDelta, refocusTemp);

    // create capture sequences
    KVERIFY_SUB(m_CaptureHelper->fillCaptureSequences(target, sequence, exptime, imagepath, delay));

    // everything successfully completed
    return true;
}

void TestEkosCaptureWorkflow::prepareTestData(double exptime, QList<QString> sequenceList)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(exptime)
    Q_UNUSED(sequence)
#else
    QTest::addColumn<double>("exptime");             /*!< exposure time */
    QTest::addColumn<QString>("sequence");           /*!< list of filters */

    for (QString sequence : sequenceList)
        QTest::newRow(QString("seq=%2, exp=%1").arg(exptime).arg(sequence).toStdString().c_str()) << exptime << sequence;
#endif
}

QDir *TestEkosCaptureWorkflow::getImageLocation()
{
    if (imageLocation == nullptr || imageLocation->exists())
        imageLocation = new QDir(destination->path() + "/images");

    return imageLocation;
}

void TestEkosCaptureWorkflow::fillCaptureScripts()
{
    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    bool success = false;
    QTimer::singleShot(1000, capture, [&] {success = m_CaptureHelper->fillScriptManagerDialog(m_CaptureHelper->getScripts());});
    // open script manager
    KTRY_CLICK(capture, scriptManagerB);
    // verify if script configuration succeeded
    QVERIFY2(success, "Scripts set up failed!");
}

/* *********************************************************************************
 *
 * Main function
 *
 * ********************************************************************************* */

QTEST_KSTARS_WITH_GUIDER_MAIN(TestEkosCaptureWorkflow)
