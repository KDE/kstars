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
#include "indi/guimanager.h"
#include "ekos/guide/internalguide/gmath.h"
#include "Options.h"

TestEkosMeridianFlipBase::TestEkosMeridianFlipBase(QObject *parent) : TestEkosMeridianFlipBase::TestEkosMeridianFlipBase("Internal", parent){}

TestEkosMeridianFlipBase::TestEkosMeridianFlipBase(QString guider, QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper(guider);
    m_CaptureHelper->m_FocuserDevice = "Focuser Simulator";
}

bool TestEkosMeridianFlipBase::startEkosProfile()
{
    // use the helper to start the profile
    m_CaptureHelper->startEkosProfile();

    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // set mount defaults
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000));
    KWRAP_SUB(QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->slewStatus() != IPState::IPS_ALERT, 1000));
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
    // focus tolerance 20%
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->focusModule(), toleranceIN, 20.0);
    // use polynomial algorithm
    KTRY_SET_COMBO_SUB(ekos->focusModule(), focusAlgorithmCombo, "Polynomial");

    // check if astrometry files exist
    if (! checkAstrometryFiles())
    {
        QWARN(QString("No astrometry index files found in %1").arg(
                  KSUtils::getAstrometryDataDirs().join(", ")).toStdString().c_str());
        astrometry_available = false;
    }
    // switch to alignment module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->alignModule(), 1000));
    // set the CCD
    KTRY_SET_COMBO_SUB(ekos->alignModule(), CCDCaptureCombo, m_CaptureHelper->m_CCDDevice);
    // select the Luminance filter
    KTRY_SET_COMBO_SUB(ekos->alignModule(), FilterPosCombo, "Luminance");
    // select local solver
    ekos->alignModule()->setSolverMode(Ekos::Align::SOLVER_LOCAL);
    // select internal SEP method
    Options::setSolveSextractorType(SSolver::EXTRACTOR_BUILTIN);
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

    if (m_CaptureHelper->m_Guider == "PHD2")
    {
        KTRY_GADGET_SUB(Ekos::Manager::Instance()->guideModule(), QPushButton, externalConnectB);
        // ensure that PHD2 is connected
        if (externalConnectB->isEnabled())
        {
            // click "Connect"
            KTRY_CLICK_SUB(Ekos::Manager::Instance()->guideModule(), externalConnectB);
            // wait max 60 sec that PHD2 is connected (sometimes INDI connections hang)
            KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->guideModule()->status() == Ekos::GUIDE_CONNECTED, 60000);
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
        KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), guiderCombo, m_CaptureHelper->m_GuiderDevice);
        KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), ST4Combo, m_CaptureHelper->m_GuiderDevice);
        // select primary scope (higher focal length seems better for the guiding simulator)
        KTRY_SET_COMBO_INDEX_SUB(Ekos::Manager::Instance()->guideModule(), FOVScopeCombo, ISD::CCD::TELESCOPE_PRIMARY);
    }

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
    KMessageBox::saveDontShowAgainYesNo("astronomical_twilight_warning", KMessageBox::ButtonCode::No);
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
    m_CaptureHelper->init();

    // disable by default
    refocus_checked   = false;
    autofocus_checked = false;
    use_aligning      = false;
    dithering_checked = false;

    // clear the queues
    m_CaptureHelper->expectedAlignStates.clear();
    m_CaptureHelper->expectedCaptureStates.clear();
    m_CaptureHelper->expectedFocusStates.clear();
    m_CaptureHelper->expectedGuidingStates.clear();
    m_CaptureHelper->expectedMeridianFlipStates.clear();

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

    // initialize the capture helper
    m_CaptureHelper->init();

    // Reduce the noise setting to ensure a working focusing and plate solving
    KTRY_INDI_PROPERTY("CCD Simulator", "Simulator Config", "SIMULATOR_SETTINGS", ccd_settings);
    INDI_E *noise_setting = ccd_settings->getElement("SIM_NOISE");
    QVERIFY(ccd_settings != nullptr);
    noise_setting->setValue(2.0);
    ccd_settings->processSetButton();

    // close INDI window
    GUIManager::Instance()->close();

    // switch to mount module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule(), 1000);

    // activate meridian flip
    QVERIFY(enableMeridianFlip(0.0));

    // expected beginning of the meridian flip
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_PLANNED);
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_RUNNING);
}

