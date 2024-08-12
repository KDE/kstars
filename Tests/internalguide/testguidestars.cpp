/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../indi/indiproperty.h"
#include "ekos/guide/internalguide/guidestars.h"

#include <QTest>

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
        void calibrationTest();
        void testFindGuideStar();
};

#include "testguidestars.moc"
#include "Options.h"

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

#define CompareFloat(d1,d2) QVERIFY(fabs((d1) - (d2)) < .001)

void TestGuideStars::basicTest()
{
    Options::setMinDetectionsSEPMultistar(5);
    Options::setMaxMultistarReferenceStars(10);
    GuideStars g;

    // Test setCalibration() and calibration in general.

    // angle in degrees, pixel size in mm, focal length in mm.
    const int binning = 1;
    double angle = 0.0;
    const double pixel_size = 3e-3, focal_length = 750.0;
    dms ra, dec;
    ra.setFromString("120:30:40");
    dec.setFromString("10:20:30");
    ISD::Mount::PierSide side = ISD::Mount::PIER_EAST;

    Calibration cal;
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, dec);
    cal.setAngle(angle);
    g.setCalibration(cal);

    // arcseconds = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    // Then needs to be rotated by the angle. Start with angle = 0;
    double arcsecondsPerPixel = (3600.0 * 180.0 / M_PI) * binning * pixel_size / focal_length;

    double x1 = 10, y1 = 50;
    GuiderUtils::Vector p(x1, y1, 0);
    GuiderUtils::Vector as = cal.convertToArcseconds(p);
    CompareFloat(as.x, arcsecondsPerPixel * x1);
    CompareFloat(as.y, arcsecondsPerPixel * y1);

    // Test computeStarDrift(), computing the distance between two stars in arcseconds.
    double dx = 2.5, dy = -0.7;
    Edge refStar = makeEdge(x1, y1);
    Edge star = makeEdge(x1 + dx, y1 + dy);
    double dRa, dDec;
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    // Since the angle is 0, x differences should be reflected in RA, and y in DEC.
    CompareFloat(dRa, dx * arcsecondsPerPixel);
    // Y is inverted, because y=0 is at the top.
    CompareFloat(dDec, -dy * arcsecondsPerPixel);

    // Change the angle to 90, 180 and -90 degrees
    angle = 90.0;
    cal.setAngle(angle);
    g.setCalibration(cal);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(-dDec, dx * arcsecondsPerPixel);
    CompareFloat(dRa, -dy * arcsecondsPerPixel);

    angle = 180.0;
    cal.setAngle(angle);
    g.setCalibration(cal);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(-dRa, dx * arcsecondsPerPixel);
    CompareFloat(-dDec, -dy * arcsecondsPerPixel);

    angle = -90.0;
    cal.setAngle(angle);
    g.setCalibration(cal);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(dDec, dx * arcsecondsPerPixel);
    CompareFloat(-dRa, -dy * arcsecondsPerPixel);

    // Use angle -90 so changes in x are changes in RA, and y --> -DEC.
    angle = 0.0;
    cal.setAngle(angle);
    g.setCalibration(cal);

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

    bool success = g.getDrift(1, 100, 70, &dRa, &dDec);
    QVERIFY(success);
    // drift should be 0.
    CompareFloat(dRa, 0);
    CompareFloat(dDec, 0);

    // Move the reticle 1 pixel in x, should result in a drift of "arcsecondsPerPixel" in RA
    // and 0 in DEC.
    success = g.getDrift(1, 99, 70, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, arcsecondsPerPixel);
    CompareFloat(dDec, 0);

    // Similarly 2 pixels upward in y would affect DEC.
    success = g.getDrift(1, 100, 68, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, 0);
    CompareFloat(dDec, -2 * arcsecondsPerPixel);

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
    CompareFloat(dRa, 0.5 * arcsecondsPerPixel);
    CompareFloat(dDec, -0.75 * arcsecondsPerPixel);

    // We don't accept multi-star drifts where the reference stars are too far (> 2 a-s)
    // from the guide-star drift. Here we move the guide star so it's different than
    // the rest, and it should control the drift.
    stars[0].x = 105;
    stars[0].y = 76;
    g.setDetectedStars(stars);
    success = g.getDrift(1, 100, 70, &dRa, &dDec);
    QVERIFY(success);
    CompareFloat(dRa, 5 * arcsecondsPerPixel);
    CompareFloat(dDec, -6 * arcsecondsPerPixel);

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

