/*
    KStars UI tests for field derotation with a rotator on an Alt-Az mount

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_fieldderotation_sim.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "ksutils.h"
#include "skymapcomposite.h"
#include "ekos/align/align.h"
#include "ekos/mount/mount.h"
#include "ekos/auxiliary/fieldderotation.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "indi/guimanager.h"
#include "indi/indicamera.h"
#include "indi/indimount.h"
#include "indi/indirotator.h"

#include <QSignalSpy>
#include <QTimeZone>

// Allowed deviation of a solved position angle from the expected one in degrees
#define PA_TOLERANCE 0.3
// Allowed deviation between the parallactic angles of KStars and of the simulator in degrees. The simulator
// angle is the one of the last frame, a few seconds before KStars computes its own.
#define Q_TOLERANCE 0.15
// Derotation threshold in arcminutes
#define DEROTATION_THRESHOLD 6.0
// Every solver attempt may take up to the astrometry timeout, and Align may make up to four attempts
#define SOLVE_TIMEOUT (4 * 1000 * static_cast<int>(Options::astrometryTimeout()) + 60000)

namespace
{
SkyObject *testField()
{
    // Kocab is circumpolar for the test location
    return KStarsData::Instance()->skyComposite()->findByName("Kocab");
}

// JNow position of the test field at the KStars time
SkyPoint testFieldNow()
{
    SkyObject *target = testField();
    SkyPoint position(target->ra0(), target->dec0());
    position.apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->ut().djd());
    return position;
}
}

TestEkosFieldDerotation::TestEkosFieldDerotation(QObject *parent) : QObject(parent)
{
    m_CaptureHelper = new TestEkosCaptureHelper();
    m_CaptureHelper->m_RotatorDevice = "Rotator Simulator";
}

void TestEkosFieldDerotation::testFieldRotation()
{
    QVERIFY(captureAndSolve());
    const double pa0 = m_SolvedPA;
    const double q0 = simulatorParallacticAngle();
    const double rotator0 = rotatorAngle();

    // KStars and the simulator agree on the parallactic angle
    QVERIFY2(std::abs(KSUtils::rangePA(RotatorUtils::Instance()->parallacticAngle() - q0)) < Q_TOLERANCE,
             QString("KStars parallactic angle %1° differs from the simulator angle %2°")
             .arg(RotatorUtils::Instance()->parallacticAngle()).arg(q0).toLocal8Bit());

    QVERIFY(advanceTime(30 * 60));
    // KStars expects the camera to turn with the field before any new solve
    const double predictedPA = cameraPA();

    QVERIFY(captureAndSolve());
    const double dq = KSUtils::rangePA(simulatorParallacticAngle() - q0);
    QVERIFY2(std::abs(dq) > 5, QString("The parallactic angle changed by %1° only").arg(dq).toLocal8Bit());
    QVERIFY2(std::abs(KSUtils::rangePA(rotatorAngle() - rotator0)) < 0.01, "The rotator moved without derotation");

    // The field turned with the parallactic angle, in the same direction
    const double dpa = KSUtils::rangePA(m_SolvedPA - pa0);
    qCInfo(KSTARS_EKOS_TEST) << "Without derotation: solved PA" << pa0 << "->" << m_SolvedPA << "change" << dpa
                             << "parallactic angle change" << dq << "KStars expected PA" << predictedPA;
    QVERIFY2(std::abs(dpa - dq) < PA_TOLERANCE,
             QString("The solved position angle changed by %1°, the parallactic angle by %2°")
             .arg(dpa).arg(dq).toLocal8Bit());
    QVERIFY2(std::abs(KSUtils::rangePA(predictedPA - m_SolvedPA)) < PA_TOLERANCE,
             QString("KStars expected the position angle %1°, the solver measured %2°")
             .arg(predictedPA).arg(m_SolvedPA).toLocal8Bit());
}

void TestEkosFieldDerotation::testDerotationHoldsPA()
{
    QVERIFY(captureAndSolve());
    const double pa0 = m_SolvedPA;

    double heldPA = 0;
    QVERIFY(setDerotation(true, &heldPA));
    QVERIFY2(std::abs(KSUtils::rangePA(heldPA - pa0)) < PA_TOLERANCE,
             QString("Derotation holds %1°, the solved position angle is %2°").arg(heldPA).arg(pa0).toLocal8Bit());

    const double rotator0 = rotatorAngle();
    const double q0 = RotatorUtils::Instance()->parallacticAngle();

    // 30 minutes in steps of 2 minutes, each turning the field by less than 1°
    for (int step = 0; step < 15; step++)
    {
        QVERIFY(advanceTime(120));
        QVERIFY2(waitForDerotation(heldPA), QString("Derotation failed at step %1").arg(step).toLocal8Bit());
    }

    const double dq = KSUtils::rangePA(RotatorUtils::Instance()->parallacticAngle() - q0);
    QVERIFY2(std::abs(dq) > 5, QString("The parallactic angle changed by %1° only").arg(dq).toLocal8Bit());

    // The rotator took out the field rotation
    const double turned = KSUtils::rangePA(rotatorAngle() - rotator0);
    QVERIFY2(std::abs(std::abs(turned) - std::abs(dq)) < 0.5,
             QString("The rotator turned by %1°, the parallactic angle changed by %2°").arg(turned).arg(dq).toLocal8Bit());

    // The camera still has the position angle it had at the start
    QVERIFY(captureAndSolve());
    qCInfo(KSTARS_EKOS_TEST) << "With derotation: solved PA" << pa0 << "->" << m_SolvedPA << "held PA" << heldPA
                             << "parallactic angle change" << dq << "rotator turned" << turned;
    QVERIFY2(std::abs(KSUtils::rangePA(m_SolvedPA - pa0)) < PA_TOLERANCE,
             QString("The solved position angle moved from %1° to %2°").arg(pa0).arg(m_SolvedPA).toLocal8Bit());
    QVERIFY(!derotationLogContains("Holding the new position angle"));
}

void TestEkosFieldDerotation::testDerotationLimit()
{
    QVERIFY(captureAndSolve());
    const double pa0 = m_SolvedPA;
    const double q0 = simulatorParallacticAngle();

    QVERIFY(setDerotation(true));
    const double rotator0 = rotatorAngle();

    // One hour at once turns the field by more than the 10° correction limit
    QVERIFY(advanceTime(60 * 60));
    QTRY_VERIFY_WITH_TIMEOUT(derotationLogContains("Holding the new position angle"), 10000);
    QVERIFY(waitForCoordinates(2));
    QVERIFY2(std::abs(KSUtils::rangePA(rotatorAngle() - rotator0)) < 0.01, "The rotator corrected beyond the limit");
    // Derotation holds the position angle KStars expects now
    const double newHeldPA = cameraPA();

    // The rotation was not corrected
    QVERIFY(captureAndSolve());
    const double dq = KSUtils::rangePA(simulatorParallacticAngle() - q0);
    QVERIFY2(std::abs(dq) > 10, QString("The parallactic angle changed by %1° only").arg(dq).toLocal8Bit());
    const double dpa = KSUtils::rangePA(m_SolvedPA - pa0);
    qCInfo(KSTARS_EKOS_TEST) << "Beyond the limit: solved PA" << pa0 << "->" << m_SolvedPA << "change" << dpa
                             << "parallactic angle change" << dq << "new held PA" << newHeldPA;
    QVERIFY2(std::abs(dpa - dq) < PA_TOLERANCE,
             QString("The solved position angle changed by %1°, the parallactic angle by %2°")
             .arg(dpa).arg(dq).toLocal8Bit());

    // From then on, derotation holds the new position angle
    QVERIFY(advanceTime(120));
    QVERIFY(waitForDerotation(newHeldPA));
    QVERIFY2(std::abs(KSUtils::rangePA(rotatorAngle() - rotator0)) > 0.1, "The rotator did not correct after the limit");
}

/* *********************************************************************************
 *
 * Test helpers
 *
 * ********************************************************************************* */

