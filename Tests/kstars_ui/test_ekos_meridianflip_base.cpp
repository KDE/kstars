/*
    Base class of KStars UI tests for meridian flip

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_meridianflip_base.h"

#if defined(HAVE_INDI)

#include <qdir.h>

#include "kstars_ui_tests.h"
#include "test_ekos.h"

#include "auxiliary/dms.h"
#include "ksutils.h"
#include "ekos/guide/internalguide/gmath.h"
#include "Options.h"

TestEkosMeridianFlipBase::TestEkosMeridianFlipBase(QObject *parent) : QObject(parent)
{
    m_Guider = "Internal";
}

TestEkosMeridianFlipBase::TestEkosMeridianFlipBase(QString guider, QObject *parent) : QObject(parent)
{
    m_Guider = guider;
}

bool TestEkosMeridianFlipBase::startEkosProfile()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->setupTab, 1000));

    if (m_Guider == "PHD2")
    {
        // setup the EKOS profile
        KWRAP_SUB(QVERIFY(setupEkosProfile("Simulators (PHD2)", true)));
    }
    else
        KWRAP_SUB(QVERIFY(setupEkosProfile("Simulators", false)));
    // start the profile
    KTRY_EKOS_CLICK(processINDIB);
    // wait for the devices to come up
    QTest::qWait(15000);

    // set mount defaults
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000));
    // set primary scope to my favorite FSQ-85 with QE 0.73 Reducer
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), primaryScopeFocalIN, 339.0);
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), primaryScopeApertureIN, 85.0);
    // set guide scope to a Tak 7x50
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), guideScopeFocalIN, 170.0);
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), guideScopeApertureIN, 50.0);
    // save values
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->mountModule(), saveB);

    // set focus mode defaults
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->focusModule(), 1000));
    // use full field
    KTRY_SET_CHECKBOX_SUB(ekos->focusModule(), useFullField, true);
    //initial step size 5000
    KTRY_SET_SPINBOX_SUB(ekos->focusModule(), stepIN, 5000);
    // max travel 50000
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->focusModule(), maxTravelIN, 50000.0);
    // focus tolerance 10%
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->focusModule(), toleranceIN, 10.0);
    // use polynomial algorithm
    KTRY_SET_COMBO_SUB(ekos->focusModule(), focusAlgorithmCombo, "Polynomial");

    // check if astrometry files exist
    if (! checkAstrometryFiles())
    {
        QWARN(QString("No astrometry index files found in %1").arg(KSUtils::getAstrometryDataDirs().join(", ")).toStdString().c_str());
        return false;
    }
    // switch to alignment module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->alignModule(), 1000));
    // set the CCD
    KTRY_SET_COMBO_SUB(ekos->alignModule(), CCDCaptureCombo, m_CCDDevice);
    // select the Luminance filter
    KTRY_SET_COMBO_SUB(ekos->alignModule(), FilterPosCombo, "Luminance");
    // select local solver
    ekos->alignModule()->setSolverType(Ekos::Align::SOLVER_LOCAL);
    // select internal SEP method
    Options::setSolveSextractorType(SSolver::SEXTRACTOR_BUILTIN);
    // select StellarSolver
    Options::setSolverType(SSolver::SOLVER_LOCALASTROMETRY);
    // select fast solve profile option
    Options::setSolveOptionsProfile(SSolver::Parameters::FAST_SOLVING);
    // select the "Slew to Target" mode
    KTRY_SET_RADIOBUTTON_SUB(ekos->alignModule(), slewR, true);

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000));

    // preserve guiding calibration as good as possible
    Options::setResetGuideCalibration(false);
    Options::setReuseGuideCalibration(true);

    if (m_Guider == "PHD2")
    {
        KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, externalConnectB);
        // ensure that PHD2 is connected
        if (externalConnectB->isEnabled()) {
            // as soon as PHD2 is connected, it reports the idle state
            expectedGuidingStates.enqueue(Ekos::GUIDE_IDLE);
            // click "Connect"
            KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), externalConnectB);
            // wait max 60 sec that PHD2 is connected (sometimes INDI connections hang)
            KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedGuidingStates, 60000);
            qCInfo(KSTARS_EKOS_TEST) << "PHD2 connected successfully.";
        }
    }
    else
    {
        // select smart, multi-star seems to have problems with the simulator
        Options::setGuideAlgorithm(SMART_THRESHOLD);
        // auto star select
        KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->guideModule(), autoStarCheck, true);
        // set the guide star box to size 64
        KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), boxSizeCombo, "64");
        // Set the guiding and ST4 device
        KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), guiderCombo, m_GuiderDevice);
        KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), ST4Combo, m_GuiderDevice);
        // select primary scope (higher focal length seems better for the guiding simulator)
        KTRY_SET_COMBO_INDEX_SUB(Ekos::Manager::Instance()->guideModule(), FOVScopeCombo, ISD::CCD::TELESCOPE_PRIMARY);
    }

    // connect to the alignment process to receive align status changes
    connect(ekos->alignModule(), &Ekos::Align::newStatus, this, &TestEkosMeridianFlipBase::alignStatusChanged,
            Qt::UniqueConnection);

    // connect to the mount process to receive meridian flip status changes
    connect(ekos->mountModule(), &Ekos::Mount::newMeridianFlipStatus, this,
            &TestEkosMeridianFlipBase::meridianFlipStatusChanged, Qt::UniqueConnection);

    // connect to the guiding process to receive guiding status changes
    connect(ekos->guideModule(), &Ekos::Guide::newStatus, this, &TestEkosMeridianFlipBase::guidingStatusChanged,
            Qt::UniqueConnection);

    // connect to the capture process to receive capture status changes
    connect(ekos->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosMeridianFlipBase::captureStatusChanged,
            Qt::UniqueConnection);

    // connect to the focus process to receive focus status changes
    connect(ekos->focusModule(), &Ekos::Focus::newStatus, this, &TestEkosMeridianFlipBase::focusStatusChanged,
            Qt::UniqueConnection);
    // Everything completed successfully
    return true;
}

void TestEkosMeridianFlipBase::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    if (m_Guider == "PHD2")
    {
        // Start a parallel PHD2 instance
        preparePHD2();
        startPHD2();
    }

    // start the profile (depending on the selected guider)
    QVERIFY(startEkosProfile());
}

bool TestEkosMeridianFlipBase::shutdownEkosProfile(QString guider)
{
    // disconnect to the focus process to receive focus status changes
    disconnect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newStatus, this, &TestEkosMeridianFlipBase::focusStatusChanged);
    // disconnect the guiding process to receive the current guiding status
    disconnect(Ekos::Manager::Instance()->guideModule(), &Ekos::Guide::newStatus, this,
               &TestEkosMeridianFlipBase::guidingStatusChanged);
    // disconnect the mount process to receive meridian flip status changes
    disconnect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newMeridianFlipStatus, this,
               &TestEkosMeridianFlipBase::meridianFlipStatusChanged);
    // disconnect to the capture process to receive capture status changes
    disconnect(Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::newStatus, this, &TestEkosMeridianFlipBase::captureStatusChanged);
    // disconnect to the alignment process to receive align status changes
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this, &TestEkosMeridianFlipBase::alignStatusChanged);

    if (guider == "PHD2")
    {
        KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, externalDisconnectB);
        // ensure that PHD2 is connected
        if (externalDisconnectB->isEnabled()) {
            // click "Disconnect"
            KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), externalDisconnectB);
            // Stop PHD2
            stopPHD2();
        }
    }

    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ...";
    KWRAP_SUB(KTRY_EKOS_STOP_SIMULATORS());
    qCInfo(KSTARS_EKOS_TEST) << "Stopping profile ... (done)";
    // Everything completed successfully
    return true;
}

void TestEkosMeridianFlipBase::cleanupTestCase()
{
    QVERIFY(shutdownEkosProfile(m_Guider));
    if (m_Guider == "PHD2")
        cleanupPHD2();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosMeridianFlipBase::init()
{
    // initialize the recorded states
    m_AlignStatus   = Ekos::ALIGN_IDLE;
    m_CaptureStatus = Ekos::CAPTURE_IDLE;
    m_FocusStatus   = Ekos::FOCUS_IDLE;
    m_GuideStatus   = Ekos::GUIDE_IDLE;
    m_MFStatus      = Ekos::Mount::FLIP_NONE;

    // clear the queues
    expectedAlignStates.clear();
    expectedCaptureStates.clear();
    expectedFocusStates.clear();
    expectedGuidingStates.clear();
    expectedMeridianFlipStates.clear();

    // switch to mount module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule(), 1000);

    // activate meridian flip
    QVERIFY(enableMeridianFlip(0.0));

    // expected beginning of the meridian flip
    expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_PLANNED);
    expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_RUNNING);
}

void TestEkosMeridianFlipBase::cleanup()
{
    // ensure that capturing, focusing and guiding is stopped
    QVERIFY(stopFocusing());
    QVERIFY(stopAligning());
    QVERIFY(stopCapturing());
    QVERIFY(stopGuiding());

    // clean up capture module
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_GADGET(Ekos::Manager::Instance()->captureModule(), QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);
}



/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */

