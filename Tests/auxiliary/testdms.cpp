/*  KStars Testing - DMS
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "testdms.h"
#include "nan.h"

TestDMS::TestDMS(): QObject()
{
}

TestDMS::~TestDMS()
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
    QVERIFY(d.Degrees() == (14.0+55.0/60.0+20.0/3600.0));
}

void TestDMS::angleCtor()
{
    /*
     * Test 3: Checks Angle Ctor
    */

    // Angle = -112.56 Degrees ---> HMS (16:29:45)

    double angle = -112.56;

    dms d(angle);

    QVERIFY(d.degree() == (int) angle);

    QVERIFY(d.Hours()  == (angle+360)/15.0);
    QVERIFY(d.hour()   == 16);
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
    QVERIFY(d.Degrees() == (14.0+55.0/60.0+20.0/3600.0));

    // From Hours
    dms h(hms, false);
    QVERIFY(h.Degrees() == d.Degrees()*15.0);
    QVERIFY(h.Hours() == d.Degrees());
}

QTEST_MAIN(TestDMS)