bool TestEkosFieldDerotation::setMountType(bool altAz)
{
    auto mount = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(mount != nullptr);

    auto mountType = mount->getSwitch("TELESCOPE_MOUNT_TYPE");
    KVERIFY2_SUB(mountType.isValid(), "The telescope simulator has no TELESCOPE_MOUNT_TYPE property");
    mountType.reset();
    mountType.findWidgetByName(altAz ? "ALTAZ" : "EQ_GEM")->setState(ISS_ON);
    mount->sendNewProperty(mountType);

    KTRY_VERIFY_WITH_TIMEOUT_SUB(mount->isAltAz() == altAz && RotatorUtils::Instance()->isAltAz() == altAz, 10000);
    KVERIFY_SUB(m_RotatorSettings->DerotationGroup->isEnabled() == altAz);
    return true;
}

bool TestEkosFieldDerotation::setHourAngle(double hours)
{
    SkyObject *target = testField();
    KVERIFY_SUB(target != nullptr);

    KStarsData *data = KStarsData::Instance();
    // sidereal hours to go until the local sidereal time is RA + hour angle
    const double siderealHours = std::remainder(target->ra().Hours() + hours - data->lst()->Hours(), 24.0);
    const qint64 seconds = std::llround(siderealHours * 3600.0 / 1.00273790935);
    data->clock()->setUTC(data->ut().addSecs(seconds));
    KVERIFY_SUB(waitForSimulatorTime());
    return true;
}

