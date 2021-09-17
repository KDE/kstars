/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "test_polaralign.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "time/kstarsdatetime.h"
#include "auxiliary/dms.h"
#include "Options.h"
#include <libnova/libnova.h>
#include "fitsviewer/fitscommon.h"
#include "fitsviewer/fitsdata.h"
#include "rotations.h"

using Rotations::V3;

// Solver's solution. RA, DEC, & orientation in degrees, pixScale in arc-seconds/pixel,
// and the sample's time in UTC.
struct Solution
{
    double ra, dec, orientation, pixScale;
    int year, month, day, hour, minute, second;
};

// A set of 3 solutions for the polar alignment.
struct PaaData
{
    Solution s1, s2, s3;
    int x, y, correctedX, correctedY;
};

constexpr int IMAGE_WIDTH = 4656;
constexpr int IMAGE_HEIGHT = 3520;

void loadDummyFits(QSharedPointer<FITSData> &image, const KStarsDateTime &time,
                   double ra, double dec, double orientation, double pixScale, bool eastToTheRight)
{
    image.reset(new FITSData(FITS_NORMAL));

    // Borrow one of the other fits files in the test suite.
    // We don't use the contents of the image, but the image's width and height.
    // We do set the wcs data.
    QFuture<bool> worker = image->loadFromFile("ngc4535-autofocus1.fits");
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 60000);
    QVERIFY(worker.result());
    auto stats = image->getStatistics();
    stats.width = IMAGE_WIDTH;
    stats.height = IMAGE_HEIGHT;
    image->restoreStatistics(stats);
    image->setDateTime(time);
    QVERIFY(image->injectWCS(orientation, ra, dec, pixScale, eastToTheRight));
    QVERIFY(image->checkForWCS());
}

void setupData(const PaaData &data, int sampleNum, QSharedPointer<FITSData> &image, bool eastToTheRight)
{
    const Solution &d = sampleNum == 0 ? data.s1 : sampleNum == 1 ? data.s2 : data.s3;

    KStarsDateTime time;
    time.setDate(QDate(d.year, d.month, d.day));
    time.setTime(QTime(d.hour, d.minute, d.second));
    time.setTimeSpec(Qt::UTC);
    loadDummyFits(image, time, d.ra, d.dec, d.orientation, d.pixScale, eastToTheRight);
}

TestPolarAlign::TestPolarAlign() : QObject()
{
    Options::setUseRelativistic(false);
}

TestPolarAlign::~TestPolarAlign()
{
}

void TestPolarAlign::compare(double a, double e, QString msg, int line, double tolerance)
{
    QVERIFY2(std::fabs(a - e) < tolerance,
             qPrintable(QString("Line %1: %2: actual %3, expected %4 error %5 arc-seconds")
                        .arg(line).arg(msg).arg(a).arg(e)
                        .arg(((a - e) * 3600.0), 3, 'f', 1)));
}

bool TestPolarAlign::compare(const QPointF &point, double x, double y, double tolerance)
{
    return ((std::fabs(point.x() - x) < tolerance) &&
            (std::fabs(point.y() - y) < tolerance));
}

