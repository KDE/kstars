/*
    KStars UI tests for blind plate solving in alignment

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_align_blind.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "ksuserdb.h"
#include "skymapcomposite.h"
#include "ekos/align/align.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "indi/guimanager.h"
#include "indi/indicamera.h"
#include "indi/indimount.h"

#include <QScopeGuard>

// Allowed deviation of the solution from the true pointing in degrees
#define POSITION_TOLERANCE 0.05
// Allowed relative deviation of the solved pixel scale from the true pixel scale
#define PIXSCALE_TOLERANCE 0.02
// Every solver attempt may take up to the astrometry timeout, and Align may make up to four attempts
#define SOLVE_TIMEOUT (4 * 1000 * static_cast<int>(Options::astrometryTimeout()) + 60000)

TestEkosAlignBlind::TestEkosAlignBlind(QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper();
}

void TestEkosAlignBlind::testPointingOffset_data()
{
    QTest::addColumn<double>("raOffset");
    QTest::addColumn<double>("deOffset");
    QTest::addColumn<bool>("beyondSearchRadius");

    // the solver searches within 15 degrees of the mount position
    QTest::newRow("within search radius") << 0.0 << -5.0 << false;
    QTest::newRow("beyond search radius") << 10.0 << -25.0 << true;
}

void TestEkosAlignBlind::testPointingOffset()
{
    QFETCH(double, raOffset);
    QFETCH(double, deOffset);
    QFETCH(bool, beyondSearchRadius);

    QVERIFY(slewToTestField());
    QVERIFY(setPointingOffset(raOffset, deOffset));

    QVERIFY(captureAndSolve());
    QVERIFY(verifySolution(raOffset, deOffset));

    // beyond the search radius, only a solve without the position hint succeeds
    QCOMPARE(logContains("Retrying without position constraint"), beyondSearchRadius);
}

void TestEkosAlignBlind::testWrongFOV_data()
{
    QTest::addColumn<double>("reducer");

    QTest::newRow("reducer 0.5x") << 0.5;
    QTest::newRow("barlow 2x") << 2.0;
}

void TestEkosAlignBlind::testWrongFOV()
{
    QFETCH(double, reducer);

    QVERIFY(slewToTestField());
    QVERIFY(setTrainReducer(reducer));

    QVERIFY(captureAndSolve());
    QVERIFY(verifySolution(0, 0));
    QVERIFY(logContains("Retrying without scale constraint"));

    // Align learns the effective FOV from the solution, so the next solve succeeds with the scale hint
    m_AlignLog.clear();
    QVERIFY(captureAndSolve());
    QVERIFY(verifySolution(0, 0));
    QVERIFY(!logContains("Retrying"));
}

void TestEkosAlignBlind::testWrongFOVAndPointingOffset()
{
    const double raOffset = 10.0, deOffset = -25.0;

    QVERIFY(slewToTestField());
    // use a reducer no other test uses, so no effective FOV is stored for it yet
    QVERIFY(setTrainReducer(0.6));
    QVERIFY(setPointingOffset(raOffset, deOffset));

    QVERIFY(captureAndSolve());
    QVERIFY(verifySolution(raOffset, deOffset));
    QVERIFY(logContains("Retrying without scale and position constraints"));
}

void TestEkosAlignBlind::testExternalSolverTempFiles()
{
    if (!QFileInfo(Options::aSTAPExecutable()).isExecutable())
        QSKIP("ASTAP is not installed.");

    const uint solverType = Options::solverType();
    const uint timeout = Options::astrometryTimeout();
    auto restore = qScopeGuard([solverType, timeout]()
    {
        Options::setSolverType(solverType);
        Options::setAstrometryTimeout(timeout);
    });
    Options::setSolverType(SSolver::SOLVER_ASTAP);
    Options::setAstrometryTimeout(30);

    const QStringList filter {"solver*.fits"};
    const QDir tempDir(QDir::tempPath());
    const int before = tempDir.entryList(filter, QDir::Files).size();

    QVERIFY(slewToTestField());
    m_AlignStatus = Ekos::ALIGN_IDLE;
    QVERIFY(m_CaptureHelper->startAligning(3.0));
    // The result does not matter, ASTAP may not have its star database
    QTRY_VERIFY_WITH_TIMEOUT(m_AlignStatus == Ekos::ALIGN_COMPLETE || m_AlignStatus == Ekos::ALIGN_FAILED
                             || m_AlignStatus == Ekos::ALIGN_ABORTED, SOLVE_TIMEOUT);

    QTRY_COMPARE_WITH_TIMEOUT(tempDir.entryList(filter, QDir::Files).size(), before, 5000);
}

/* *********************************************************************************
 *
 * Test helpers
 *
 * ********************************************************************************* */

