/*  GuideStars class test.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "ekos/guide/internalguide/guidestars.h"

#include <QtTest>

#include <QObject>

// The high-level methods, selectGuideStar() and findGuideStar() are not yet tested.
// Neither are the SEP-related EvaluateSEPStars, findTopStars, findAllSEPStars().

class TestGuideStars : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestGuideStars();

        /** @short Destructor */
        ~TestGuideStars() override = default;

    private slots:
        void basicTest();
};

#include "testguidestars.moc"

TestGuideStars::TestGuideStars() : QObject()
{
}

Edge makeEdge(float x, float y)
{
    Edge e;
    e.x = x;
    e.y = y;
    e.HFR = 2.0;
    e.numPixels = 20;
    e.sum = 2000;
    return e;
}

#define CompareFloat(d1,d2) QVERIFY(fabs((d1) - (d2)) < .0001)

void TestGuideStars::basicTest()
{
    GuideStars g;

    // Test setCalibration() and point2arcsec().

    // angle in degrees, pixel size in mm, focal length in mm.
    const int binning = 1;
    double angle = 0.0;
    const double pixel_size = 3e-3, focal_length = 750.0;
    g.setCalibration(angle, binning, pixel_size, focal_length);

    // arcseconds = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    // Then needs to be rotated by the angle. Start with angle = 0;
    double constant = (3600.0 * 180.0 / M_PI) * binning * pixel_size / focal_length;
    double x1 = 10, y1 = 50;
    Vector p(x1, y1, 0);
    Vector as = g.point2arcsec(p);
    CompareFloat(as.x, constant * x1);
    CompareFloat(as.y, constant * y1);

    // Test computeStarDrift(), computing the distance between two stars in arcseconds.
    double dx = 2.5, dy = -0.7;
    Edge refStar = makeEdge(x1, y1);
    Edge star = makeEdge(x1 + dx, y1 + dy);
    double dRa, dDec;
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    // Since the angle is 0, x differences should be reflected in RA, and y in DEC.
    CompareFloat(dRa, dx * constant);
    // Y is inverted, because y=0 is at the top.
    CompareFloat(dDec, -dy * constant);

    // Change the angle to 90, 180 and -90 degrees
    angle = 90.0;
    g.setCalibration(angle, binning, pixel_size, focal_length);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(-dDec, dx * constant);
    CompareFloat(dRa, -dy * constant);

    angle = 180.0;
    g.setCalibration(angle, binning, pixel_size, focal_length);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(-dRa, dx * constant);
    CompareFloat(-dDec, -dy * constant);

    angle = -90.0;
    g.setCalibration(angle, binning, pixel_size, focal_length);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(dDec, dx * constant);
    CompareFloat(-dRa, -dy * constant);

    // Use angle -90 so changes in x are changes in RA, and y --> -DEC.
    angle = 0.0;
    g.setCalibration(angle, binning, pixel_size, focal_length);

    // Select the guide star.

    // Setup with 5 reference stars--a guide star and 4 others.
    double gstarX = 100, gstarY = 70;
    double gstar1X = 75, gstar1Y = 150;
    double gstar2X = 200, gstar2Y = 500;
    QList<Edge> stars;
    stars.append(makeEdge(gstarX, gstarY));
    stars.append(makeEdge(gstar1X, gstar1Y));
    stars.append(makeEdge(gstar2X, gstar2Y));
    stars.append(makeEdge(250, 800));
    stars.append(makeEdge(300, 800));
    QList<double> scores = { 40, 200, 70, 0, 0};
    QList<double> distances = { 100, 100, 100, 100, 100 };
    QVector3D gstar0 = g.selectGuideStar(stars, scores, 1000, 1000, distances);
    // It should select the highest scoring star (1), as it has the best score
    // and it isn't near an edge (near x,y = 0 or near x,y = 1000).
    CompareFloat(gstar0.x(), gstar1X);
    CompareFloat(gstar0.y(), gstar1Y);

    // If the guide star (#1) position is moved close to the border,
    // the next best star (#2) should be chosen.
    gstar1X = 10;
    stars[1].x = gstar1X;
    QVector3D gstar = g.selectGuideStar(stars, scores, 1000, 1000, distances);
    CompareFloat(gstar.x(), gstar2X);
    CompareFloat(gstar.y(), gstar2Y);

    // Now say that that star 2 has a close neighbor, it should go the #0
    distances[2] = 5;
    gstar = g.selectGuideStar(stars, scores, 1000, 1000, distances);
    CompareFloat(gstar.x(), gstarX);
    CompareFloat(gstar.y(), gstarY);

    // We have a guide star at (100,70) and other stars at
    // (10, 150), (200,500), (250, 800), (300, 800).
    // Normally the detected stars and the mapping from stars to the reference stars
    // is done inside findGuideStar(). To test the internals, we set these directly.
    g.setDetectedStars(stars);
    QVector<int> map = {0, 1, 2, 3, 4};
    g.setStarMap(map);

    // This is needed so SNR calculations can work. Parameters aren't important
    // as long as the star SNR wind up above 8dB.
    SkyBackground bg(10, 10, 1000);
    g.setSkyBackground(bg);

    // Test computing the overall drift.
    // This should fail if the passed in main guide star drift is too big (> 4 arc-seconds).
    bool success = g.getDrift(10, 100, 70, &dRa, &dDec);
    QVERIFY(!success);

    success = g.getDrift(1, 100, 70, &dRa, &dDec);
    QVERIFY(success);
    // drift should be 0.
    CompareFloat(dRa, 0);
    CompareFloat(dDec, 0);

    // Move the reticle 1 pixel in x, should result in a drift of "constant" in RA
    // and 0 in DEC.
    success = g.getDrift(1, 99, 70, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, constant);
    CompareFloat(dDec, 0);

    // Similarly 2 pixels upward in y would affect DEC.
    success = g.getDrift(1, 100, 68, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, 0);
    CompareFloat(dDec, -2 * constant);

    // Finally, since the drift is the median drift of the guide stars,
    // we move half up and half down and the middle will control the drift.
    // We only consider drifts within 2a-s of the guidestar (star 0) so
    // these drifts are bunched together.
    stars[0].x += 1;
    stars[0].y += 1;
    stars[1].x += 1;
    stars[1].y += 1;
    stars[2].x += 0.5;   // this controls the x-drift
    stars[2].y += 0.75;  // this controls the y-drift
    stars[3].x -= -.25;
    stars[3].y -= -.25;
    stars[4].x -= -.25;
    stars[4].y -= -.25;
    g.setDetectedStars(stars);
    success = g.getDrift(1, 100, 70, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, 0.5 * constant);
    CompareFloat(dDec, -0.75 * constant);

    // We don't accept multi-star drifts where the reference stars are too far (> 2 a-s)
    // from the guide-star drift. Here we move the guide star so it's different than
    // the rest, and it should control the drift.
    stars[0].x = 105;
    stars[0].y = 76;
    g.setDetectedStars(stars);
    success = g.getDrift(1, 100, 70, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, 5 * constant);
    CompareFloat(dDec, -6 * constant);

    // This should fail if either there aren't enough reference stars (< 2)

    // Test findMinDistance.
    double xx0 = 10, xx1 = 12, xx2 = 20;
    double yy0 = 7, yy1 = 13, yy2 = 4;
    Edge e0 = makeEdge(xx0, yy0);
    Edge e1 = makeEdge(xx1, yy1);
    Edge e2 = makeEdge(xx2, yy2);
    double d01 = hypot(xx0 - xx1, yy0 - yy1);
    double d02 = hypot(xx0 - xx2, yy0 - yy2);
    double d12 = hypot(xx1 - xx2, yy1 - yy2);
    QList<Edge *> edges;
    edges.append(&e0);
    edges.append(&e1);
    edges.append(&e2);
    CompareFloat(g.findMinDistance(0, edges), std::min(d01, d02));
    CompareFloat(g.findMinDistance(1, edges), std::min(d01, d12));
    CompareFloat(g.findMinDistance(2, edges), std::min(d02, d12));
}

QTEST_GUILESS_MAIN(TestGuideStars)