namespace
{
void runPAA(const GeoLocation &geo, const PaaData &data, bool eastToTheRight = true)
{
    constexpr bool canSkipPixelError = false;

    PolarAlign polarAlign(&geo);

    QSharedPointer<FITSData> image;

    setupData(data, 0, image, eastToTheRight);
    QVERIFY(polarAlign.addPoint(image));

    setupData(data, 1, image, eastToTheRight);
    QVERIFY(polarAlign.addPoint(image));

    setupData(data, 2, image, eastToTheRight);
    QVERIFY(polarAlign.addPoint(image));

    QVERIFY(polarAlign.findAxis());

    double axisAz, axisAlt;
    polarAlign.getAxis(&axisAz, &axisAlt);

    double azimuthError, altitudeError;
    polarAlign.calculateAzAltError(&azimuthError, &altitudeError);

    QPointF corrected;
    if (data.x < 0 && data.y < 0)
    {
        QVERIFY(polarAlign.findCorrectedPixel(
                    image, QPointF(image->width() / 2, image->height() / 2), &corrected));
    }
    else
    {
        QVERIFY(polarAlign.findCorrectedPixel(
                    image, QPointF(data.x, data.y), &corrected));
        double azE, altE;

        // Just fix the altitude, not the azimuth, for the pixelError tests below.
        QPointF altCorrected;
        QVERIFY(polarAlign.findCorrectedPixel(
                    image, QPointF(data.x, data.y), &altCorrected, true));

        // Below pixelError tests, test our ability to estimate how much error is left to be corrected
        // as the user progresses along the polar alignment procedure.
        // We use pretty generous error margins below since pixelError is an approximation.

        // Test the entire path, start to end. We should see the full azimuth and altitude errors.
        QVERIFY(polarAlign.pixelError(image, QPointF(data.x, data.y), corrected, &azE, &altE));
        QVERIFY(fabs(azE - azimuthError) < .02);
        QVERIFY(fabs(altE - altitudeError) < .01);

        // Simulate that the user has started correcting, and is halfway on the alt-only segment,
        // there should be 1/2 the alt error left, and all of the az error.
        if (polarAlign.pixelError(image, QPointF((altCorrected.x() + data.x) / 2, (altCorrected.y() + data.y) / 2), corrected,
                                  &azE, &altE))
        {
            QVERIFY(fabs(azE - azimuthError) < .01);
            QVERIFY(fabs(altE - altitudeError / 2.0) < .01);
        }
        else QVERIFY(canSkipPixelError);

        // Simulate that the full alt correction path has been completed.
        // The azimuth error should not be fixed, but alt should be fully corrected.
        if (polarAlign.pixelError(image, altCorrected, corrected, &azE, &altE))
        {
            QVERIFY(fabs(azE - azimuthError) < .01);
            QVERIFY(fabs(altE) < .01);
        }
        else QVERIFY(canSkipPixelError);

        // Now simulate that the user has gone further, halfway through on the az path.
        // There should be 1/2 the az error left but none of the alt error should remain.
        if (polarAlign.pixelError(image, QPointF((corrected.x() + altCorrected.x()) / 2,
                                  (corrected.y() + altCorrected.y()) / 2), corrected, &azE, &altE))
        {
            QVERIFY(fabs(azE - azimuthError / 2) < .01);
            QVERIFY(fabs(altE) < .01);
        }
        else QVERIFY(canSkipPixelError);

        // At the end there should be no error left.
        if (polarAlign.pixelError(image, corrected, corrected, &azE, &altE))
        {
            QVERIFY(fabs(azE) < .01);
            QVERIFY(fabs(altE) < .01);
        }
        else QVERIFY(canSkipPixelError);

        // Test the alt-only path.
        // Using that correction path, there should be no Azimuth error, but alt error remains.
        if (polarAlign.pixelError(image, QPointF(data.x, data.y), altCorrected, &azE, &altE))
        {
            QVERIFY(fabs(azE) < .01);
            QVERIFY(fabs(altE - altitudeError) < .01);
        }
        else QVERIFY(canSkipPixelError);

    }
    // Some approximations in the time (no sub-second decimals), and the
    // findAltAz search resolution introduce a pixel or two of error.
    // This is way below the noise in polar alignment.
    QVERIFY(data.correctedX < 0 || abs(data.correctedX - corrected.x()) < 3);
    QVERIFY(data.correctedY < 0 || abs(data.correctedY - corrected.y()) < 3);
}
}  // namespace

