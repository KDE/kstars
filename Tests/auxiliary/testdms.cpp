/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testdms.h"

#include "auxiliary/dms.h"

#include <QtTest>

TestDMS::TestDMS() : QObject()
{
}

void TestDMS::defaultCtor()
{
    /*
     * Test 1: Check Default Constructor
    */

    // Check default empty constructor
    dms d;
    QVERIFY(std::isnan(d.Degrees()));
}

void TestDMS::explicitSexigesimalCtor()
{
    /*
     * Test 2: Checks Sexigesimal Ctor
    */

    // HH:MM:SS
    // 14:55:20

    dms d(14, 55, 20);

    QVERIFY(d.degree() == 14);
    QVERIFY(d.arcmin() == 55);
    QVERIFY(d.arcsec() == 20);
    QVERIFY(qFuzzyCompare(d.Degrees(), (14.0 + 55.0 / 60.0 + 20.0 / 3600.0)));
}

void TestDMS::angleCtor()
{
    /*
     * Test 3: Checks Angle Ctor
    */

    // Angle = -112.56 Degrees ---> HMS (16:29:45)

    double angle = -112.56;

    dms d(angle);

    QVERIFY(d.degree() == (int)angle);

    QVERIFY(qFuzzyCompare(d.Hours(), (angle + 360) / 15.0));
    QVERIFY(d.hour() == 16);
    QVERIFY(d.minute() == 29);
    QVERIFY(d.second() == 45);
}

void TestDMS::stringCtor()
{
    QString hms("14:55:20");

    // From Degree
    dms d(hms);

    QVERIFY(d.degree() == 14);
    QVERIFY(d.arcmin() == 55);
    QVERIFY(d.arcsec() == 20);
    QVERIFY(qFuzzyCompare(d.Degrees(), (14.0 + 55.0 / 60.0 + 20.0 / 3600.0)));

    // From Hours
    dms h(hms, false);
    QVERIFY(qFuzzyCompare(h.Degrees(), d.Degrees() * 15.0));
    QVERIFY(qFuzzyCompare(h.Hours(), d.Degrees()));
}

void TestDMS::testReduceToRange()
{
    double base = 67.8;
    double a    = 360.0 * 11. + base;
    double b    = -360.0 * 12. + base;

    dms d;
    d.setD(a);
    d.reduceToRange(dms::ZERO_TO_2PI);
    QVERIFY(fabs(d.Degrees() - base) < 1e-9);

    d.setD(b);
    d.reduceToRange(dms::ZERO_TO_2PI);
    QVERIFY(fabs(d.Degrees() - base) < 1e-9);

    d.setD(360.0);
    d.reduceToRange(dms::ZERO_TO_2PI);
    QVERIFY(fabs(d.Degrees() - 0.) < 1e-9);

    double c = 180.0 * 13. + base;
    double e = 180.0 * 14. + base;
    double f = -180.0 * 15. + base;
    double g = -180.0 * 16. + base;

    d.setD(c);
    d.reduceToRange(dms::MINUSPI_TO_PI);
    QVERIFY(fabs(d.Degrees() - (base - 180.0)) < 1e-9);

    d.setD(e);
    d.reduceToRange(dms::MINUSPI_TO_PI);
    QVERIFY(fabs(d.Degrees() - base) < 1e-9);

    d.setD(f);
    d.reduceToRange(dms::MINUSPI_TO_PI);
    QVERIFY(fabs(d.Degrees() - (base - 180.0)) < 1e-9);

    d.setD(g);
    d.reduceToRange(dms::MINUSPI_TO_PI);
    QVERIFY(fabs(d.Degrees() - base) < 1e-9);
}

void TestDMS::testSubstraction()
{
    // Diff 359 and 1
    dms sub = dms(359) - dms(1);
    QVERIFY(sub.Degrees() == 358.);

    // The reverse is -358
    sub = dms(1) - dms(359);
    QVERIFY(sub.Degrees() == -358.);

    // Diff 100 and 300
    sub = dms(100) - dms(300);
    QVERIFY(sub.Degrees() == -200.0);

    // Diff 310 and 110 should be 200
    sub = dms(310) - dms(110);
    QVERIFY(sub.Degrees() == 200.0);

    // Diff 170 and 130 should be 40
    sub = dms(170) - dms(130);
    QVERIFY(sub.Degrees() == 40.0);

    // Reverse is -40
    sub = dms(130) - dms(170);
    QVERIFY(sub.Degrees() == -40.0);
}

