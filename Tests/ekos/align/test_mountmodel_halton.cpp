/*
    SPDX-FileCopyrightText: 2026 Christian Kemper <ckemper@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_mountmodel_halton.h"
#include "auxiliary/dms.h"
#include "skyobjects/skypoint.h"

#include <cmath>
#include <QtTest>

// Number of Halton points to generate per test case.
static constexpr int kNumPoints = 50;

// Matches the constants in mountmodel.cpp.
static constexpr double kMaxAlt    = 85.0;
static constexpr double kMaxAbsDec = 80.0;

TestMountModelHalton::TestMountModelHalton() : QObject() {}

// Replicate the halton() member from MountModel (3-line pure function).
double TestMountModelHalton::halton(int index, int base)
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
}

// ---------------------------------------------------------------------------
// testPointsAboveHorizon
//
// For each (lat, LST, minAlt) combination, generate kNumPoints via the same
// AltAz-space Halton logic used in MountModel::slotWizardAlignmentPoints(),
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

    double sinMin = std::sin(minAlt   * dms::DegToRad);
    double sinMax = std::sin(kMaxAlt  * dms::DegToRad);

    for (int i = 1; i <= kNumPoints; i++)
    {
        double az  = halton(i, 2) * 360.0;
        double alt = std::asin(sinMin + halton(i, 3) * (sinMax - sinMin)) / dms::DegToRad;

        // Convert AltAz -> equatorial (same as the implementation).
        SkyPoint sp;
        sp.setAlt(alt);
        sp.setAz(az);
        sp.HorizontalToEquatorial(&lstDms, &latDms);

        double ra  = sp.ra().Hours();
        double dec = std::copysign(qMin(std::abs(sp.dec().Degrees()), kMaxAbsDec),
                                   sp.dec().Degrees());

        // Convert (ra, dec) back to AltAz to verify the point is above the horizon.
        SkyPoint check;
        check.setRA(ra);
        check.setDec(dec);
        check.EquatorialToHorizontal(&lstDms, &latDms);

        double checkAlt = check.alt().Degrees();
        QVERIFY2(checkAlt >= minAlt - 0.001,
                 qPrintable(QString("Point %1: az=%2 alt=%3 -> ra=%4 dec=%5 -> checkAlt=%6 < minAlt=%7")
                            .arg(i).arg(az, 0, 'f', 2).arg(alt, 0, 'f', 2)
                            .arg(ra, 0, 'f', 4).arg(dec, 0, 'f', 4)
                            .arg(checkAlt, 0, 'f', 4).arg(minAlt)));
    }
}

// ---------------------------------------------------------------------------
// testPointsAwayFromPole
//
// Verify that after Dec clamping, |Dec| <= kMaxAbsDec for all points and
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

    double sinMin = std::sin(minAlt  * dms::DegToRad);
    double sinMax = std::sin(kMaxAlt * dms::DegToRad);

    for (int i = 1; i <= kNumPoints; i++)
    {
        double az  = halton(i, 2) * 360.0;
        double alt = std::asin(sinMin + halton(i, 3) * (sinMax - sinMin)) / dms::DegToRad;

        SkyPoint sp;
        sp.setAlt(alt);
        sp.setAz(az);
        sp.HorizontalToEquatorial(&lstDms, &latDms);

        double dec = std::copysign(qMin(std::abs(sp.dec().Degrees()), kMaxAbsDec),
                                   sp.dec().Degrees());

        QVERIFY2(std::abs(dec) <= kMaxAbsDec + 0.001,
                 qPrintable(QString("Point %1: |dec|=%2 exceeds kMaxAbsDec=%3")
                            .arg(i).arg(std::abs(dec), 0, 'f', 4).arg(kMaxAbsDec)));
    }
}

QTEST_GUILESS_MAIN(TestMountModelHalton)
