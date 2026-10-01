/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsviewer/pipeline/stackmemoryestimator.h"

#include <QTemporaryDir>
#include <QTest>

#include <fitsio.h>

#include <vector>

using namespace StackMemoryEstimator;

class TestStackMemoryEstimator : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testFrameBytes();
        void testEngineFrames();
        void testMinFirstBatch();
        void testFitBatch_data();
        void testFitBatch();
        void testEstimate();
        void testEstimateUnknownMemory();
        void testEstimateChannelStacks();
        void testChooseEvictions();
        void testPeekFrameGeometry();
};

namespace
{
constexpr double MB = 1e6;

EngineConfig sigmaWithMasters()
{
    EngineConfig config;
    config.method = StackingMethod::SIGMA;
    config.masterDark = true;
    config.masterFlat = true;
    return config;
}

// 2D USHORT image, or a 3-plane one when planes == 3, optionally with a BAYERPAT
bool writeFrame(const QString &path, int width, int height, int planes, const char *bayerPattern)
{
    fitsfile *fptr = nullptr;
    int status = 0;
    long naxes[3] = { width, height, planes };
    const QByteArray name = path.toLocal8Bit();
    if (fits_create_diskfile(&fptr, name.constData(), &status))
        return false;
    fits_create_img(fptr, USHORT_IMG, planes > 1 ? 3 : 2, naxes, &status);
    if (bayerPattern)
        fits_update_key(fptr, TSTRING, "BAYERPAT", const_cast<char *>(bayerPattern), nullptr, &status);
    std::vector<unsigned short> pixels(static_cast<size_t>(width) * height * planes, 1000);
    fits_write_img(fptr, TUSHORT, 1, static_cast<LONGLONG>(pixels.size()), pixels.data(), &status);
    fits_close_file(fptr, &status);
    return status == 0;
}
}

void TestStackMemoryEstimator::testFrameBytes()
{
    const FrameGeometry mono { 6252, 4176, 1 };
    QCOMPARE(frameBytes(mono, StackDownscale::NONE), 6252.0 * 4176 * 4);
    // Integer division of each side, like FITSStack::convertMat()
    QCOMPARE(frameBytes(mono, StackDownscale::X2), 3126.0 * 2088 * 4);
    QCOMPARE(frameBytes(mono, StackDownscale::X3), 2084.0 * 1392 * 4);

    const FrameGeometry cfa { 100, 50, 3 };
    QCOMPARE(frameBytes(cfa, StackDownscale::NONE), 100.0 * 50 * 3 * 4);

    QCOMPARE(frameBytes(FrameGeometry(), StackDownscale::NONE), 0.0);
}

void TestStackMemoryEstimator::testEngineFrames()
{
    EngineConfig mean;
    mean.method = StackingMethod::MEAN;
    QCOMPARE(engineFrames(mean), 2.0);

    QCOMPARE(engineFrames(sigmaWithMasters()), 7.0);

    EngineConfig imageMM;
    imageMM.method = StackingMethod::IMAGEMM;
    imageMM.linearNormalization = true;
    QCOMPARE(engineFrames(imageMM), 4.0);
    QCOMPARE(framesPerBatchSub(StackingMethod::IMAGEMM), 2.0);
    QCOMPARE(framesPerBatchSub(StackingMethod::WINDSOR), 1.0);
}

void TestStackMemoryEstimator::testMinFirstBatch()
{
    QCOMPARE(minFirstBatch(StackingMethod::SIGMA, 30), kMinRejectionBatch);
    QCOMPARE(minFirstBatch(StackingMethod::WINDSOR, 3), 3);
    QCOMPARE(minFirstBatch(StackingMethod::MEAN, 30), 1);
    QCOMPARE(minFirstBatch(StackingMethod::SIGMA, 0), 1);
}

void TestStackMemoryEstimator::testFitBatch_data()
{
    QTest::addColumn<double>("frame");
    QTest::addColumn<int>("method");
    QTest::addColumn<double>("reserve");
    QTest::addColumn<int>("files");
    QTest::addColumn<double>("avail");
    QTest::addColumn<int>("expected");

    // budget = 0.8 x avail; batch = budget/frame - reserve - 6 working frames
    QTest::newRow("everything fits") << 100 * MB << int(StackingMethod::SIGMA) << 7.0 << 30 << 100000 * MB << 30;
    QTest::newRow("tight") << 100 * MB << int(StackingMethod::SIGMA) << 7.0 << 30 << 2000 * MB << 3;
    QTest::newRow("engine already allocated") << 100 * MB << int(StackingMethod::SIGMA) << 0.0 << 30 << 2000 * MB << 10;
    QTest::newRow("imagemm keeps a history") << 100 * MB << int(StackingMethod::IMAGEMM) << 0.0 << 30 << 2000 * MB << 5;
    QTest::newRow("nothing fits") << 100 * MB << int(StackingMethod::SIGMA) << 7.0 << 30 << 500 * MB << 1;
    QTest::newRow("unknown memory") << 100 * MB << int(StackingMethod::SIGMA) << 7.0 << 30 << 0.0 << 30;
}

