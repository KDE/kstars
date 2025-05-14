/*
    Base class of KStars UI tests for meridian flip

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "test_ekos_meridianflip_base.h"

#if defined(HAVE_INDI)

#include <qdir.h>

#include "kstars_ui_tests.h"
#include "test_ekos.h"

#include "auxiliary/dms.h"
#include "ksutils.h"
#include "indicom.h"
#include "Options.h"
#include "ekos/capture/capture.h"

TestEkosMeridianFlipBase::TestEkosMeridianFlipBase(QObject *parent) :
    TestEkosMeridianFlipBase::TestEkosMeridianFlipBase("Internal", parent) {}

TestEkosMeridianFlipBase::TestEkosMeridianFlipBase(QString guider, QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper(guider);
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
}

bool TestEkosMeridianFlipBase::startEkosProfile()
{
    // use the helper to start the profile
    KWRAP_SUB(m_CaptureHelper->startEkosProfile());
    // prepare optical trains for testing
    m_CaptureHelper->prepareOpticalTrains();
    // prepare the mount module for testing (OAG guiding seems more robust)
    m_CaptureHelper->prepareMountModule(TestEkosHelper::SCOPE_FSQ85, TestEkosHelper::SCOPE_FSQ85);
    // prepare for focusing tests
    m_CaptureHelper->prepareFocusModule();

    // Everything completed successfully
    return true;
}

void TestEkosMeridianFlipBase::initTestCase()
{
    // ensure EKOS is running
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    // Prepare PHD2 usage
    if (m_CaptureHelper->m_Guider == "PHD2")
        m_CaptureHelper->preparePHD2();

    // disable twilight warning
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    KMessageBox::saveDontShowAgainTwoActions("astronomical_twilight_warning", KMessageBox::ButtonCode::Cancel);
#else
    KMessageBox::saveDontShowAgainYesNo("astronomical_twilight_warning", KMessageBox::ButtonCode::No);
#endif
}

bool TestEkosMeridianFlipBase::shutdownEkosProfile()
{
    // cleanup the capture helper
    m_CaptureHelper->cleanup();
    // shutdown the profile
    return m_CaptureHelper->shutdownEkosProfile();
}

void TestEkosMeridianFlipBase::cleanupTestCase()
{
    if (m_CaptureHelper->m_Guider == "PHD2")
        m_CaptureHelper->cleanupPHD2();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosMeridianFlipBase::init()
{
    // initialize the capture helper
    m_CaptureHelper->init();

    // disable by default
    refocus_checked   = false;
    use_aligning      = false;
    dithering_checked = false;

    // reset initial focuser position
    initialFocusPosition = -1;

    // set geo location
    KStarsData * const d = KStars::Instance()->data();
    QVERIFY(d != nullptr);
    QFETCH(QString, location);
    GeoLocation * const geo = d->locationNamed(location);
    QVERIFY(geo != nullptr);
    d->setLocation(*geo);

    // start the profile (depending on the selected guider)
    QVERIFY(startEkosProfile());

    // disable FITS viewer
    Options::setUseFITSViewer(false);

    // Eliminate the noise setting to ensure a working focusing and plate solving
    KTRY_INDI_PROPERTY("CCD Simulator", "Simulator Config", "SIMULATOR_SETTINGS", ccd_settings);
    INDI_E *noise_setting = ccd_settings->getElement("SIM_NOISE");
    QVERIFY(ccd_settings != nullptr);
    noise_setting->setValue(0.0);
    ccd_settings->processSetButton();

    // clear guiding calibration to ensure proper guiding
    // TestEkosHelper::prepareGuidingModule() stores a guiding calibration, override if necessary
    // KTRY_CLICK(Ekos::Manager::Instance()->guideModule(), clearCalibrationB);

    // switch to mount module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule(), 1000);

    // activate meridian flip
    QVERIFY(enableMeridianFlip(0.0));

    // expected beginning of the meridian flip
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_PLANNED);
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_RUNNING);
}

void TestEkosMeridianFlipBase::cleanup()
{
    // ensure that capturing, focusing and guiding is stopped
    QVERIFY(stopScheduler());
    QVERIFY(m_CaptureHelper->stopFocusing());
    QVERIFY(stopAligning());
    QVERIFY(stopCapturing());
    QVERIFY(m_CaptureHelper->stopGuiding());

    // clean up capture module
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_GADGET(Ekos::Manager::Instance()->captureModule(), QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);

    // cleanup the scheduler
    m_CaptureHelper->cleanupScheduler();

    // shutdown profile and local INDI server
    QVERIFY(shutdownEkosProfile());
}



/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlipBase::prepareTestData(double exptime, QList<QString> locationList, QList<bool> culminationList,
        QList<std::pair<QString, int> > filterList, QList<int> focusList, QList<bool> guideList, QList<bool> ditherList)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(exptime)
    Q_UNUSED(locationList)
    Q_UNUSED(culminationList)
    Q_UNUSED(filterList)
    Q_UNUSED(focusList)
    Q_UNUSED(guideList)
    Q_UNUSED(ditherList)
#else
    QTest::addColumn<QString>("location"); /*!< locations the KStars test is running for */
    QTest::addColumn<bool>("culmination"); /*!< upper culmination? */
    QTest::addColumn<int>("count");        /*!< frame counts to capture */
    QTest::addColumn<double>("exptime");   /*!< exposure time of a single frame */
    QTest::addColumn<QString>("filters");  /*!< list of filters for the capture sequence */
    QTest::addColumn<int>("focus");       /*!< focusing: 0=no, 1=refocus, 2=HFR autofocus, 3=after meridian flip */
    QTest::addColumn<bool>("guide");       /*!< use guiding */
    QTest::addColumn<bool>("dither");      /*!< execute dithering after each capture */

    KStarsData * const d = KStars::Instance()->data();
    QVERIFY(d != nullptr);

    for (QString location : locationList)
        for (bool culmination : culminationList)
            for (int focus : focusList)
                for (bool guide : guideList)
                    for (bool dither : ditherList)
                        for(std::pair<QString, int> filter :  filterList)
                        {
                            int count = filter.second;
                            // both guide==false && dither==true does not make sense
                            if (guide == true || dither == false)
                                QTest::newRow(QString("%7: culm=%8, %1x%2, foc=%3, di=%4, guider=%5").arg(count).arg(filter.first)
                                              .arg(focus == 0 ? "no" : (focus == 1 ? "refocus" : (focus == 2 ? "auto" : "mf"))).arg(dither ? "yes" : "no")
                                              .arg(guide ? m_CaptureHelper->m_Guider : "none").arg(location)
                                              .arg(culmination ? "up" : "low").toLocal8Bit())
                                        << location << culmination << count << exptime << filter.first << focus << guide << dither;
                        }