void TestEkosMeridianFlipBase::cleanup()
{
    // ensure that capturing, focusing and guiding is stopped
    QVERIFY(stopScheduler());
    QVERIFY(stopFocusing());
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

void TestEkosMeridianFlipBase::prepareTestData(double exptime, QList<QString> locationList, QList<bool> culminationList, QList<QString> filterList,
                                               QList<bool> focusList, QList<bool> autofocusList, QList<bool> guideList, QList<bool> ditherList)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(exptime)
    Q_UNUSED(locationList)
    Q_UNUSED(culminationList)
    Q_UNUSED(filterList)
    Q_UNUSED(focusList)
    Q_UNUSED(autofocusList)
    Q_UNUSED(guideList)
    Q_UNUSED(ditherList)
#else
    QTest::addColumn<QString>("location"); /*!< locations the KStars test is running for */
    QTest::addColumn<bool>("culmination"); /*!< upper culmination? */
    QTest::addColumn<int>("count");        /*!< frame counts to capture */
    QTest::addColumn<double>("exptime");   /*!< exposure time of a single frame */
    QTest::addColumn<QString>("filters");  /*!< list of filters for the capture sequence */
    QTest::addColumn<bool>("focus");       /*!< refocus every minute */
    QTest::addColumn<bool>("autofocus");   /*!< refocus on HFR change */
    QTest::addColumn<bool>("guide");       /*!< use guiding */
    QTest::addColumn<bool>("dither");      /*!< execute dithering after each capture */

    KStarsData * const d = KStars::Instance()->data();
    QVERIFY(d != nullptr);

    for (QString location : locationList)
        for (bool culmination : culminationList)
            for (bool focus : focusList)
                for (bool autofocus : autofocusList)
                    for (bool guide : guideList)
                        for (bool dither : ditherList)
                            for(QString filter :  filterList)
                            {
                                int count = 6 / (QString(filter).count(",") + 1);
                                // both focus==true && autofocus==true does not make sense
                                // same with guide==false && dither==true
                                if ((focus == false || autofocus == false) && (guide == true || dither == false))
                                    QTest::newRow(QString("%7: culm=%8, %1x%2, foc=%3, af=%4, di=%5, guider=%6").arg(count).arg(filter)
                                                  .arg(focus ? "yes" : "no").arg(autofocus ? "yes" : "no").arg(dither ? "yes" : "no")
                                                  .arg(guide ? m_CaptureHelper->m_Guider : "none").arg(location).arg(culmination ? "up" : "low").toLocal8Bit())
                                            << location << culmination << count << exptime << filter << focus << autofocus << guide << dither;
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
    Q_UNUSED(calibrate)
    Q_UNUSED(initialFocus)
    Q_UNUSED(guideDeviation)
#else
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
    target = new SkyPoint(range24(meridianRA + delta), culmination ? (lat-45) : (90-lat/2));

    if (fast)
    {
        // reset mount model
        Ekos::Manager::Instance()->mountModule()->resetModel();
        // sync to a point close before the meridian to speed up slewing
        mount->sync(range24(target->ra().Hours() + 0.002), target->dec().Degrees());
    }
    // now slew very close before the meridian
    mount->slew(target->ra().Hours(), target->dec().Degrees());
    // wait a certain time until the mount slews
    QTest::qWait(3000);
    // wait until the mount is tracking
    KTRY_VERIFY_WITH_TIMEOUT_SUB(Ekos::Manager::Instance()->mountModule()->status() == ISD::Telescope::MOUNT_TRACKING, 15000);
    // check whether a meridian flip is announced
    KTRY_GADGET_SUB(mount, QLabel, meridianFlipStatusText);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(meridianFlipStatusText->text().length() > 0, 20000);
    KTRY_VERIFY_TEXTFIELD_STARTS_WITH_TIMEOUT_SUB(meridianFlipStatusText, "Meridian flip in", 20000);
#endif
    // all checks succeeded
    return true;
}


bool TestEkosMeridianFlipBase::prepareCaptureTestcase(int secsToMF, bool initialFocus, bool guideDeviation)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(secsToMF)
    Q_UNUSED(initialFocus)
    Q_UNUSED(guideDeviation)
#else
    // switch to capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // create the capture sequences
    QTemporaryDir destination;
    KWRAP_SUB(QVERIFY(destination.isValid()));
    KWRAP_SUB(QVERIFY(destination.autoRemove()));
    qCInfo(KSTARS_EKOS_TEST) << "FITS path: " << destination.path();

    QFETCH(int, count);
    QFETCH(double, exptime);
    QFETCH(QString, filters);

    KWRAP_SUB(foreach(QString filter, filters.split(",")) KTRY_CAPTURE_ADD_LIGHT(exptime, count, 0, filter,
              destination.path()));

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

    // set guide deviation guard
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->captureModule(), limitGuideDeviationS, guideDeviation);

    QFETCH(bool, guide);
    if (guide)
    {
        // slew roughly to the position to calibrate first
        KWRAP_SUB(QVERIFY(positionMountForMF(600, true)));
        qCInfo(KSTARS_EKOS_TEST) << "Slewed roughly to the position to calibrate first.";
        // calibrate guiding
        if (! m_CaptureHelper->startGuiding(2.0)) return false;
        if (! m_CaptureHelper->stopGuiding()) return false;
    }

    // focus upfront if necessary to have a reliable delay
    if (initialFocus && (refocus_checked || autofocus_checked))
        if (! startFocusing()) return false;

    // slew close to the meridian
    KWRAP_SUB(QVERIFY(positionMountForMF(refocus_checked ? std::max(secsToMF, 30) : secsToMF, !guide)));
    qCInfo(KSTARS_EKOS_TEST) << "Slewed close to the meridian.";
