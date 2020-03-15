/*  KStars class tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QtTest>
#include <QApplication>

#include "testcsvparser.h"
#include "testfwparser.h"
#include "testdms.h"
#include "testcachingdms.h"
#include "testcolorscheme.h"
#include "testbinhelper.h"
#include "testfov.h"
#include "testgeolocation.h"
#include "testksuserdb.h"

#define RUN(result, TestClass) do { \
    TestClass tc; \
    result |= QTest::qExec(&tc, argc, argv); } while(false)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTEST_SET_MAIN_SOURCE_PATH \

    int result = 0;

    RUN(result, TestCSVParser);
    RUN(result, TestFWParser);
    RUN(result, TestDMS);
    RUN(result, TestCachingDms);
    RUN(result, TestColorScheme);
    RUN(result, TestBinHelper);
    RUN(result, TestFOV);
    RUN(result, TestGeolocation);
    RUN(result, TestKSUserDB);

    return result;
}
