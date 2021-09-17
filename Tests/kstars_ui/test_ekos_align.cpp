/*
    KStars UI tests for alignment

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_align.h"

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "mountmodel.h"
#include "Options.h"
#include "indi/guimanager.h"

TestEkosAlign::TestEkosAlign(QObject *parent) : QObject(parent)
{
    m_test_ekos_helper = new TestEkosHelper();
}

void TestEkosAlign::testAlignRecoverFromSlewing()
{
    // slew to Kocab
    m_test_ekos_helper->slewTo(14.845, 74.106, true);
    // expect align events: suspend --> progress --> complete
    expectedAlignStates.append(Ekos::ALIGN_SUSPENDED);
    expectedAlignStates.append(Ekos::ALIGN_PROGRESS);
    expectedAlignStates.append(Ekos::ALIGN_COMPLETE);
    // start alignment
    QVERIFY(m_test_ekos_helper->startAligning(10.0));
    // ensure that capturing has started
    QTest::qWait(2000);
    // move the mount to interrupt alignment
    qCInfo(KSTARS_EKOS_TEST) << "Moving the mount to interrupt alignment...";
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->slew(14.86, 74.106);
    mount->slew(14.845, 74.106);
    // check if event sequence has been completed
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedAlignStates, 20000);
    // check the number of images and plate solves
    KTRY_CHECK_ALIGNMENTS(1);
}

void TestEkosAlign::testMountModel()
{
    // test two runs
    for (int points = 2; points < 4; points++)
    {
        init();
        // prepare the mount model
        QVERIFY(prepareMountModel(points));
        // execute the mount model tool
        QVERIFY(runMountModelTool(points, false));
    }
}

void TestEkosAlign::testMountModelToolRecoverFromSlewing()
{
    int points = 3;
    // prepare the mount model
    QVERIFY(prepareMountModel(points));
    // execute the mount model tool
    QVERIFY(runMountModelTool(points, true));
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosAlign::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    // start the profile
    QVERIFY(m_test_ekos_helper->startEkosProfile());
    m_test_ekos_helper->init();
    QStandardPaths::setTestModeEnabled(true);

    prepareTestCase();
}

void TestEkosAlign::cleanupTestCase()
{
    // connect to the align process to count solving results
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newSolverResults, this,
               &TestEkosAlign::solverResultReceived);
    // disconnect to the capture process to receive capture status changes
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newImage, this,
               &TestEkosAlign::imageReceived);
    // disconnect to the alignment process to receive align status changes
    disconnect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this,
               &TestEkosAlign::alignStatusChanged);
    // disconnect to the alignment process to receive align status changes
    disconnect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newStatus, this,
               &TestEkosAlign::telescopeStatusChanged);

    m_test_ekos_helper->cleanup();
    QVERIFY(m_test_ekos_helper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosAlign::prepareTestCase()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // set logging defaults for alignment
    Options::setVerboseLogging(false);
    Options::setAlignmentLogging(false);
    Options::setLogToFile(false);

    // set mount defaults
    QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000);
    // set primary scope to my favorite FSQ-85 with QE 0.73 Reducer
    KTRY_SET_DOUBLESPINBOX(ekos->mountModule(), primaryScopeFocalIN, 339.0);
    KTRY_SET_DOUBLESPINBOX(ekos->mountModule(), primaryScopeApertureIN, 85.0);
    // set guide scope to a Tak 7x50
    KTRY_SET_DOUBLESPINBOX(ekos->mountModule(), guideScopeFocalIN, 170.0);
    KTRY_SET_DOUBLESPINBOX(ekos->mountModule(), guideScopeApertureIN, 50.0);
    // save values
    KTRY_CLICK(Ekos::Manager::Instance()->mountModule(), saveB);
    // disable meridian flip
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->mountModule(), meridianFlipCheckBox, false);

    // check if astrometry files exist
    if (! m_test_ekos_helper->checkAstrometryFiles())
    {
        QFAIL(QString("No astrometry index files found in %1").arg(
                  KSUtils::getAstrometryDataDirs().join(", ")).toStdString().c_str());
    }
    // switch to alignment module
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->alignModule(), 1000);
    // set the CCD
    KTRY_SET_COMBO(ekos->alignModule(), CCDCaptureCombo, m_test_ekos_helper->m_CCDDevice);
    // select the Luminance filter
    KTRY_SET_COMBO(ekos->alignModule(), FilterPosCombo, "Luminance");
    // set 1x1 binning
    KTRY_SET_COMBO(ekos->alignModule(), binningCombo, "1x1");
    // select local solver
    ekos->alignModule()->setSolverMode(Ekos::Align::SOLVER_LOCAL);
    // set exposure time for alignment
    KTRY_SET_DOUBLESPINBOX(ekos->alignModule(), exposureIN, 5.0);
    // set gain
    KTRY_SET_DOUBLESPINBOX(ekos->alignModule(), GainSpin, 50.0);
    // select internal SEP method
    Options::setSolveSextractorType(SSolver::EXTRACTOR_INTERNAL);
    // select StellarSolver
    Options::setSolverType(SSolver::SOLVER_LOCALASTROMETRY);
    // select fast solve profile option
    Options::setSolveOptionsProfile(SSolver::Parameters::FAST_SOLVING);
    // select the "Slew to Target" mode
    KTRY_SET_RADIOBUTTON(ekos->alignModule(), slewR, true);
    // reduce the accuracy to avoid testing problems
    KTRY_SET_SPINBOX(Ekos::Manager::Instance()->alignModule(), accuracySpin, 300);

    // Reduce the noise setting to ensure a working plate solving
    KTRY_INDI_PROPERTY("CCD Simulator", "Simulator Config", "SIMULATOR_SETTINGS", ccd_settings);
    INDI_E *noise_setting = ccd_settings->getElement("SIM_NOISE");
    QVERIFY(ccd_settings != nullptr);
    noise_setting->setValue(2.0);
    ccd_settings->processSetButton();
    // close INDI window
    GUIManager::Instance()->close();

    // connect to the alignment process to receive align status changes
    connect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this,
            &TestEkosAlign::alignStatusChanged, Qt::UniqueConnection);

    // connect to the mount process to receive status changes
    connect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newStatus, this,
            &TestEkosAlign::telescopeStatusChanged, Qt::UniqueConnection);

    // connect to the align process to receive captured images
    connect(ekos->alignModule(), &Ekos::Align::newImage, this, &TestEkosAlign::imageReceived,
            Qt::UniqueConnection);

    // connect to the align process to count solving results
    connect(ekos->alignModule(), &Ekos::Align::newSolverResults, this, &TestEkosAlign::solverResultReceived,
            Qt::UniqueConnection);
}

void TestEkosAlign::init()
{
    // reset counters
    solver_count = 0;
    image_count  = 0;
}

void TestEkosAlign::cleanup() {}

bool TestEkosAlign::prepareMountModel(int points)
{
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->alignModule(), 1000));
    // open the mount model tool
    KTRY_CLICK_SUB(Ekos::Manager::Instance()->alignModule(), mountModelB);

    // wait until the mount model is instantiated
    QTest::qWait(1000);
    Ekos::MountModel * mountModel = Ekos::Manager::Instance()->alignModule()->mountModel();
    KVERIFY_SUB(mountModel != nullptr);

    // clear alignment points
    if (mountModel->alignTable->rowCount() > 0)
    {
        QTimer::singleShot(2000, [&]()
        {
            QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
            if (dialog != nullptr)
                QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Yes), Qt::LeftButton);
        });
        mountModel->clearAllAlignB->click();
    }
    KVERIFY_SUB(mountModel->alignTable->rowCount() == 0);
    // set number of alignment points
    mountModel->alignPtNum->setValue(points);
    KVERIFY_SUB(mountModel->alignPtNum->value() == points);
    // use named stars for alignment (this hopefully improves plate soving speed)
    mountModel->alignTypeBox->setCurrentIndex(Ekos::MountModel::OBJECT_NAMED_STAR);
    KVERIFY_SUB(mountModel->alignTypeBox->currentIndex() == Ekos::MountModel::OBJECT_NAMED_STAR);
    // create the alignment points
    KVERIFY_SUB(mountModel->wizardAlignB->isEnabled());
    mountModel->wizardAlignB->click();

    // success
    return true;
}

bool TestEkosAlign::runMountModelTool(int points, bool moveMount)
{
    Ekos::MountModel * mountModel = Ekos::Manager::Instance()->alignModule()->mountModel();

    // start model creation
    KVERIFY_SUB(mountModel->startAlignB->isEnabled());
    mountModel->startAlignB->click();

    // check alignment
    for (int i = 1; i <= points; i++)
    {
        KVERIFY_SUB(trackSingleAlignment(i == points, moveMount));
        KWRAP_SUB(KTRY_CHECK_ALIGNMENTS(i));
    }

    // success
    return true;
}


bool TestEkosAlign::trackSingleAlignment(bool lastPoint, bool moveMount)
{
    expectedTelescopeStates.append(ISD::Telescope::MOUNT_TRACKING);
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedTelescopeStates, 60000);

    // wait one second ensuring that capturing starts
    QTest::qWait(1000);
    // determine current target position
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    SkyPoint target = mount->currentTarget();
    double ra = target.ra().Hours();
    double dec = target.dec().Degrees();
    // expect a suspend event if mount is moved
    if (moveMount)
        expectedAlignStates.append(Ekos::ALIGN_SUSPENDED);
    // expect a progress signal for start capturing and solving
    expectedAlignStates.append(Ekos::ALIGN_PROGRESS);
    // if last point is reached, expect a complete signal, else expect a slew
    if (lastPoint == true)
        expectedAlignStates.append(Ekos::ALIGN_COMPLETE);
    else
        expectedAlignStates.append(Ekos::ALIGN_SLEWING);
    if (moveMount)
    {
        // move the mount to interrupt alignment
        qCInfo(KSTARS_EKOS_TEST) << "Moving the mount to interrupt alignment...";
        mount->slew(ra + 0.05, dec + 0.1);
        mount->slew(ra, dec);
    }
    // check if event sequence has been completed
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedAlignStates, 180000);

    // everything succeeded
    return true;
}
/* *********************************************************************************
 *
 * Slots for catching state changes
 *
 * ********************************************************************************* */

