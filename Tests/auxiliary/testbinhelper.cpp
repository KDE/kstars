/*  KStars class tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