#endif
}

/* *********************************************************************************
 *
 * Helpers functions
 *
 * ********************************************************************************* */

bool TestEkosMeridianFlipBase::positionMountForMF(int secsToMF, bool fast)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(secsToMF)
    Q_UNUSED(fast)
#else
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();

    findMFTestTarget(secsToMF, fast);

    // now slew very close before the meridian
    mount->slew(target->ra().Hours(), target->dec().Degrees());
    // wait a certain time until the mount slews
    QTest::qWait(3000);
    // wait until the mount is tracking
    KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->mountModule()->status() == ISD::Mount::MOUNT_TRACKING, 60000);
    // check whether a meridian flip is announced
    const QString flipmsg = Ekos::Manager::Instance()->mountModule()->meridianFlipStatusWidget->getStatus();
    const int prefixlength = QString("Meridian flip in").length();
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(flipmsg.length() >= prefixlength
                                       && flipmsg.left(prefixlength).compare("Meridian flip in") == 0, 20000));
#endif
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::prepareMFTestcase(bool guideDeviation, bool initialGuideDeviation)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(guideDeviation)
    Q_UNUSED(initialGuideDeviation)
#else
    // switch to capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // set refocus after meridian flip
    QFETCH(int, focus);
    refocus_checked = (focus > 0);

    // regular refocusing
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), enforceRefocusEveryN, (focus == 1));
    // HFR based refocusing
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), enforceAutofocusHFR, (focus == 2));
    // focus after flip
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), refocusAfterMeridianFlip, (focus == 3));

    // set guide deviation guard
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), enforceStartGuiderDrift, initialGuideDeviation);
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), enforceGuideDeviation, guideDeviation);
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->captureModule(), startGuideDeviation, 3.0);
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->captureModule(), guideDeviation, 3.0);

    // create the capture directory
    QTemporaryDir destination;
    KWRAP_SUB(QVERIFY(destination.isValid()));
    KWRAP_SUB(QVERIFY(destination.autoRemove()));
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << destination.path();

    // create capture sequence
    QFETCH(int, count);
    QFETCH(double, exptime);
    QFETCH(QString, filters);

    KWRAP_SUB(foreach(QString filter, filters.split(",")) KTRY_CAPTURE_ADD_LIGHT(exptime, count, 0, filter, "test",
              destination.path()));

    QFETCH(bool, guide);
    if (guide)
    {
        // slew roughly to the position to calibrate first
        KWRAP_SUB(QVERIFY(positionMountForMF(600, true)));
        qCInfo(KSTARS_EKOS_TEST) << "Slewed roughly to the position to calibrate first.";
        // calibrate guiding
        if (! m_CaptureHelper->startGuiding(2.0)) return false;
        if (! m_CaptureHelper->stopGuiding()) return false;
        qCInfo(KSTARS_EKOS_TEST) << "Guider calibration" << Options::serializedCalibration();
    }

    // set dithering due to test data set
    QFETCH(bool, dither);
    dithering_checked = dither;
    Options::setDitherEnabled(dither);

    // all steps succeeded
    return true;
