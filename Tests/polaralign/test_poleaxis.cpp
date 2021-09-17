/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "test_poleaxis.h"
#include "ksnumbers.h"
#include "time/kstarsdatetime.h"
#include "auxiliary/dms.h"
#include "Options.h"
#include <libnova/libnova.h>

TestPoleAxis::TestPoleAxis() : QObject()
{
}

TestPoleAxis::~TestPoleAxis()
{
}

void TestPoleAxis::compare(Rotations::V3 v, double x, double y, double z)
{
    QVERIFY2(std::fabs(v.x() - x) < 0.00001,
             qPrintable(QString("dc.x %1, x %2 error %3").arg(v.x()).arg(x).arg(((v.x() - x) * 3600.0), 6, 'f', 1)));
    QVERIFY2(std::fabs(v.y() - y) < 0.00001,
             qPrintable(QString("dc.y %1, y %2 error %3").arg(v.y()).arg(y).arg(((v.y() - y) * 3600.0), 6, 'f', 1)));
    QVERIFY2(std::fabs(v.z() - z) < 0.00001,
             qPrintable(QString("dc.z %1, z %2 error %3").arg(v.z()).arg(z).arg(((v.z() - z) * 3600.0), 6, 'f', 1)));
}

void TestPoleAxis::compare(double a, double e, QString msg)
{
    QVERIFY2(std::fabs(a - e) < 0.0003,
             qPrintable(QString("%1: actual %2, expected %3 error %4").arg(msg).arg(a).arg(e).arg(((a - e) * 3600.0), 6, 'f', 1)));
}


void TestPoleAxis::testDirCos_data()
{
    QTest::addColumn<double>("Ha");
    QTest::addColumn<double>("Dec");
    QTest::addColumn<double>("X");
    QTest::addColumn<double>("Y");
    QTest::addColumn<double>("Z");

    QTest::newRow("HaDec0") << 0.0 << 0.0  << 1.0 << 0.0 << 0.0;
    QTest::newRow("Ha0Dec45") << 0.0 << 45.0  << 0.707107 << 0.0 << 0.707107;
    QTest::newRow("Ha6Dec45") << 6.0 << 45.0 << 0.0 << 0.707107 << 0.707107;
    QTest::newRow("Ha-6Dec45") << -6.0 << 45.0 << 0.0 << -0.707107 << 0.707107;
    QTest::newRow("at Pole") << 0.0 << 90.0 << 0.0 << 0.0 << 1.0;
    QTest::newRow("near S Pole") << -3.0 << -85.0 << 0.0616284 << -0.0616284 << -0.996195;
}

void TestPoleAxis::testDirCos()
{
    dms h;
    dms d;
    QFETCH(double, Ha);
    QFETCH(double, Dec);
    h.setH(Ha);
    d.setD(Dec);


    Rotations::V3 dc;
    dc = PoleAxis::dirCos(h, d);


    QFETCH(double, X);
    QFETCH(double, Y);
    QFETCH(double, Z);

    compare(dc, X, Y, Z);

    SkyPoint sp(Ha, Dec);
    dc = PoleAxis::dirCos(sp);
    compare(dc.length(), 1.0, "length");
    compare(dc, X, Y, Z);
}

void TestPoleAxis::testPriSec_data()
{
    testDirCos_data();
}

void TestPoleAxis::testPriSec()
{
    QFETCH(double, Ha);
    QFETCH(double, Dec);
    QFETCH(double, X);
    QFETCH(double, Y);
    QFETCH(double, Z);
    Rotations::V3 dc(X, Y, Z);
    dms p = PoleAxis::primary(dc);
    compare(p.HoursHa(), Ha, "Ha");
    compare(PoleAxis::secondary(dc).Degrees(), Dec, "Dec");
}


void TestPoleAxis::testPoleAxis_data()
{
    QTest::addColumn<double>("Ha1");
    QTest::addColumn<double>("Dec1");
    QTest::addColumn<double>("Ha2");
    QTest::addColumn<double>("Dec2");
    QTest::addColumn<double>("Ha3");
    QTest::addColumn<double>("Dec3");
    QTest::addColumn<double>("X");
    QTest::addColumn<double>("Y");
    QTest::addColumn<double>("Z");

    QTest::newRow("Ha-606Dec0") << -6.0 << 0.0 << 0.0 << 0.0  << 6.0 << 0.0  << 0.0 << 0.0 << 1.0;
    QTest::newRow("Ha20-2Dec89") << 2.0 << 89.0 << 0.0 << 89.0  << -2.0 << 89.0  << 0.0 << 0.0 << -1.0;
    QTest::newRow("Ha0-22Dec89v") << 0.0 << 89.0 << -2.0 << 89.1  << 2.0 << 88.9  << -0.0006 << -0.003386 << -0.99999;
    QTest::newRow("Ha2-20Dec-89") << 2.0 << -89.0  << -2.0 << -89.0 << 0.0 << -89.0  << 0.0 << 0.0 << 1.0;
    QTest::newRow("Ha20-2Dec89") << 2.0 << 89.0  << 0.0 << 89.0 << -2.0 << 88.0  << 0.05633683 << 0.0150954 << 0.998298;
    // failure cases, 2 or more points the same should ruturn a null matrix
    QTest::newRow("Ha000Dec0") << 0.0 << 0.0 << 0.0 << 0.0  << 0.0 << 0.0  << 0.0 << 0.0 << 0.0;
    QTest::newRow("Ha100Dec0") << 1.0 << 0.0 << 0.0 << 0.0  << 0.0 << 0.0  << 0.0 << 0.0 << 0.0;
    QTest::newRow("Ha110Dec0") << 1.0 << 0.0 << 1.0 << 0.0  << 0.0 << 0.0  << 0.0 << 0.0 << 0.0;
    QTest::newRow("Ha011Dec0") << 0.0 << 0.0 << 1.0 << 0.0  << 1.0 << 0.0  << 0.0 << 0.0 << 0.0;
}

void TestPoleAxis::testPoleAxis()
{
    QFETCH(double, Ha1);
    QFETCH(double, Dec1);
    QFETCH(double, Ha2);
    QFETCH(double, Dec2);
    QFETCH(double, Ha3);
    QFETCH(double, Dec3);

    QFETCH(double, X);
    QFETCH(double, Y);
    QFETCH(double, Z);

    SkyPoint p1(Ha1, Dec1);
    SkyPoint p2(Ha2, Dec2);
    SkyPoint p3(Ha3, Dec3);


    Rotations::V3 pa = PoleAxis::poleAxis(p1, p2, p3);

    compare(pa.x(), X, "X");
    compare(pa.y(), Y, "Y");
    compare(pa.z(), Z, "Z");
}

QTEST_GUILESS_MAIN(TestPoleAxis)