void TestDMS::testDeltaAngle()
{
    // Diff 359 and 1 should be 2 (shortest path normalized)
    dms sub = dms(359).deltaAngle(dms(1));
    QVERIFY(sub.Degrees() == 2.);

    // Diff 1 to 350 is -11
    sub = dms(1).deltaAngle(dms(350));
    QVERIFY(sub.Degrees() == -11.);

    // Diff 310 and 110 should be 160
    sub = dms(310).deltaAngle(dms(110));
    QVERIFY(sub.Degrees() == 160.0);

    // Diff 100 and 300 should be -160 (NOT -200) since 160 is the shorest path CCW
    sub = dms(100).deltaAngle(dms(300));
    QVERIFY(sub.Degrees() == -160.0);
}

void TestDMS::testUnitTransition()
{
    // check for rounding/truncating errors around unit transition
    // in DMS and HMS angle representation strings
    dms sp; 
    sp.setD(10.0 - 1.0E-14);
    QVERIFY(sp.degree() == 9);
    QVERIFY(sp.arcmin() == 59);
    QVERIFY(sp.arcsec() == 59);
    QVERIFY(sp.marcsec() == 999);

    sp.setH(10.0 - 1.0E-14);
    QVERIFY(sp.hour() == 9);
    QVERIFY(sp.minute() == 59);
    QVERIFY(sp.second() == 59);
    QVERIFY(sp.msecond() == 999);
 
    sp.setD(10.0);
    QVERIFY(sp.degree() == 10);
    QVERIFY(sp.arcmin() == 0);
    QVERIFY(sp.arcsec() == 0);
    QVERIFY(sp.marcsec() == 0);

    sp.setH(10.0);
    QVERIFY(sp.hour() == 10);
    QVERIFY(sp.minute() == 0);
    QVERIFY(sp.second() == 0);
    QVERIFY(sp.msecond() == 0);

    sp.setD(10.0 + 1.0E-14);
    QVERIFY(sp.degree() == 10);
    QVERIFY(sp.arcmin() == 0);
    QVERIFY(sp.arcsec() == 0);
    QVERIFY(sp.marcsec() == 0);

    sp.setH(10.0 + 1.0E-14);
    QVERIFY(sp.hour() == 10);
    QVERIFY(sp.minute() == 0);
    QVERIFY(sp.second() == 0);
    QVERIFY(sp.msecond() == 0);
}