void TestEkosMeridianFlipBase::prepareTestData(QList<QString> filterList, QList<bool> focusList, QList<bool> autofocusList, QList<bool> ditherList)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
QSKIP("Bypassing fixture test on old Qt");
Q_UNUSED(filterList)
Q_UNUSED(focusList)
Q_UNUSED(autofocusList)
Q_UNUSED(ditherList)
#else
    QTest::addColumn<int>("count");       /*!< frame counts to capture */
    QTest::addColumn<double>("exptime");  /*!< exposure time of a single frame */
    QTest::addColumn<QString>("filters"); /*!< list of filters for the capture sequence */
    QTest::addColumn<bool>("focus");      /*!< refocus every minute */
    QTest::addColumn<bool>("autofocus");  /*!< refocus on HFR change */
    QTest::addColumn<bool>("dither");     /*!< execute dithering after each capture */

    for (bool focus: focusList)
        for (bool autofocus: autofocusList)
            for (bool dither: ditherList)
                for(QString filter:  filterList)
                {
                    int count = 6 / (QString(filter).count(",")+1);
                    // both focus==true && autofocus==true does not make sense
                    if (focus==false || autofocus==false)
                        QTest::newRow(QString("%6: %1x%2, foc=%3, af=%4, di=%5").arg(count).arg(filter).arg(focus?"yes":"no").arg(autofocus?"yes":"no").arg(dither?"yes":"no").arg(m_Guider).toLocal8Bit()) << count << 18.0 << filter << focus << autofocus << dither;
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
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();

    // translate seconds into fractions of hours
    double delta = static_cast<double>(secsToMF) / 3600.0;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    if (fast)
    {
        // reset mount model
        Ekos::Manager::Instance()->mountModule()->resetModel();
        // sync to a point close before the meridian to speed up slewing
        mount->sync(lst.Hours()+ delta + 0.002, 21.0);
    }
    // now slew very close before the meridian
    mount->slew(lst.Hours() + delta, 21.0);
    // wait a certain time until the mount slews
    QTest::qWait(3000);
    // wait until the mount is tracking
    KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 10000);
    // check whether a meridian flip is announced
    KTRY_GADGET_SUB(mount, QLabel, meridianFlipStatusText);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(meridianFlipStatusText->text().length() > 0, 20000);
    KTRY_VERIFY_TEXTFIELD_STARTS_WITH_TIMEOUT_SUB(meridianFlipStatusText, "Meridian flip in", 20000);
    // all checks succeeded
    return true;
}


