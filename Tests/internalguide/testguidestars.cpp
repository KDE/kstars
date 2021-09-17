/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../indi/indiproperty.h"
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
        void calibrationTest();
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

#define CompareFloat(d1,d2) QVERIFY(fabs((d1) - (d2)) < .001)

void TestGuideStars::basicTest()
{
    GuideStars g;

    // Test setCalibration() and calibration in general.

    // angle in degrees, pixel size in mm, focal length in mm.
    const int binning = 1;
    double angle = 0.0;
    const double pixel_size = 3e-3, focal_length = 750.0;
    dms ra, dec;
    ra.setFromString("120:30:40");
    dec.setFromString("10:20:30");
    ISD::Telescope::PierSide side = ISD::Telescope::PIER_EAST;

    Calibration cal;
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, dec);
    cal.setAngle(angle);
    g.setCalibration(cal);

    // arcseconds = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    // Then needs to be rotated by the angle. Start with angle = 0;
    double constant = (3600.0 * 180.0 / M_PI) * binning * pixel_size / focal_length;

    double x1 = 10, y1 = 50;
    GuiderUtils::Vector p(x1, y1, 0);
    GuiderUtils::Vector as = cal.convertToArcseconds(p);
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
    cal.setAngle(angle);
    g.setCalibration(cal);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(-dDec, dx * constant);
    CompareFloat(dRa, -dy * constant);

    angle = 180.0;
    cal.setAngle(angle);
    g.setCalibration(cal);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(-dRa, dx * constant);
    CompareFloat(-dDec, -dy * constant);

    angle = -90.0;
    cal.setAngle(angle);
    g.setCalibration(cal);
    g.computeStarDrift(star, refStar, &dRa, &dDec);
    CompareFloat(dDec, dx * constant);
    CompareFloat(-dRa, -dy * constant);

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
    ISD::Telescope::PierSide side = ISD::Telescope::PIER_EAST;

    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, dec);

    // arcseconds = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    // Then needs to be rotated by the angle. Start with angle = 0;
    double constant = (3600.0 * 180.0 / M_PI) * binning * pixel_size / focal_length;

    CompareFloat(angle, cal.getAngle());
    CompareFloat(focal_length, cal.getFocalLength());

    for (int i = -10; i <= 10; ++i)
    {
        GuiderUtils::Vector input(i, 2 * i, 0);
        GuiderUtils::Vector as = cal.convertToArcseconds(input);
        GuiderUtils::Vector px = cal.convertToPixels(input);
        CompareFloat(as.x, i * constant);
        CompareFloat(as.y, 2 * i * constant);
        CompareFloat(px.x, i / constant);
        CompareFloat(px.y, 2 * i / constant);
        double x, y;
        cal.convertToPixels(input.x, input.y, &x, &y);
        CompareFloat(x, i / constant);
        CompareFloat(y, 2 * i / constant);
    }

    CompareFloat(constant, cal.xArcsecondsPerPixel());
    CompareFloat(constant, cal.yArcsecondsPerPixel());
    CompareFloat(1.0 / constant, cal.xPixelsPerArcsecond());
    CompareFloat(1.0 / constant, cal.yPixelsPerArcsecond());

    // These are not yet estimated iniside Calibrate() so for now, just set them.
    double raRate = 5.5, decRate = 3.3;
    cal.setRaPulseMsPerPixel(raRate);
    cal.setDecPulseMsPerPixel(decRate);

    CompareFloat(raRate, cal.raPulseMillisecondsPerPixel());
    CompareFloat(raRate / constant, cal.raPulseMillisecondsPerArcsecond());
    CompareFloat(decRate, cal.decPulseMillisecondsPerPixel());
    CompareFloat(decRate / constant, cal.decPulseMillisecondsPerArcsecond());

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
        CompareFloat(as.x, bFactor * i * constant);
        CompareFloat(as.y, bFactor * 2 * i * constant);
        CompareFloat(px.x, i / (bFactor * constant));
        CompareFloat(px.y, 2 * i / (bFactor * constant));
        double x, y;
        cal.convertToPixels(input.x, input.y, &x, &y);
        CompareFloat(x, i / (bFactor * constant));
        CompareFloat(y, 2 * i / (bFactor * constant));
    }
    // per-pixel rates should change, but not per-arc-second rates.
    CompareFloat(raRate * bFactor, cal.raPulseMillisecondsPerPixel());
    CompareFloat(raRate / constant, cal.raPulseMillisecondsPerArcsecond());
    CompareFloat(decRate * bFactor, cal.decPulseMillisecondsPerPixel());
    CompareFloat(decRate / constant, cal.decPulseMillisecondsPerArcsecond());

    CompareFloat(constant * bFactor, cal.xArcsecondsPerPixel());
    CompareFloat(constant * bFactor, cal.yArcsecondsPerPixel());
    CompareFloat(1.0 / (bFactor * constant), cal.xPixelsPerArcsecond());
    CompareFloat(1.0 / (bFactor * constant), cal.yPixelsPerArcsecond());
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
    cal.calculate1D(0, 10, raRate * 10);
    angle = 270;
    const int binX = 2, binY = 3;
    const double pixSzW = .005, pixSzH = .006;
    cal.setParameters(pixSzW, pixSzH, focal_length, binX, binY, side, ra, dec);
    QString encodedCal = cal.serialize();
    Calibration cal2;
    QVERIFY(cal2.getFocalLength() != focal_length);
    QVERIFY(cal2.ccd_pixel_width != pixSzW);
    QVERIFY(cal2.ccd_pixel_height != pixSzH);
    QVERIFY(cal2.getAngle() != angle);
    QVERIFY(cal2.subBinX != binX);
    QVERIFY(cal2.subBinY != binY);
    QVERIFY(cal2.raPulseMillisecondsPerPixel() != raRate);
    QVERIFY(cal2.decPulseMillisecondsPerPixel() != decRate);
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
    QCOMPARE(cal2.raPulseMillisecondsPerPixel(), raRate);
    QCOMPARE(cal2.decPulseMillisecondsPerPixel(), decRate);
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
    QVERIFY(cal2.restore(encodedCal, ISD::Telescope::PIER_EAST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle);
    QCOMPARE(cal2.declinationSwapEnabled(), false);
    // This tests that the rotation matrix got adjusted with the angle.
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, rdx);
    CompareFloat(px.x, rdy);

    // If we are now west, the angle should change by 180 degrees and dec-swap should invert.
    QVERIFY(cal2.restore(encodedCal, ISD::Telescope::PIER_WEST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle - 180.0);
    QCOMPARE(cal2.declinationSwapEnabled(), true);
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, -rdx);
    CompareFloat(px.x, -rdy);

    // Set the user option to reverse DEC on pier-side change.
    reverseDecOnPierChange = true;
    QVERIFY(cal2.restore(encodedCal, ISD::Telescope::PIER_WEST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle - 180.0);
    QCOMPARE(cal2.declinationSwapEnabled(), false);
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, -rdx);
    CompareFloat(px.x, -rdy);
    reverseDecOnPierChange = false;

    // If we go back east, the angle and decSwap should revert to their original values.
    QVERIFY(cal2.restore(encodedCal, ISD::Telescope::PIER_EAST, reverseDecOnPierChange, binning, binning));
    QCOMPARE(cal2.getAngle(), angle);
    QCOMPARE(cal2.declinationSwapEnabled(), false);
    cal2.rotateToRaDec(px.x, px.y, &rdx, &rdy);
    CompareFloat(-px.y, rdx);
    CompareFloat(px.x, rdy);

    // Should not restore if the pier is unknown.
    QVERIFY(!cal2.restore(encodedCal, ISD::Telescope::PIER_UNKNOWN, reverseDecOnPierChange, binning, binning));

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

    x = 3.0, y = -4.0;
    int pulseLength = 10000;
    side = ISD::Telescope::PIER_WEST;
    cal.setParameters(pixSzW, pixSzH, focal_length, binX, binY, side, ra, dec);
    cal.calculate1D(x, y, pulseLength);
    QCOMPARE(cal.getAngle(), toDegrees(atan2(-y, x)));
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), pulseLength / std::hypot(x, y));

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
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), ra_factor);
    QCOMPARE(cal.decPulseMillisecondsPerPixel(), dec_factor);
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
    QVERIFY(cal.restore(cal.serialize(), ISD::Telescope::PIER_EAST, reverseDecOnPierChange, binning, binning));
    // ...RA moves should be inverted
    cal.rotateToRaDec(1.0, 0.0, &rdx, &rdy);
    CompareFloat(rdx, size1side);
    CompareFloat(rdy, -size1side);
    // ...and DEC moves should also invert.
    cal.rotateToRaDec(0.0, -1.0, &rdx, &rdy);
    CompareFloat(rdx, size1side);
    CompareFloat(rdy, size1side);

    // If we then move back to the WEST side, we should get the original results.
    QVERIFY(cal.restore(cal.serialize(), ISD::Telescope::PIER_WEST, reverseDecOnPierChange, binning, binning));
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
    double raPulseRate = cal.raPulseMillisecondsPerPixel();
    dms currDec;
    currDec.setD(0, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate);
    currDec.setD(70, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate / std::cos(70.0 * M_PI / 180.0));
    currDec.setD(-45, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate / std::cos(45.0 * M_PI / 180.0));
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate / std::cos(20.0 * M_PI / 180.0));
    // Set the rate back to its original value.
    currDec.setD(0, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate);

    // Change the calibration DEC.
    // A null calibration DEC should result in no-change to the ra pulse rate.
    dms nullDEC;
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, nullDEC);
    encodedCal = cal.serialize();
    raPulseRate = cal.raPulseMillisecondsPerPixel();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate);
    // Both 20 should result in no change.
    calDec.setD(20, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(), raPulseRate);
    // Cal 20 and current 45 should change the rate accordingly.
    currDec.setD(45, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    QCOMPARE(cal.raPulseMillisecondsPerPixel(),
             raPulseRate * std::cos(20.0 * M_PI / 180.0) / std::cos(45.0 * M_PI / 180.0));
    // Changing cal dec to > 60-degrees or < -60 degrees results in no rate change.
    calDec.setD(65, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    raPulseRate = cal.raPulseMillisecondsPerPixel();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerPixel(), raPulseRate);
    calDec.setD(-70, 0, 0);
    cal.setParameters(pixel_size, pixel_size, focal_length, binning, binning, side, ra, calDec);
    encodedCal = cal.serialize();
    raPulseRate = cal.raPulseMillisecondsPerPixel();
    currDec.setD(20, 0, 0);
    cal.restore(encodedCal, side, reverseDecOnPierChange, binning, binning, &currDec);
    CompareFloat(cal.raPulseMillisecondsPerPixel(), raPulseRate);
}

QTEST_GUILESS_MAIN(TestGuideStars)
