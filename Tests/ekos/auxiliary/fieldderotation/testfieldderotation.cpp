/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/auxiliary/fieldderotation.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "indi/clientmanager.h"
#include "indi/deviceinfo.h"
#include "indi/driverinfo.h"
#include "indi/indimount.h"
#include "indi/indistd.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobjects/skypoint.h"

#include <parentdevice.h>

#include "../../../testhelpers.h"

#include <QSignalSpy>
#include <QTest>

#include <cmath>

using Ekos::FieldDerotation;

class TestFieldDerotation : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void cleanup();

        // Parallactic angle
        void testParallacticAngleKnownValues_data();
        void testParallacticAngleKnownValues();
        void testParallacticAngleIsZenithPA_data();
        void testParallacticAngleIsZenithPA();

        // Mount type
        void testMountTypeFromSwitchName();
        void testMountTypeProperty();

        // RotatorUtils offset model
        void testOffsetUnchangedForEquatorialMount();
        void testOffsetFollowsParallacticAngle();

        // Decision logic
        void testFirstTrackingSampleHoldsPA();
        void testNoCorrectionWhileNotTracking();
        void testSmallErrorsAccumulate();
        void testNoCorrectionWhileRotatorBusy();
        void testErrorWrapsAround180();
        void testLargeErrorIsHeldNotChased();
        void testSlewToNewTargetHoldsNewPA();
        void testRecenterKeepsHeldPA();
        void testExplicitHold();

        // Closed loop against a simulated Alt-Az mount and rotator
        void testClosedLoop_data();
        void testClosedLoop();
        void testClosedLoopWithRecentering();

    private:
        QSharedPointer<ISD::GenericDevice> createDevice(const QString &name);
        void resetRotatorUtils();

        ClientManager m_ClientManager;
};

namespace
{
constexpr double SiderealRate = 360.98564736629 / 86400.0; // degrees of hour angle per second

// Rotator that moves at a constant speed along the shortest path, like a real device
struct SimulatedRotator
{
    double angle { 0 };
    double target { 0 };
    double speed { 3 }; // degrees per second
    bool busy { false };

    void command(double newTarget)
    {
        target = KSUtils::range360(newTarget);
        busy = true;
    }
    void advance(double dt)
    {
        if (!busy)
            return;
        const double diff = KSUtils::rangePA(target - angle);
        const double step = speed * dt;
        if (std::abs(diff) <= step)
        {
            angle = target;
            busy = false;
        }
        else
            angle = KSUtils::range360(angle + (diff > 0 ? step : -step));
    }
};

// The camera rides on an Alt-Az mount, so its sky PA is its angle to the zenith (fixed by the
// rotator) plus the parallactic angle. Sign is +1 when KStars' normal rotator model applies,
// -1 when the rotator turns the image the other way (reversed driver or parity).
double trueCameraPA(double frameAngle, double parallacticAngle, double rotatorAngle, int sign)
{
    return KSUtils::rangePA(frameAngle + parallacticAngle + sign * rotatorAngle);
}
}

void TestFieldDerotation::initTestCase()
{
    KTEST_BEGIN();
    QVERIFY(KStarsData::Create() != nullptr);
}

void TestFieldDerotation::cleanupTestCase()
{
    KTEST_END();
}

void TestFieldDerotation::cleanup()
{
    resetRotatorUtils();
}

void TestFieldDerotation::resetRotatorUtils()
{
    auto utils = RotatorUtils::Instance();
    utils->setAltAz(false);
    utils->setReversed(false);
    utils->revertParity();
    utils->updateOffset(0);
}

QSharedPointer<ISD::GenericDevice> TestFieldDerotation::createDevice(const QString &name)
{
    QSharedPointer<DriverInfo> driver(new DriverInfo(name));
    INDI::ParentDevice baseDevice(INDI::ParentDevice::Valid);
    baseDevice.setDeviceName(name.toLatin1().constData());
    QSharedPointer<DeviceInfo> deviceInfo(new DeviceInfo(driver, baseDevice));
    return QSharedPointer<ISD::GenericDevice>(new ISD::GenericDevice(deviceInfo, &m_ClientManager));
}

