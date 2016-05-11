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

void TestDMS::checkDefaultConstructor()
{
  /*
   * Test 1: Checks Default Constructor
  */

  dms d;

  QCOMPARE(d.Degrees(), NaN::d);

}

QTEST_MAIN(TestDMS)


