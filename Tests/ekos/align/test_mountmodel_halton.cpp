/*
    SPDX-FileCopyrightText: 2026 Christian Kemper <ckemper@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_mountmodel_halton.h"
#include "auxiliary/dms.h"
#include "skyobjects/skypoint.h"
#include "../../kstars/skycomponents/linelist.h"
#include "../../kstars/skycomponents/artificialhorizoncomponent.h"
#include "../../kstars/ekos/align/haltonsequence.h"
#include "../../kstars/ekos/align/mountmodel.h"

#include <cmath>
#include <QtTest>

// Number of Halton points to generate per test case.
static constexpr int kNumPoints = 50;

// Matches the constants in mountmodel.cpp.
static constexpr double kMaxAlt    = 85.0;
static constexpr double kMaxAbsDec = 80.0;

TestMountModelHalton::TestMountModelHalton() : QObject() {}

// ---------------------------------------------------------------------------
// testPointsAboveHorizon
//
// For each (lat, LST, minAlt) combination, generate kNumPoints via MountModel::generateHaltonPoints(),
// then convert the stored (ra, dec) back to AltAz and verify alt >= minAlt.
// ---------------------------------------------------------------------------

void TestMountModelHalton::testPointsAboveHorizon_data()
{
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lst");   // hours
    QTest::addColumn<double>("minAlt");

    // Mid-northern latitudes
    QTest::newRow("lat=34 lst=0  minAlt=10") << 34.0 << 0.0  << 10.0;
    QTest::newRow("lat=34 lst=6  minAlt=20") << 34.0 << 6.0  << 20.0;
    QTest::newRow("lat=34 lst=12 minAlt=30") << 34.0 << 12.0 << 30.0;
    QTest::newRow("lat=34 lst=18 minAlt=30") << 34.0 << 18.0 << 30.0;

    // Mid-southern latitudes
    QTest::newRow("lat=-34 lst=0  minAlt=20") << -34.0 << 0.0  << 20.0;
    QTest::newRow("lat=-34 lst=12 minAlt=30") << -34.0 << 12.0 << 30.0;

    // Near-equatorial
    QTest::newRow("lat=0 lst=6 minAlt=20")  << 0.0  << 6.0  << 20.0;
    QTest::newRow("lat=5 lst=12 minAlt=20") << 5.0  << 12.0 << 20.0;

    // High northern latitude (NCP nearly overhead)
    QTest::newRow("lat=60 lst=0  minAlt=10") << 60.0 << 0.0  << 10.0;
    QTest::newRow("lat=60 lst=12 minAlt=20") << 60.0 << 12.0 << 20.0;

    // High southern latitude
    QTest::newRow("lat=-60 lst=6 minAlt=10") << -60.0 << 6.0 << 10.0;
}

void TestMountModelHalton::testPointsAboveHorizon()
{
    QFETCH(double, lat);
    QFETCH(double, lst);
    QFETCH(double, minAlt);

    dms latDms(lat);
    dms lstDms;
    lstDms.setH(lst);

    QVector<Ekos::MountModel::AlignmentPoint> points = Ekos::MountModel::generateHaltonPoints(
            kNumPoints,
            minAlt,
            kMaxAlt,
            kMaxAbsDec,
            lstDms,
            latDms,
            nullptr, // horizon
            false    // snap
        );

    QCOMPARE(points.size(), kNumPoints);

    for (int i = 0; i < points.size(); ++i)
    {
        dms raDms = dms::fromString(points[i].ra, false);
        dms decDms = dms::fromString(points[i].dec, true);

        // Convert (ra, dec) back to AltAz to verify the point is above the horizon.
        SkyPoint check;
        check.setRA(raDms.Hours());
        check.setDec(decDms.Degrees());
        check.EquatorialToHorizontal(&lstDms, &latDms);

        double checkAlt = check.alt().Degrees();
        QVERIFY2(checkAlt >= minAlt - 0.001,
                 qPrintable(QString("Point %1: ra=%2 dec=%3 -> checkAlt=%4 < minAlt=%5")
                            .arg(i + 1).arg(points[i].ra).arg(points[i].dec)
                            .arg(checkAlt, 0, 'f', 4).arg(minAlt)));
    }
}

// ---------------------------------------------------------------------------
// testPointsAwayFromPole
//
// Verify that after Dec filtering, |Dec| <= kMaxAbsDec for all points and
// all observer locations.
// ---------------------------------------------------------------------------

void TestMountModelHalton::testPointsAwayFromPole_data()
{
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lst");
    QTest::addColumn<double>("minAlt");

    QTest::newRow("lat=34  lst=0  minAlt=30") << 34.0  << 0.0  << 30.0;
    QTest::newRow("lat=-34 lst=12 minAlt=30") << -34.0 << 12.0 << 30.0;
    QTest::newRow("lat=60  lst=6  minAlt=10") << 60.0  << 6.0  << 10.0;
    QTest::newRow("lat=-60 lst=18 minAlt=10") << -60.0 << 18.0 << 10.0;
    QTest::newRow("lat=0   lst=0  minAlt=20") << 0.0   << 0.0  << 20.0;
}

void TestMountModelHalton::testPointsAwayFromPole()
{
    QFETCH(double, lat);
    QFETCH(double, lst);
    QFETCH(double, minAlt);

    dms latDms(lat);
    dms lstDms;
    lstDms.setH(lst);

    QVector<Ekos::MountModel::AlignmentPoint> points = Ekos::MountModel::generateHaltonPoints(
            kNumPoints,
            minAlt,
            kMaxAlt,
            kMaxAbsDec,
            lstDms,
            latDms,
            nullptr, // horizon
            false    // snap
        );

    QCOMPARE(points.size(), kNumPoints);

    for (int i = 0; i < points.size(); ++i)
    {
        dms decDms = dms::fromString(points[i].dec, true);
        double dec = decDms.Degrees();

        QVERIFY2(std::abs(dec) <= kMaxAbsDec + 0.001,
                 qPrintable(QString("Point %1: |dec|=%2 exceeds kMaxAbsDec=%3")
                            .arg(i + 1).arg(std::abs(dec), 0, 'f', 4).arg(kMaxAbsDec)));
    }
}

void TestMountModelHalton::testStatefulHaltonSequence()
{
    auto halton = [](int index, int base) -> double
    {
        double result = 0.0;
        double f      = 1.0;
        while (index > 0)
        {
            f /= static_cast<double>(base);
            result += (index % base) * f;
            index /= base;
        }
        return result;
    };

    Ekos::HaltonSequence hs2(2);
    Ekos::HaltonSequence hs3(3);
    for (int i = 1; i <= 100; ++i)
    {
        double val2_stateful = hs2.next();
        double val2_stateless = halton(i, 2);
        QCOMPARE(hs2.index(), i);
        QVERIFY2(std::abs(val2_stateful - val2_stateless) < 1e-9,
                 qPrintable(QString("Base 2 mismatch at index %1: stateful=%2, stateless=%3")
                            .arg(i).arg(val2_stateful).arg(val2_stateless)));

        double val3_stateful = hs3.next();
        double val3_stateless = halton(i, 3);
        QCOMPARE(hs3.index(), i);
        QVERIFY2(std::abs(val3_stateful - val3_stateless) < 1e-9,
                 qPrintable(QString("Base 3 mismatch at index %1: stateful=%2, stateless=%3")
                            .arg(i).arg(val3_stateful).arg(val3_stateless)));
    }
}

void TestMountModelHalton::testHorizonRejection()
{
    ArtificialHorizon horizon;
    horizon.setTesting();

    // Setup a blocked region: Azimuth 45 to 135 degrees, altitude up to 50 degrees
    std::shared_ptr<LineList> list(new LineList());

    auto p1 = std::make_shared<SkyPoint>();
    p1->setAz(dms(45.0));
    p1->setAlt(dms(50.0));
    list->append(p1);

    auto p2 = std::make_shared<SkyPoint>();
    p2->setAz(dms(135.0));
    p2->setAlt(dms(50.0));
    list->append(p2);

    horizon.addRegion("BlockedRegion", true, list, false);
    QVERIFY(horizon.altitudeConstraintsExist());

    // Point generation parameters
    constexpr double minAlt = 15.0;
    constexpr double maxAlt = 85.0;

    dms lstDms(12.0); // Arbitrary LST
    dms latDms(45.0); // Arbitrary latitude

    QVector<Ekos::MountModel::AlignmentPoint> points = Ekos::MountModel::generateHaltonPoints(
            100,
            minAlt,
            maxAlt,
            kMaxAbsDec,
            lstDms,
            latDms,
            &horizon,
            false    // snap
        );

    QCOMPARE(points.size(), 100);

    for (int i = 0; i < points.size(); ++i)
    {
        dms raDms = dms::fromString(points[i].ra, false);
        dms decDms = dms::fromString(points[i].dec, true);

        SkyPoint sp(raDms, decDms);
        sp.EquatorialToHorizontal(&lstDms, &latDms);

        double az = sp.az().Degrees();
        double alt = sp.alt().Degrees();

        // Ensure the generated point is NOT in the blocked zone.
        // The blocked zone is: Azimuth 45 to 135, altitude < 50.
        if (az >= 45.0 && az <= 135.0)
        {
            QVERIFY2(alt >= 50.0 - 0.001,
                     qPrintable(QString("Accepted point in blocked zone! az=%1, alt=%2")
                                .arg(az).arg(alt)));
        }
    }
}

QTEST_GUILESS_MAIN(TestMountModelHalton)