// Takes the radians value input and converts to an angle in degrees 0 <= degrees < 360.
double toDegrees(double radians)
{
    double degrees = 360.0 * radians / (2.0 * M_PI);
    while (degrees < 0) degrees += 360.0;
    while (degrees >= 360.0) degrees -= 360.0;
    return degrees;
}

// This tests the Calibration class' API.
void TestGuideStars::calibrationTest()
{
    const int binning = 1;
    double angle = 0.0;
    const double pixel_size = 3e-3, focal_length = 750.0;
    Calibration cal;
    dms ra, dec;
    ra.setFromString("120:30:40");
    dec.setFromString("10:20:30");
    ISD::Mount::PierSide side = ISD::Mount::PIER_EAST;

    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, dec);

    // arcseconds = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    // Then needs to be rotated by the angle. Start with angle = 0;
    double arcsecondsPerPixel = (3600.0 * 180.0 / M_PI) * binning * pixel_size / focal_length;

    CompareFloat(angle, cal.getAngle());
    CompareFloat(focal_length, cal.getFocalLength());

    for (int i = -10; i <= 10; ++i)
    {
        GuiderUtils::Vector input(i, 2 * i, 0);
        GuiderUtils::Vector as = cal.convertToArcseconds(input);
        GuiderUtils::Vector px = cal.convertToPixels(input);
        CompareFloat(as.x, i * arcsecondsPerPixel);
        CompareFloat(as.y, 2 * i * arcsecondsPerPixel);
        CompareFloat(px.x, i / arcsecondsPerPixel);
        CompareFloat(px.y, 2 * i / arcsecondsPerPixel);
        double x, y;
        cal.convertToPixels(input.x, input.y, &x, &y);
        CompareFloat(x, i / arcsecondsPerPixel);
        CompareFloat(y, 2 * i / arcsecondsPerPixel);
    }

    CompareFloat(arcsecondsPerPixel, cal.xArcsecondsPerPixel());
    CompareFloat(arcsecondsPerPixel, cal.yArcsecondsPerPixel());
    CompareFloat(1.0 / arcsecondsPerPixel, cal.xPixelsPerArcsecond());
    CompareFloat(1.0 / arcsecondsPerPixel, cal.yPixelsPerArcsecond());


    // These are not yet estimated iniside Calibrate() so for now, just set them.
    //double raRate = 5.5, decRate = 3.3;
    double raMillisecondsPerPixel = 5.5;
    double raMillisecondsPerArcsecond = raMillisecondsPerPixel / arcsecondsPerPixel;
    double decMillisecondsPerPixel = 5.5;
    double decMillisecondsPerArcsecond = decMillisecondsPerPixel / arcsecondsPerPixel;

    cal.setRaPulseMsPerArcsecond(raMillisecondsPerArcsecond);
    cal.setDecPulseMsPerArcsecond(decMillisecondsPerArcsecond);

    CompareFloat(raMillisecondsPerArcsecond, cal.raPulseMillisecondsPerArcsecond());
    CompareFloat(decMillisecondsPerArcsecond, cal.decPulseMillisecondsPerArcsecond());

    /////////////////////////////////////////////////////////////////////
    // Check that conversions are right if binning is changed on the fly.
    /////////////////////////////////////////////////////////////////////
    const double bFactor = 3.0;
    cal.setBinningUsed(bFactor * binning, bFactor * binning);
    // In the loop below, the conversions to pixels will be smaller by bFactor (since we have
    // larger pixels with the binning), and the conversions to arc-seconds will increase by a factor
    // of bFactor, because each binned pixel is more arc-seconds than before.
    for (int i = -10; i <= 10; ++i)
    {
        GuiderUtils::Vector input(i, 2 * i, 0);
        GuiderUtils::Vector as = cal.convertToArcseconds(input);
        GuiderUtils::Vector px = cal.convertToPixels(input);
        CompareFloat(as.x, bFactor * i * arcsecondsPerPixel);
        CompareFloat(as.y, bFactor * 2 * i * arcsecondsPerPixel);
        CompareFloat(px.x, i / (bFactor * arcsecondsPerPixel));
        CompareFloat(px.y, 2 * i / (bFactor * arcsecondsPerPixel));
        double x, y;
        cal.convertToPixels(input.x, input.y, &x, &y);
        CompareFloat(x, i / (bFactor * arcsecondsPerPixel));
        CompareFloat(y, 2 * i / (bFactor * arcsecondsPerPixel));
    }

    CompareFloat(raMillisecondsPerArcsecond, cal.raPulseMillisecondsPerArcsecond());
    CompareFloat(decMillisecondsPerArcsecond, cal.decPulseMillisecondsPerArcsecond());

    CompareFloat(arcsecondsPerPixel * bFactor, cal.xArcsecondsPerPixel());
    CompareFloat(arcsecondsPerPixel * bFactor, cal.yArcsecondsPerPixel());
    CompareFloat(1.0 / (bFactor * arcsecondsPerPixel), cal.xPixelsPerArcsecond());
    CompareFloat(1.0 / (bFactor * arcsecondsPerPixel), cal.yPixelsPerArcsecond());
    cal.setBinningUsed(binning, binning);
    /////////////////////////////////////////////////////////////////////

    GuiderUtils::Vector px(1.0, 0.0, 0.0);
    cal.setAngle(0);
    GuiderUtils::Vector raDec = cal.rotateToRaDec(px);
    CompareFloat(px.x, raDec.x);
    CompareFloat(px.y, raDec.y);
    double rdx, rdy;
    cal.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(px.x, rdx);
    CompareFloat(px.y, rdy);

    cal.setAngle(90);
    raDec = cal.rotateToRaDec(px);
    CompareFloat(px.y, raDec.x);
    CompareFloat(-px.x, raDec.y);
    cal.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(px.y, rdx);
    CompareFloat(-px.x, rdy);

    cal.setAngle(180);
    raDec = cal.rotateToRaDec(px);
    CompareFloat(-px.x, raDec.x);
    CompareFloat(-px.y, raDec.y);
    cal.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.x, rdx);
    CompareFloat(-px.y, rdy);

    cal.setAngle(270);
    raDec = cal.rotateToRaDec(px);
    CompareFloat(-px.y, raDec.x);
    CompareFloat(px.x, raDec.y);
    cal.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, rdx);
    CompareFloat(px.x, rdy);

    // Test saving and restoring the calibration.

    // This should set the angle to 270 and keep raRate.
    cal.calculate1D(0, 10, raMillisecondsPerPixel * 10);
    angle = 270;
    int binX = 2, binY = 3;
    double pixSzW = .005, pixSzH = .006;
    cal.setParameters(pixSzW, pixSzH, focal_length, binX, binY, side, ra, dec);
    QString encodedCal = cal.serialize();
    Calibration cal2;
    QVERIFY(cal2.getFocalLength() != focal_length);
    QVERIFY(cal2.ccd_pixel_width != pixSzW);
    QVERIFY(cal2.ccd_pixel_height != pixSzH);
    QVERIFY(cal2.getAngle() != angle);
    QVERIFY(cal2.subBinX != binX);
    QVERIFY(cal2.subBinY != binY);
    QVERIFY(cal2.raPulseMillisecondsPerArcsecond() != raMillisecondsPerArcsecond);
    QVERIFY(cal2.decPulseMillisecondsPerArcsecond() != decMillisecondsPerArcsecond);
    QVERIFY(cal2.calibrationPierSide != side);
    QVERIFY(!(cal2.calibrationRA == ra));
    QVERIFY(!(cal2.calibrationDEC == dec));
    // swap defaults to false, and we haven't done anything to change it.
    QVERIFY(cal2.declinationSwapEnabled() == false);

    QVERIFY(!cal2.restore(""));
    QVERIFY(cal2.restore(encodedCal));
    cal2.setBinningUsed(binX, binY);
    QCOMPARE(cal2.getFocalLength(), focal_length);
    QCOMPARE(cal2.ccd_pixel_width, pixSzW);
    QCOMPARE(cal2.ccd_pixel_height, pixSzH);
    QCOMPARE(cal2.getAngle(), angle);
    QCOMPARE(cal2.subBinX, binX);
    QCOMPARE(cal2.subBinY, binY);
    CompareFloat(cal2.raPulseMillisecondsPerArcsecond(), raMillisecondsPerArcsecond);
    CompareFloat(cal2.decPulseMillisecondsPerArcsecond(), decMillisecondsPerArcsecond);
    QCOMPARE(cal2.calibrationPierSide, side);
    QVERIFY(cal2.calibrationRA == ra);
    QVERIFY(cal2.calibrationDEC == dec);
    // Swap still should be false.
    QVERIFY(cal2.declinationSwapEnabled() == false);
    bool swap;

    // This is an options checkbox that the user modifies depending on his/her mount.
    bool reverseDecOnPierChange = false;

    // Test restoring with a pier side.
    // This is same as above, as the encoded pier side was east.
    QVERIFY(cal2.restore(encodedCal, ISD::Mount::PIER_EAST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle);
    QCOMPARE(cal2.declinationSwapEnabled(), false);
    // This tests that the rotation matrix got adjusted with the angle.
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, rdx);
    CompareFloat(px.x, rdy);

    // If we are now west, the angle should change by 180 degrees and dec-swap should invert.
    QVERIFY(cal2.restore(encodedCal, ISD::Mount::PIER_WEST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle - 180.0);
    QCOMPARE(cal2.declinationSwapEnabled(), true);
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, -rdx);
    CompareFloat(px.x, -rdy);

    // Set the user option to reverse DEC on pier-side change.
    reverseDecOnPierChange = true;
    QVERIFY(cal2.restore(encodedCal, ISD::Mount::PIER_WEST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle - 180.0);
    QCOMPARE(cal2.declinationSwapEnabled(), false);
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, -rdx);
    CompareFloat(px.x, -rdy);
    reverseDecOnPierChange = false;

    // If we go back east, the angle and decSwap should revert to their original values.
    QVERIFY(cal2.restore(encodedCal, ISD::Mount::PIER_EAST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle);
    QCOMPARE(cal2.declinationSwapEnabled(), false);
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, rdx);
    CompareFloat(px.x, rdy);

    // Should not restore if the pier is unknown.
    QVERIFY(!cal2.restore(encodedCal, ISD::Mount::PIER_UNKNOWN, reverseDecOnPierChange, binning, binning));

    // Calculate the rotation.
    // Compute the angle the coordinates passed in make with the x-axis.
    // Oddly, though, the method first negates the y-coorainate (as images have y=0 on
    // top). So, account for that. Test in all 4 quadrents. Returns values 0-360 degrees.
    double x = 5.0, y = -7.0;
    QCOMPARE(Calibration::calculateRotation(x, y), toDegrees(atan2(-y, x)));
    x = -8.3, y = -2.4;
    QCOMPARE(Calibration::calculateRotation(x, y), toDegrees(atan2(-y, x)));
    x = -10.3, y = 8.2;
    QCOMPARE(Calibration::calculateRotation(x, y), toDegrees(atan2(-y, x)));
    x = 1.7, y = 8.2;
    QCOMPARE(Calibration::calculateRotation(x, y), toDegrees(atan2(-y, x)));
    x = 0, y = -8.0;
    QCOMPARE(Calibration::calculateRotation(x, y), toDegrees(atan2(-y, x)));
    x = 10, y = 0;
    QCOMPARE(Calibration::calculateRotation(x, y), toDegrees(atan2(-y, x)));
    // Short vectors (size less than 1.0) should return -1.
    x = .10, y = .20;
    QCOMPARE(Calibration::calculateRotation(x, y), -1.0);

    // Similar to above, a 1-D calibration.
    // Distance was 5.0 pixels, took 10 seconds of pulse

    // Set the pixels square for the below tests, since angles are in arc-seconds
    // and not pixel coordinates (this way both are equivalent).
    binX = 1.0;
    binY = 1.0;
    pixSzH = .005;
    pixSzW = .005;

    x = 3.0, y = -4.0;
    int pulseLength = 10000;
    side = ISD::Mount::PIER_WEST;
    cal.setParameters(pixSzW, pixSzH, focal_length, binX, binY, side, ra, dec);
    cal.calculate1D(x, y, pulseLength);
    CompareFloat(cal.getAngle(), toDegrees(atan2(-y, x)));
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), pulseLength / std::hypot(x, y));

    // 2-D calibrations take coordinates and pulse lengths for both axes.

    // Made sure the ra and dec vectors were orthogonal and dec was 90-degrees greater
    // than ra (ignoring that y-flip).
    double ra_x = -16.0, ra_y = 16.0, dec_x = -12.0, dec_y = -12.0;
    double ra_factor = 750.0;
    int ra_pulse = std::hypot(ra_x, ra_y) * ra_factor;
    // Correct for rounding, since pulse is an integer.
    ra_factor = ra_pulse / std::hypot(ra_x, ra_y);
    double dec_factor = 300.0;
    int dec_pulse = std::hypot(dec_x, dec_y) * dec_factor;
    dec_factor = dec_pulse / std::hypot(dec_x, dec_y);
    cal.calculate2D(ra_x, ra_y, dec_x, dec_y, &swap, ra_pulse, dec_pulse);
    QCOMPARE(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), ra_factor);
    QCOMPARE(cal.decPulseMillisecondsPerArcsecond() * cal.yArcsecondsPerPixel(), dec_factor);
    QCOMPARE(cal.getAngle(), toDegrees(atan2(-ra_y, ra_x)));
    QCOMPARE(swap, false);

    // Above we created a calibration where movement of size 1.0 along the x-axis in pixels
    // gets rotated towards -16,16 but keeps its original length.
    // Note, a 45-degree vector of length 1.0 has size length of sqrt(2)/2 in x and y.
    const double size1side = sqrt(2.0) / 2.0;
    cal.rotateToRaDec(1.0, 0.0, &rdx, &rdy);
    CompareFloat(rdx, -size1side);
    CompareFloat(rdy, size1side);
    // Similarly, a move (in dec) of size 1 down the y-axis would rotate toward -12,-12
    cal.rotateToRaDec(0.0, -1.0, &rdx, &rdy);
    CompareFloat(rdx, -size1side);
    CompareFloat(rdy, -size1side);

    // If we restored this on the EAST side
    QVERIFY(cal.restore(cal.serialize(), ISD::Mount::PIER_EAST, reverseDecOnPierChange, binning, binning));
    // ...RA moves should be inverted
    cal.rotateToRaDec(1.0, 0.0, &rdx, &rdy);
    CompareFloat(rdx, size1side);
    CompareFloat(rdy, -size1side);
    // ...and DEC moves should also invert.
    cal.rotateToRaDec(0.0, -1.0, &rdx, &rdy);
    CompareFloat(rdx, size1side);
    CompareFloat(rdy, size1side);

    // If we then move back to the WEST side, we should get the original results.
    QVERIFY(cal.restore(cal.serialize(), ISD::Mount::PIER_WEST, reverseDecOnPierChange, binning, binning));
    cal.rotateToRaDec(1.0, 0.0, &rdx, &rdy);
    CompareFloat(rdx, -size1side);
    CompareFloat(rdy, size1side);
    cal.rotateToRaDec(0.0, -1.0, &rdx, &rdy);
    CompareFloat(rdx, -size1side);
    CompareFloat(rdy, -size1side);

    // Test adjusting the RA rate according to DEC.

    dms calDec;
    calDec.setD(0, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    double raPulseRate = cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel();
    dms currDec;
    currDec.setD(0, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), raPulseRate);
    currDec.setD(70, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(),
                 raPulseRate / std::cos(70.0 * M_PI / 180.0));
    currDec.setD(-45, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(),
                 raPulseRate / std::cos(45.0 * M_PI / 180.0));
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(),
                 raPulseRate / std::cos(20.0 * M_PI / 180.0));
    // Set the rate back to its original value.
    currDec.setD(0, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), raPulseRate);

    // Change the calibration DEC.
    // A null calibration DEC should result in no-change to the ra pulse rate.
    dms nullDEC;
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, nullDEC);
    encodedCal = cal.serialize();
    raPulseRate = cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), raPulseRate);
    // Both 20 should result in no change.
    calDec.setD(20, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), raPulseRate);
    // Cal 20 and current 45 should change the rate accordingly.
    currDec.setD(45, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(),
                 raPulseRate * std::cos(20.0 * M_PI / 180.0) / std::cos(45.0 * M_PI / 180.0));
    // Changing cal dec to > 60-degrees or < -60 degrees results in no rate change.
    calDec.setD(65, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    raPulseRate = cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), raPulseRate);
    calDec.setD(-70, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    raPulseRate = cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerArcsecond() * cal.xArcsecondsPerPixel(), raPulseRate);
}