bool TestEkosMeridianFlipBase::prepareCaptureTestcase(int secsToMF, bool calibrate, bool initialFocus)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
QSKIP("Bypassing fixture test on old Qt");
Q_UNUSED(secsToMF)
Q_UNUSED(calibrate)
Q_UNUSED(initialFocus)
#else
    // switch to capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // create the capture sequences
    QTemporaryDir destination;
    KWRAP_SUB(QVERIFY(destination.isValid()));
    KWRAP_SUB(QVERIFY(destination.autoRemove()));
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " <<destination.path();

    QFETCH(int, count);
    QFETCH(double, exptime);
    QFETCH(QString, filters);

    KWRAP_SUB(foreach(QString filter, filters.split(",")) KTRY_CAPTURE_ADD_LIGHT(exptime, count, 0, filter, destination.path()));

    // set refocus to 1 min and select due to test data set
    QFETCH(bool, focus);
    refocus_checked = focus;
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), limitRefocusS, focus);
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->captureModule(), limitRefocusN, 1);

    QFETCH(bool, autofocus);
    autofocus_checked = autofocus;
    // set HFR autofocus
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), limitFocusHFRS, autofocus);
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->captureModule(), limitFocusHFRN, 0.1);

    // set dithering due to test data set
    QFETCH(bool, dither);
    dithering_checked = dither;
    Options::setDitherEnabled(dither);

    if (calibrate)
    {
        // slew roughly to the position to calibrate first
        KWRAP_SUB(QVERIFY(positionMountForMF(600, true)));
        qCInfo(KSTARS_EKOS_TEST) << "Slewed roughly to the position to calibrate first.";
        // calibrate guiding
        if (! startGuiding(2.0)) return false;
        if (! stopGuiding()) return false;
    }

    // focus upfront if necessary to have a reliable delay
    if (initialFocus && (refocus_checked || autofocus_checked))
        if (! startFocusing()) return false;

    // slew close to the meridian
    KWRAP_SUB(QVERIFY(positionMountForMF(refocus_checked?std::max(secsToMF, 30):secsToMF, !calibrate)));
    qCInfo(KSTARS_EKOS_TEST) << "Slewed close to the meridian.";