void TestEkosAlign::alignStatusChanged(Ekos::AlignState status)
{
    m_AlignStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedAlignStates.isEmpty() && expectedAlignStates.head() == status)
        expectedAlignStates.dequeue();
}

void TestEkosAlign::telescopeStatusChanged(ISD::Telescope::Status status)
{
    m_TelescopeStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedTelescopeStates.isEmpty() && expectedTelescopeStates.head() == status)
        expectedTelescopeStates.dequeue();
}

void TestEkosAlign::captureStatusChanged(Ekos::CaptureState status)
{
    m_CaptureStatus = status;
    // check if the new state is the next one expected, then remove it from the stack
    if (!expectedCaptureStates.isEmpty() && expectedCaptureStates.head() == status)
        expectedCaptureStates.dequeue();
}

void TestEkosAlign::imageReceived(FITSView *view)
{
    Q_UNUSED(view);
    captureStatusChanged(Ekos::CAPTURE_COMPLETE);
    image_count++;
}

void TestEkosAlign::solverResultReceived(double orientation, double ra, double dec, double pixscale)
{
    Q_UNUSED(orientation);
    Q_UNUSED(ra);
    Q_UNUSED(dec);
    Q_UNUSED(pixscale);
    solver_count++;
}

/* *********************************************************************************
 *
 * Main function
 *
 * ********************************************************************************* */

QTEST_KSTARS_MAIN(TestEkosAlign)