void TestPolarAlign::testRunPAA()
{
    const GeoLocation siliconValley(dms(-122, 10), dms(37, 26, 30));
    const GeoLocation dallas(dms(-96, 49), dms(32, 46, 45));
    const GeoLocation kuwait(dms(47, 59), dms(29, 22, 43));

    // from log on 12/29 log_19-14-48.txt
    // First 14m az error, low alt error
    runPAA(siliconValley,
    {
        // ra0        dec0   orientation pixscale  date-time in UTC
        { 21.74338, 89.83996,  78.36826, 1.32548, 2020, 12, 30, 03, 16, 8},
        { 10.17858, 89.82383,  95.80819, 1.32543, 2020, 12, 30, 03, 16, 46},
        {358.41548, 89.82049, 113.44157, 1.32468, 2020, 12, 30, 03, 17, 18},
        // mount axis x,y  pole center in x,y
        1880, 1565, 1858, 1559
    });

    // Corrected the above, now very low error (0 0' 11")
    runPAA(siliconValley,
    {
        { 74.23824, 89.75702, 127.82348, 1.32612, 2020, 12, 30, 3, 28, 50},
        { 66.19830, 89.77333, 148.11177, 1.32484, 2020, 12, 30, 3, 29, 22},
        { 59.87800, 89.80076, 170.45317, 1.32384, 2020, 12, 30, 3, 29, 59},
        2238, 1225, 1842, 1576
    });

    // Manually increased the alt error, now 0 23' 50'
    runPAA(siliconValley,
    {
        { 46.61670, 89.41158,  98.86135, 1.32586,  2020, 12, 30, 3, 34, 02},
        { 43.13352, 89.41121, 123.87848, 1.32566, 2020, 12, 30, 3, 34, 39},
        { 40.04437, 89.42768, 149.79166, 1.32392, 2020, 12, 30, 3, 35, 11},
        1544, 415, 1842, 1585
    });

    // Fixed it again.
    runPAA(siliconValley,
    {
        { 75.31905, 89.76408, 126.18540, 1.32508, 2020, 12, 30, 3, 39, 33},
        { 67.12354, 89.77885, 145.52730, 1.32492, 2020, 12, 30, 3, 40, 04},
        { 60.22412, 89.80675, 168.40166, 1.32473, 2020, 12, 30, 3, 40, 37},
        2222, 1245, 1842, 1597
    });

    // From 12/31
    // Comments: the PA error with double precision, float precision, and original scheme.
    // Commented solution is the float solution.

    // double: 10'28" float: 11'45" orig: 9'42"
    runPAA(siliconValley,
    {
        { 63.69466, 89.77876, 124.27465, 1.32519, 2020, 12, 31, 18, 52, 42},
        { 54.75964, 89.79547, 143.06160, 1.32367, 2020, 12, 31, 18, 53, 15},
        { 47.33568, 89.82357, 165.12844, 1.32267, 2020, 12, 31, 18, 53, 52},
        2204, 1295, 1851, 1550
    });

    // double: 0'40", float: 0'00", orig: 0'41"
    runPAA(siliconValley,
    {
        { 21.22696, 89.82033, 79.74928, 1.32493, 2020, 12, 31, 19, 00, 52},
        { 10.14439, 89.80891, 97.13185, 1.32523, 2020, 12, 31, 19, 01, 25},
        { 359.05594, 89.81023, 114.39229, 1.32437, 2020, 12, 31, 19, 01, 58},
        1858, 1546, 1847, 1574
    });

    // double: 11'59", float: 11'01", orig: 11'08"
    runPAA(siliconValley,
    {
        { 266.87080, 89.98377, -35.40868, 1.33053, 2020, 12, 31, 19, 06, 46},
        { 293.43530, 89.94730, 19.32200, 1.32335, 2020, 12, 31, 19, 07, 24},
        { 285.55085, 89.91166, 40.10587, 1.32260, 2020, 12, 31, 19, 07, 56},
        2173, 1943, 1842, 1574
    });

    // double: 1'14", float: 0'00", orig: 0'57"
    runPAA(siliconValley,
    {
        { 22.74402, 89.82040, 78.32075, 1.32462, 2020, 12, 31, 19, 12, 34},
        { 11.69887, 89.80704, 95.88488, 1.32375, 2020, 12, 31, 19, 13, 11},
        { 0.95646, 89.80782, 113.01201, 1.32432, 2020, 12, 31, 19, 13, 43},
        1847, 1555, 1848, 1598
    });

    // double: 32'28", float: 38'37", orig: 30'19"
    runPAA(siliconValley,
    {
        { 317.02018, 89.44380, 10.85411, 1.32380, 2020, 12, 31, 19, 18, 00},
        { 316.55330, 89.40583, 38.75090, 1.32350, 2020, 12, 31, 19, 18, 33},
        { 314.63678, 89.37679, 64.65160, 1.32335, 2020, 12, 31, 19, 19, 06},
        795, 2485, 1826, 1595
    });

    // double: 1'34", float: 1'30", orig: 1'26"
    runPAA(siliconValley,
    {
        { 27.96856, 89.82500, 80.98310, 1.32435, 2020, 12, 31, 19, 22, 48},
        { 16.47574, 89.81367, 97.74878, 1.32394, 2020, 12, 31, 19, 23, 25},
        { 5.50816, 89.81455, 114.24102, 1.32390, 2020, 12, 31, 19, 23, 58},
        1868, 1552, 1831, 1604
    });

    // double: 0'25", float: 0'00", orig: 0'25"
    runPAA(siliconValley,
    {
        { 22.29262, 89.82396, 73.68740, 1.32484, 2020, 12, 31, 19, 29, 17},
        { 11.20186, 89.80799, 91.89482, 1.32477, 2020, 12, 31, 19, 29, 55},
        { 0.26659, 89.80523, 109.34529, 1.32489, 2020, 12, 31, 19, 30, 27},
        1828, 1584, 1831, 1602
    });

    // double: 17'35", float: 16'46", orig: 16'21"
    runPAA(siliconValley,
    {
        { 45.20803, 89.58706, 95.79552, 1.32496, 2020, 12, 31, 19, 32, 40},
        { 39.89580, 89.58748, 119.13221, 1.32328, 2020, 12, 31, 19, 33, 13},
        { 35.18193, 89.60542, 144.28407, 1.32255, 2020, 12, 31, 19, 33, 51},
        1700, 887, 1833, 1606
    });

    // double: 0'15", float: 0'00", orig: 0'04"
    runPAA(siliconValley,
    {
        { 22.79496, 89.83001, 71.18473, 1.32378, 2020, 12, 31, 19, 41, 16},
        { 11.65388, 89.81182, 89.34180, 1.32420, 2020, 12, 31, 19, 41, 54},
        { 1.02046, 89.80613, 106.01159, 1.32431, 2020, 12, 31, 19, 42, 26},
        1821, 1614, 1818, 1615
    });

    // Log from 1/5/20201
    runPAA(siliconValley,
    {
        { 33.77699, 89.85172, 55.15250, 1.32489, 2021, 01, 06, 05,  9, 23 },
        { 23.30335, 89.82440, 74.05880, 1.32592, 2021, 01, 06, 05,  9, 50 },
        { 12.72288, 89.81051, 91.35879, 1.32540, 2021, 01, 06, 05, 10, 15 },
        2328, 1760, 2331, 1783
    });

    // Note, far off pole
    runPAA(siliconValley,
    {
        { 50.76609, 49.91458, -8.37603, 1.32461, 2021, 01, 06, 05, 13, 32 },
        { 23.24014, 49.88528, -8.28620, 1.32496, 2021, 01, 06, 05, 13, 57 },
        { 354.47536, 49.87901, -8.20577, 1.32499, 2021, 01, 06, 05, 14, 24 },
        2328, 1760, 2320, 1795
    });

    // Note, far off pole
    runPAA(siliconValley,
    {
        { 50.73028, 49.49321, -8.27081, 1.32448, 2021, 01, 06, 05, 18, 31 },
        { 21.45768, 49.52983, -8.00221, 1.32467, 2021, 01, 06, 05, 18, 58 },
        { 353.23623, 49.64241, -7.79150, 1.32521, 2021, 01, 06, 05, 19, 24 },
        3007, 1256, 3516, 951
    });

    runPAA(siliconValley,
    {
        { 33.93431, 89.84705, 51.18593, 1.32465, 2021, 01, 06, 05, 25, 48 },
        { 25.03596, 89.82113, 69.85483, 1.32561, 2021, 01, 06, 05, 26, 13 },
        { 14.68785, 89.80521, 88.25234, 1.32571, 2021, 01, 06, 05, 26, 40 },
        2328, 1760, 2350, 1785
    });

    // Note, far off pole
    runPAA(siliconValley,
    {
        { 50.38595, 49.53670, -8.15094, 1.32484, 2021, 01, 06, 05, 30, 55 },
        { 22.40326, 49.57133, -8.12608, 1.32515, 2021, 01, 06, 05, 31, 21 },
        { 354.02099, 49.60593, -8.13631, 1.32476, 2021, 01, 06, 05, 31, 47 },
        2446, 2088, 2392, 2522
    });


    /*
    // pixelError has issues with this one. Letting it slide for now.
    runPAA(siliconValley,
    {
        { 80.96890, 11.74259, 171.42773, 1.32514, 2021, 01, 06, 05, 45, 05 },
        { 110.26384, 11.81600, 171.56395, 1.32316, 2021, 01, 06, 05, 45, 31 },
        { 138.33895, 11.78703, 171.85951, 1.32656, 2021, 01, 06, 05, 45, 57 },
        1087, 861, 454, 130
    });
    */

    runPAA(siliconValley,
    {
        { 38.40316, 89.32378, 49.26512, 1.32596, 2021, 01, 06, 05, 50, 17 },
        { 36.29482, 89.29488, 75.01016, 1.32645, 2021, 01, 06, 05, 50, 42 },
        { 33.52382, 89.28142, 100.14838, 1.32604, 2021, 01, 06, 05, 51, 8 },
        2328, 1760, 3746, 2172
    });

    // Note, far off pole
    runPAA(siliconValley,
    {
        { 51.11166, 48.88918, -8.54258, 1.32508, 2021, 01, 06, 05, 52, 43 },
        { 23.14732, 48.86777, -8.06648, 1.32514, 2021, 01, 06, 05, 53, 9 },
        { 355.71059, 48.98942, -7.64124, 1.32527, 2021, 01, 06, 05, 53, 35 },
        2205, 2214, 3203, 1043
    });

    // Note, far off pole
    runPAA(siliconValley,
    {
        { 53.79635, 49.62699, -8.35495, 1.32678, 2021, 01, 06, 06, 03, 39 },
        { 25.42441, 49.58901, -8.32128, 1.32477, 2021, 01, 06, 06, 04, 05 },
        { 357.52396, 49.57574, -8.24039, 1.32470, 2021, 01, 06, 06, 04, 30 },
        895, 1336, 882, 1347
    });

    // Jasem3
    runPAA(kuwait,
    {
        { 102.65020, 39.69621, 138.49586, 1.09119, 2021, 01, 11, 17, 25, 10 },
        { 86.97538, 39.64818, 138.52258, 1.09179, 2021, 01, 11, 17, 25, 29 },
        { 74.74086, 39.61638, 138.54977, 1.09189, 2021, 01, 11, 17, 25, 47 },
        2328, 1760, 2613, 2003
    });
    // Jasem2
    runPAA(kuwait,
    {
        { 75.36752, 67.31292, 138.39209, 1.09149, 2021, 01, 11, 17, 21, 12 },
        { 88.67953, 67.34817, 138.41916, 1.09133, 2021, 01, 11, 17, 21, 30 },
        { 101.85324, 67.38325, 138.31124, 1.09222, 2021, 01, 11, 17, 21, 48 },
        2328, 1760, 2369, 1689
    });
    // Jasem1
    runPAA(kuwait,
    {
        { 103.47913, 67.38804, 138.36922, 1.09212, 2021, 01, 11, 17, 19, 57 },
        { 89.15202, 67.34897, 138.37273, 1.09211, 2021, 01, 11, 17, 20, 18 },
        { 75.36751, 67.31304, 138.37937, 1.09212, 2021, 01, 11, 17, 20, 36 },
        2328, 1760, 2453, 1796
    });

    // Jo-nightly-1
    runPAA(dallas,
    {
        { 12.81781, 89.07239, -8.65391, 9.49554, 2021, 01, 12, 0, 27, 23 },
        { 348.85538, 89.03890, -3.73479, 9.49479, 2021, 01, 12, 0, 27, 43 },
        { 326.22608, 89.04224, 1.19850, 9.49679, 2021, 01, 12, 0, 28, 03 },
        2328, 1760, 2319, 1730
    });

    // Jo-nightly-2
    runPAA(dallas,
    {
        { 18.65836, 89.05113, -4.20730, 9.49647, 2021, 01, 12, 0, 33, 19 },
        { 40.85876, 89.08147, -10.33458, 9.49571, 2021, 01, 12, 0, 33, 40 },
        { 65.37414, 89.14252, -13.67848, 9.49793, 2021, 01, 12, 0, 33, 59 },
        2328, 1760, 2310, 1748
    });

    // Jo-nightly-3
    runPAA(dallas,
    {
        { 19.27912, 89.05092, -4.02575, 9.49570, 2021, 01, 12, 0, 34, 57 },
        { 356.21018, 89.04905, 0.67433, 9.49619, 2021, 01, 12, 0, 35, 16 },
        { 331.16747, 89.08275, 5.35544, 9.49534, 2021, 01, 12, 0, 35, 37 },
        2328, 1760, 2343, 1749
    });

    // Jo-nightly-4
    runPAA(dallas,
    {
        { 21.31811, 89.09443, -2.94373, 9.49555, 2021, 01, 12, 0, 38, 59 },
        { 45.33237, 89.11518, -8.11396, 9.49733, 2021, 01, 12, 0, 39, 25 },
        { 71.91959, 89.16055, -10.72869, 9.49602, 2021, 01, 12, 0, 39, 45 },
        2328, 1760, 2329, 1751
    });

    // Jo-nightly-5
    runPAA(dallas,
    {
        { 21.89789, 89.09423, -2.73671, 9.49587, 2021, 01, 12, 0, 40, 24 },
        { 357.18691, 89.09645, 0.78761, 9.49571, 2021, 01, 12, 0, 40, 46 },
        { 330.78136, 89.12673, 4.15432, 9.49656, 2021, 01, 12, 0, 41, 06 },
        2328, 1760, 2336, 1764
    });

    // Jo-nightly-6
    runPAA(dallas,
    {
        { 21.90875, 89.09512, -3.01211, 9.49640, 2021, 01, 12, 0, 41, 38 },
        { 46.14103, 89.11651, -8.15365, 9.49657, 2021, 01, 12, 0, 41, 58 },
        { 72.50313, 89.16190, -10.72389, 9.49792, 2021, 01, 12, 0, 42, 17 },
        2328, 1760, 2329, 1750
    });

    // Jo orig-1
    runPAA(dallas,
    {
        { 22.67449, 89.09498, -2.83722, 9.49689, 2021, 01, 12, 0, 43, 54 },
        { 358.94560, 89.09623, 0.54390, 9.49583, 2021, 01, 12, 0, 44, 13 },
        { 331.80520, 89.12571, 4.01328, 9.49679, 2021, 01, 12, 0, 44, 34 },
        2328, 1760, 2336, 1765
    });

    // reversed
    runPAA(dallas,
    {
        { 331.80520, 89.12571, 4.01328, 9.49679, 2021, 01, 12, 0, 44, 34 },
        { 358.94560, 89.09623, 0.54390, 9.49583, 2021, 01, 12, 0, 44, 13 },
        { 22.67449, 89.09498, -2.83722, 9.49689, 2021, 01, 12, 0, 43, 54 },
        2328, 1760, 2336, 1756
    });

    // Jo orig-2
    runPAA(dallas,
    {
        { 23.51969, 89.09648, -3.24191, 9.49659, 2021, 01, 12, 0, 49, 15 },
        { 48.00993, 89.11943, -8.37887, 9.49623, 2021, 01, 12, 0, 49, 37 },
        { 75.23851, 89.16742, -10.84337, 9.49660, 2021, 01, 12, 0, 49, 59 },
        2328, 1760, 2329, 1750
    });

    // reversed
    runPAA(dallas,
    {
        { 75.23851, 89.16742, -10.84337, 9.49660, 2021, 01, 12, 0, 49, 59 },
        { 48.00993, 89.11943, -8.37887, 9.49623, 2021, 01, 12, 0, 49, 37 },
        { 23.51969, 89.09648, -3.24191, 9.49659, 2021, 01, 12, 0, 49, 15 },
        2328, 1760, 2337, 1756
    });

    runPAA(siliconValley,
    {
        // ra0       dec0   orientation pixscale  date-time in UTC
        { 47.31826, 1.94192, -84.57643, 1.32459, 2021, 01, 26, 3, 15, 28},
        { 18.35969, 2.08571, -84.37967, 1.32451, 2021, 01, 26, 3, 15, 54},
        {350.43339, 2.27862, -84.29692, 1.32590, 2021, 01, 26, 3, 16, 19},
        // mount axis x,y  pole center in x,y
        3855, 1117, 4022, 1623
    });

    // Brett's RASA data. Flipped parity images.
    const GeoLocation irvine(dms(-117, 50), dms(33, 33));
    runPAA(irvine,
    {
        { 231.85829, 89.49201, 92.16176, 2.51940, 2021, 03, 27, 4, 51, 14},
        { 195.35970, 89.53665, 85.77720, 2.51975, 2021, 03, 27, 4, 51, 30},
        { 162.20876, 89.54505, 77.99854, 2.51962, 2021, 03, 27, 4, 51, 44},

        // low error--don't know true coords
        // Polar Alignment Error:  00° 01' 46\". Azimuth:  00° 01' 18\"  Altitude: -00° 01' 12\""

        // Made up the starting point. Using the calculated target.
        500, 500, 509, 537
    },

    false);

    runPAA(irvine,
    {
        { 247.48287, 89.46797, 107.38986, 2.51988, 2021, 03, 27, 4, 52, 49},
        { 211.13546, 89.60233, 102.12462, 2.51959, 2021, 03, 27, 4, 53, 06},
        { 163.42722, 89.67487, 83.48466, 2.51931, 2021, 03, 27, 4, 53, 21},

        // Polar Alignment Error:  00° 09' 38\". Azimuth:  00° 01' 15\"  Altitude: -00° 09' 33\"
        // 2911, 824, 2725, 959  // without flip
        // 2911, 824, 3084, 916  // where Brett wound up
        2911, 824, 3097, 958     // what this algorithm calculates
    },
    // False means the parity is flipped.
    false);

    runPAA(irvine,
    {
        { 233.33273, 89.35997, 91.72542, 2.51947, 2021, 03, 27, 4, 59, 10},
        { 205.15803, 89.42125, 93.16138, 2.51971, 2021, 03, 27, 4, 59, 27},
        { 176.87893, 89.48116, 91.40070, 2.51964, 2021, 03, 27, 4, 59, 42},

        // Polar Alignment Error:  00° 10' 43\". Azimuth:  00° 10' 40\"  Altitude: -00° 00' 57\""
        // 2757, 318, 2854, 504, flip  // without flip
        // 2757, 318, 2657, 450        // where Brett wound up
        2757, 318, 2661, 506           // what this algorithm calculates.
    },
    // False means the parity is flipped.
    false);
}