void TestGuideStars::testFindGuideStar()
{
    // Helpful for development/debugging.
    // The fits files needed below are too large to include at this point.
#if 0
    Options::setAlwaysInventGuideStar(false);
    Options::setMaxMultistarReferenceStars(50);
    Options::setMinDetectionsSEPMultistar(2);
    Options::setGuideOptionsProfile(0);
    Options::setGuideMaxHFR(5.5);

    QSharedPointer<GuideView> view;  // Not set

    // The x,y coordinates of a set of reference stars taken from a guider run.
    QList<double> xs = { 2048.52, 3980.48, 4546.17, 2554.72, 399.831, 312.933, 3965.32, 3981.4, 1414.81, 1460.85, 59.3435, 1365.59,
                         1320.91, 1853.8, 1402.75, 1233.19, 1521.91, 1476.17, 1200.38, 1871.45, 1142.82, 1556.88, 1301.77, 1411.87,
                         1296.59, 1417.35, 1217.52, 1610.88, 1721.06, 1245.83, 1254.56, 3083.38, 1333.77, 258.988, 1552.37, 1257.76,
                         1447.29, 1425.92, 2399.24, 1440.08, 1247.51, 1587.41, 1191.43, 1270.65, 1225.95, 782.994, 1250.56,
                         1540.07, 2246.5, 1283.3
                       };
    QList<double> ys = { 1344.35, 1612.64, 2100.24, 3349.58, 1334.91, 737.765, 1860.82, 210.059, 2090.72, 1847.37, 927.152, 1868.9,
                         2135.84, 1975.16, 2121.04, 2097.39, 2097.34, 2111.59, 2081.62, 2737.78, 2041.4, 2193.32, 1983.86, 2161.06,
                         2133.74, 1944.36, 2113.3, 2291.96, 1982.19, 2148.68, 2018.46, 1362.44, 2247.04, 3369.18, 2179.43, 2116.58,
                         1953.96, 2099.11, 1289.22, 3302.04, 2080.56, 2058.46, 2176.74, 2438.52, 2048.3, 1973.86, 1998.22, 2219.29,
                         2669.61, 2279.58
                       };

    // Setup the reference stars.
    QList<Edge> refs;
    for (int i = 0; i < xs.size(); ++i)
    {
        Edge e;
        e.x = xs[i];
        e.y = ys[i];
        refs.push_back(e);
    }

    // Setup the rest of GuideStars.
    Calibration cal;
    // Restored calibration--flipped angles. Angle 113.811, swap T ms/as: 102.31 69.3976. Encoding: Cal v1.0,bx=2,by=2,pw=0.0038,ph=0.0038,fl=2030,ang=293.811,angR=294.501,angD=203.121,ramspas=72.0329,decmspas=69.3976,swap=0,ra= 194:36:15,dec=00:00:27,side=1,when=2023-06-01 21:53:02,calEnd"
    QString serializedCal("Cal v1.0,bx=2,by=2,pw=0.0038,ph=0.0038,fl=2030,ang=293.811,angR=294.501,angD=203.121,ramspas=72.0329,decmspas=69.3976,swap=0,ra= 194:36:15,dec=00:00:27,side=1,when=2023-06-01 21:53:02,calEnd");
    dms currentDEC(12, 16);
    cal.restore(serializedCal, ISD::Mount::PierSide::PIER_WEST, false, 1, 1, &currentDEC);
    GuideStars guideStars;
    guideStars.setCalibration(cal);
    guideStars.setupStarCorrespondence(refs, 2);

    // Find the guide star for a set of guide fits files.
    QStringList files({"guide_frame_23-50-36.fits",
                       "guide_frame_23-50-44.fits",
                       "guide_frame_23-50-52.fits",
                       "guide_frame_23-51-01.fits",
                       "guide_frame_23-51-09.fits",
                       "guide_frame_23-51-19.fits",
                       "guide_frame_23-51-27.fits",
                       "guide_frame_23-51-35.fits",
                       "guide_frame_23-51-43.fits",
                       "guide_frame_23-51-51.fits",
                       "guide_frame_23-51-59.fits",
                       "guide_frame_23-52-07.fits",
                       "guide_frame_23-52-22.fits",
                       "guide_frame_23-52-31.fits",
                       "guide_frame_23-52-39.fits",
                       "guide_frame_23-52-47.fits",
                       "guide_frame_23-52-55.fits",
                       "guide_frame_23-53-03.fits",
                       "guide_frame_23-53-12.fits",
                       "guide_frame_23-53-20.fits",
                       "guide_frame_23-53-29.fits"});

    for (const auto &file : files)
    {
        // Load the test fits file.
        const QString FITS_FILENAME(QString("./2024-08-10/%1").arg(file));
        fprintf(stderr, "Loading %s\n", FITS_FILENAME.toLatin1().data());
        QSharedPointer<FITSData> fits(new FITSData(FITS_NORMAL));
        QVERIFY(fits != nullptr);
        QFuture<bool> worker = fits->loadFromFile(FITS_FILENAME);
        QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 60000);
        QVERIFY(worker.result());

        auto v = guideStars.findGuideStar(fits, QRect(), view, false);
        fprintf(stderr, "findGuideStar returned %.2f,%.2f\n", v.x, v.y);
        fprintf(stderr, "------------------------------------------------\n");
    }
#endif
}

QTEST_GUILESS_MAIN(TestGuideStars)
