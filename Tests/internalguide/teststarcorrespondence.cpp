/*  StarCorrespondence class test.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "ekos/guide/internalguide/starcorrespondence.h"

#include <QtTest>

#include <QObject>


class TestStarCorrespondence : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestStarCorrespondence();

        /** @short Destructor */
        ~TestStarCorrespondence() override = default;

    private slots:
        void basicTest();
};

#include "teststarcorrespondence.moc"

TestStarCorrespondence::TestStarCorrespondence() : QObject()
{
}

Edge makeEdge(float x, float y)
{
    Edge e;
    e.x = x;
    e.y = y;
    return e;
}

/*
void printStars(const QList<Edge>& stars)
{
  for (int i = 0; i < stars.size(); ++i) {
    printf("[%2d] %5.2f,%5.2f ", i, stars[i].x, stars[i].y);
  }
}
*/
void runTest(int guideStar)
{

    constexpr double maxDistanceToStar = 5.0;

    // Setup a number of stars.
    QList<Edge> stars;
    stars.append(makeEdge(90, 70));
    stars.append(makeEdge(110, 70));
    stars.append(makeEdge(0, 0));
    stars.append(makeEdge(10, 30));
    stars.append(makeEdge(20, 70));
    stars.append(makeEdge(70, 10));
    stars.append(makeEdge(70, 80));
    stars.append(makeEdge(80, 40));
    stars.append(makeEdge(30, 20));
    stars.append(makeEdge(30, 80));
    stars.append(makeEdge(50, 60));
    // Assign guideStar as the guide star.
    StarCorrespondence c(stars, guideStar);
    QVector<int> output;

    // Identity test.
    c.find(stars, maxDistanceToStar, &output, false);
    for (int i = 0; i < stars.size(); ++i)
    {
        QVERIFY2(output[i] == i, "Identity");
    }

    srand(21);
    // Move the stars randomly upto 2 pixels away
    QList<Edge> stars2;
    for (int i = 0; i < stars.size(); ++i)
    {
        double rand_fraction = ((rand() % 200) - 100) / 50.0;
        double x = stars[i].x + rand_fraction;
        rand_fraction = ((rand() % 200) - 100) / 50.0;
        double y = stars[i].y + rand_fraction;
        stars2.append(makeEdge(x, y));
    }
    c.find(stars2, maxDistanceToStar, &output, false);
    for (int i = 0; i < stars.size(); ++i)
    {
        QVERIFY2(output[i] == i, "small move");
    }

    // Remove one of the stars (not the guide star).
    stars2.removeAt(8);
    c.find(stars2, maxDistanceToStar, &output, false);
    for (int i = 0; i < stars2.size(); ++i)
    {
        if (i < 8) QVERIFY2(output[i] == i, "one removed");
        else QVERIFY2(output[i] == (i + 1), "one removed");
    }

    // Remove the guide star. Nothing should be returned.
    stars2.removeAt(guideStar);
    c.find(stars2, maxDistanceToStar, &output, false);
    for (int i = 0; i < stars2.size(); ++i)
    {
        QVERIFY2(output[i] == -1, "guide removed");
    }
}

void runAdaptationTest()
{
    // Setup a number of stars.
    QList<Edge> stars;
    stars.append(makeEdge(100, 50));
    stars.append(makeEdge(150, 50));
    stars.append(makeEdge(100, 80));
    StarCorrespondence c(stars, 0);
    QVERIFY(c.reference(0).x == 100);
    QVERIFY(c.reference(0).y == 50);
    QVERIFY(c.reference(1).x == 150);
    QVERIFY(c.reference(1).y == 50);
    QVERIFY(c.reference(2).x == 100);
    QVERIFY(c.reference(2).y == 80);

    // Move star #1 from 150,50 to 151,148. Keep the other two in place.
    stars[1].x = 151;
    stars[1].y = 48;
    QVector<int> output;
    // Find the stars. All 3 should still correspond.
    // The position of star#1 should move a little towards its new position.
    c.find(stars, 5.0, &output, true);
    QVERIFY(output[0] == 0);
    QVERIFY(output[1] == 1);
    QVERIFY(output[2] == 2);
    QVERIFY(c.reference(0).x == 100);
    QVERIFY(c.reference(0).y == 50);
    QVERIFY(c.reference(1).x > 150);
    QVERIFY(c.reference(1).y < 50);
    QVERIFY(c.reference(2).x == 100);
    QVERIFY(c.reference(2).y == 80);

    // Check the exact position it moved to.
    // Exact values for ref 1 using the adaptation equation should be:
    // alpha = 1 / 25**0.865
    // x(1) = alpha * 151 + (1-alpha) * 150
    // y(1) = alpha * 48 + (1-alpha) * 50
    // Of course, if the time constant or equation changes, this shuld be updated.
    const double alpha = 1 / pow(25.0, 0.865);
    const double x1 = alpha * 151 + (1 - alpha) * 150;
    const double y1 = alpha * 48 + (1 - alpha) * 50;
    QVERIFY(fabs(c.reference(1).x - x1) < .0001);
    QVERIFY(fabs(c.reference(1).y - y1) < .0001);
}

void TestStarCorrespondence::basicTest()
{
    for (int i = 0; i < 6; ++i)
        runTest(i);
    runAdaptationTest();
}

QTEST_GUILESS_MAIN(TestStarCorrespondence)
