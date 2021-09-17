/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * This file contains unit tests for the GreatCircle class.
 */

#include <QObject>
#include <QtTest>
#include <memory>

#include "greatcircle.h"

class TestGreatCircle : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestGreatCircle();

        /** @short Destructor */
        ~TestGreatCircle() override = default;

    private slots:
        void greatCircleTest_data();
        void greatCircleTest();

    private:
};

// This include must go after the class declaration.
#include "testgreatcircle.moc"

TestGreatCircle::TestGreatCircle() : QObject()
{
}

namespace
{

// Tests that the doubles are within tolerance.
bool compareFloat(double d1, double d2, double tolerance = .0001)
{
    return (fabs(d1 - d2) < tolerance);
}

}  // namespace

void TestGreatCircle::greatCircleTest_data()
{
    QTest::addColumn<double>("AZ1");
    QTest::addColumn<double>("ALT1");
    QTest::addColumn<double>("AZ2");
    QTest::addColumn<double>("ALT2");
    QTest::addColumn<double>("FRACTION");
    QTest::addColumn<double>("AZ_SOLUTION");
    QTest::addColumn<double>("ALT_SOLUTION");

    // These tests are actually longitude/latitude as opposed to az/alt.
    QTest::newRow("SF_TO_NYC_half")
            << -122.420 << 37.770 << -74.010 << 40.710 << 0.50 << -98.754805 << 41.842196;
    QTest::newRow("SF_TO_PARIS_half")
            << -122.420 << 37.770 <<   2.350 << 48.860 << 0.50 << -69.959876 << 63.476436;
    QTest::newRow("SF_TO_PARIS_0")
            << -122.420 << 37.770 <<   2.350 << 48.860 << 0.0  << -122.420   << 37.770;
    QTest::newRow("SF_TO_PARIS_1")
            << -122.420 << 37.770 <<   2.350 << 48.860 << 1.0  <<    2.350   << 48.860;
}

void TestGreatCircle::greatCircleTest()
{
    QFETCH(double, AZ1);
    QFETCH(double, ALT1);
    QFETCH(double, AZ2);
    QFETCH(double, ALT2);
    QFETCH(double, FRACTION);
    QFETCH(double, AZ_SOLUTION);
    QFETCH(double, ALT_SOLUTION);
    GreatCircle gc(AZ1, ALT1, AZ2, ALT2);
    double az, alt;
    gc.waypoint(FRACTION, &az, &alt);
    QVERIFY(compareFloat(az, AZ_SOLUTION));
    QVERIFY(compareFloat(alt, ALT_SOLUTION));
}

QTEST_GUILESS_MAIN(TestGreatCircle)