void TestFieldDerotation::testParallacticAngleKnownValues_data()
{
    QTest::addColumn<double>("hourAngle");
    QTest::addColumn<double>("declination");
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("expected");

    // On the meridian south of the zenith the zenith is due north of the target
    QTest::newRow("meridian-south") << 0.0 << 20.0 << 45.0 << 0.0;
    // On the meridian between zenith and pole the zenith is due south
    QTest::newRow("meridian-north") << 0.0 << 60.0 << 45.0 << 180.0;
    // On the equator, a target on the celestial equator has the zenith due east (west of meridian)
    QTest::newRow("equator-west") << 45.0 << 0.0 << 0.0 << 90.0;
    QTest::newRow("equator-east") << -45.0 << 0.0 << 0.0 << -90.0;
    // Southern hemisphere, target on the meridian north of the zenith: zenith is due south
    QTest::newRow("south-meridian-north") << 0.0 << 0.0 << -33.0 << 180.0;
}

void TestFieldDerotation::testParallacticAngleKnownValues()
{
    QFETCH(double, hourAngle);
    QFETCH(double, declination);
    QFETCH(double, latitude);
    QFETCH(double, expected);

    const double q = FieldDerotation::parallacticAngle(hourAngle, declination, latitude);
    QVERIFY2(std::abs(KSUtils::rangePA(q - expected)) < 1e-9, qPrintable(QString("q = %1").arg(q)));
    // Mirror symmetry about the meridian
    const double mirrored = FieldDerotation::parallacticAngle(-hourAngle, declination, latitude);
    QVERIFY(std::abs(KSUtils::rangePA(q + mirrored)) < 1e-9 || std::abs(std::abs(q) - 180) < 1e-9);
}

void TestFieldDerotation::testParallacticAngleIsZenithPA_data()
{
    QTest::addColumn<double>("hourAngle");
    QTest::addColumn<double>("declination");
    QTest::addColumn<double>("latitude");

    // Targets well above the horizon and away from the zenith, where the finite difference is accurate
    const QList<double> latitudes = { -33.0, 0.0, 45.0, 64.0 };
    const QList<double> declinations = { -60.0, -20.0, 0.0, 30.0, 70.0 };
    const QList<double> hourAngles = { -100.0, -45.0, -5.0, 10.0, 60.0, 150.0 };
    for (double latitude : latitudes)
        for (double declination : declinations)
            for (double hourAngle : hourAngles)
            {
                const double h = hourAngle * M_PI / 180, d = declination * M_PI / 180, p = latitude * M_PI / 180;
                const double alt = std::asin(std::sin(p) * std::sin(d) + std::cos(p) * std::cos(d) * std::cos(h)) * 180 / M_PI;
                if (alt > 10 && alt < 85)
                    QTest::addRow("lat%g-dec%g-ha%g", latitude, declination, hourAngle) << hourAngle << declination << latitude;
            }
}

// Independent check: compute the position angle of the zenith direction with the KStars
// coordinate transforms and compare it to the closed formula.
void TestFieldDerotation::testParallacticAngleIsZenithPA()
{
    QFETCH(double, hourAngle);
    QFETCH(double, declination);
    QFETCH(double, latitude);

    const dms lat(latitude);
    const dms lst(100.0);
    SkyPoint target(dms(KSUtils::range360(lst.Degrees() - hourAngle)), dms(declination));
    target.EquatorialToHorizontal(&lst, &lat);

    SkyPoint towardsZenith;
    towardsZenith.setAz(target.az());
    towardsZenith.setAlt(target.alt().Degrees() + 0.01);
    towardsZenith.HorizontalToEquatorial(&lst, &lat);

    double zenithPA = 0;
    target.angularDistanceTo(&towardsZenith, &zenithPA);

    const double q = FieldDerotation::parallacticAngle(hourAngle, declination, latitude);
    QVERIFY2(std::abs(KSUtils::rangePA(q - zenithPA)) < 0.05,
             qPrintable(QString("q = %1, zenith PA = %2, alt = %3").arg(q).arg(zenithPA).arg(target.alt().Degrees())));
}

