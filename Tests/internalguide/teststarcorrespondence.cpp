/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/guide/internalguide/starcorrespondence.h"

#include <QTest>

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
    c.setImageSize(120, 100); // Not necessary, but speeds things up.
    QVector<int> output;

    // Identity test.
    Edge gStar = c.find(stars, maxDistanceToStar, &output, false);
    QVERIFY2(gStar.x == stars[guideStar].x, "Identity");
    QVERIFY2(gStar.y == stars[guideStar].y, "Identity");
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
    gStar = c.find(stars2, maxDistanceToStar, &output, false);
    QVERIFY2(gStar.x == stars2[guideStar].x, "small move");
    QVERIFY2(gStar.y == stars2[guideStar].y, "small move");
    for (int i = 0; i < stars.size(); ++i)
    {
        QVERIFY2(output[i] == i, "small move");
    }

    // Remove one of the stars (not the guide star which is 0-5).
    stars2.removeAt(8);
    gStar = c.find(stars2, maxDistanceToStar, &output, false);
    QVERIFY2(gStar.x == stars2[guideStar].x, "one removed");
    QVERIFY2(gStar.y == stars2[guideStar].y, "one removed");
    for (int i = 0; i < stars2.size(); ++i)
    {
        if (i < 8) QVERIFY2(output[i] == i, "one removed");
        else QVERIFY2(output[i] == (i + 1), "one removed");
    }

    // Remove the guide star. Nothing should be returned.
    double guideX = stars2[guideStar].x;
    double guideY = stars2[guideStar].y;
    stars2.removeAt(guideStar);
    c.setAllowMissingGuideStar(false);
    gStar = c.find(stars2, maxDistanceToStar, &output, false);
    QVERIFY2(gStar.x == -1, "guide removed");
    QVERIFY2(gStar.y == -1, "guide removed");
    for (int i = 0; i < stars2.size(); ++i)
    {
        QVERIFY2(output[i] == -1, "guide removed");
    }

    // Allow it to be resilient to missing guide stars.
    c.setAllowMissingGuideStar(true);
    gStar = c.find(stars2, maxDistanceToStar, &output, false);
    // There's been noise added so it won't recover the guide star exactly.
    QVERIFY2(fabs(gStar.x - guideX) < 2, "guide and 8 removed");
    QVERIFY2(fabs(gStar.y - guideY) < 2, "guide and 8 removed");
    for (int i = 0; i < stars2.size(); ++i)
    {
        // previously we removed 8, but also removing the guide star moves the index for star 8 to 7.
        const int missingStar = 7;
        if (i < missingStar && i < guideStar)
            QVERIFY2(output[i] == i, "guide and 8 removed");
        else if (i < guideStar || i < missingStar)
            QVERIFY2(output[i] == (i + 1), "guide and 8 removed");
        else
            QVERIFY2(output[i] == (i + 2), "guide and 8 removed");
    }

    // Use  clean star positions to exactly recover the lost guide star.
    guideX = stars[guideStar].x;
    guideY = stars[guideStar].y;
    stars.removeAt(guideStar);
    c.setAllowMissingGuideStar(true);
    gStar = c.find(stars, maxDistanceToStar, &output, false);
    QVERIFY2(gStar.x == guideX, "guide removed and recovered");
    QVERIFY2(gStar.y == guideY, "guide removed and recovered");
    for (int i = 0; i < stars2.size(); ++i)
    {
        if (i < guideStar)
            QVERIFY2(output[i] == i, "guide removed & recovered");
        else
            QVERIFY2(output[i] == (i + 1), "guide removed & recovered");
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

// Checks a set of reference stars and detected stars which should fail star correspondence.
void runNoCorrespondenceTest()
{
    constexpr double maxDistanceToStar = 10.0;

    // Setup a number of stars.
    QList<Edge> stars;
    stars.append(makeEdge(1186.73, 483.568));
    stars.append(makeEdge(367.712, 293.589));
    stars.append(makeEdge(190.09, 944.418));
    stars.append(makeEdge(1067.85, 30.3361));
    stars.append(makeEdge(1067.58, 313.792));
    // Assign guideStar as the guide star.
    StarCorrespondence c(stars, 0);
    c.setImageSize(1280, 960); // Not necessary, but speeds things up.
    QVector<int> output;

    // This detected set only has 2 of the ref stars, so multistar should fail.
    QList<Edge> stars2;
    stars2.append(makeEdge(241.8, 125.5));
    stars2.append(makeEdge(233.5, 119.3));
    stars2.append(makeEdge(1186.6, 483.5)); // ref 0
    stars2.append(makeEdge(368.1, 294.1));  // ref 1
    stars2.append(makeEdge(1006.5, 909.7));
    stars2.append(makeEdge(903.3, 448.7));
    stars2.append(makeEdge(940.5, 177.2));
    stars2.append(makeEdge(695.4, 97.6));

    c.setAllowMissingGuideStar(true);
    Edge gStar = c.find(stars2, maxDistanceToStar, &output, false);
    QVERIFY(gStar.x == -1);
    QVERIFY(gStar.y == -1);
    for (int i = 0; i < output.size(); ++i)
        QVERIFY(output[i] == -1);
}

void TestStarCorrespondence::basicTest()
{
    for (int i = 0; i < 6; ++i)
        runTest(i);
    runAdaptationTest();
    runNoCorrespondenceTest();
}

QTEST_GUILESS_MAIN(TestStarCorrespondence)
