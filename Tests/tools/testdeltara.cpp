/*
    SPDX-FileCopyrightText: 2024 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QObject>

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <memory>

#include "skypoint.h"

class TestDeltaRA : public QObject
{
        Q_OBJECT

    public:
        TestDeltaRA();
        ~TestDeltaRA() override = default;

    private slots:
        void basicDeltaTest();
        void crossZeroTest();
        void edgeCasesTest();
        void negativeValuesTest();
        void deltaAngleTest();
};

TestDeltaRA::TestDeltaRA() : QObject()
{
}

void TestDeltaRA::basicDeltaTest()
{
    // Test the original case: 22:32:27 - 22:09:51
    SkyPoint p1, p2;
    p1.setRA(22.541);  // 22:32:27
    p2.setRA(22.164);  // 22:09:51
    const double delta = fabs(p1.ra().deltaAngle(p2.ra()).Degrees());
    QVERIFY(qAbs(delta - 5.655) < 0.001);
}

void TestDeltaRA::crossZeroTest()
{
    // Test RA values across 0h/24h boundary
    SkyPoint p1, p2;
    p1.setRA(23.917);  // 23:55:00
    p2.setRA(0.083);   // 00:05:00
    const double delta = fabs(p1.ra().deltaAngle(p2.ra()).Degrees());
    QVERIFY(qAbs(delta - 2.49) < 0.001);

    // Test larger gap across boundary
    SkyPoint p3, p4;
    p3.setRA(23.0);    // 23:00:00
    p4.setRA(1.0);     // 01:00:00
    const double delta2 = fabs(p3.ra().deltaAngle(p4.ra()).Degrees());
    QVERIFY(qAbs(delta2 - 30.0) < 0.001);
}

void TestDeltaRA::edgeCasesTest()
{
    // Test identical values
    SkyPoint p1, p2;
    p1.setRA(12.0);
    p2.setRA(12.0);
    const double delta = fabs(p1.ra().deltaAngle(p2.ra()).Degrees());
    QVERIFY(qAbs(delta - 0.0) < 0.001);

    // Test 12-hour difference (180 degrees)
    SkyPoint p3, p4;
    p3.setRA(0.0);
    p4.setRA(12.0);
    const double delta2 = fabs(p3.ra().deltaAngle(p4.ra()).Degrees());
    QVERIFY(qAbs(delta2 - 180.0) < 0.001);

    // Test almost 24-hour difference
    SkyPoint p5, p6;
    p5.setRA(0.0);
    p6.setRA(23.999);
    // Should be close to 0 since it takes shorter arc
    QVERIFY(fabs(p5.ra().deltaAngle(p6.ra()).Degrees()) < 1);
}

void TestDeltaRA::negativeValuesTest()
{
    // Test with negative hour angle values
    SkyPoint p1, p2;
    p1.setRA(-1.0);
    p2.setRA(1.0);
    const double delta = fabs(p1.ra().deltaAngle(p2.ra()).Degrees());
    QVERIFY(qAbs(delta - 30.0) < 0.001);

    // Test with both negative values
    SkyPoint p3, p4;
    p3.setRA(-2.0);
    p4.setRA(-1.0);
    const double delta2 = fabs(p3.ra().deltaAngle(p4.ra()).Degrees());
    QVERIFY(qAbs(delta2 - 15.0) < 0.001);
}

bool checkAnswer(double result, double answer)
{
    // they're both 180, or both 0, who cares about the sign.
    if (fabs(fabs(result) - 180) < .0001 && fabs(fabs(answer) - 180) < .0001) return true;
    if (fabs(result) < .0001 && fabs(answer) < .0001) return true;

    return fabs(result - answer) < .0001;
}

void TestDeltaRA::deltaAngleTest()
{
    for (double a1 = -720; a1 <= 720; a1 += 5.0)
    {
        const dms d1(a1);
        constexpr double increment = 5.0;

        // Go forward 180 degrees
        for (double x = 0.0; x <= 180.0; x += increment)
        {
            const double a2 = a1 + x;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = -x;
            QVERIFY(checkAnswer(result, answer));
        }
        // Go forward from 180 to 360 degrees
        for (double x = 0.0; x <= 30;/*180.0;*/ x += increment)
        {
            const double a2 = a1 + 180.0 + x;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = 180.0 - x;
            QVERIFY(checkAnswer(result, answer));
        }
        // Go forward from 360 to 540 degrees
        for (double x = 0.0; x <= 180.0; x += increment)
        {
            const double a2 = a1 + 360.0 + x;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = -x;
            QVERIFY(checkAnswer(result, answer));
        }
        // Go forward from 540 to 720 degrees
        for (double x = 0.0; x <= 180.0; x += increment)
        {
            const double a2 = a1 + 540.0 + x;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = 180 - x;
            QVERIFY(checkAnswer(result, answer));
        }
        // Go backward 180 degrees
        for (double x = 0.0; x >= -180.0; x -= increment)
        {
            const double a2 = a1 + x;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = -x;
            QVERIFY(checkAnswer(result, answer));
        }
        // Go backward from 180 to 360 degrees
        for (double x = 0.0; x >= -180.0; x -= increment)
        {
            const double a2 = a1 + x - 180.0;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = -(180 + x);
            QVERIFY(checkAnswer(result, answer));
        }
        // Go backward from 360 to 540 degrees
        for (double x = 0.0; x >= -180.0; x -= increment)
        {
            const double a2 = a1 + x - 360.0;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = -x;
            QVERIFY(checkAnswer(result, answer));
        }
        // Go backward from 540 to 720 degrees
        for (double x = 0.0; x >= -180.0; x -= increment)
        {
            const double a2 = a1 + x - 540.0;
            const dms d2(a2);
            const double result = d1.deltaAngle(d2).Degrees();
            const double answer = -(180 + x);
            QVERIFY(checkAnswer(result, answer));
        }
    }
}

QTEST_GUILESS_MAIN(TestDeltaRA)

#include "testdeltara.moc"