#endif
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::prepareSchedulerTestcase(int secsToMF, bool useFocus,
                                                        SchedulerJob::CompletionCondition completionCondition, int iterations)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(secsToMF)
    Q_UNUSED(useFocus)
    Q_UNUSED(completionCondition)
    Q_UNUSED(iterations)
#else
    // execute all similar preparation steps for a capture test case
    KVERIFY_SUB(prepareCaptureTestcase(secsToMF, useFocus, false));

    // save current capture sequence to Ekos sequence file
    QString sequenceFile = m_CaptureHelper->destination->filePath("test.esq");
    qCInfo(KSTARS_EKOS_TEST) << "Sequence file" << sequenceFile << "created.";
    KVERIFY_SUB(Ekos::Manager::Instance()->captureModule()->saveSequenceQueue(sequenceFile));

    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    // set sequence file
    scheduler->setSequence(sequenceFile);
    // set Kocab as target
    KTRY_SET_LINEEDIT_SUB(scheduler, nameEdit, "test");
    KTRY_SET_LINEEDIT_SUB(scheduler, raBox, target->ra0().toHMSString());
    KTRY_SET_LINEEDIT_SUB(scheduler, decBox, target->dec0().toDMSString());
    // disable all step checks
    QFETCH(bool, guide);
    KTRY_SET_CHECKBOX_SUB(scheduler, trackStepCheck, true);
    KTRY_SET_CHECKBOX_SUB(scheduler, focusStepCheck, false); // initial focusing during capture preparation
    KTRY_SET_CHECKBOX_SUB(scheduler, alignStepCheck, false);
    KTRY_SET_CHECKBOX_SUB(scheduler, guideStepCheck, guide);
    // ignore twilight
    KTRY_SET_CHECKBOX_SUB(scheduler, twilightCheck, false);
    // disable remember job progress
    Options::setRememberJobProgress(false);
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
    }
    // add scheduler job
    KTRY_CLICK_SUB(scheduler, addToQueueB);