#endif
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::checkAstrometryFiles()
{
    // first check the current configuration
    QStringList dataDirs = KSUtils::getAstrometryDataDirs();

    // search if configured directories contain some index files
    for(QString dirname: dataDirs)
    {
        QDir dir(dirname);
        dir.setNameFilters(QStringList("index*"));
        dir.setFilter(QDir::Files);
        if (! dir.entryList().isEmpty())
            return true;
    }
    return false;
}


bool TestEkosMeridianFlipBase::checkDithering()
{
    // if dithering is not selected, to nothing
    if (! dithering_checked)
        return true;

    // check if dithering took place
    expectedGuidingStates.enqueue(Ekos::GUIDE_DITHERING_SUCCESS);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedGuidingStates, 60000);
    // all checks succeeded
    return true;
}


bool TestEkosMeridianFlipBase::checkRefocusing(bool guiding)
{
    if (refocus_checked || autofocus_checked)
    {
        expectedFocusStates.enqueue(Ekos::FOCUS_COMPLETE);
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedFocusStates, 60000);
    }
    if (guiding)
        KTRY_VERIFY_WITH_TIMEOUT_SUB(getGuidingStatus() == Ekos::GUIDE_GUIDING, 20000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startAligning(double expTime)
{
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000));
    // set the exposure time to the given value
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), exposureIN, expTime);
    // reduce the accuracy to avoid testing problems
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), accuracySpin, 300);

    // start alignment
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->alignModule(), QPushButton, solveB);
    // ensure that the guiding button is enabled (after MF it may take a while)
    KTRY_VERIFY_WITH_TIMEOUT_SUB(solveB->isEnabled(), 10000);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), solveB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getAlignStatus() == Ekos::ALIGN_COMPLETE, 60000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopAligning()
{
    // check whether alignment is already stopped
    if (getAlignStatus() == Ekos::ALIGN_IDLE || getAlignStatus() == Ekos::ALIGN_ABORTED ||
            getAlignStatus() == Ekos::ALIGN_FAILED || getAlignStatus() == Ekos::ALIGN_COMPLETE)
        return true;

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000));

    // stop alignment
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->alignModule(), QPushButton, stopB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), stopB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getAlignStatus() == Ekos::ALIGN_IDLE || getAlignStatus() == Ekos::ALIGN_ABORTED ||
                                 getAlignStatus() == Ekos::ALIGN_FAILED || getAlignStatus() == Ekos::ALIGN_COMPLETE, 5000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startCapturing()
{
    // Now we can estimate how many captures will happen before the meridian flip.
    int t2mf = secondsToMF();
    if (t2mf > 0)
    {
        // Ensure that the first autofocus event takes place after the flip
        if (autofocus_checked)
            Options::setInSequenceCheckFrames(static_cast<uint>(1 + t2mf / 18));

        // Ensure that dithering takes place after the flip
        if (dithering_checked)
            Options::setDitherFrames(static_cast<uint>(1 + t2mf / 18));
    }

    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // check if capture is in a stopped state
    KWRAP_SUB(QVERIFY(getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED || getCaptureStatus() == Ekos::CAPTURE_COMPLETE));

    // press start
    expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedCaptureStates, 5000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopCapturing()
{
    // check if capture is in a stopped state
    if (getCaptureStatus() == Ekos::CAPTURE_IDLE || getCaptureStatus() == Ekos::CAPTURE_ABORTED ||
            getCaptureStatus() == Ekos::CAPTURE_COMPLETE)
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

bool TestEkosMeridianFlipBase::startFocusing()
{
    // check whether focusing is already running
    if (! (getFocusStatus() == Ekos::FOCUS_IDLE || getFocusStatus() == Ekos::FOCUS_COMPLETE || getFocusStatus() == Ekos::FOCUS_ABORTED))
        return true;

    // switch to focus module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule(), 1000));
    // select the Luminance filter
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->focusModule(), FilterPosCombo, "Luminance");
    // exp time 5sec
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->focusModule(), exposureIN, 5.0);
    // absolute position 40000
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->focusModule(), absTicksSpin, 40000);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule(), startGotoB);
    // wait one second for settling
    QTest::qWait(1000);
    // start focusing
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule(), startFocusB);
    // wait for successful completion
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getFocusStatus() == Ekos::FOCUS_COMPLETE, 90000);
    qCInfo(KSTARS_EKOS_TEST) << "Focusing finished.";
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopFocusing()
{
    // check whether focusing is already stopped
    if (getFocusStatus() == Ekos::FOCUS_IDLE || getFocusStatus() == Ekos::FOCUS_COMPLETE || getFocusStatus() == Ekos::FOCUS_ABORTED)
        return true;

    // switch to focus module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule(), 1000));
    // stop guiding
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->focusModule(), QPushButton, stopFocusB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule(), stopFocusB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getFocusStatus() == Ekos::FOCUS_IDLE || getFocusStatus() == Ekos::FOCUS_COMPLETE ||
                                 getFocusStatus() == Ekos::FOCUS_ABORTED, 15000);

    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startGuiding(double expTime)
{
    if (getGuidingStatus() == Ekos::GUIDE_GUIDING)
    {
        QWARN("Start guiding ignored, guiding already running!");
        return false;
    }

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000));
    //set the exposure time
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->guideModule(), exposureIN, expTime);

    // start guiding
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, guideB);
    // ensure that the guiding button is enabled (after MF it may take a while)
    KTRY_VERIFY_WITH_TIMEOUT_SUB(guideB->isEnabled(), 10000);
    expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), guideB);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedGuidingStates, 120000);
    qCInfo(KSTARS_EKOS_TEST) << "Guiding started.";
    qCInfo(KSTARS_EKOS_TEST) << "Waiting 2sec for settle guiding ...";
    QTest::qWait(2000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopGuiding()
{
    // check whether guiding is already stopped
    if (getGuidingStatus() == Ekos::GUIDE_IDLE || getGuidingStatus() == Ekos::GUIDE_ABORTED)
        return true;

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000));

    // stop guiding
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, stopB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), stopB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(getGuidingStatus() == Ekos::GUIDE_IDLE || getGuidingStatus() == Ekos::GUIDE_ABORTED, 5000);
    qCInfo(KSTARS_EKOS_TEST) << "Guiding stopped.";
    qCInfo(KSTARS_EKOS_TEST) << "Waiting 2sec for settle guiding stop..."; // Avoid overlapping with focus pausing
    QTest::qWait(2000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::enableMeridianFlip(double delay)
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    KTRY_SET_CHECKBOX_SUB(ekos->mountModule(), meridianFlipCheckBox, true);
    // set the delay value to degrees
    KTRY_SET_RADIOBUTTON_SUB(ekos->mountModule(), meridianFlipDegreesR, true);
    // set the delay in degrees
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), meridianFlipTimeBox, 15.0*delay/3600.0);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::checkMFExecuted(int startDelay)
{
    // check if the meridian flip starts running
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedMeridianFlipStates, startDelay*1000);
    // if dithering test required, set the dithering frequency to 1
    if (dithering_checked)
        Options::setDitherFrames(1);
    // check if the meridian flip is completed latest after one minute
    expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_COMPLETED);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedMeridianFlipStates, 60000);
    // all checks succeeded
    return true;
}