bool TestEkosAlignBlind::slewToTestField()
{
    // Kocab is circumpolar for the test location
    SkyObject *target = KStarsData::Instance()->skyComposite()->findByName("Kocab");
    KVERIFY_SUB(target != nullptr);

    // Really slew instead of syncing. After a sync, the simulator alignment model reports the target
    // while the physical position (EQUATORIAL_PE), which the CCD simulator renders, stays where it was.
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->resetModel();
    KVERIFY_SUB(mount->slew(target->ra().Hours(), target->dec().Degrees()));
    auto scope = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(scope != nullptr);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(mount->status() == ISD::Mount::MOUNT_TRACKING
                                 && scope->currentCoordinates().angularDistanceTo(target).Degrees() < 0.1, 120000);
    return true;
}

bool TestEkosAlignBlind::setPointingOffset(double raOffset, double deOffset)
{
    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(camera != nullptr);

    auto offset = camera->getNumber("SIM_POINTING_OFFSET");
    if (!offset.isValid())
    {
        // without the property, the offset is 0 anyway
        if (raOffset == 0 && deOffset == 0)
            return true;
        QWARN("CCD simulator has no SIM_POINTING_OFFSET property, INDI is too old.");
        return false;
    }

    offset.findWidgetByName("SIM_OFFSET_RA")->setValue(raOffset);
    offset.findWidgetByName("SIM_OFFSET_DE")->setValue(deOffset);
    camera->sendNewProperty(offset);

    // wait until the driver confirms the new offset
    KTRY_VERIFY_WITH_TIMEOUT_SUB(
        std::abs(camera->getNumber("SIM_POINTING_OFFSET").findWidgetByName("SIM_OFFSET_RA")->getValue() - raOffset) < 1e-6 &&
        std::abs(camera->getNumber("SIM_POINTING_OFFSET").findWidgetByName("SIM_OFFSET_DE")->getValue() - deOffset) < 1e-6,
        5000);
    return true;
}

bool TestEkosAlignBlind::setTrainReducer(double reducer)
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    Ekos::OpticalTrainManager *otm = Ekos::OpticalTrainManager::Instance();

    QVariantMap train = otm->getOpticalTrain(m_CaptureHelper->m_primaryTrain);
    train["reducer"] = reducer;
    KStarsData::Instance()->userdb()->UpdateOpticalTrain(train, train["id"].toInt());
    otm->refreshTrains();
    KTRY_VERIFY_WITH_TIMEOUT_SUB(std::abs(align->telescopeInfo()[2] - reducer) < 1e-6, 5000);

    // Align sent the reduced focal length to the camera. The real optics did not change,
    // so let the simulator render with the true focal length again.
    auto camera = otm->getCamera(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(camera != nullptr);
    KVERIFY_SUB(camera->setScopeInfo(m_TrueFocalLength, m_TrueAperture));
    KTRY_VERIFY_WITH_TIMEOUT_SUB(std::abs(camera->getNumber("SCOPE_INFO")[0].getValue() - m_TrueFocalLength) < 1e-6, 5000);
    return true;
}

bool TestEkosAlignBlind::captureAndSolve()
{
    m_AlignStatus = Ekos::ALIGN_IDLE;
    const int solverCount = m_SolverCount;

    // Use a fixed gain, the simulator scales the star brightness with it. Without it, the camera
    // keeps whatever gain another module set last, and at gain 0 too few stars are visible to solve.
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), alignGain, 90.0);
    KVERIFY_SUB(m_CaptureHelper->startAligning(3.0));
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_AlignStatus == Ekos::ALIGN_COMPLETE || m_AlignStatus == Ekos::ALIGN_FAILED
                                 || m_AlignStatus == Ekos::ALIGN_ABORTED, SOLVE_TIMEOUT);
    KVERIFY2_SUB(m_AlignStatus == Ekos::ALIGN_COMPLETE,
                 QString("Alignment did not complete, align log:\n%1").arg(m_AlignLog.join("\n")).toLocal8Bit());
    KVERIFY_SUB(m_SolverCount == solverCount + 1);
    return true;
}