#endif
    // preparation successful
    return true;
}

bool TestEkosMeridianFlipBase::checkAstrometryFiles()
{
    // first check the current configuration
    QStringList dataDirs = KSUtils::getAstrometryDataDirs();

    // search if configured directories contain some index files
    for(QString dirname : dataDirs)
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
    m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_DITHERING_SUCCESS);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedGuidingStates, 60000);
    // all checks succeeded
    return true;
}


bool TestEkosMeridianFlipBase::checkRefocusing()
{
    // wait until additional steps are reached
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedFocusStates, 30000);
    // check if focus completion is reached (successful or not)
    if (refocus_checked || autofocus_checked)
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_COMPLETE || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_FAILED ||
                                     m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_ABORTED, 120000);
    // focusing might have suspended guiding
    if (m_CaptureHelper->use_guiding)
        KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getGuidingStatus() == Ekos::GUIDE_GUIDING, 20000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startAligning(double expTime)
{
    // mark alignment
    use_aligning = true;

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
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_COMPLETE, 60000);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopAligning()
{
    // check whether alignment is already stopped
    if (m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_IDLE || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_ABORTED ||
            m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_FAILED || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_COMPLETE)
        return true;

    // switch to guiding module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000));

    // stop alignment
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->alignModule(), QPushButton, stopB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), stopB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_IDLE || m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_ABORTED ||
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

    // Ensure that the first autofocus event takes place after the flip
    if (autofocus_checked)
        Options::setInSequenceCheckFrames(static_cast<uint>(1 + t2mf / exptime));

    // Ensure that dithering takes place after the flip
    if (dithering_checked)
        Options::setDitherFrames(static_cast<uint>(1 + t2mf / exptime));

    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->captureModule(), 1000));

    // check if capture is in a stopped state
    KWRAP_SUB(QVERIFY(m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_IDLE || m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED
                      || m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_SUSPENDED|| m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE));

    // press start
    m_CaptureHelper->expectedCaptureStates.enqueue(Ekos::CAPTURE_CAPTURING);
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->captureModule(), QPushButton, startB);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->captureModule(), startB);

    // check if capturing has started
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates, 5000);
#endif
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopCapturing()
{
    // check if capture is in a stopped state
    if (m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_IDLE || m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_ABORTED ||
            m_CaptureHelper->getCaptureStatus() == Ekos::CAPTURE_COMPLETE)
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
    m_CaptureHelper->expectedMountStates.append(ISD::Telescope::MOUNT_SLEWING);
    m_CaptureHelper->expectedMountStates.append(ISD::Telescope::MOUNT_TRACKING);
    if (m_CaptureHelper->use_guiding == true)
        m_CaptureHelper->expectedGuidingStates.append(Ekos::GUIDE_GUIDING);
    m_CaptureHelper->expectedCaptureStates.append(Ekos::CAPTURE_CAPTURING);

    // switch to the scheduler module and start
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    KVERIFY_SUB(m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_IDLE || m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_ABORTED);
    KTRY_CLICK_SUB(scheduler, startB);

    // check mount slew and tracking
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedMountStates, 30000);
    // check guiding
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedGuidingStates, 30000);
    // check capturing
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedCaptureStates, 30000);

    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopScheduler()
{
    Ekos::Scheduler *scheduler = Ekos::Manager::Instance()->schedulerModule();
    // switch to the capture module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    if (m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_RUNNING || m_CaptureHelper->getSchedulerStatus() == Ekos::SCHEDULER_PAUSED)
        KTRY_CLICK_SUB(scheduler, startB);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::startFocusing()
{
    // check whether focusing is already running
    if (! (m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_IDLE || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_COMPLETE
            || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_ABORTED))
        return true;

    // switch to focus module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule(), 1000));
    // select the Luminance filter
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->focusModule(), FilterPosCombo, "Luminance");
    // select SEP algorithm for star detection
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->focusModule(), focusDetectionCombo, "SEP");
    // exp time 5sec
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->focusModule(), exposureIN, 5.0);
    // absolute position 40000
    initialFocusPosition = 40000;
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->focusModule(), absTicksSpin, initialFocusPosition);
    // gain 100
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->focusModule(), gainIN, 100.0);
    // start focusing
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule(), startGotoB);
    // initial step size 15000
    KTRY_SET_SPINBOX_SUB(Ekos::Manager::Instance()->focusModule(), stepIN, 15000);
    // suspend guiding while focusing
    KTRY_SET_CHECKBOX_SUB(Ekos::Manager::Instance()->focusModule(), suspendGuideCheck, true);
    // wait one second for settling
    QTest::qWait(3000);
    // start focusing
    m_CaptureHelper->expectedFocusStates.append(Ekos::FOCUS_COMPLETE);
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule(), startFocusB);
    // wait for successful completion
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedFocusStates, 180000);
    qCInfo(KSTARS_EKOS_TEST) << "Focusing finished.";
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::stopFocusing()
{
    // check whether focusing is already stopped
    if (m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_IDLE || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_COMPLETE
            || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_ABORTED)
        return true;

    // switch to focus module
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule(), 1000));
    // stop focusing if necessary
    KTRY_GADGET_SUB(Ekos::Manager::Instance()->focusModule(), QPushButton, stopFocusB);
    if (stopFocusB->isEnabled())
        KTRY_CLICK_SUB(Ekos::Manager::Instance()->focusModule(), stopFocusB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_IDLE || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_COMPLETE ||
                                 m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_ABORTED || m_CaptureHelper->getFocusStatus() == Ekos::FOCUS_FAILED, 15000);

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
    KTRY_SET_DOUBLESPINBOX_SUB(ekos->mountModule(), meridianFlipTimeBox, 15.0 * delay / 3600.0);
    // all checks succeeded
    return true;
}

