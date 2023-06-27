/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/focus/focusstars.h"

#include <QTest>
#include <cmath>
#include <memory>

#include <QObject>

class TestFocusStars : public QObject
{
        Q_OBJECT

    public:
        TestFocusStars();
        ~TestFocusStars() override = default;

    private slots:
        void basicTest();
};

#include "testfocusstars.moc"

using Ekos::FocusStars;

TestFocusStars::TestFocusStars() : QObject()
{
}

#define CompareFloat(d1,d2) QVERIFY(fabs((d1) - (d2)) < .0001)

Edge makeEdge(float x, float y, float hfr)
{
    Edge e;
    e.x = x;
    e.y = y;
    e.HFR = hfr;
    return e;
}

struct Triple
{
    double x, y, h;
};
QList<Edge> MakeEdges(const std::vector<Triple> triples)
{
    QList<Edge> edges;
    for (const auto &t : triples)
        edges.append(makeEdge(t.x, t.y, t.h));
    return edges;
};

void TestFocusStars::basicTest()
{
    double maxDistance = 1.0;
    FocusStars f1(MakeEdges(
    {
        {2.0, 2.0, 3.0},
        {5.0, 5.0, 4.0},
        {7.0, 5.0, 4.5},
        {5.0, 1.0, 1.0},
        {6.0, 3.0, 2.5}
    }), maxDistance);

    FocusStars f2(MakeEdges(
    {
        {2.1, 2.2, 3.0},
        {5.3, 5.2, 2.0},
        {7.2, 5.1, 4.5},
        {15.0, 11.0, 11.0},
        {16.0, 13.0, 12.5}
    }), maxDistance);

    CompareFloat(3.0, f1.getHFR());
    CompareFloat(4.5, f2.getHFR());

    double h1, h2;
    f1.commonHFR(f2, &h1, &h2);
    // The stars in common are the 1st 3 of each.
    CompareFloat(4.0, h1);
    CompareFloat(3.0, h2);

    double hh1, hh2;
    f2.commonHFR(f1, &hh2, &hh1);
    CompareFloat(hh1, h1);
    CompareFloat(hh2, h2);

    double r1 = f1.relativeHFR(f2, 10);
    CompareFloat(13.33333, r1);

    double r2 = f2.relativeHFR(f1, 10);
    CompareFloat(7.5, r2);
}

QTEST_GUILESS_MAIN(TestFocusStars)