int TestEkosMeridianFlipBase::secondsToMF()
{
    const QString flipmsg = Ekos::Manager::Instance()->mountModule()->meridianFlipStatusText->text();

    QRegExp mfPattern("(\\d+)h:(\\d+)m:(\\d+)s");

    int pos = mfPattern.indexIn(flipmsg);
    if (pos > -1)
    {
        int hh  = mfPattern.cap(1).toInt();
        int mm  = mfPattern.cap(2).toInt();
        int sec = mfPattern.cap(3).toInt();
        if (hh >= 0)
            return (((hh*60) + mm)*60 + sec);
        else
            return (((hh*60) - mm)*60 - sec);
    }

    // unknown time
    return -1;
}

void TestEkosMeridianFlipBase::fillProfile(bool *isDone)
{
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: starting...";
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    // Select the mount device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, mountCombo);
    setTreeviewCombo(mountCombo, m_MountDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Mount selected.";

    // Selet the CCD device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, ccdCombo);
    setTreeviewCombo(ccdCombo, m_CCDDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: CCD selected.";

    // Select the focuser device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, focuserCombo);
    setTreeviewCombo(focuserCombo, m_FocuserDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Focuser selected.";

    // Select the guider device
    KTRY_PROFILEEDITOR_GADGET(QComboBox, guiderCombo);
    setTreeviewCombo(guiderCombo, m_GuiderDevice);
    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Guider selected.";

    // wait a short time to make the setup visible
    QTest::qWait(1000);
    // Save the profile using the "Save" button
    QDialogButtonBox* buttons = profileEditor->findChild<QDialogButtonBox*>("dialogButtons");
    QVERIFY(nullptr != buttons);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);

    qCInfo(KSTARS_EKOS_TEST) << "Fill profile: Selections saved.";

    *isDone = true;
}

void TestEkosMeridianFlipBase::createEkosProfile(QString name, bool isPHD2, bool *isDone)
{
    ProfileEditor* profileEditor = Ekos::Manager::Instance()->findChild<ProfileEditor*>("profileEditorDialog");

    // Set the profile name
    KTRY_SET_LINEEDIT(profileEditor, profileIN, name);
    // select the guider type
    KTRY_SET_COMBO(profileEditor, guideTypeCombo, isPHD2 ? "PHD2" : "Internal");
    if (isPHD2)
    {
        // Write PHD2 server specs
        KTRY_SET_LINEEDIT(profileEditor, externalGuideHost, "localhost");
        KTRY_SET_LINEEDIT(profileEditor, externalGuidePort, "4400");
    }

    qCInfo(KSTARS_EKOS_TEST) << "Ekos profile " << name << " created.";
    // and now continue with filling the profile
    fillProfile(isDone);
}

bool TestEkosMeridianFlipBase::setupEkosProfile(QString name, bool isPHD2)
{
    qCInfo(KSTARS_EKOS_TEST) << "Setting up Ekos profile...";
    bool isDone = false;
    Ekos::Manager * const ekos = Ekos::Manager::Instance();
    // check if the profile with the given name exists
    KTRY_GADGET_SUB(ekos, QComboBox, profileCombo);
    if (profileCombo->findText(name) >= 0)
    {
        KTRY_GADGET_SUB(ekos, QPushButton, editProfileB);

        // edit Simulators profile
        KWRAP_SUB(KTRY_EKOS_SELECT_PROFILE(name));

        // edit only editable profiles
        if (editProfileB->isEnabled())
        {
            // start with a delay of 1 sec a new thread that edits the profile
            QTimer::singleShot(1000, ekos, [&]{fillProfile(&isDone);});
            KTRY_CLICK_SUB(ekos, editProfileB);
        }
        else
        {
            qCInfo(KSTARS_EKOS_TEST) << "Profile " << name << " not editable, setup finished.";
            isDone = true;
            return true;
        }
    }
    else
    {
        // start with a delay of 1 sec a new thread that edits the profile
        qCInfo(KSTARS_EKOS_TEST) << "Creating new profile " << name << " ...";
        QTimer::singleShot(1000, ekos, [&]{createEkosProfile(name, isPHD2, &isDone);});
        // create new profile addProfileB
        KTRY_CLICK_SUB(ekos, addProfileB);
    }


    // Cancel the profile editor if test did not close the editor dialog within 10 sec
    QTimer * closeDialog = new QTimer(this);
    closeDialog->setSingleShot(true);
    closeDialog->setInterval(10000);
    ekos->connect(closeDialog, &QTimer::timeout, [&]
    {
        ProfileEditor* profileEditor = ekos->findChild<ProfileEditor*>("profileEditorDialog");
        if (profileEditor != nullptr)
        {
            profileEditor->reject();
            qWarning(KSTARS_EKOS_TEST) << "Editing profile aborted.";
        }
    });


    // Click handler returned, stop the timer closing the dialog on failure
    closeDialog->stop();
    delete closeDialog;

    // Verification of the first test step
    return isDone;

}


void TestEkosMeridianFlipBase::setTreeviewCombo(QComboBox *combo, QString lookup)
{
    // Match the text recursively in the model, this results in a model index with a parent
    QModelIndexList const list = combo->model()->match(combo->model()->index(0, 0), Qt::DisplayRole, QVariant::fromValue(lookup), 1, Qt::MatchRecursive);
    QVERIFY(0 < list.count());
    QModelIndex const &index = list.first();
    QCOMPARE(list.value(0).data().toString(), lookup);
    QVERIFY(!index.parent().parent().isValid());
    // Now set the combobox model root to the match's parent
    combo->setRootModelIndex(index.parent());
    combo->setModelColumn(index.column());
    combo->setCurrentIndex(index.row());

    // Now reset
    combo->setRootModelIndex(QModelIndex());
    combo->view()->setCurrentIndex(index);

    // Check, if everything went well
    QCOMPARE(combo->currentText(), lookup);
}

void TestEkosMeridianFlipBase::preparePHD2()
{
    QString const phd2_config = ".PHDGuidingV2";
    QString const phd2_config_bak = ".PHDGuidingV2.bak";
    QString const phd2_config_orig = ".PHDGuidingV2_mf";
    QStandardPaths::enableTestMode(false);
    QFileInfo phd2_config_home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config);
    QFileInfo phd2_config_home_bak(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config_bak);
    QStandardPaths::enableTestMode(true);
    QWARN(QString("Writing PHD configuration file to '%1'").arg(phd2_config_home.filePath()).toStdString().c_str());
    if (phd2_config_home.exists())
    {
        // remove existing backup file
        if (phd2_config_home_bak.exists())
            QVERIFY(QFile::remove(phd2_config_home_bak.filePath()));
        // rename existing file to backup file
        QVERIFY(QFile::rename(phd2_config_home.filePath(), phd2_config_home_bak.filePath()));
    }
    QVERIFY(QFile::copy(phd2_config_orig, phd2_config_home.filePath()));
}

void TestEkosMeridianFlipBase::startPHD2()
{
    phd2 = new QProcess(this);

    // Start PHD2 with the proper configuration
    phd2->start(QString("phd2"));
    QVERIFY(phd2->waitForStarted(3000));
    QTest::qWait(2000);
    QTRY_VERIFY_WITH_TIMEOUT(phd2->state() == QProcess::Running, 1000);

    // Try to connect to the PHD2 server
    QTcpSocket phd2_server(this);
    phd2_server.connectToHost(phd2_guider_host, phd2_guider_port.toUInt(), QIODevice::ReadOnly, QAbstractSocket::IPv4Protocol);
    if(!phd2_server.waitForConnected(5000))
    {
        QWARN(QString("Cannot continue, PHD2 server is unavailable (%1)").arg(phd2_server.errorString()).toStdString().c_str());
        return;
    }
    phd2_server.disconnectFromHost();
    if (phd2_server.state() == QTcpSocket::ConnectedState)
        QVERIFY(phd2_server.waitForDisconnected(1000));
}

void TestEkosMeridianFlipBase::cleanupPHD2()
{
    QString const phd2_config = ".PHDGuidingV2";
    QString const phd2_config_bak = ".PHDGuidingV2.bak";
    QStandardPaths::enableTestMode(false);
    QFileInfo phd2_config_home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config);
    QFileInfo phd2_config_home_bak(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), phd2_config_bak);
    // remove PHD2 test config
    if (phd2_config_home.exists())
        QVERIFY(QFile::remove(phd2_config_home.filePath()));
    // restore the backup
    if (phd2_config_home_bak.exists())
        QVERIFY(QFile::rename(phd2_config_home_bak.filePath(), phd2_config_home.filePath()));
}

