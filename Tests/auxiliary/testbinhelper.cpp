/*  KStars class tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "testbinhelper.h"

TestBinHelper::TestBinHelper(QObject *parent) : QObject(parent)
{
}

void TestBinHelper::initTestCase()
{
}

void TestBinHelper::cleanupTestCase()
{
}

void TestBinHelper::init()
{
}

void TestBinHelper::cleanup()
{
}

void TestBinHelper::testLoadBinary_data()
{
}

void TestBinHelper::testLoadBinary()
{
    QSKIP("Not implemented yet.");
}

QTEST_GUILESS_MAIN(TestBinHelper)