#endif
}

bool TestEkosMeridianFlipBase::prepareCaptureTestcase(int secsToMF, bool guideDeviation, bool initialGuideDeviation)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(secsToMF)
    Q_UNUSED(initialFocus)
    Q_UNUSED(guideDeviation)
    Q_UNUSED(initialGuideDeviation)
#else
    // execute preparation steps
    KWRAP_SUB(QVERIFY(prepareMFTestcase(guideDeviation, initialGuideDeviation)));

    // slew close to the meridian
    QFETCH(bool, guide);
    KWRAP_SUB(QVERIFY(positionMountForMF(secsToMF, !guide)));
    qCInfo(KSTARS_EKOS_TEST) << "Slewed close to the meridian.";
#endif
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::prepareSchedulerTestcase(int secsToMF, bool useAlign,
        Ekos::CompletionCondition completionCondition, int iterations)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(secsToMF)
    Q_UNUSED(algorithm)
    Q_UNUSED(completionCondition)
    Q_UNUSED(iterations)
#else
    // execute all similar preparation steps for a capture test case except for target slew
    KVERIFY_SUB(prepareMFTestcase(false, false));

    // determine the target and sync close to it
    findMFTestTarget(secsToMF, true);

    // save current capture sequence to Ekos sequence file
    QString sequenceFile = m_CaptureHelper->destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file" << sequenceFile << "created.";
    KVERIFY_SUB(Ekos::Manager::Instance()->captureModule()->saveSequenceQueue(sequenceFile));

    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    // set sequence file
    scheduler->setSequence(sequenceFile);
    // set target
    KTRY_SET_LINEEDIT_SUB(scheduler, nameEdit, "test");
    KTRY_SET_LINEEDIT_SUB(scheduler, raBox, target->ra0().toHMSString());
    KTRY_SET_LINEEDIT_SUB(scheduler, decBox, target->dec0().toDMSString());
    // define step checks
    QFETCH(bool, guide);
    // disable all step checks
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerTrackStep, true);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerFocusStep, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerAlignStep, useAlign);
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerGuideStep, guide);
    // ignore twilight
    KTRY_SET_CHECKBOX_SUB(scheduler, schedulerTwilight, false);
    prepareTestData(18.0, {"Greenwich"}, {true}, {{"Luminance", 6}}, {0}, {false}, {false});
    // disable remember job progress
    Options::setRememberJobProgress(false);
    // disable INDI stopping after scheduler finished
    Options::setStopEkosAfterShutdown(false);

    // set the completion condition
    switch (completionCondition)
    {
        case Ekos::FINISH_REPEAT:
            // repeat the job for a fixed amount
            KTRY_SET_RADIOBUTTON_SUB(scheduler, schedulerRepeatSequences, true);
            KTRY_SET_SPINBOX_SUB(scheduler, schedulerRepeatSequencesLimit, iterations);
            break;
        case Ekos::FINISH_LOOP:
            KTRY_SET_RADIOBUTTON_SUB(scheduler, schedulerUntilTerminated, true);
            break;
        default:
            QWARN(QString("Unsupported completion condition %1!").arg(completionCondition).toStdString().c_str());
            return false;
    }
    // add scheduler job
    KTRY_CLICK_SUB(scheduler, addToQueueB);