bool TestEkosFieldDerotation::advanceTime(int seconds)
{
    KStarsData *data = KStarsData::Instance();
    data->clock()->setUTC(data->ut().addSecs(seconds));
    KVERIFY_SUB(waitForSimulatorTime());
    KVERIFY_SUB(waitForTracking());
    KVERIFY_SUB(waitForFieldRotation());
    return true;
}

bool TestEkosFieldDerotation::waitForSimulatorTime()
{
    Ekos::OpticalTrainManager *otm = Ekos::OpticalTrainManager::Instance();
    QList<ISD::ConcreteDevice *> devices;
    devices << otm->getMount(m_CaptureHelper->m_primaryTrain) << otm->getCamera(m_CaptureHelper->m_primaryTrain);

    // KStars sends a changed time to all devices that have TIME_UTC
    auto synced = [](ISD::ConcreteDevice * device)
    {
        auto time = device->getText("TIME_UTC");
        if (!time.isValid())
            return false;
        const QDateTime utc = QDateTime::fromString(QString(time.findWidgetByName("UTC")->getText()), Qt::ISODate);
        const QDateTime driverTime(utc.date(), utc.time(), QTimeZone::utc());
        return std::abs(driverTime.secsTo(KStarsData::Instance()->ut())) < 5;
    };
    for (auto device : devices)
    {
        KVERIFY_SUB(device != nullptr);
        KTRY_VERIFY_WITH_TIMEOUT_SUB(synced(device), 10000);
    }
    return true;
}

bool TestEkosFieldDerotation::slewToTestField()
{
    // Really slew instead of syncing, the CCD simulator renders where the mount physically points
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    mount->resetModel();
    const SkyPoint target = testFieldNow();
    KVERIFY_SUB(mount->slew(target.ra().Hours(), target.dec().Degrees()));
    KVERIFY_SUB(waitForTracking());
    return true;
}

bool TestEkosFieldDerotation::waitForTracking()
{
    Ekos::Mount *mount = Ekos::Manager::Instance()->mountModule();
    auto scope = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(scope != nullptr);
    auto onTarget = [scope]()
    {
        const SkyPoint target = testFieldNow();
        return scope->currentCoordinates().angularDistanceTo(&target).Degrees() < 0.05;
    };
    KTRY_VERIFY_WITH_TIMEOUT_SUB(mount->status() == ISD::Mount::MOUNT_TRACKING && onTarget(), 120000);
    return true;
}

bool TestEkosFieldDerotation::waitForFieldRotation()
{
    KTRY_VERIFY_WITH_TIMEOUT_SUB(
        std::abs(KSUtils::rangePA(RotatorUtils::Instance()->parallacticAngle() - expectedParallacticAngle())) < 0.05,
        10000);
    return true;
}

