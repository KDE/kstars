/*
    KStars UI tests for alignment

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_align.h"

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_scheduler_helper.h"
#include "mountmodel.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "Options.h"
#include "indi/guimanager.h"
#include "ekos/align/align.h"

#include "skymapcomposite.h"

TestEkosAlign::TestEkosAlign(QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper();
}


void TestEkosAlign::testSlewAlign()
{
    // slew to Kocab as target
    SkyObject *target = findTargetByName("Kocab");
    QVERIFY(m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true));

    // execute an alignment and verify the result
    QVERIFY(executeAlignment(target));
}

void TestEkosAlign::testEmptySkyAlign()
{
    SkyObject *emptySky = new SkyObject(SkyObject::TYPE_UNKNOWN, 15.254722222222222, 72.031388888888889);
    // update J2000 coordinates
    updateJ2000Coordinates(emptySky);

    QVERIFY(m_CaptureHelper->slewTo(emptySky->ra().Hours(), emptySky->dec().Degrees(), true));

    // execute an alignment and verify the result
    QVERIFY(executeAlignment(emptySky));
}

void TestEkosAlign::testSlewDriftAlign()
{
    // slew to Kocab as target
    SkyObject *target = findTargetByName("Kocab");
    QVERIFY(m_CaptureHelper->slewTo(target->ra().Hours(), target->dec().Degrees(), true));
    // execute an alignment and verify the result
    QVERIFY(executeAlignment(target));
    // now send motion north and west to create a guiding deviation
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->doPulse(RA_INC_DIR, 5000, DEC_INC_DIR, 5000);
    // wait until the pulse ends
    QTest::qWait(6000);
    // execute an alignment and verify if the target is the same
    QVERIFY(executeAlignment(target));
}

void TestEkosAlign::testFitsAlign()
{
    // check the FITS file
    QString const fits_file = "../Tests/kstars_ui/Kocab.fits";
    QFileInfo target_fits(fits_file);
    QVERIFY(target_fits.exists());

    // define expected target (center of the FITS file, J2000)
    SkyObject targetObject(SkyObject::STAR, 14.844809110652733, 74.1587773591246, 1.8f, "Kocab");
    targetObject.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

    // slew somewhere close to the target
    SkyObject *close_to_target = findTargetByName("Pherkab");
    QVERIFY(m_CaptureHelper->slewTo(close_to_target->ra().Hours(), close_to_target->dec().Degrees(), true));
    // wait until the slew has finished
    QTRY_VERIFY_WITH_TIMEOUT(m_TelescopeStatus == ISD::Mount::MOUNT_TRACKING, 60000);

    // ensure that we wait until alignment completion
    expectedAlignStates.append(Ekos::ALIGN_PROGRESS);
    expectedAlignStates.append(Ekos::ALIGN_COMPLETE);

    // execute the FITS alignment and verify the result
    Ekos::Manager::Instance()->alignModule()->loadAndSlew(target_fits.filePath());

    // wait until the alignment succeeds
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedAlignStates, 20000);

    // check if the alignment target matches
    verifyAlignmentTarget(&targetObject);
}

void TestEkosAlign::testAlignTargetScheduledJob()
{
    // test scheduler with target
    alignWithScheduler(findTargetByName("Pherkab"));
}

void TestEkosAlign::testFitsAlignTargetScheduledJob()
{
    // check the FITS file
    QString const fits_file = "../Tests/kstars_ui/Kocab.fits";
    QFileInfo target_fits(fits_file);
    QVERIFY(target_fits.exists());

    // define expected target (center of the FITS file, J2000)
    SkyObject targetObject(SkyObject::STAR, 14.844809110652733, 74.1587773591246, 1.8f, "Kocab");
    targetObject.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

    // test scheduler with target
    alignWithScheduler(&targetObject, target_fits.absoluteFilePath());
}

void TestEkosAlign::testSyncOnlyAlign()
{
    QSKIP("Align feature of sync only not implemented yet.");
    Ekos::Manager *ekos = Ekos::Manager::Instance();
    // select to Pherkab as target
    SkyObject *pherkab = findTargetByName("Pherkab");

    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->mountModule(), 1000);
    // ensure that the mount is tracking
    KTRY_CLICK(ekos->mountModule(), trackOnB);
    // sync to Pherkab (use JNow for sync)
    ekos->mountModule()->sync(pherkab->ra().Hours(), pherkab->dec().Degrees());

    // execute an alignment and verify the result
    // Align::getTargetCoords() returns J2000 values
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(ekos->alignModule(), 1000);
    QVERIFY(executeAlignment(pherkab));
}

void TestEkosAlign::testAlignRecoverFromSlewing()
{
    // slew to Kocab
    m_CaptureHelper->slewTo(14.845, 74.106, true);
    // expect align events: suspend --> progress --> complete
    expectedAlignStates.append(Ekos::ALIGN_SUSPENDED);
    expectedAlignStates.append(Ekos::ALIGN_PROGRESS);
    expectedAlignStates.append(Ekos::ALIGN_COMPLETE);
    // start alignment
    QVERIFY(m_CaptureHelper->startAligning(5));
    // ensure that capturing has started
    QTest::qWait(2000);
    // move the mount to interrupt alignment
    qCInfo(KSTARS_EKOS_TEST) << "Moving the mount to interrupt alignment...";
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->slew(14.86, 74.106);
    mount->slew(14.845, 74.106);
    // check if event sequence has been completed
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(expectedAlignStates, 30000);
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
    QStandardPaths::setTestModeEnabled(true);
    // create temporary directory
    testDir = new QTemporaryDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");

    // use the helper to start the profile
    m_CaptureHelper->startEkosProfile();

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

    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
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

    // prepare optical trains for testing
    m_CaptureHelper->prepareOpticalTrains();
    // prepare the mount module for testing
    m_CaptureHelper->prepareMountModule();
    // prepare for focusing tests for those that use the focuser
    if (m_CaptureHelper->m_FocuserDevice != "")
        m_CaptureHelper->prepareFocusModule();
    // prepare for alignment tests
    m_CaptureHelper->prepareAlignmentModule();

    m_CaptureHelper->init();
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

void TestEkosAlign::cleanup()
{
    Ekos::Manager *ekos = Ekos::Manager::Instance();
    Ekos::Scheduler *scheduler = ekos->schedulerModule();
    // press stop button if running
    if (scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_STARTUP
            || scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_RUNNING)
        KTRY_CLICK(scheduler, startB);
    // in all cases, stop the scheduler and remove all jobs
    scheduler->process()->stop();
    scheduler->process()->removeAllJobs();
}

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
    expectedTelescopeStates.append(ISD::Mount::MOUNT_TRACKING);
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

bool TestEkosAlign::executeAlignment(SkyObject *targetObject)
{
    // start alignment
    KVERIFY_SUB(m_CaptureHelper->startAligning(5.0));
    // verify if alignment completes
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_CaptureHelper->getAlignStatus() == Ekos::ALIGN_COMPLETE, 60000);
    // check if the alignment target matches
    KVERIFY_SUB(verifyAlignmentTarget(targetObject));
    // success
    return true;
}

bool TestEkosAlign::verifyAlignmentTarget(SkyObject *targetObject)
{
    QList<double> alignmentTarget = Ekos::Manager::Instance()->alignModule()->getTargetCoords();
    KVERIFY2_SUB(std::abs(alignmentTarget[0] - targetObject->ra0().Hours()) <
                 0.005, // difference small enough to capture JNow/J2000 errors
                 QString("RA target J2000 deviation too big: %1 received, %2 expected.").arg(alignmentTarget[0]).arg(
                     targetObject->ra0().Hours()).toLocal8Bit());
    KVERIFY2_SUB(std::abs(alignmentTarget[1] - targetObject->dec0().Degrees()) < 0.005,
                 QString("DEC target J2000 deviation too big: %1 received, %2 expected.").arg(alignmentTarget[1]).arg(
                     targetObject->dec0().Degrees()).toLocal8Bit());
    // success
    return true;
}

bool TestEkosAlign::alignWithScheduler(SkyObject *targetObject, QString fitsTarget)
{
    Ekos::Manager *ekos = Ekos::Manager::Instance();
    Ekos::Scheduler *scheduler = ekos->schedulerModule();
    // start ASAP
    TestEkosSchedulerHelper::StartupCondition startupCondition;
    startupCondition.type = Ekos::START_ASAP;
    TestEkosSchedulerHelper::CompletionCondition completionCondition;
    completionCondition.type = Ekos::FINISH_SEQUENCE;

    // create the capture sequence file
    const QString sequenceFile = TestEkosSchedulerHelper::getDefaultEsqContent();
    const QString esqFile = testDir->filePath(QString("test.esq"));
    // create the scheduler file
    const QString schedulerFile = TestEkosSchedulerHelper::getSchedulerFile(targetObject, startupCondition,
                                  completionCondition, {true, false, true, false}, false, false, 30, 90.0, fitsTarget);
    const QString eslFile = testDir->filePath(QString("test.esl"));
    // write both files to the test directory
    KVERIFY_SUB(TestEkosSchedulerHelper::writeSimpleSequenceFiles(schedulerFile, eslFile, sequenceFile, esqFile));
    // load the scheduler file
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(scheduler, 1000));
    scheduler->process()->loadScheduler(eslFile);

    // start
    KTRY_CLICK_SUB(scheduler, startB);

    // expect a slew
    expectedTelescopeStates.append(ISD::Mount::MOUNT_SLEWING);
    expectedTelescopeStates.append(ISD::Mount::MOUNT_TRACKING);
    // ensure that we wait until alignment completion
    expectedAlignStates.append(Ekos::ALIGN_PROGRESS);
    expectedAlignStates.append(Ekos::ALIGN_COMPLETE);

    // wait until the slew has finished
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedTelescopeStates, 60000);
    // wait until the alignment succeeds
    KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(expectedAlignStates, 20000);

    // check if the alignment target matches
    return verifyAlignmentTarget(targetObject);
}

SkyObject *TestEkosAlign::findTargetByName(QString name)
{
    SkyObject *targetObject = KStarsData::Instance()->skyComposite()->findByName(name);
    // update J2000 coordinates
    updateJ2000Coordinates(targetObject);

    return targetObject;
}

void TestEkosAlign::updateJ2000Coordinates(SkyPoint *target)
{
    SkyPoint J2000Coord(target->ra(), target->dec());
    J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
    target->setRA0(J2000Coord.ra());
    target->setDec0(J2000Coord.dec());
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

void TestEkosAlign::telescopeStatusChanged(ISD::Mount::Status status)
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

void TestEkosAlign::imageReceived(const QSharedPointer<FITSView> &view)
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
