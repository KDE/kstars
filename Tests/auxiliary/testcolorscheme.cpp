/*  KStars class tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