bool TestEkosAlignBlind::verifySolution(double raOffset, double deOffset)
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    auto mount = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(mount != nullptr);

    // The simulator renders the sky at the JNow mount position plus the offset
    const SkyPoint &mountCoord = mount->currentCoordinates();
    SkyPoint pointing(dms(mountCoord.ra().Degrees() + raOffset).reduce(), dms(mountCoord.dec().Degrees() + deOffset));
    pointing.catalogueCoord(KStarsData::Instance()->ut().djd());
    SkyPoint solved(m_SolvedRA / 15.0, m_SolvedDE);
    const double distance = solved.angularDistanceTo(&pointing).Degrees();
    KVERIFY2_SUB(distance < POSITION_TOLERANCE,
                 QString("Solution RA %1 DE %2 is %3 deg away from the true pointing RA %4 DE %5")
                 .arg(m_SolvedRA).arg(m_SolvedDE).arg(distance)
                 .arg(pointing.ra0().Degrees()).arg(pointing.dec0().Degrees()).toLocal8Bit());

    // The pixel scale must match the true optics, not the ones in the optical train
    KTRY_GADGET_SUB(align, QComboBox, alignBinning);
    const double pixelWidth = align->cameraInfo()[2];
    const double truePixScale = 206.2648062 * pixelWidth / m_TrueFocalLength * (alignBinning->currentIndex() + 1);
    KVERIFY2_SUB(std::abs(m_SolvedPixScale / truePixScale - 1.0) < PIXSCALE_TOLERANCE,
                 QString("Solved pixel scale %1 differs from the true pixel scale %2")
                 .arg(m_SolvedPixScale).arg(truePixScale).toLocal8Bit());
    return true;
}

bool TestEkosAlignBlind::logContains(const QString &text) const
{
    for (const auto &line : m_AlignLog)
        if (line.contains(text))
            return true;
    return false;
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosAlignBlind::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);

    // solve with position and scale hints, retrying without them on failure
    Options::setAstrometryUseImageScale(true);
    Options::setAstrometryImageScaleUnits(1);
    Options::setAstrometryUsePosition(true);
    Options::setAstrometryDynamicThreshold(false);

    m_CaptureHelper->startEkosProfile();

    Options::setVerboseLogging(false);
    Options::setAlignmentLogging(false);
    Options::setLogToFile(false);
    m_CaptureHelper->prepareOpticalTrains();
    m_CaptureHelper->prepareMountModule();
    m_CaptureHelper->prepareAlignmentModule();
    m_CaptureHelper->init();
    GUIManager::Instance()->close();

    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    // only solve, a constant simulator offset cannot be synced away
    KTRY_SET_RADIOBUTTON(align, nothingR, true);

    // the true optics are the ones of the train as prepared by the helper
    QTRY_VERIFY_WITH_TIMEOUT(align->telescopeInfo()[0] > 0, 5000);
    QCOMPARE(align->telescopeInfo()[2], 1.0);
    m_TrueFocalLength = align->telescopeInfo()[0];
    m_TrueAperture = align->telescopeInfo()[1];

    connect(align, &Ekos::Align::newStatus, this, &TestEkosAlignBlind::alignStatusChanged, Qt::UniqueConnection);
    connect(align, &Ekos::Align::newLog, this, &TestEkosAlignBlind::alignLogReceived, Qt::UniqueConnection);
    connect(align, &Ekos::Align::newSolverResults, this, &TestEkosAlignBlind::solverResultReceived,
            Qt::UniqueConnection);
}

void TestEkosAlignBlind::cleanupTestCase()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    disconnect(align, &Ekos::Align::newStatus, this, &TestEkosAlignBlind::alignStatusChanged);
    disconnect(align, &Ekos::Align::newLog, this, &TestEkosAlignBlind::alignLogReceived);
    disconnect(align, &Ekos::Align::newSolverResults, this, &TestEkosAlignBlind::solverResultReceived);

    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosAlignBlind::init()
{
    m_AlignLog.clear();
}

void TestEkosAlignBlind::cleanup()
{
    // restore the true optics and pointing for the next test
    QVERIFY(setPointingOffset(0, 0));
    QVERIFY(setTrainReducer(1.0));
}

void TestEkosAlignBlind::alignStatusChanged(Ekos::AlignState status)
{
    m_AlignStatus = status;
}

void TestEkosAlignBlind::alignLogReceived(const QString &text)
{
    m_AlignLog.append(text);
}

void TestEkosAlignBlind::solverResultReceived(double orientation, double ra, double dec, double pixscale)
{
    Q_UNUSED(orientation);
    m_SolverCount++;
    m_SolvedRA = ra;
    m_SolvedDE = dec;
    m_SolvedPixScale = pixscale;
}

QTEST_KSTARS_MAIN(TestEkosAlignBlind)

#endif // HAVE_INDI