bool TestEkosMeridianFlipBase::checkMFExecuted(int startDelay)
{
    // check if the meridian flip starts running
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedMeridianFlipStates, startDelay * 1000);
    // if dithering test required, set the dithering frequency to 1
    if (dithering_checked)
        Options::setDitherFrames(1);
    // check if the meridian flip is completed latest after one minute
    m_CaptureHelper->expectedMeridianFlipStates.enqueue(Ekos::Mount::FLIP_COMPLETED);
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
    {
        m_CaptureHelper->expectedGuidingStates.enqueue(Ekos::GUIDE_GUIDING);
        KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(m_CaptureHelper->expectedGuidingStates, 30000);
    }

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

    return true;
}

int TestEkosMeridianFlipBase::secondsToMF()
{
    const QString flipmsg = Ekos::Manager::Instance()->mountModule()->meridianFlipStatusText->text();

    QRegExp mfPattern("Meridian flip in (\\d+):(\\d+):(\\d+)");

    int pos = mfPattern.indexIn(flipmsg);
    if (pos > -1)
    {
        int hh  = mfPattern.cap(1).toInt();
        int mm  = mfPattern.cap(2).toInt();
        int sec = mfPattern.cap(3).toInt();
        if (hh >= 0)
            return (((hh * 60) + mm) * 60 + sec);
        else
            return (((hh * 60) - mm) * 60 - sec);
    }

    // unknown time
    return -1;
}

#endif // HAVE_INDI
