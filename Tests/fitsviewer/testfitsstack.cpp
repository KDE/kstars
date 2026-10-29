/*
    SPDX-FileCopyrightText: 2026 KStars contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsviewer/fitsstack.h"

#include <QTest>

#include <wcs.h>

#include <cmath>
#include <cstring>
#include <memory>

// Covers FITSStack::calcWarpMatrix(), which aligns a sub to the align master from their WCS
class TestFitsStack : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testCalcWarpMatrix_data();
        void testCalcWarpMatrix();
};

namespace
{
// ZWO ASI2600MM sub at 2.586"/px
constexpr int FULL_WIDTH = 6248;
constexpr int FULL_HEIGHT = 4176;
constexpr double PIXEL_SCALE_DEG = 2.586 / 3600.0;

struct WcsDeleter
{
    void operator()(wcsprm *wcs) const
    {
        wcsfree(wcs);
        delete wcs;
    }
};
using WcsPtr = std::unique_ptr<wcsprm, WcsDeleter>;

// A plate-solved TAN WCS for a full resolution sub centered on ra/dec and rotated by rotationDeg
WcsPtr makeWcs(double ra, double dec, double rotationDeg)
{
    WcsPtr wcs(new wcsprm);
    wcs->flag = -1;
    if (wcsini(1, 2, wcs.get()) != 0)
        return nullptr;

    std::strcpy(wcs->ctype[0], "RA---TAN");
    std::strcpy(wcs->ctype[1], "DEC--TAN");
    wcs->crval[0] = ra;
    wcs->crval[1] = dec;
    wcs->crpix[0] = (FULL_WIDTH + 1) / 2.0;
    wcs->crpix[1] = (FULL_HEIGHT + 1) / 2.0;
    wcs->cdelt[0] = -PIXEL_SCALE_DEG;
    wcs->cdelt[1] = PIXEL_SCALE_DEG;

    const double rotation = rotationDeg * M_PI / 180.0;
    wcs->pc[0] = std::cos(rotation);
    wcs->pc[1] = -std::sin(rotation);
    wcs->pc[2] = std::sin(rotation);
    wcs->pc[3] = std::cos(rotation);

    if (wcsset(wcs.get()) != 0)
        return nullptr;
    return wcs;
}
} // namespace

void TestFitsStack::testCalcWarpMatrix_data()
{
    QTest::addColumn<int>("downscale");
    QTest::addColumn<double>("rotationDeg");
    QTest::addColumn<double>("raOffsetFrames");
    QTest::addColumn<bool>("expectAligned");

    const QList<QPair<StackDownscale, QString>> downscales
    {
        { StackDownscale::NONE, "none" },
        { StackDownscale::X2, "2x" },
        { StackDownscale::X3, "3x" },
        { StackDownscale::X4, "4x" },
    };

    for (const auto &downscale : downscales)
    {
        const QByteArray nameBytes = downscale.second.toLatin1();
        const char *name = nameBytes.constData();
        QTest::addRow("%s same pier side", name) << static_cast<int>(downscale.first) << 0.0 << 0.0 << true;
        // A meridian flip rotates the frame 180 degrees about its center
        QTest::addRow("%s meridian flip", name) << static_cast<int>(downscale.first) << 180.0 << 0.0 << true;
        QTest::addRow("%s flip with small offset", name) << static_cast<int>(downscale.first) << 180.0 << 0.02 << true;
        // A sub whose center is most of a frame away has to be rejected
        QTest::addRow("%s wrong target", name) << static_cast<int>(downscale.first) << 0.0 << 0.75 << false;
    }
}

void TestFitsStack::testCalcWarpMatrix()
{
    QFETCH(int, downscale);
    QFETCH(double, rotationDeg);
    QFETCH(double, raOffsetFrames);
    QFETCH(bool, expectAligned);

    const double ra = 1.0;
    const double dec = 66.0;
    // Offset along RA by a fraction of the frame width (on sky, so scaled by cos(dec))
    const double raOffset = raOffsetFrames * FULL_WIDTH * PIXEL_SCALE_DEG / std::cos(dec * M_PI / 180.0);

    WcsPtr master = makeWcs(ra, dec, 0.0);
    WcsPtr sub = makeWcs(ra + raOffset, dec, rotationDeg);
    QVERIFY(master);
    QVERIFY(sub);

    StackData params;
    params.downscale = static_cast<StackDownscale>(downscale);
    FITSStack stack(nullptr, StackChannel::SINGLE, params);

    // addSub() records the size after downscaling, so do the same here
    const double factor = stack.getDownscaleFactor();
    const int width = static_cast<int>(FULL_WIDTH / factor);
    const int height = static_cast<int>(FULL_HEIGHT / factor);
    QVERIFY(stack.checkSub(width, height, 2, 1));

    cv::Mat warp;
    QCOMPARE(stack.calcWarpMatrix(master.get(), sub.get(), warp), expectAligned);
    if (!expectAligned)
        return;

    // The warp is returned for the downscaled image. Check it maps the sub's pixel at the
    // master's center back onto the master's center, i.e. the alignment is right.
    QCOMPARE(warp.rows, 3);
    QCOMPARE(warp.cols, 3);

    double world[2], imgcrd[2], pixcrd[2], phi, theta;
    int stat[1];
    const double masterCenter[2] = { (FULL_WIDTH + 1) / 2.0, (FULL_HEIGHT + 1) / 2.0 };
    QCOMPARE(wcsp2s(master.get(), 1, 2, masterCenter, imgcrd, &phi, &theta, world, stat), 0);
    QCOMPARE(wcss2p(sub.get(), 1, 2, world, &phi, &theta, imgcrd, pixcrd, stat), 0);

    const double subX = (pixcrd[0] - 1.0) / factor;
    const double subY = (pixcrd[1] - 1.0) / factor;
    const double alignedX = warp.at<double>(0, 0) * subX + warp.at<double>(0, 1) * subY + warp.at<double>(0, 2);
    const double alignedY = warp.at<double>(1, 0) * subX + warp.at<double>(1, 1) * subY + warp.at<double>(1, 2);
    QVERIFY2(std::abs(alignedX - (masterCenter[0] - 1.0) / factor) < 1.0,
             qPrintable(QString("x %1").arg(alignedX)));
    QVERIFY2(std::abs(alignedY - (masterCenter[1] - 1.0) / factor) < 1.0,
             qPrintable(QString("y %1").arg(alignedY)));
}

QTEST_GUILESS_MAIN(TestFitsStack)

#include "testfitsstack.moc"