void TestFieldDerotation::testMountTypeFromSwitchName()
{
    QCOMPARE(ISD::Mount::mountTypeFromSwitchName("ALTAZ"), ISD::Mount::MOUNT_TYPE_ALTAZ);
    QCOMPARE(ISD::Mount::mountTypeFromSwitchName("EQ_FORK"), ISD::Mount::MOUNT_TYPE_EQ_FORK);
    QCOMPARE(ISD::Mount::mountTypeFromSwitchName("EQ_GEM"), ISD::Mount::MOUNT_TYPE_EQ_GEM);
    QCOMPARE(ISD::Mount::mountTypeFromSwitchName("SOMETHING"), ISD::Mount::MOUNT_TYPE_UNKNOWN);
}

void TestFieldDerotation::testMountTypeProperty()
{
    auto device = createDevice("Test Mount");
    ISD::Mount mount(device.get());
    QCOMPARE(mount.mountType(), ISD::Mount::MOUNT_TYPE_UNKNOWN);
    QVERIFY(!mount.isAltAz());

    INDI::PropertySwitch mountType {3};
    mountType[0].fill("ALTAZ", "ALTAZ", ISS_OFF);
    mountType[1].fill("EQ_FORK", "Fork (Eq)", ISS_OFF);
    mountType[2].fill("EQ_GEM", "GEM", ISS_ON);
    mountType.fill("Test Mount", "TELESCOPE_MOUNT_TYPE", "Mount Type", "Motion", IP_RO, ISR_1OFMANY, 60, IPS_IDLE);

    QSignalSpy spy(&mount, &ISD::Mount::mountTypeChanged);
    mount.registerProperty(mountType);
    QCOMPARE(mount.mountType(), ISD::Mount::MOUNT_TYPE_EQ_GEM);
    QCOMPARE(spy.count(), 1);

    // Simulator switched to Alt-Az
    mountType[0].setState(ISS_ON);
    mountType[2].setState(ISS_OFF);
    mount.processSwitch(mountType);
    QVERIFY(mount.isAltAz());
    QCOMPARE(spy.count(), 2);

    // Same value again does not emit
    mount.processSwitch(mountType);
    QCOMPARE(spy.count(), 2);
}

void TestFieldDerotation::testOffsetUnchangedForEquatorialMount()
{
    auto utils = RotatorUtils::Instance();
    utils->setAltAz(false);
    utils->updateOffset(10);
    utils->updateParallacticAngle(20);
    utils->updateParallacticAngle(40);
    QCOMPARE(utils->offset(), 10.0);
}

void TestFieldDerotation::testOffsetFollowsParallacticAngle()
{
    auto utils = RotatorUtils::Instance();
    utils->updateOffset(10);
    QSignalSpy spy(utils, &RotatorUtils::altAzChanged);
    utils->setAltAz(true);
    QCOMPARE(spy.count(), 1);

    // The first sample takes the current offset as valid
    utils->updateParallacticAngle(20);
    QCOMPARE(utils->offset(), 10.0);
    // An unmoved camera turns with the parallactic angle
    const double rotatorAngle = 30;
    const double pa0 = utils->calcCameraAngle(rotatorAngle, false);
    utils->updateParallacticAngle(25);
    QCOMPARE(utils->offset(), 15.0);
    QCOMPARE(utils->calcCameraAngle(rotatorAngle, false), pa0 + 5);

    // A plate solve recalibrates at the current parallactic angle
    utils->updateOffset(40);
    utils->updateParallacticAngle(30);
    QCOMPARE(utils->offset(), 45.0);

    // Wraps into the PA range
    utils->updateParallacticAngle(170);
    QCOMPARE(utils->offset(), -175.0);

    // Switching back to equatorial freezes the offset
    utils->setAltAz(false);
    utils->updateParallacticAngle(0);
    QCOMPARE(utils->offset(), -175.0);
}

namespace
{
FieldDerotation::Sample sample(double pa, bool tracking = true, bool idle = true, double ra = 150, double dec = 13)
{
    FieldDerotation::Sample s;
    s.cameraPA = pa;
    s.tracking = tracking;
    s.rotatorIdle = idle;
    s.ra = ra;
    s.dec = dec;
    return s;
}
}