bool TestEkosFieldDerotation::waitForDerotation(double heldPA)
{
    auto rotator = Ekos::OpticalTrainManager::Instance()->getRotator(m_CaptureHelper->m_primaryTrain);
    KVERIFY_SUB(rotator != nullptr);
    const double threshold = DEROTATION_THRESHOLD / 60.0;
    KTRY_VERIFY_WITH_TIMEOUT_SUB(rotator->absoluteAngleState() != IPS_BUSY
                                 && std::abs(KSUtils::rangePA(cameraPA() - heldPA)) < threshold + 0.02, 30000);
    return true;
}

bool TestEkosFieldDerotation::setDerotation(bool enabled, double *heldPA)
{
    m_RotatorSettings->derotationThreshold->setValue(DEROTATION_THRESHOLD);
    m_RotatorSettings->derotateField->setChecked(enabled);
    KVERIFY_SUB(Options::rotatorFieldDerotation() == enabled);
    if (enabled)
    {
        // The first coordinate update after enabling holds the current position angle
        KVERIFY_SUB(waitForCoordinates(2));
        if (heldPA != nullptr)
            *heldPA = cameraPA();
    }
    return true;
}

bool TestEkosFieldDerotation::captureAndSolve()
{
    m_AlignStatus = Ekos::ALIGN_IDLE;
    const int solverCount = m_SolverCount;

    // Use a fixed gain, the simulator scales the star brightness with it
    KTRY_SET_DOUBLESPINBOX_SUB(Ekos::Manager::Instance()->alignModule(), alignGain, 90.0);
    KVERIFY_SUB(m_CaptureHelper->startAligning(3.0));
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_AlignStatus == Ekos::ALIGN_COMPLETE || m_AlignStatus == Ekos::ALIGN_FAILED
                                 || m_AlignStatus == Ekos::ALIGN_ABORTED, SOLVE_TIMEOUT);
    KVERIFY2_SUB(m_AlignStatus == Ekos::ALIGN_COMPLETE,
                 QString("Alignment did not complete, align log:\n%1").arg(m_AlignLog.join("\n")).toLocal8Bit());
    KVERIFY_SUB(m_SolverCount == solverCount + 1);
    return true;
}

bool TestEkosFieldDerotation::waitForCoordinates(int count)
{
    QSignalSpy spy(RotatorUtils::Instance(), &RotatorUtils::fieldRotationChanged);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(spy.count() >= count, 10000);
    return true;
}

double TestEkosFieldDerotation::expectedParallacticAngle() const
{
    auto mount = Ekos::OpticalTrainManager::Instance()->getMount(m_CaptureHelper->m_primaryTrain);
    const SkyPoint &position = mount->currentCoordinates();
    KStarsData *data = KStarsData::Instance();
    const double hourAngle = data->lst()->Degrees() - position.ra().Degrees();
    return Ekos::FieldDerotation::parallacticAngle(hourAngle, position.dec().Degrees(), data->geo()->lat()->Degrees());
}

double TestEkosFieldDerotation::simulatorParallacticAngle() const
{
    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    return camera->getNumber("SIM_FIELD_ROTATION")[0].getValue();
}

double TestEkosFieldDerotation::cameraPA() const
{
    return RotatorUtils::Instance()->calcCameraAngle(rotatorAngle(), false);
}

double TestEkosFieldDerotation::rotatorAngle() const
{
    return Ekos::OpticalTrainManager::Instance()->getRotator(m_CaptureHelper->m_primaryTrain)->absoluteAngle();
}

