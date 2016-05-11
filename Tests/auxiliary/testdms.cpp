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
   * Test 1: Checks Default Constructor
  */

  dms d;

  QVERIFY(std::isnan(d.Degrees()));

}

void TestDMS::explicitSexigesimalCtor()
{
    /*
     * Test 1: Checks Explicit Construct 1
    */

    // HH:MM:SS
    // 14:55:20

    dms d(14, 55, 20);

    QVERIFY(d.degree() == 14);
    QVERIFY(d.arcmin() == 55);
    QVERIFY(d.arcsec() == 20);
    QVERIFY(d.Degrees() == (14.0+55.0/60.0+20.0/3600.0));
}

QTEST_MAIN(TestDMS)