#endif
    // preparation successful
    return true;
}

bool TestEkosMeridianFlipBase::checkDithering()
{
    // if dithering is not selected, to nothing
    if (! dithering_checked)
        return true;

    // reset the dithering flag
    m_CaptureHelper->dithered = false;

    // wait for 120s if dithering happens
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->dithered == true, 120000);
    // all checks succeeded
    return true;
}


bool TestEkosMeridianFlipBase::checkRefocusing()
{
    if (refocus_checked)
    {
        // wait until focusing has started
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_PROGRESS, 60000);
        qCInfo(KSTARS_EKOS_TEST()) << "Focus progress detected.";
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedFocusStates, 60000);
        qCInfo(KSTARS_EKOS_TEST()) << "All expected focus states received.";
        // check if focus completion is reached (successful or not)
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_COMPLETE
                                     || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_FAILED ||
                                     m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_ABORTED, 120000);
        qCInfo(KSTARS_EKOS_TEST()) << "Focusing completed.";
    }
    // focusing might have suspended guiding
    if (m_CaptureHelper->use_guiding)
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getGuidingStatus() == Ekos::GUIDE_GUIDING, 20000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::executeAlignment(double expTime)
{
    // mark alignment
    use_aligning = true;
    // prepare for alignment tests
    m_CaptureHelper->prepareAlignmentModule();

    m_CaptureHelper->expectedAlignStates.enqueue(Ekos::ALIGN_COMPLETE);
    KVERIFY_SUB(m_CaptureHelper->startAligning(expTime));
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedAlignStates, 60000);

    // alignment succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopAligning()
{
    // check whether alignment is already stopped
    if (m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_IDLE || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_ABORTED ||
            m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_FAILED || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_COMPLETE)
        return true;

    // switch to alignment module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000));

    // stop alignment
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->alignModule(), QPushButton, stopB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), stopB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_IDLE
                                 || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_ABORTED ||
                                 m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_FAILED || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_COMPLETE, 5000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startCapturing()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
