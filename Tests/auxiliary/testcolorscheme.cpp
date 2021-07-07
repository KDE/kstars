/*  KStars class tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "../testhelpers.h"
#include "testcolorscheme.h"

TestColorScheme::TestColorScheme(QObject *parent) : QObject(parent)
{
}

void TestColorScheme::initTestCase()
{
}

void TestColorScheme::cleanupTestCase()
{
}

void TestColorScheme::init()
{
}

void TestColorScheme::cleanup()
{
}

void TestColorScheme::testSchemeColors_data()
{
}

void TestColorScheme::testSchemeColors()
{
    QSKIP("Not implemented yet.");
}

QTEST_GUILESS_MAIN(TestColorScheme)