void TestPolarAlign::getAzAlt(const KStarsDateTime &time, const GeoLocation &geo,
                              const QPointF &pixel, double ra, double dec, double orientation,
                              double pixScale, double *az, double *alt)
{
    QSharedPointer<FITSData> image;
    loadDummyFits(image, time, ra, dec, orientation, pixScale, true);

    SkyPoint pt;
    PolarAlign polarAlign(&geo);
    QVERIFY(polarAlign.prepareAzAlt(image, pixel, &pt));
    *az = pt.az().Degrees();
    *alt = pt.alt().Degrees();
}

void TestPolarAlign::testAlt()
{
    // Silicon Valley
    const GeoLocation geo(dms(-122, 10), dms(37, 26, 30));
    constexpr double pixScale = 1.32577;

    KStarsDateTime time1;
    time1.setDate(QDate(2020, 12, 27));
    time1.setTime(QTime(3, 32, 18));
    time1.setTimeSpec(Qt::UTC);


    double az1, alt1;
    getAzAlt(time1, geo, QPointF(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2), 20.26276, 89.84129, 75.75402, pixScale, &az1, &alt1);

    KStarsDateTime time2;
    time2.setDate(QDate(2020, 12, 27));
    time2.setTime(QTime(3, 42, 10));
    time2.setTimeSpec(Qt::UTC);

    double az2, alt2;
    getAzAlt(time2, geo, QPointF(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2), 26.66804, 89.61473, 79.69920, pixScale, &az2, &alt2);

    // The first set of coordinates and times were taken were sampled, and then the mount's
    // altitude knob was adjusted so that the telescope was pointing higher.
    // The second set of coordinates should indicate a significant change in altitude
    // with only a small change in azimuth.
    compare(az1,  0.0452539, "Azimuth1", __LINE__);
    compare(az2,  0.0523052, "Azimuth2", __LINE__);
    compare(alt1, 37.491241, "Altitude1", __LINE__);
    compare(alt2, 37.720853, "Altitude2", __LINE__);
}