#else
    QFETCH(double, exptime);
    // Now we can estimate how many captures will happen before the meridian flip.
    int t2mf = std::max(secondsToMF(), 0);

    // Ensure that dithering takes place after the flip
    if (dithering_checked)
        Options::setDitherFrames(static_cast<uint>(1 + t2mf / exptime));

    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // check if capture is in a stopped state
    Ekos::CaptureState capturestate = m_CaptureHelper->getCaptureStatus();
    KWRAP_SUB(QVERIFY(capturestate == Ekos::CAPTURE_IDLE || capturestate == Ekos::CAPTURE_ABORTED ||
                      capturestate == Ekos::CAPTURE_SUSPENDED || capturestate == Ekos::CAPTURE_COMPLETE));

    // press start
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates, 15000);
#endif
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopCapturing()
{
    // check if capture is in a stopped state
    if (m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_IDLE
            || m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED ||
            m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE
            || m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_PAUSED ||
            m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_MERIDIAN_FLIP)
        return true;

    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // else press stop
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_ABORTED);
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has stopped
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates, 5000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startScheduler()
{
    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    // set expected states
    // slewing detection unsure since the position is close to the target
    // m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_SLEWING);
    // m_CaptureHelper->expectedMountStates.append(ISD::Mount::MOUNT_TRACKING);
    if (m_CaptureHelper->use_guiding == true)
        m_CaptureHelper->expectedGuidingStates.append(Ekos::GUIDE_GUIDING);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

    // switch to the scheduler module and start
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    KVERIFY_SUB(m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_IDLE
                || m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_ABORTED);
    KTRY_CLICK_SUB(scheduler, startB);

    // check mount slew and tracking
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedMountStates, 60000);
    // check guiding
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedGuidingStates, 60000);
    // check capturing
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates, 60000);

    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopScheduler()
{
    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    if (m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_RUNNING
            || m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_PAUSED)
        KTRY_CLICK_SUB(scheduler, startB);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::enableMeridianFlip(double delay)
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    KTRY_SET_CHECKBOX_SUB(ekos->mountModule(), executeMeridianFlip, true);
    // set the delay in degrees
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), meridianFlipOffsetDegrees, 15.0 * delay / 3600.0);
    // all checks succeeded
    return true;
}


bool TestEkosMeridianFlipBase::checkMFStarted(int startDelay)
{
    // check if the meridian flip starts running
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedMeridianFlipStates, startDelay * 1000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::checkMFExecuted(int startDelay)
{
    // check if the meridian flip starts running
    KVERIFY_SUB(checkMFStarted(startDelay));
    // if dithering test required, set the dithering frequency to 1
    if (dithering_checked)
        Options::setDitherFrames(1);
    // check if the meridian flip is completed latest after one minute
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::MeridianFlipState::MOUNT_FLIP_COMPLETED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedMeridianFlipStates, 60000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::checkPostMFBehavior()
{
    // check if alignment succeeded
    if (use_aligning)
    {
        m_CaptureHelper->expectedAlignStates.enqueue(Ekos::ALIGN_COMPLETE);
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedAlignStates, 120000);
    }

    // check if guiding is running
    if (m_CaptureHelper->use_guiding)
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getGuidingStatus() == Ekos::GUIDE_GUIDING, 30000);

    // check refocusing, that should happen immediately after the guiding calibration
    KWRAP_SUB(QVERIFY(checkRefocusing()));

    // check if capturing has been started
    // dithering happen after first capture
    // otherwise it is sufficient to wait for start of capturing
    if (dithering_checked)
    {
        m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_IMAGE_RECEIVED);
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates, 60000);
    }
    else
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_CAPTURING, 60000);

    // After the first capture dithering should take place
    KWRAP_SUB(QVERIFY(checkDithering()));

    qCInfo(KSTARS_EKOS_TEST()) << "Post meridian flip steps completed.";
    return true;
}

int TestEkosMeridianFlipBase::secondsToMF()
{
    const QString flipmsg = Ekos::Manager::Instance()->mountModule()->meridianFlipStatusWidget->getStatus();

    return m_CaptureHelper->secondsToMF(flipmsg);
}

void TestEkosMeridianFlipBase::findMFTestTarget(int secsToMF, bool fast)
{
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();

    // translate seconds into fractions of hours
    double delta = static_cast<double>(secsToMF) / 3600.0;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    // upper or lower culmination?
    QFETCH(bool, culmination);
    double meridianRA = lst.Hours() + (culmination ? 0.0 : 12.0);

    // calculate a feasible declination depending to the location's latitude
    // for the upper culmination, we use an azimuth of 45 deg, for the lower culmination half way between pole and horizont
    double lat = KStarsData::Instance()->geo()->lat()->Degrees();
    target = new SkyPoint(range24(meridianRA + delta), culmination ? (lat - 45) : (90 - lat / 2));
    m_CaptureHelper->updateJ2000Coordinates(target);
    if (fast)
    {
        // reset mount model
        mount->resetModel();
        // sync to a point close before the meridian to speed up slewing
        mount->sync(range24(target->ra().Hours() + 0.002), target->dec().Degrees());
    }
}


#endif // HAVE_INDI