void TestStackMemoryEstimator::testFitBatch()
{
    QFETCH(double, frame);
    QFETCH(int, method);
    QFETCH(double, reserve);
    QFETCH(int, files);
    QFETCH(double, avail);
    QFETCH(int, expected);
    QCOMPARE(fitBatch(frame, static_cast<StackingMethod>(method), reserve, files, avail), expected);
}

void TestStackMemoryEstimator::testEstimate()
{
    // 1000x1000 mono: 4 MB frames. SIGMA with masters, 10 subs:
    //   full batch = (7 + 6 + 10) x 4 MB = 92 MB, smallest first batch = (7 + 6 + 5) x 4 MB = 72 MB
    const FrameGeometry geometry { 1000, 1000, 1 };
    const EngineConfig config = sigmaWithMasters();

    Estimate plenty = estimate(geometry, 10, config, 1000 * MB);
    QCOMPARE(plenty.options.size(), 4);
    QCOMPARE(plenty.options[0].quality, Quality::Full);
    QCOMPARE(plenty.options[0].batchSize, 10);
    QCOMPARE(plenty.options[0].fullBatchBytes, 92 * MB);
    QCOMPARE(plenty.recommended, StackDownscale::NONE);
    QVERIFY(plenty.anyFits);

    // budget 80 MB: full resolution fits only in smaller batches (80/4 - 13 = 7 subs)
    Estimate reduced = estimate(geometry, 10, config, 100 * MB);
    QCOMPARE(reduced.options[0].quality, Quality::ReducedBatch);
    QCOMPARE(reduced.options[0].batchSize, 7);
    QCOMPARE(reduced.options[1].quality, Quality::Full);
    QCOMPARE(reduced.recommended, StackDownscale::NONE);

    // budget 40 MB: full resolution doesn't fit at all, half resolution fits in one batch
    Estimate half = estimate(geometry, 10, config, 50 * MB);
    QCOMPARE(half.options[0].quality, Quality::Insufficient);
    QCOMPARE(half.options[0].batchSize, 0);
    QCOMPARE(half.options[1].quality, Quality::Full);
    QCOMPARE(half.options[1].width, 500);
    QCOMPARE(half.recommended, StackDownscale::X2);

    Estimate none = estimate(geometry, 10, config, 1 * MB);
    QVERIFY(!none.anyFits);
    QCOMPARE(none.recommended, StackDownscale::X4);
    for (const auto &option : none.options)
        QCOMPARE(option.quality, Quality::Insufficient);
}

void TestStackMemoryEstimator::testEstimateUnknownMemory()
{
    const Estimate unknown = estimate({ 1000, 1000, 1 }, 10, sigmaWithMasters(), 0.0);
    QVERIFY(unknown.anyFits);
    QCOMPARE(unknown.recommended, StackDownscale::NONE);
    QCOMPARE(unknown.options[0].quality, Quality::Full);
}

void TestStackMemoryEstimator::testEstimateChannelStacks()
{
    // Three channel stacks keep three engines: (21 + 6 + 10) x 4 MB = 148 MB
    const Estimate rgb = estimate({ 1000, 1000, 1 }, 10, sigmaWithMasters(), 1000 * MB, 3);
    QCOMPARE(rgb.options[0].fullBatchBytes, 148 * MB);
}

void TestStackMemoryEstimator::testChooseEvictions()
{
    const QVector<EvictionCandidate> candidates
    {
        { "recent", 300 * MB, 30 },
        { "oldest", 100 * MB, 10 },
        { "middle", 200 * MB, 20 },
    };

    QVERIFY(chooseEvictions(candidates, 1000 * MB, 500 * MB).isEmpty());
    QCOMPARE(chooseEvictions(candidates, 400 * MB, 500 * MB), QStringList { "oldest" });
    QCOMPARE(chooseEvictions(candidates, 250 * MB, 500 * MB), (QStringList { "oldest", "middle" }));
    // Out of reach: everything goes, since each one freed still helps
    QCOMPARE(chooseEvictions(candidates, 0, 5000 * MB), (QStringList { "oldest", "middle", "recent" }));
    QVERIFY(chooseEvictions({}, 0, 5000 * MB).isEmpty());
}

void TestStackMemoryEstimator::testPeekFrameGeometry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FrameGeometry geometry;
    QVERIFY(writeFrame(dir.filePath("mono.fits"), 64, 32, 1, nullptr));
    QVERIFY(peekFrameGeometry(dir.filePath("mono.fits"), geometry));
    QCOMPARE(geometry.width, 64);
    QCOMPARE(geometry.height, 32);
    QCOMPARE(geometry.channels, 1);

    // A CFA sub is one plane on disk and three once debayered
    QVERIFY(writeFrame(dir.filePath("cfa.fits"), 64, 32, 1, "RGGB"));
    QVERIFY(peekFrameGeometry(dir.filePath("cfa.fits"), geometry));
    QCOMPARE(geometry.channels, 3);

    QVERIFY(writeFrame(dir.filePath("rgb.fits"), 64, 32, 3, nullptr));
    QVERIFY(peekFrameGeometry(dir.filePath("rgb.fits"), geometry));
    QCOMPARE(geometry.channels, 3);

    QVERIFY(!peekFrameGeometry(dir.filePath("missing.fits"), geometry));
}

QTEST_GUILESS_MAIN(TestStackMemoryEstimator)

#include "teststackmemoryestimator.moc"