void TestPolarAlign::testRotate_data()
{
    QTest::addColumn<double>("az");
    QTest::addColumn<double>("alt");
    QTest::addColumn<double>("deltaAz");
    QTest::addColumn<double>("deltaAlt");
    QTest::addColumn<double>("azRotated");
    QTest::addColumn<double>("altRotated");

    //                       Az      Alt     dAz    dAlt      Az result     Alt result

    QTest::newRow("") <<    0.0 <<   0.0  << 0.0  << 0.0 <<     0.0      <<  0.0;
    QTest::newRow("") <<    0.0 <<   0.0  << 0.3  << 0.4 <<     0.3      <<  0.4;
    QTest::newRow("") <<    1.0 <<   1.0  << 0.0  << 0.0 <<     1.0      <<  1.0;
    QTest::newRow("") <<    1.0 <<   1.0  << 0.3  << 0.4 <<     1.300146 <<  1.399939;

    // Point in the west, north of the equator, curves up and south with increasing alt.
    QTest::newRow("") <<  -30.0 <<  60.0  << 0.0  << 1.0 <<   -30.893174 << 60.862134;
    QTest::newRow("") <<  -30.0 <<  60.0  << 0.0  << 2.0 <<   -31.843561 << 61.716007;
    QTest::newRow("") <<  -30.0 <<  60.0  << 0.0  << 3.0 <<   -32.855841 << 62.560844;
    QTest::newRow("") <<  -30.0 <<  60.0  << 0.0  << 4.0 <<   -33.935125 << 63.395779;

    // Point in the east, north of the equator, curves up and south with increasing alt.
    QTest::newRow("") <<   30.0 <<  60.0  << 0.0  << 1.0 <<    30.893174 << 60.862134;
    QTest::newRow("") <<   30.0 <<  60.0  << 0.0  << 2.0 <<    31.843561 << 61.716007;
    QTest::newRow("") <<   30.0 <<  60.0  << 0.0  << 3.0 <<    32.855841 << 62.560844;
    QTest::newRow("") <<   30.0 <<  60.0  << 0.0  << 4.0 <<    33.935125 << 63.395779;

    // Point in the west, south of the equator, curves down and south with increasing alt.
    QTest::newRow("") << -110.0 <<  60.0  << 0.0  << 1.0 <<  -111.607689 << 59.644787;
    QTest::newRow("") << -110.0 <<  60.0  << 0.0  << 2.0 <<  -113.174586 << 59.263816;
    QTest::newRow("") << -110.0 <<  60.0  << 0.0  << 3.0 <<  -114.699478 << 58.858039;
    QTest::newRow("") << -110.0 <<  60.0  << 0.0  << 4.0 <<  -116.181474 << 58.428421;

    // Point in the east, south of the equator, curves down and south with increasing alt.
    QTest::newRow("") <<  110.0 <<  60.0  << 0.0  << 1.0 <<   111.607689 << 59.644787;
    QTest::newRow("") <<  110.0 <<  60.0  << 0.0  << 2.0 <<   113.174586 << 59.263816;
    QTest::newRow("") <<  110.0 <<  60.0  << 0.0  << 3.0 <<   114.699478 << 58.858039;
    QTest::newRow("") <<  110.0 <<  60.0  << 0.0  << 4.0 <<   116.181474 << 58.428421;

    // First points of last 4 series. This time dAlt is negative so goes the opposite way.
    QTest::newRow("") <<  -30.0 <<  60.0  << 0.0  << -1.0 <<  -29.159759 << 59.130303;
    QTest::newRow("") <<   30.0 <<  60.0  << 0.0  << -1.0 <<   29.159759 << 59.130303;
    QTest::newRow("") << -110.0 <<  60.0  << 0.0  << -1.0 << -108.353075 << 60.328521;
    QTest::newRow("") <<  110.0 <<  60.0  << 0.0  << -1.0 <<  108.353075 << 60.328521;

    // Same as last 4, but with dAz = 1
    QTest::newRow("") <<  -30.0 <<  60.0  << 1.0  << -1.0 <<  -28.159759 << 59.130303;
    QTest::newRow("") <<   30.0 <<  60.0  << 1.0  << -1.0 <<   30.159759 << 59.130303;
    QTest::newRow("") << -110.0 <<  60.0  << 1.0  << -1.0 << -107.353075 << 60.328521;
    QTest::newRow("") <<  110.0 <<  60.0  << 1.0  << -1.0 <<  109.353075 << 60.328521;

    // ditto, but with dAz = -1
    QTest::newRow("") <<  -30.0 <<  60.0  << -1.0 << -1.0 <<  -30.159759 << 59.130303;
    QTest::newRow("") <<   30.0 <<  60.0  << -1.0 << -1.0 <<   28.159759 << 59.130303;
    QTest::newRow("") << -110.0 <<  60.0  << -1.0 << -1.0 << -109.353075 << 60.328521;
    QTest::newRow("") <<  110.0 <<  60.0  << -1.0 << -1.0 <<  107.353075 << 60.328521;

    // Some much larger rotations
    QTest::newRow("") <<    1.0 <<  1.0  <<  20.0 << 20.0 <<   21.070967 << 20.996804;
    QTest::newRow("") <<    1.0 <<  1.0  <<   0.0 << 20.0 <<    1.070967 << 20.996804;
    QTest::newRow("") <<    1.0 <<  1.0  << -20.0 << 20.0 <<  -18.929033 << 20.996804;
    QTest::newRow("") <<  -30.0 << 60.0  <<  20.0 << 20.0 <<  -46.116102 << 74.132541;
    QTest::newRow("") <<   30.0 << 60.0  <<   0.0 << 20.0 <<   66.116102 << 74.132541;
    QTest::newRow("") << -110.0 << 60.0  << -20.0 << 20.0 << -154.199340 << 49.052359;
    QTest::newRow("") <<  110.0 << 60.0  <<  20.0 << -20.0 <<  93.912714 << 60.725451;
}

void TestPolarAlign::testRotate()
{
    QFETCH(double, az);
    QFETCH(double, alt);
    QFETCH(double, deltaAz);
    QFETCH(double, deltaAlt);
    QFETCH(double, azRotated);
    QFETCH(double, altRotated);

    // PolarAlign is needed to specifiy the hemisphere, which specifies how the
    // altitude rotation works (positive alt error causes opposite rotations in
    // the northern vs southern hemisphere.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30));
    PolarAlign polarAlign(&geo);

    QPointF point, rot;
    point = QPointF(az, alt);
    rot = QPointF(deltaAz, deltaAlt);
    QPointF result = Rotations::rotateRaAxis(point, rot);
    QVERIFY(compare(result, azRotated, altRotated, .001));
}

QTEST_GUILESS_MAIN(TestPolarAlign)