void TestFieldDerotation::testFirstTrackingSampleHoldsPA()
{
    FieldDerotation derotation;
    QVERIFY(!derotation.hasHold());
    QCOMPARE(derotation.update(sample(42)).action, FieldDerotation::NONE);
    QVERIFY(derotation.hasHold());
    QCOMPARE(derotation.heldPA(), 42.0);
}

void TestFieldDerotation::testNoCorrectionWhileNotTracking()
{
    FieldDerotation derotation;
    derotation.update(sample(42));
    QCOMPARE(derotation.update(sample(45, false)).action, FieldDerotation::NONE);
    QCOMPARE(derotation.heldPA(), 42.0);
}

// Regression for MR !1490: the reference was reset on every capture job and every target,
// so rotation below the threshold was discarded and the field drifted.
void TestFieldDerotation::testSmallErrorsAccumulate()
{
    FieldDerotation derotation;
    derotation.setThreshold(0.1);
    derotation.update(sample(0));

    int steps = 0;
    FieldDerotation::Decision decision;
    for (steps = 1; steps < 100; steps++)
    {
        decision = derotation.update(sample(steps * 0.004));
        if (decision.action != FieldDerotation::NONE)
            break;
    }
    QCOMPARE(decision.action, FieldDerotation::ROTATE);
    QCOMPARE(steps, 25);
    QCOMPARE(decision.targetPA, 0.0);
}

void TestFieldDerotation::testNoCorrectionWhileRotatorBusy()
{
    FieldDerotation derotation;
    derotation.update(sample(0));
    QCOMPARE(derotation.update(sample(1, true, false)).action, FieldDerotation::NONE);
    QCOMPARE(derotation.update(sample(1, true, true)).action, FieldDerotation::ROTATE);
}

void TestFieldDerotation::testErrorWrapsAround180()
{
    FieldDerotation derotation;
    derotation.setThreshold(0.1);
    derotation.update(sample(179.9));
    auto decision = derotation.update(sample(-179.9));
    QCOMPARE(decision.action, FieldDerotation::ROTATE);
    QVERIFY(std::abs(decision.error - 0.2) < 1e-9);
    QCOMPARE(decision.targetPA, 179.9);
}

void TestFieldDerotation::testLargeErrorIsHeldNotChased()
{
    FieldDerotation derotation;
    derotation.setMaximumCorrection(10);
    derotation.update(sample(0));
    auto decision = derotation.update(sample(30));
    QCOMPARE(decision.action, FieldDerotation::LIMIT_EXCEEDED);
    QCOMPARE(derotation.heldPA(), 30.0);
    QCOMPARE(derotation.update(sample(30)).action, FieldDerotation::NONE);
}

void TestFieldDerotation::testSlewToNewTargetHoldsNewPA()
{
    FieldDerotation derotation;
    derotation.update(sample(0, true, true, 150, 13));
    derotation.update(sample(0, false, true, 160, 20));
    // The slew rotated the field; the new PA is held, no rotation right after the slew
    QCOMPARE(derotation.update(sample(25, true, true, 170, 30)).action, FieldDerotation::NONE);
    QCOMPARE(derotation.heldPA(), 25.0);
}

void TestFieldDerotation::testRecenterKeepsHeldPA()
{
    FieldDerotation derotation;
    derotation.update(sample(0, true, true, 150, 13));
    derotation.update(sample(0.2, false, true, 150.02, 13.01));
    auto decision = derotation.update(sample(0.5, true, true, 150.05, 13.02));
    QCOMPARE(decision.action, FieldDerotation::ROTATE);
    QCOMPARE(decision.targetPA, 0.0);
}

void TestFieldDerotation::testExplicitHold()
{
    FieldDerotation derotation;
    derotation.update(sample(0));
    derotation.hold(5, 150, 13);
    auto decision = derotation.update(sample(0));
    QCOMPARE(decision.action, FieldDerotation::ROTATE);
    QCOMPARE(decision.targetPA, 5.0);

    derotation.clearHold();
    QCOMPARE(derotation.update(sample(0)).action, FieldDerotation::NONE);
    QCOMPARE(derotation.heldPA(), 0.0);
}