void TestEkosMeridianFlipBase::stopPHD2()
{
    phd2->terminate();
    QVERIFY(phd2->waitForFinished(5000));
}




/* *********************************************************************************
 *
 * Slots for catching state changes
 *
 * ********************************************************************************* */

void TestEkosMeridianFlipBase::alignStatusChanged(Ekos::AlignState status)
{
    m_AlignStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedAlignStates.isEmpty() && expectedAlignStates.head() == status)
        expectedAlignStates.dequeue();
}

void TestEkosMeridianFlipBase::meridianFlipStatusChanged(Ekos::Mount::MeridianFlipStatus status)
{
    m_MFStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedMeridianFlipStates.isEmpty() && expectedMeridianFlipStates.head() == status)
        expectedMeridianFlipStates.dequeue();
}

void TestEkosMeridianFlipBase::focusStatusChanged(Ekos::FocusState status)
{
    m_FocusStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedFocusStates.isEmpty() && expectedFocusStates.head() == status)
        expectedFocusStates.dequeue();
}

void TestEkosMeridianFlipBase::guidingStatusChanged(Ekos::GuideState status)
{
    m_GuideStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedGuidingStates.isEmpty() && expectedGuidingStates.head() == status)
        expectedGuidingStates.dequeue();
}

void TestEkosMeridianFlipBase::captureStatusChanged(Ekos::CaptureState status) {
    m_CaptureStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedCaptureStates.isEmpty() && expectedCaptureStates.head() == status)
        expectedCaptureStates.dequeue();
}

#endif // HAVE_INDI
