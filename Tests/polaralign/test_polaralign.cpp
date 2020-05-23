/***************************************************************************
                   test_polaralign.cpp  -  KStars Planetarium
                             -------------------
    begin                : Tue 27 Sep 2016 20:54:28 CDT
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/* Project Includes */
#include "test_polaralign.h"
#include "ksnumbers.h"
#include "time/kstarsdatetime.h"
#include "auxiliary/dms.h"
#include "Options.h"
#include <libnova/libnova.h>

TestPolarAlign::TestPolarAlign() : QObject()
{
}

TestPolarAlign::~TestPolarAlign()
{
}

void TestPolarAlign::compare(QVector3D v, double x, double y, double z)
{
    QVERIFY2(std::fabs(v.x() - x) < 0.00001,
             qPrintable(QString("dc.x %1, x %2 error %3").arg(v.x()).arg(x).arg(((v.x() - x) * 3600.0), 6,'f', 1)));
    QVERIFY2(std::fabs(v.y() - y) < 0.00001,
             qPrintable(QString("dc.y %1, y %2 error %3").arg(v.y()).arg(y).arg(((v.y() - y) * 3600.0), 6,'f', 1)));
    QVERIFY2(std::fabs(v.z() - z) < 0.00001,
             qPrintable(QString("dc.z %1, z %2 error %3").arg(v.z()).arg(z).arg(((v.z() - z) * 3600.0), 6,'f', 1)));
}

void TestPolarAlign::compare(double a, double e, QString msg)
{
    QVERIFY2(std::fabs(a - e) < 0.0003,
             qPrintable(QString("%1: actual %2, expected %3 error %4").arg(msg).arg(a).arg(e).arg(((a - e) * 3600.0), 6,'f', 1)));
}


void TestPolarAlign::testDirCos_data()
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

void TestPolarAlign::testDirCos()
{
    dms h;
    dms d;
    QFETCH(double, Ha);
    QFETCH(double, Dec);
    h.setH(Ha);
    d.setD(Dec);


    QVector3D dc;
    dc = PolarAlign::dirCos(h, d);


    QFETCH(double, X);
    QFETCH(double, Y);
    QFETCH(double, Z);

    compare(dc, X, Y, Z);

    SkyPoint sp(Ha, Dec);
    dc = PolarAlign::dirCos(sp);
    QCOMPARE(dc.length(), 1.0);
    compare(dc, X, Y, Z);
}

void TestPolarAlign::testPriSec_data()
{
    testDirCos_data();
}

void TestPolarAlign::testPriSec()
{
    QFETCH(double, Ha);
    QFETCH(double, Dec);
    QFETCH(double, X);
    QFETCH(double, Y);
    QFETCH(double, Z);
    QVector3D dc(X, Y, Z);
    dms p = PolarAlign::primary(dc);
    compare(p.HoursHa(), Ha, "Ha");
    compare(PolarAlign::secondary(dc).Degrees(), Dec, "Dec");
}


void TestPolarAlign::testPoleAxis_data()
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

void TestPolarAlign::testPoleAxis()
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


    QVector3D pa = PolarAlign::poleAxis(p1, p2, p3);

    compare(pa.x(), X, "X");
    compare(pa.y(), Y, "Y");
    compare(pa.z(), Z, "Z");
}

QTEST_GUILESS_MAIN(TestPolarAlign)
