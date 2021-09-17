/*  KStars class tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