void TestFieldDerotation::testClosedLoop_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("declination");
    QTest::addColumn<double>("startHourAngle"); // degrees
    QTest::addColumn<int>("duration");          // seconds
    QTest::addColumn<bool>("driverReversed");
    QTest::addColumn<bool>("parityReversed");

    // M66 setting in the west, as in one of the field reports
    QTest::newRow("west-normal") << 48.0 << 13.0 << 15.0 << 3600 << false << false;
    QTest::newRow("west-driver-reversed") << 48.0 << 13.0 << 15.0 << 3600 << true << false;
    QTest::newRow("west-parity-reversed") << 48.0 << 13.0 << 15.0 << 3600 << false << true;
    QTest::newRow("west-both-reversed") << 48.0 << 13.0 << 15.0 << 3600 << true << true;
    // Transit between zenith and pole: the parallactic angle crosses +/-180
    QTest::newRow("transit-north") << 45.0 << 60.0 << -7.5 << 3600 << false << false;
    // Transit 5 degrees from the zenith: fast field rotation
    QTest::newRow("near-zenith") << 45.0 << 50.0 << -5.0 << 2400 << false << true;
    QTest::newRow("southern-east") << -33.0 << -60.0 << -30.0 << 3600 << true << false;
}

void TestFieldDerotation::testClosedLoop()
{
    QFETCH(double, latitude);
    QFETCH(double, declination);
    QFETCH(double, startHourAngle);
    QFETCH(int, duration);
    QFETCH(bool, driverReversed);
    QFETCH(bool, parityReversed);

    auto utils = RotatorUtils::Instance();
    utils->setReversed(driverReversed);
    if (parityReversed)
        utils->trialToggleParity();
    const int sign = (driverReversed != parityReversed) ? -1 : 1;
    utils->setAltAz(true);

    const double ra = 0;
    const double threshold = 0.1;
    const double frameAngle = 37; // camera to zenith angle at rotator zero, unknown to KStars
    const double dt = 1;
    auto q = [&](double t)
    {
        return FieldDerotation::parallacticAngle(startHourAngle + SiderealRate * t, declination, latitude);
    };

    SimulatedRotator rotator;
    rotator.angle = 100;

    // Calibrate like Align does after a plate solve
    utils->updateParallacticAngle(q(0));
    const double pa0 = trueCameraPA(frameAngle, q(0), rotator.angle, sign);
    utils->updateOffset(utils->calcOffsetAngle(rotator.angle, pa0));

    FieldDerotation derotation;
    derotation.setThreshold(threshold);
    derotation.hold(pa0, ra, declination);

    double maxError = 0, maxRate = 0, fieldRotation = 0;
    int corrections = 0;
    for (double t = dt; t <= duration; t += dt)
    {
        rotator.advance(dt);
        const double qt = q(t);
        maxRate = std::max(maxRate, std::abs(KSUtils::rangePA(qt - q(t - dt))) / dt);
        fieldRotation = std::max(fieldRotation, std::abs(KSUtils::rangePA(qt - q(0))));
        utils->updateParallacticAngle(qt);

        const double truePA = trueCameraPA(frameAngle, qt, rotator.angle, sign);
        const double estimatedPA = utils->calcCameraAngle(rotator.angle, false);
        QVERIFY2(std::abs(KSUtils::rangePA(estimatedPA - truePA)) < 1e-6,
                 qPrintable(QString("t=%1 estimated %2 true %3").arg(t).arg(estimatedPA).arg(truePA)));
        maxError = std::max(maxError, std::abs(KSUtils::rangePA(truePA - pa0)));

        auto decision = derotation.update(sample(estimatedPA, true, !rotator.busy, ra, declination));
        QVERIFY(decision.action != FieldDerotation::LIMIT_EXCEEDED);
        if (decision.action == FieldDerotation::ROTATE)
        {
            rotator.command(utils->calcRotatorAngle(decision.targetPA));
            corrections++;
        }
    }

    qInfo() << "Field rotation" << fieldRotation << "deg, max error" << maxError * 60 << "arcmin,"
            << corrections << "corrections, max rate" << maxRate * 3600 << "arcsec/s";
    // The scenario is meaningful: without derotation the field turns by several degrees
    QVERIFY(fieldRotation > 4);
    QVERIFY(corrections > 10);
    // With derotation the camera PA stays within the threshold plus one update of rotation
    QVERIFY2(maxError <= threshold + maxRate * dt + 1e-6, qPrintable(QString("max error %1").arg(maxError)));
}

