/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Test for rectangleoverlap.cpp
*/

#include "testrectangleoverlap.h"
#include "auxiliary/rectangleoverlap.h"

#include <QTest>

TestRectangleOverlap::TestRectangleOverlap(QObject * parent): QObject(parent)
{
}

void TestRectangleOverlap::testBasic_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<double>("refCenterX");
    QTest::addColumn<double>("refCenterY");
    QTest::addColumn<int>("refWidth");
    QTest::addColumn<int>("refHeight");
    QTest::addColumn<double>("refRotation");
    QTest::addColumn<double>("testCenterX");
    QTest::addColumn<double>("testCenterY");
    QTest::addColumn<int>("testWidth");
    QTest::addColumn<int>("testHeight");
    QTest::addColumn<double>("testRotation");
    QTest::addColumn<bool>("isOverlapped");

    //                     center   width height  angle     center   width height  angle   overlaps
    QTest::newRow("") << 2.0 << 4.5 << 2 << 2 <<   0.0 << 2.0 << 6.0 << 4 << 2 <<  45.0 << true;
    QTest::newRow("") << 1.0 << 1.0 << 1 << 1 <<   0.0 << 5.0 << 5.0 << 1 << 1 <<   0.0 << false;
    QTest::newRow("") << 4.0 << 4.0 << 4 << 4 <<   0.0 << 7.0 << 4.0 << 4 << 4 <<   0.0 << true;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 <<  45.0 << 6.0 << 4.0 << 2 << 2 <<   0.0 << false;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 <<  20.0 << 6.0 << 4.0 << 4 << 2 <<  20.0 << false;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 << -70.0 << 6.0 << 4.0 << 4 << 2 << -70.0 << true;
    QTest::newRow("") << 4.0 << 7.0 << 4 << 2 << -70.0 << 6.0 << 4.0 << 4 << 2 << -70.0 << true;
    QTest::newRow("") << 4.0 << 8.0 << 4 << 2 << -70.0 << 6.0 << 4.0 << 4 << 2 << -70.0 << false;
    QTest::newRow("") << 5.0 << 7.0 << 4 << 2 <<   0.0 << 5.0 << 4.0 << 2 << 2 <<  45.0 << false;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 <<  45.0 << 6.0 << 5.0 << 2 << 2 <<   0.0 << true;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 <<  45.0 << 6.0 << 5.0 << 2 << 2 <<  45.0 << false;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 <<  45.0 << 6.0 << 5.0 << 2 << 2 <<  35.0 << true;
    QTest::newRow("") << 5.0 << 5.0 << 2 << 2 <<   0.0 << 5.0 << 5.0 << 4 << 4 <<   0.0 << true;
    QTest::newRow("") << 5.0 << 5.0 << 4 << 4 <<   0.0 << 5.0 << 5.0 << 2 << 2 <<   0.0 << true;
    QTest::newRow("") << 4.0 << 6.0 << 4 << 2 <<  30.0 << 6.0 << 5.0 << 2 << 2 <<  60.0 << true;
    QTest::newRow("") << 6.0 << 4.0 << 2 << 2 <<   0.0 << 4.0 << 6.0 << 4 << 2 <<  45.0 << false;
    QTest::newRow("") << 6.0 << 4.0 << 2 << 2 <<   0.0 << 2.0 << 6.0 << 4 << 2 <<  45.0 << false;
    QTest::newRow("") << 2.0 << 0.6 << 2 << 2 <<   0.0 << 2.0 << 6.0 << 4 << 2 <<  45.0 << false;
#endif
}

void TestRectangleOverlap::testBasic()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(double, refCenterX);
    QFETCH(double, refCenterY);
    QFETCH(int, refWidth);
    QFETCH(int, refHeight);
    QFETCH(double, refRotation);

    QFETCH(double, testCenterX);
    QFETCH(double, testCenterY);
    QFETCH(int, testWidth);
    QFETCH(int, testHeight);
    QFETCH(double, testRotation);

    QFETCH(bool, isOverlapped);

    const RectangleOverlap overlap(QPointF(refCenterX, refCenterY), refWidth, refHeight, refRotation);
    const bool result = overlap.intersects(QPointF(testCenterX, testCenterY), testWidth, testHeight, testRotation);

    QVERIFY(result == isOverlapped);
#endif
}

QTEST_GUILESS_MAIN(TestRectangleOverlap)