bool TestEkosFieldDerotation::derotationLogContains(const QString &text) const
{
    for (const auto &line : m_DerotationLog)
        if (line.contains(text))
            return true;
    return false;
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosFieldDerotation::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);

    // solve with position and scale hints
    Options::setAstrometryUseImageScale(true);
    Options::setAstrometryImageScaleUnits(1);
    Options::setAstrometryUsePosition(true);
    Options::setAstrometryDynamicThreshold(false);
    // KStars sends its time and location to the simulators
    Options::setUseTimeUpdate(true);
    Options::setTimeSource("KStars");
    Options::setUseGeographicUpdate(true);
    Options::setLocationSource("KStars");

    QVERIFY(m_CaptureHelper->startEkosProfile());

    Options::setVerboseLogging(false);
    Options::setAlignmentLogging(false);
    Options::setLogToFile(false);
    m_CaptureHelper->prepareOpticalTrains();
    m_CaptureHelper->prepareMountModule();
    m_CaptureHelper->prepareAlignmentModule();
    m_CaptureHelper->init();
    GUIManager::Instance()->close();

    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    // only solve
    KTRY_SET_RADIOBUTTON(align, nothingR, true);
    // an Alt-Az mount does not flip
    KTRY_SET_CHECKBOX(Ekos::Manager::Instance()->mountModule(), executeMeridianFlip, false);

    auto camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_CaptureHelper->m_primaryTrain);
    QVERIFY(camera != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(camera->getNumber("CCD_EXPOSURE").isValid(), 10000);
    if (!camera->getNumber("SIM_FIELD_ROTATION").isValid())
        QSKIP("The CCD simulator does not simulate field rotation, the INDI drivers are too old.");

    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->getRotatorController(m_CaptureHelper->m_RotatorDevice,
                             m_RotatorSettings), 10000);
    // Capture initialized the rotator controller for the primary train
    QTRY_COMPARE_WITH_TIMEOUT(m_RotatorSettings->RotatorName->text(), m_CaptureHelper->m_RotatorDevice, 10000);
    connect(m_RotatorSettings.data(), &RotatorSettings::newLog, this, [this](const QString & text)
    {
        m_DerotationLog.append(text);
    });

    QVERIFY(setMountType(true));

    // Step the clock instead of following the system clock. It runs on at the normal rate between steps.
    SimClock *clock = KStarsData::Instance()->clock();
    m_WasRealTime = clock->isRealTime();
    clock->setRealTime(false);
    if (!clock->isActive())
        clock->start();

    connect(align, &Ekos::Align::newStatus, this, &TestEkosFieldDerotation::alignStatusChanged, Qt::UniqueConnection);
    connect(align, &Ekos::Align::newLog, this, &TestEkosFieldDerotation::alignLogReceived, Qt::UniqueConnection);
    connect(align, &Ekos::Align::newSolverResults, this, &TestEkosFieldDerotation::solverResultReceived,
            Qt::UniqueConnection);
}

void TestEkosFieldDerotation::cleanupTestCase()
{
    Ekos::Align *align = Ekos::Manager::Instance()->alignModule();
    disconnect(align, &Ekos::Align::newStatus, this, &TestEkosFieldDerotation::alignStatusChanged);
    disconnect(align, &Ekos::Align::newLog, this, &TestEkosFieldDerotation::alignLogReceived);
    disconnect(align, &Ekos::Align::newSolverResults, this, &TestEkosFieldDerotation::solverResultReceived);

    KStarsData::Instance()->clock()->setRealTime(m_WasRealTime);
    if (m_RotatorSettings)
    {
        m_RotatorSettings->derotateField->setChecked(false);
        m_RotatorSettings->disconnect(this);
        // the telescope simulator saves its mount type
        QVERIFY(setMountType(false));
    }

    m_CaptureHelper->cleanup();
    QVERIFY(m_CaptureHelper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosFieldDerotation::init()
{
    m_AlignLog.clear();
    m_DerotationLog.clear();
    // one hour before the upper culmination of the test field
    QVERIFY(setHourAngle(-1.0));
    QVERIFY(slewToTestField());
}

void TestEkosFieldDerotation::cleanup()
{
    QVERIFY(setDerotation(false));
}

void TestEkosFieldDerotation::alignStatusChanged(Ekos::AlignState status)
{
    m_AlignStatus = status;
}

void TestEkosFieldDerotation::alignLogReceived(const QString &text)
{
    m_AlignLog.append(text);
}

void TestEkosFieldDerotation::solverResultReceived(double orientation, double ra, double dec, double pixscale)
{
    Q_UNUSED(ra);
    Q_UNUSED(dec);
    Q_UNUSED(pixscale);
    m_SolverCount++;
    // Align reports the position angle of the camera
    m_SolvedPA = orientation;
}

QTEST_KSTARS_MAIN(TestEkosFieldDerotation)

#endif // HAVE_INDI