void TestDMS::testPrecisionTransition()
{
    // check for transitions in DMS and HMS angle representation strings
    // with respect to DMS and HMS precision around angle units 
    dms sp;
    double half_precision_DMS = 1.0 / 7200.0;
    double half_precision_HMS = 15.0 / 7200.0;
    double epsilon = 0.000000001;

    QLocale::setDefault(QLocale::c());

    // toDMSString

    bool forceSign = false;
    bool machineReadable = false;
    bool highPrecision = false;

    sp.setD(10.0 + half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 10° 00' 01\""));
    sp.setD(10.0 + half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 10° 00' 00\""));
    sp.setD(10.0 - half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 10° 00' 00\""));
    sp.setD(10.0 - half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 09° 59' 59\""));

    sp.setD(-10.0 + half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-09° 59' 59\""));
    sp.setD(-10.0 + half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-10° 00' 00\""));
    sp.setD(-10.0 - half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-10° 00' 00\""));
    sp.setD(-10.0 - half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-10° 00' 01\""));

    machineReadable = true;

    for (int i=0; i < 2; i++)
    {
        sp.setD(10.0 + half_precision_DMS + epsilon);
        QCOMPARE(sp.toDMSString(
 	    forceSign, machineReadable, highPrecision), QString(" 10:00:01"));
        sp.setD(10.0 + half_precision_DMS - epsilon);
        QCOMPARE(sp.toDMSString(
 	    forceSign, machineReadable, highPrecision), QString(" 10:00:00"));
      	sp.setD(10.0 - half_precision_DMS + epsilon);
        QCOMPARE(sp.toDMSString(
 	    forceSign, machineReadable, highPrecision), QString(" 10:00:00"));
        sp.setD(10.0 - half_precision_DMS - epsilon);
        QCOMPARE(sp.toDMSString(
	    forceSign, machineReadable, highPrecision), QString(" 09:59:59"));

        sp.setD(-10.0 + half_precision_DMS + epsilon);
        QCOMPARE(sp.toDMSString(
	    forceSign, machineReadable, highPrecision), QString("-09:59:59"));
        sp.setD(-10.0 + half_precision_DMS - epsilon);
        QCOMPARE(sp.toDMSString(
	    forceSign, machineReadable, highPrecision), QString("-10:00:00"));
        sp.setD(-10.0 - half_precision_DMS + epsilon);
        QCOMPARE(sp.toDMSString(
	    forceSign, machineReadable, highPrecision), QString("-10:00:00"));
        sp.setD(-10.0 - half_precision_DMS - epsilon);
        QCOMPARE(sp.toDMSString(
	    forceSign, machineReadable, highPrecision), QString("-10:00:01"));

	highPrecision = true;
    }

    machineReadable = false;

    sp.setD(10.0 + half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 10° 00' 0.50\""));
    sp.setD(10.0 + half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 10° 00' 0.50\""));
    sp.setD(10.0 - half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 09° 59' 59.50\""));
    sp.setD(10.0 - half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString(" 09° 59' 59.50\""));

    sp.setD(-10.0 + half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-09° 59' 59.50\""));
    sp.setD(-10.0 + half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-09° 59' 59.50\""));
    sp.setD(-10.0 - half_precision_DMS + epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-10° 00' 0.50\""));
    sp.setD(-10.0 - half_precision_DMS - epsilon);
    QCOMPARE(sp.toDMSString(
	forceSign, machineReadable, highPrecision), QString("-10° 00' 0.50\""));

    // toHMSString
    
    highPrecision = false;

    sp.setD(10.0 * 15.0 + half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("10h 00m 01s"));
    sp.setD(10.0 * 15.0 + half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("10h 00m 00s"));
    sp.setD(10.0 * 15.0 - half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("10h 00m 00s"));
    sp.setD(10.0 * 15.0 - half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("09h 59m 59s"));

    sp.setD(-10.0 * 15.0 + half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("14h 00m 01s"));
    sp.setD(-10.0 * 15.0 + half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("14h 00m 00s"));
    sp.setD(-10.0 * 15.0 - half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("14h 00m 00s"));
    sp.setD(-10.0 * 15.0 - half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("13h 59m 59s"));

    machineReadable = true;

    for (int i=0; i < 2; i++)
    {
        sp.setD(10.0 * 15.0 + half_precision_HMS + epsilon);
        QCOMPARE(sp.toHMSString(
 	    machineReadable, highPrecision), QString("10:00:01"));
        sp.setD(10.0 * 15.0 + half_precision_HMS - epsilon);
        QCOMPARE(sp.toHMSString(
 	    machineReadable, highPrecision), QString("10:00:00"));
      	sp.setD(10.0 * 15.0 - half_precision_HMS + epsilon);
        QCOMPARE(sp.toHMSString(
 	    machineReadable, highPrecision), QString("10:00:00"));
        sp.setD(10.0 * 15.0 - half_precision_HMS - epsilon);
        QCOMPARE(sp.toHMSString(
	    machineReadable, highPrecision), QString("09:59:59"));

        sp.setD(-10.0 * 15.0 + half_precision_HMS + epsilon);
        QCOMPARE(sp.toHMSString(
	    machineReadable, highPrecision), QString("14:00:01"));
        sp.setD(-10.0 * 15.0 + half_precision_HMS - epsilon);
        QCOMPARE(sp.toHMSString(
	    machineReadable, highPrecision), QString("14:00:00"));
        sp.setD(-10.0 * 15.0 - half_precision_HMS + epsilon);
        QCOMPARE(sp.toHMSString(
	    machineReadable, highPrecision), QString("14:00:00"));
        sp.setD(-10.0 * 15.0 - half_precision_HMS - epsilon);
        QCOMPARE(sp.toHMSString(
	    machineReadable, highPrecision), QString("13:59:59"));

	highPrecision = true;
    }

    machineReadable = false;

    sp.setD(10.0 * 15.0 + half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("10h 00m 0.50s"));
    sp.setD(10.0 * 15.0 + half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("10h 00m 0.50s"));
    sp.setD(10.0 * 15.0 - half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("09h 59m 59.50s"));
    sp.setD(10.0 * 15.0 - half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("09h 59m 59.50s"));

    sp.setD(-10.0 * 15.0 + half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("14h 00m 0.50s"));
    sp.setD(-10.0 * 15.0 + half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("14h 00m 0.50s"));
    sp.setD(-10.0 * 15.0 - half_precision_HMS + epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("13h 59m 59.50s"));
    sp.setD(-10.0 * 15.0 - half_precision_HMS - epsilon);
    QCOMPARE(sp.toHMSString(
	machineReadable, highPrecision),QString("13h 59m 59.50s"));

    QLocale::setDefault(QLocale::system());
}

QTEST_GUILESS_MAIN(TestDMS)