// Periodic plate solves with re-centering, as in the unguided Dob workflow from the field reports.
// The solve reads the rotator angle seconds after the exposure; each solve recalibrates the offset
// and each re-centering slew pauses derotation. The error must not accumulate.
void TestFieldDerotation::testClosedLoopWithRecentering()
{
    const double latitude = 45, declination = 50, startHourAngle = -10;
    const double ra = 0, threshold = 0.1, frameAngle = -120, dt = 1;
    auto q = [&](double t)
    {
        return FieldDerotation::parallacticAngle(startHourAngle + SiderealRate * t, declination, latitude);
    };

    auto utils = RotatorUtils::Instance();
    utils->setAltAz(true);
    SimulatedRotator rotator;
    rotator.angle = 200;

    utils->updateParallacticAngle(q(0));
    const double pa0 = trueCameraPA(frameAngle, q(0), rotator.angle, 1);
    utils->updateOffset(utils->calcOffsetAngle(rotator.angle, pa0));

    FieldDerotation derotation;
    derotation.setThreshold(threshold);
    derotation.hold(pa0, ra, declination);

    // The first solve is the calibration above
    double maxError = 0, maxRate = 0, exposurePA = pa0;
    double dec = declination;
    const int slewDuration = 4;
    for (int t = 1; t <= 3600; t++)
    {
        rotator.advance(dt);
        const double qt = q(t);
        maxRate = std::max(maxRate, std::abs(KSUtils::rangePA(qt - q(t - dt))) / dt);
        utils->updateParallacticAngle(qt);
        const double truePA = trueCameraPA(frameAngle, qt, rotator.angle, 1);
        maxError = std::max(maxError, std::abs(KSUtils::rangePA(truePA - pa0)));

        const int phase = t % 120;
        // Exposure for the solve at phase 0, solution 8 s later with the rotator angle at that time
        if (phase == 0)
            exposurePA = truePA;
        if (phase == 8)
            utils->updateOffset(utils->calcOffsetAngle(rotator.angle, exposurePA));
        // Re-centering slew of a few arcminutes
        const bool tracking = !(phase >= 10 && phase < 10 + slewDuration);
        if (phase == 10)
            dec += (t / 120) % 2 ? 0.05 : -0.05;

        auto decision = derotation.update(sample(utils->calcCameraAngle(rotator.angle, false), tracking,
                                          !rotator.busy, ra, dec));
        if (decision.action == FieldDerotation::ROTATE)
            rotator.command(utils->calcRotatorAngle(decision.targetPA));
    }

    qInfo() << "Field rotation" << KSUtils::rangePA(q(3600) - q(0)) << "deg, max error" << maxError * 60 << "arcmin";
    QVERIFY(std::abs(KSUtils::rangePA(q(3600) - q(0))) > 20);
    // Error sources, none of which accumulate: the threshold, the uncorrected field rotation during a
    // re-centering slew, and the calibration error of a solve (the camera moves by up to the threshold
    // plus one update between exposure and solution)
    const double bound = threshold + maxRate * (slewDuration + dt) + threshold + maxRate * dt;
    QVERIFY2(maxError <= bound, qPrintable(QString("max error %1, bound %2").arg(maxError).arg(bound)));
    // For comparison: MR !1490 drifted by about 11 degrees in 33 minutes in this workflow
    QVERIFY(maxError < 0.5);
}

QTEST_MAIN(TestFieldDerotation)

#include "testfieldderotation.moc"
