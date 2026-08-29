/*  KStars tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <memory>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include "testfitsdata.h"
#include "Options.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "fitsviewer/fpack.h"
#include "fitsviewer/pipeline/stackcontroller.h"
#include "fitsviewer/pipeline/masterbuilder.h"
#include "fitsviewer/pipeline/cropoperation.h"
#include "fitsviewer/pipeline/autostretch.h"
#include "fitsviewer/pipeline/curveoperation.h"
#include "fitsviewer/pipeline/saturationoperation.h"
#include "fitsviewer/pipeline/contrastoperation.h"
#include "fitsviewer/pipeline/channelblendoperation.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QSignalSpy>
#include <fitsio.h>
#include <wcs.h>

Q_DECLARE_METATYPE(FITSMode);

// Writes a minimal single-channel 16-bit FITS file of constant `value`, with an optional
// rectangular region overridden to `outlierValue` (defaults to a single pixel) — used to
// build synthetic calibration-sub folders for testMasterBuilder() without depending on
// real camera fixture files.
static bool writeSyntheticFrame(const QString &path, int width, int height, uint16_t value,
                                int outlierRow = -1, int outlierCol = -1, uint16_t outlierValue = 0,
                                int outlierHeight = 1, int outlierWidth = 1)
{
    fitsfile *fptr = nullptr;
    int status = 0;
    long naxes[2] = { width, height };

    // fits_create_diskfile() passes the filename straight to fopen() — the "!"
    // clobber-prefix convention only applies to fits_create_file()'s extended filename
    // syntax, so remove any pre-existing file explicitly instead.
    QFile::remove(path);
    QByteArray pathBytes = path.toLocal8Bit();
    if (fits_create_diskfile(&fptr, pathBytes.constData(), &status))
        return false;
    if (fits_create_img(fptr, USHORT_IMG, 2, naxes, &status))
    {
        fits_close_file(fptr, &status);
        return false;
    }

    std::vector<uint16_t> pixels(static_cast<size_t>(width) * height, value);
    if (outlierRow >= 0 && outlierCol >= 0)
    {
        for (int r = outlierRow; r < outlierRow + outlierHeight && r < height; r++)
            for (int c = outlierCol; c < outlierCol + outlierWidth && c < width; c++)
                pixels[static_cast<size_t>(r) * width + c] = outlierValue;
    }

    // cfitsio's USHORT_IMG datatype expects signed-short-biased data via TUSHORT, which
    // handles the bias internally when writing unsigned pixel values directly.
    fits_write_img(fptr, TUSHORT, 1, pixels.size(), pixels.data(), &status);
    fits_close_file(fptr, &status);
    return status == 0;
}

TestFitsData::TestFitsData(QObject *parent) : QObject(parent)
{
}

void TestFitsData::initTestCase()
{
    Options::setStellarSolverPartition(true);
}

void TestFitsData::cleanupTestCase()
{
}

void TestFitsData::init()
{

}

void TestFitsData::cleanup()
{

}

void TestFitsData::testComputeHFR_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<FITSMode>("MODE");
    QTest::addColumn<int>("NSTARS");
    QTest::addColumn<double>("HFR");

    // Normal HFR differs from focusHFR, in that the 'quickHFR' setting
    // defaults to true, so only the 25% in the center of the image
    // will have stars detected and HFRs computed. 'quickHFR' does not apply
    // to FITS_FOCUS images, only to FITS_NORMAL images.

    // Normal HFR tests
    QTest::newRow("NGC4535-1-FOCUS") << "ngc4535-autofocus1.fits" << FITS_FOCUS << 11 << 3.92;
    QTest::newRow("NGC4535-2-FOCUS") << "ngc4535-autofocus2.fits" << FITS_FOCUS << 17 << 2.13;
    QTest::newRow("NGC4535-3-FOCUS") << "ngc4535-autofocus3.fits" << FITS_FOCUS << 126 << 1.254911;

    // Focus HFR tests
    QTest::newRow("NGC4535-1-NORMAL") << "ngc4535-autofocus1.fits" << FITS_NORMAL << 3 << 3.17;
    QTest::newRow("NGC4535-2-NORMAL") << "ngc4535-autofocus2.fits" << FITS_NORMAL << 4 << 1.99;
    QTest::newRow("NGC4535-3-NORMAL") << "ngc4535-autofocus3.fits" << FITS_NORMAL << 30 << 1.22;
#endif
}

void TestFitsData::testComputeHFR()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);
    QFETCH(FITSMode, MODE);
    QFETCH(int, NSTARS);
    QFETCH(double, HFR);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> d(new FITSData(MODE));
    QVERIFY(d != nullptr);

    QFuture<bool> worker = d->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 60000);
    QVERIFY(worker.result());

    worker = d->findStars(ALGORITHM_SEP);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    const int detectedStars = d->getDetectedStars();
    const int starCenters = d->getStarCenters().count();
    QVERIFY2(std::abs(detectedStars - NSTARS) <= 2,
             qPrintable(QString("Detected stars %1 outside expected %2±2").arg(detectedStars).arg(NSTARS)));
    QVERIFY2(std::abs(starCenters - NSTARS) <= 2,
             qPrintable(QString("Star centers %1 outside expected %2±2").arg(starCenters).arg(NSTARS)));
    QVERIFY2(std::abs(d->getHFR() - HFR) <= 0.1,
             qPrintable(QString("HFR expected(measured): %1(%2)").arg(HFR).arg(d->getHFR())));
#endif
}

void TestFitsData::testBahtinovFocusHFR_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<FITSMode>("MODE");
    QTest::addColumn<int>("NSTARS");
    QTest::addColumn<double>("HFR");

    QTest::newRow("BAHTINOV-1-NORMAL") << "bahtinov-focus.fits" << FITS_NORMAL << 1 << 1.544;
#endif
}

void TestFitsData::testBahtinovFocusHFR()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);
    QFETCH(FITSMode, MODE);
    QFETCH(int, NSTARS);
    QFETCH(double, HFR);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> d(new FITSData(MODE));
    QVERIFY(d != nullptr);

    QFuture<bool> worker = d->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    // The bahtinov algorithm depends on which star is selected and number of average rows - not sure how to fiddle with that yet
    const QRect trackingBox(204, 240, 128, 128);

    d->findStars(ALGORITHM_BAHTINOV, trackingBox).waitForFinished();
    QCOMPARE(d->getDetectedStars(), NSTARS);
    QCOMPARE(d->getStarCenters().count(), 1);
    QVERIFY(std::abs(d->getHFR() - HFR) < 0.01);
#endif
}

void TestFitsData::initGenericDataFixture()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<FITSMode>("MODE");

    // Star count
    QTest::addColumn<int>("NSTARS_CENTROID");
    QTest::addColumn<int>("NSTARS_STELLARSOLVER");

    // HFRs using variouls methods
    QTest::addColumn<double>("HFR_CENTROID");
    QTest::addColumn<double>("HFR_GRADIENT");
    QTest::addColumn<double>("HFR_THRESHOLD");
    QTest::addColumn<double>("HFR_STELLARSOLVER");

    // Statistics
    QTest::addColumn<double>("ADU");
    QTest::addColumn<double>("MEAN");
    QTest::addColumn<double>("STDDEV");
    QTest::addColumn<double>("SNR");
    QTest::addColumn<long>("MAXIMUM");
    QTest::addColumn<long>("MINIMUM");
    QTest::addColumn<double>("MEDIAN");

    // This tracking box should detect a single centered star using SEP
    QTest::addColumn<QRect>("TRACKING_BOX");

    QTest::newRow("M47-1-NORMAL")
            << "m47_sim_stars.fits"
            << FITS_NORMAL
            << 80       // Stars found with the Centroid detection
            << 104      // Stars found with the StellarSolver detection - default profile limits count
            << 1.49     // HFR found with the Centroid detection
            << 1.482291 // HFR found with the Gradient detection
            << 0.0      // HFR found with the Threshold detection - not used
            << 1.482291 // HFR found with the StellarSolver detection
            << 41.08    // ADU
            << 41.08    // Mean
            << 360.2932 // StdDev
            << 0.114    // SNR
            << 57832L   // Max
            << 21L      // Min
            << 31.0     // Median
            << QRect(591 - 16 / 2, 482 - 16 / 2, 16, 16);
#endif
}

void TestFitsData::testLoadFits_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    initGenericDataFixture();
#endif
}

void TestFitsData::testLoadFits()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);
    QFETCH(FITSMode, MODE);
    QFETCH(int, NSTARS_CENTROID);
    QFETCH(int, NSTARS_STELLARSOLVER);
    QFETCH(double, HFR_CENTROID);
    QFETCH(double, HFR_GRADIENT);
    QFETCH(double, HFR_THRESHOLD);
    Q_UNUSED(HFR_THRESHOLD);
    QFETCH(double, HFR_STELLARSOLVER);
    QFETCH(double, ADU);
    QFETCH(double, MEAN);
    QFETCH(double, STDDEV);
    QFETCH(double, SNR);
    QFETCH(long, MAXIMUM);
    QFETCH(long, MINIMUM);
    QFETCH(double, MEDIAN);
    QFETCH(QRect, TRACKING_BOX);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> fd(new FITSData(MODE));
    QVERIFY(fd != nullptr);

    QFuture<bool> worker = fd->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    // Statistics computation
    QVERIFY(std::abs(fd->getADU() - ADU) < 0.01);
    QVERIFY(std::abs(fd->getMean() - MEAN) < 0.01);
    fprintf(stderr, "%f vs %f (%f\n", fd->getStdDev(), STDDEV, std::abs(fd->getStdDev() - STDDEV));
    QVERIFY(std::abs(fd->getStdDev() - STDDEV) < 0.01);
    QVERIFY(std::abs(fd->getSNR() - SNR) < 0.001);

    // Minmax
    QCOMPARE((long)fd->getMax(), MAXIMUM);
    QCOMPARE((long)fd->getMin(), MINIMUM);

    QVERIFY(std::abs(fd->getMedian() - MEDIAN) < 0.01);

    // Without searching for stars, there are no stars found
    QCOMPARE(fd->getStarCenters().count(), 0);
    QCOMPARE(fd->getHFR(), -1.0);

    // Default algorithm is centroid, 80 stars with 1.495 as HFR
    fd->findStars().waitForFinished();
    QCOMPARE(fd->getDetectedStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_CENTROID);
    QVERIFY(std::abs(fd->getHFR() - HFR_CENTROID) < 0.01);

    // With the centroid algorithm, 80 stars with MEAN HFR 1.495
    fd->findStars(ALGORITHM_CENTROID).waitForFinished();
    QCOMPARE(fd->getDetectedStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_CENTROID);
    QVERIFY(std::abs(fd->getHFR() - HFR_CENTROID) < 0.01);

    // With the gradient algorithm, one single star found with HFR 1.801
    fd->findStars(ALGORITHM_GRADIENT).waitForFinished();
    QCOMPARE(fd->getDetectedStars(), 1);
    QCOMPARE(fd->getStarCenters().count(), 1);
    QVERIFY(std::abs(fd->getHFR() - HFR_GRADIENT) < 0.01);

    // The threshold algorithm depends on a global option - skip until we know how to fiddle with that
    //QCOMPARE(fd->findStars(ALGORITHM_THRESHOLD), -1);
    //QCOMPARE(fd->getDetectedStars(), 0);
    //QCOMPARE(fd->getStarCenters().count(), 0);
    //QCOMPARE(fd->getHFR(), -1.0);

    // With the SEP algorithm, 100 stars with MEAN HFR 2.08
    fd->findStars(ALGORITHM_SEP).waitForFinished();
    const int sepDetectedStars = fd->getDetectedStars();
    const int sepStarCenters = fd->getStarCenters().count();
    QVERIFY2(std::abs(sepDetectedStars - NSTARS_STELLARSOLVER) <= 2,
             qPrintable(QString("SEP detected stars %1 outside expected %2±2")
                        .arg(sepDetectedStars).arg(NSTARS_STELLARSOLVER)));
    QVERIFY2(std::abs(sepStarCenters - NSTARS_STELLARSOLVER) <= 2,
             qPrintable(QString("SEP star centers %1 outside expected %2±2")
                        .arg(sepStarCenters).arg(NSTARS_STELLARSOLVER)));
    QVERIFY(std::abs(fd->getHFR() - HFR_STELLARSOLVER) < 0.1);

    // Test the SEP algorithm with a tracking box, as used by the internal guider and subframe focus.
    fd->findStars(ALGORITHM_SEP, TRACKING_BOX).waitForFinished();
    auto centers = fd->getStarCenters();
    QCOMPARE(centers.count(), 1);
    // QWARN(QString("Center    %1,%2").arg(centers[0]->x).arg(centers[0]->y).toStdString().c_str());
    // QWARN(QString("TB Center %1,%2").arg(TRACKING_BOX.center().x()).arg(TRACKING_BOX.center().y()).toStdString().c_str());
    QVERIFY(std::abs(centers[0]->x - TRACKING_BOX.center().x()) <= 5);
    QVERIFY(std::abs(centers[0]->y - TRACKING_BOX.center().y()) <= 5);
#endif
}

void TestFitsData::testLoadCompressedFits_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::newRow("M47-1-COMPRESSED") << "m47_sim_stars.fits";
#endif
}

void TestFitsData::testLoadCompressedFits()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);

    if (!QFile::exists(NAME))
        QSKIP("Skipping compressed load test because of missing fixture");

    std::unique_ptr<FITSData> uncompressed(new FITSData(FITS_NORMAL));
    QVERIFY(uncompressed != nullptr);

    QFuture<bool> worker = uncompressed->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY2(worker.result(), qPrintable(uncompressed->getLastError()));

    QTemporaryDir compressedDir;
    QVERIFY(compressedDir.isValid());

    const QString sourceFits = QFileInfo(NAME).absoluteFilePath();
    const QString compressedFits = compressedDir.filePath(QString("%1.fits.fz").arg(QFileInfo(NAME).baseName()));

    fpstate fpvar;
    QCOMPARE(fp_init(&fpvar), 0);

    int isLossless = 0;
    QByteArray sourcePath = QFile::encodeName(sourceFits);
    QByteArray compressedPath = QFile::encodeName(compressedFits);
    QVERIFY2(fp_pack(sourcePath.data(), compressedPath.data(), fpvar, &isLossless) == 0,
             qPrintable(QString("Failed to create compressed FITS fixture from %1").arg(sourceFits)));
    QVERIFY(QFile::exists(compressedFits));

    std::unique_ptr<FITSData> compressed(new FITSData(FITS_NORMAL));
    QVERIFY(compressed != nullptr);

    worker = compressed->loadFromFile(compressedFits);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY2(worker.result(), qPrintable(compressed->getLastError()));

    QVERIFY(compressed->isCompressed());
    QCOMPARE(compressed->width(), uncompressed->width());
    QCOMPARE(compressed->height(), uncompressed->height());
    QCOMPARE(compressed->channels(), uncompressed->channels());
    QVERIFY(std::abs(compressed->getMean() - uncompressed->getMean()) < 0.01);
    QVERIFY(std::abs(compressed->getStdDev() - uncompressed->getStdDev()) < 0.01);
#endif
}

void TestFitsData::testCentroidAlgorithmBenchmark_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    initGenericDataFixture();
#endif
}

void TestFitsData::testCentroidAlgorithmBenchmark()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> d(new FITSData());
    QVERIFY(d != nullptr);

    QFuture<bool> worker = d->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_CENTROID).waitForFinished(); }
#endif
}

void TestFitsData::testGradientAlgorithmBenchmark_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    initGenericDataFixture();
#endif
}

void TestFitsData::testGradientAlgorithmBenchmark()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> d(new FITSData());
    QVERIFY(d != nullptr);

    QFuture<bool> worker = d->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_GRADIENT).waitForFinished(); }
#endif
}

void TestFitsData::testThresholdAlgorithmBenchmark_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    initGenericDataFixture();
#endif
}

void TestFitsData::testThresholdAlgorithmBenchmark()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QSKIP("Skipping benchmark of Threshold Algorithm, which requires fiddling with a global option");

    QFETCH(QString, NAME);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> d(new FITSData());
    QVERIFY(d != nullptr);

    QFuture<bool> worker = d->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_THRESHOLD).waitForFinished(); }
#endif
}

void TestFitsData::testSEPAlgorithmBenchmark_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    initGenericDataFixture();
#endif
}

void TestFitsData::testSEPAlgorithmBenchmark()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);

    if(!QFile::exists(NAME))
        QSKIP("Skipping load test because of missing fixture");

    std::unique_ptr<FITSData> d(new FITSData());
    QVERIFY(d != nullptr);

    QFuture<bool> worker = d->loadFromFile(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_SEP).waitForFinished(); }
#endif
}

QString SolverLoop::status() const
{
    return QString("%1/%2 %3% %4 %5")
           .arg(upto()).arg(repetitions).arg(upto() * 100.0 / repetitions, 2, 'f', 0)
           .arg(solver.get() && solver->isRunning() ? " running" : "")
           .arg(done() ? " Done" : "");
}

SolverLoop::SolverLoop(const QVector<QString> &files, const QString &dir, bool isDetecting, int numReps)
{
    filenames = files;
    repetitions = numReps;
    directory = dir;
    detecting = isDetecting;

    // Preload the images.
    for (const QString &fname : files)
    {
        images.push_back(QSharedPointer<FITSData>(new FITSData));
        images.back().get()->loadFromFile(QString("%1/%2").arg(dir, fname)).waitForFinished();
    }
}

void SolverLoop::start()
{
    startDetect(numDetects % filenames.size());
}

bool SolverLoop::done() const
{
    return numDetects >= repetitions;
}

int SolverLoop::upto() const
{
    return numDetects;
}

void SolverLoop::timeout()
{
    qInfo() << QString("timed out on %1 %2 !!!!!!!!!!!!!!!!!!!!!").arg(currentIndex).arg(filenames[currentIndex]);
    if (detecting)
        watcher.cancel();
    else
        solver->abort();

    startDetect(currentIndex);
}

void SolverLoop::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution,
                            double elapsedSeconds)
{
    disconnect(&timer, &QTimer::timeout, this, &SolverLoop::timeout);
    timer.stop();
    disconnect(solver.get(), &SolverUtils::done, this, &SolverLoop::solverDone);

    const auto &filename = filenames[currentIndex];
    if (timedOut)
        qInfo() << QString("#%1: %2 Solver timed out: %3s").arg(numDetects).arg(filename).arg(elapsedSeconds, 0, 'f', 1);
    else if (!success)
        qInfo() << QString("#%1: %2 Solver failed: %3s").arg(numDetects).arg(filename).arg(elapsedSeconds, 0, 'f', 1);
    else
    {
        const double ra = solution.ra;
        const double dec = solution.dec;
        const double scale = solution.pixscale;
        qInfo() << QString("#%1: %2 Solver returned RA %3 DEC %4 Scale %5: %6s").arg(numDetects).arg(filename)
                .arg(ra, 6, 'f', 3).arg(dec, 6, 'f', 3).arg(scale).arg(elapsedSeconds, 4, 'f', 1);
    }

    if (++numDetects < repetitions)
        startDetect(numDetects % filenames.size());
}

void SolverLoop::randomTimeout()
{
    disconnect(&watcher, &QFutureWatcher<bool>::finished, this, &SolverLoop::detectFinished);
    disconnect(&watcher, &QFutureWatcher<bool>::canceled, this, &SolverLoop::detectFinished);
    disconnect(&randomAbortTimer, &QTimer::timeout, this, &SolverLoop::randomTimeout);
    disconnect(&timer, &QTimer::timeout, this, &SolverLoop::timeout);
    timer.stop();
    randomAbortTimer.stop();
    qInfo() << QString("#%1: %2 random timeout was %3s (%4s)").arg(numDetects).arg(filenames[currentIndex])
            .arg(thisRandomTimeout, 3, 'f', 3).arg(dTimer.elapsed() / 1000.0, 3, 'f', 3);
    thisImage.reset();
    if (++numDetects < repetitions)
    {
        startDetect(numDetects % filenames.size());
    }
}

void SolverLoop::detectFinished()
{
    disconnect(&timer, &QTimer::timeout, this, &SolverLoop::timeout);
    timer.stop();
    randomAbortTimer.stop();
    disconnect(&watcher, &QFutureWatcher<bool>::finished, this, &SolverLoop::detectFinished);
    disconnect(&watcher, &QFutureWatcher<bool>::canceled, this, &SolverLoop::detectFinished);
    bool result = watcher.result();
    if (result)
    {
        qInfo() << QString("#%1: %2 HFR %3 (%4s)").arg(numDetects).arg(filenames[currentIndex])
                .arg(thisImage->getHFR(), 4, 'f', 2).arg(dTimer.elapsed() / 1000.0, 3, 'f', 3);
        if (++numDetects < repetitions)
        {
            startDetect(numDetects % filenames.size());
        }
    }
    else
    {
        QFAIL("Detect failed");
    }
}

void SolverLoop::startDetect(int index)
{
    connect(&timer, &QTimer::timeout, this, &SolverLoop::timeout, Qt::UniqueConnection);
    timer.setSingleShot(true);
    timer.start(timeoutSecs * 1000);

    currentIndex = index;
    if (detecting)
    {
        thisImage.reset(new FITSData(images[currentIndex]));
        // detecting stars
        connect(&watcher, &QFutureWatcher<bool>::finished, this, &SolverLoop::detectFinished);
        connect(&watcher, &QFutureWatcher<bool>::canceled, this, &SolverLoop::detectFinished);

        if (randomAbortSecs > 0)
        {
            randomAbortTimer.setSingleShot(true);
            connect(&randomAbortTimer, &QTimer::timeout, this, &SolverLoop::randomTimeout, Qt::UniqueConnection);
            thisRandomTimeout = rand.generateDouble() * randomAbortSecs;
            randomAbortTimer.start(thisRandomTimeout * 1000);

        }
        dTimer.start();
        future = thisImage->findStars(ALGORITHM_SEP);
        watcher.setFuture(future);
    }
    else
    {
        // plate solving
        auto profiles = Ekos::getDefaultAlignOptionsProfiles();
        auto parameters = profiles.at(Options::solveOptionsProfile());

        // Double search radius
        Options::setSolverType(0); // Internal solver
        parameters.search_radius = parameters.search_radius * 2;
        solver.reset(new SolverUtils(parameters, 20), &QObject::deleteLater);
        connect(solver.get(), &SolverUtils::done, this, &SolverLoop::solverDone, Qt::UniqueConnection);
        solver->useScale(false, 0, 0);
        solver->usePosition(false, 0, 0);
        solver->runSolver(images[currentIndex]);
    }
}

static bool ensureIndexFiles()
{
    if (Options::astrometryIndexFolderList().isEmpty())
    {
        const QStringList candidates =
        {
            QDir::homePath() + "/Library/Application Support/kstars/astrometry",
            QDir::homePath() + "/.local/share/kstars/astrometry",
        };
        for (const auto &d : candidates)
            if (QDir(d).exists())
            {
                Options::setAstrometryIndexFolderList(QStringList() << d);
                break;
            }
    }
    return !Options::astrometryIndexFolderList().isEmpty();
}

// Parallel solver stress test. Runs concurrent star detection and plate
// solving loops. A failure would typically be a segv or deadlock.
//
// Set KSTARS_SOLVER_STRESS_ITERATIONS to control intensity (default: 50).
// For heavy stress testing:
//   KSTARS_SOLVER_STRESS_ITERATIONS=10000 QTEST_FUNCTION_TIMEOUT=1000000000 \
//     testfitsdata testParallelSolvers -maxwarnings 1000000
void TestFitsData::runParallelSolvers(int multiAlgoOverride)
{
    Options::setAutoDebayer(false);

    const QString probe = QFINDTESTDATA("ngc4535-autofocus1.fits");
    if (probe.isEmpty())
        QSKIP("Skipping parallel solver test -- missing fixture files");
    const QString dir = QFileInfo(probe).absolutePath();

    const QVector<QString> detectFiles1 =
    {
        "ngc4535-autofocus1.fits",
        "ngc4535-autofocus2.fits",
        "ngc4535-autofocus3.fits",
        "m47_sim_stars.fits",
    };
    const QVector<QString> detectFiles2 =
    {
        "m47_sim_stars.fits",
        "ngc4535-autofocus3.fits",
        "ngc4535-autofocus1.fits",
    };
    const QVector<QString> solveFiles =
    {
        "ngc4535-autofocus1.fits",
        "ngc4535-autofocus2.fits",
        "ngc4535-autofocus3.fits",
    };

    if (!ensureIndexFiles())
        QSKIP("No astrometry index files found -- skipping parallel solver test");

    if (multiAlgoOverride >= 0)
        SolverUtils::setMultiAlgorithmOverride(multiAlgoOverride);

    bool ok = false;
    int numIterations = qEnvironmentVariableIntValue("KSTARS_SOLVER_STRESS_ITERATIONS", &ok);
    if (!ok || numIterations <= 0)
        numIterations = 50;

    SolverLoop loop1(detectFiles1, dir, true, numIterations);
    loop1.setRandomAbort(1);

    SolverLoop loop2(detectFiles2, dir, true, numIterations);
    loop2.setRandomAbort(1);

    SolverLoop loop3(detectFiles1, dir, true, numIterations / 15);
    loop3.setRandomAbort(3);

    SolverLoop loop4(solveFiles, dir, false, numIterations / 50);

    SolverLoop loop5(solveFiles, dir, false, numIterations / 50);

    loop1.start();
    loop2.start();
    loop3.start();
    loop4.start();
    loop5.start();

    int iteration = 0;
    while(!loop1.done()
            || !loop2.done()
            || !loop3.done()
            || !loop4.done()
            || !loop5.done())
    {
        QTest::qWait(10);
        if (iteration++ % 400 == 0)
            qInfo() << QString("%1 -- %2 -- %3 -- %4 -- %5")
                    .arg(loop1.status())
                    .arg(loop2.status())
                    .arg(loop3.status())
                    .arg(loop4.status())
                    .arg(loop5.status());
    }

    qInfo() << QString("%1 -- %2 -- %3 -- %4 -- %5")
            .arg(loop1.status())
            .arg(loop2.status())
            .arg(loop3.status())
            .arg(loop4.status())
            .arg(loop5.status());

    qInfo() << QString("Done!");

    if (multiAlgoOverride >= 0)
        SolverUtils::clearMultiAlgorithmOverride();
}

void TestFitsData::testParallelSolvers()
{
    runParallelSolvers(-1);
}

void TestFitsData::testParallelSolversMultiScales()
{
    runParallelSolvers(MULTI_SCALES);
}

void TestFitsData::testParallelSolversMultiDepths()
{
    runParallelSolvers(MULTI_DEPTHS);
}

// Exercises StackController end-to-end (start -> stackReady, redoPostProcess ->
// stackReady again, cancel) without instantiating any FITSViewer/FITSTab/FITSView widget,
// confirming the LiveStacker engine (FITSData/FITSStack) is reusable on its own as a
// headless orchestration layer for the batch post-processing pipeline.
void TestFitsData::testStackController()
{
    if (!QFile::exists("ngc4535-autofocus1.fits"))
        QSKIP("Skipping test because of missing fixture");

    // Both subs come from the same fixture file (just copied twice) rather than two
    // different ngc4535-autofocusN.fits files, since those were captured at different
    // binning/ROI and have mismatched dimensions — irrelevant to what's being tested here
    // (that the engine runs end-to-end with no FITSViewer/FITSTab/FITSView involved).
    QTemporaryDir stackDir;
    QVERIFY(stackDir.isValid());
    QVERIFY(QFile::copy("ngc4535-autofocus1.fits", stackDir.filePath("sub1.fits")));
    QVERIFY(QFile::copy("ngc4535-autofocus1.fits", stackDir.filePath("sub2.fits")));

    StackController controller;
    QSignalSpy readySpy(&controller, &StackController::stackReady);
    QVERIFY(readySpy.isValid());

    StackData params {};
    params.calcSNR = false;
    params.alignMethod = StackAlignMethod::NONE;
    params.numInMem = 10;
    params.downscale = StackDownscale::NONE;
    params.weighting = StackFrameWeighting::EQUAL;
    params.normalization = StackNormalization::NONE;
    params.stackingMethod = StackingMethod::MEAN;
    params.lowSigma = 2.0;
    params.highSigma = 3.0;
    params.iterations = 1;
    params.kappa = 1.0;
    params.alpha = 1.0;
    params.sigma = 1.0;
    params.postProcessing.postProcess = false;
    params.postProcessing.sharpenKernal = 3;
    params.postProcessing.sharpenSigma = 1.0;
    params.postProcessing.PSFSigma = 1.0;

    controller.start(QStringList { stackDir.path() }, params);

    QVERIFY(readySpy.wait(60000));
    QVERIFY(controller.imageData() != nullptr);
    QVERIFY(!controller.imageData()->isStackedImageEmpty());

    // Re-run post processing (no GUI widget reads these parameters back — they come
    // straight from the caller, unlike FITSTab::getPPSettings()).
    StackPPData pp = params.postProcessing;
    pp.postProcess = true;
    pp.sharpenAmt = 0.1;
    readySpy.clear();
    controller.redoPostProcess(pp);
    QVERIFY(readySpy.wait(60000));
    QVERIFY(!controller.imageData()->isStackedImageEmpty());

    // §2 crop, wired through StackController -> FITSData::cropStack(). alignMethod is
    // NONE here (see params above) so there's no WCS to adjust — this exercises the
    // integration wrapper's no-WCS path; CropOperation::apply()'s WCS math itself is
    // covered directly (with a real synthetic wcsprm) by testCropOperation().
    const int croppedWidth = controller.imageData()->getStackStatistics().width / 2;
    const int croppedHeight = controller.imageData()->getStackStatistics().height / 2;
    QString cropError;
    QVERIFY2(controller.crop(QRect(0, 0, croppedWidth, croppedHeight), cropError), qPrintable(cropError));
    QCOMPARE(static_cast<int>(controller.imageData()->getStackStatistics().width), croppedWidth);
    QCOMPARE(static_cast<int>(controller.imageData()->getStackStatistics().height), croppedHeight);
    QVERIFY(!controller.imageData()->isStackedImageEmpty());

    // Should be a safe no-op after the stack has already completed.
    controller.cancel();
}

// Exercises MasterBuilder against synthetic calibration
// subs: a dark-combine that must reject a single extreme outlier pixel via sigma-clipping
// (needs a realistic N — single-pass mean/stddev sigma-clipping is subject to a
// "masking" effect against small N, so this uses N=12, well above the ~10+ subs
// astrophotography stacking tools generally recommend for rejection to be meaningful),
// a flat-combine that must correct for per-frame illumination drift via median
// normalization before combining, and buildAndSave()'s FITS output round-trip.
void TestFitsData::testMasterBuilder()
{
    QTemporaryDir darkDir;
    QVERIFY(darkDir.isValid());
    const int width = 16, height = 16;
    const uint16_t baseline = 1000;
    for (int i = 0; i < 12; i++)
    {
        const QString path = darkDir.filePath(QString("dark%1.fits").arg(i));
        // Frame 0 carries one wildly out-of-family pixel at (5,5); every other frame (and
        // every other pixel) is exactly at baseline.
        if (i == 0)
            QVERIFY(writeSyntheticFrame(path, width, height, baseline, 5, 5, 50000));
        else
            QVERIFY(writeSyntheticFrame(path, width, height, baseline));
    }

    cv::Mat darkMaster;
    QString error;
    QVERIFY2(MasterBuilder::build(darkDir.path(), MasterBuilder::Type::DARK, darkMaster, error),
             qPrintable(error));
    QCOMPARE(darkMaster.cols, width);
    QCOMPARE(darkMaster.rows, height);
    QCOMPARE(darkMaster.channels(), 1);

    // A normal pixel (no outlier anywhere) should combine to exactly baseline.
    QCOMPARE(darkMaster.at<float>(0, 0), static_cast<float>(baseline));
    // The outlier pixel should be pulled back close to baseline by sigma-clip rejection,
    // not dragged up toward the 50000 outlier value.
    const float outlierPixel = darkMaster.at<float>(5, 5);
    QVERIFY2(std::abs(outlierPixel - baseline) < 50.0f,
             qPrintable(QString("Outlier pixel combined to %1, expected close to baseline %2 — sigma-clip"
                                 " rejection did not work").arg(outlierPixel).arg(baseline)));

    // Flat: three frames at different uniform illumination levels. Per-frame median
    // normalization should equalize them to ~1.0 before combining, so the combined flat
    // is uniformly ~1.0 despite the raw frames having no pixel value in common.
    QTemporaryDir flatDir;
    QVERIFY(flatDir.isValid());
    QVERIFY(writeSyntheticFrame(flatDir.filePath("flat0.fits"), width, height, 20000));
    QVERIFY(writeSyntheticFrame(flatDir.filePath("flat1.fits"), width, height, 25000));
    QVERIFY(writeSyntheticFrame(flatDir.filePath("flat2.fits"), width, height, 30000));

    cv::Mat flatMaster;
    QVERIFY2(MasterBuilder::build(flatDir.path(), MasterBuilder::Type::FLAT, flatMaster, error),
             qPrintable(error));
    QVERIFY2(std::abs(flatMaster.at<float>(8, 8) - 1.0f) < 0.01f,
             qPrintable(QString("Flat master pixel is %1, expected ~1.0 after median normalization")
                         .arg(flatMaster.at<float>(8, 8))));

    // Bias subtraction: build() must subtract the given master before normalizing, not
    // just normalize the raw (still bias-included) frame — a plain "does it normalize to
    // ~1.0" check wouldn't catch this, since flat normalization is self-scaling regardless
    // of the absolute level. Uses a two-level flat (background 1000, a bright region 2000)
    // with a uniform bias of 500 baked into the raw subs (1500/2500): bias-subtracted, the
    // bright/background ratio should come out as 2000/1000=2.0; uncorrected, it would be
    // 2500/1500=1.667 instead — the two are far enough apart to distinguish reliably.
    QTemporaryDir biasDir;
    QVERIFY(biasDir.isValid());
    for (int i = 0; i < 5; i++)
        QVERIFY(writeSyntheticFrame(biasDir.filePath(QString("bias%1.fits").arg(i)), width, height, 500));
    const QString masterBiasPath = biasDir.filePath("master_bias.fits");
    QVERIFY2(MasterBuilder::buildAndSave(biasDir.path(), MasterBuilder::Type::BIAS, masterBiasPath, error),
             qPrintable(error));

    QTemporaryDir biasedFlatDir;
    QVERIFY(biasedFlatDir.isValid());
    for (int i = 0; i < 3; i++)
    {
        QVERIFY(writeSyntheticFrame(biasedFlatDir.filePath(QString("flat%1.fits").arg(i)), width, height,
                                    1500, 0, 0, 2500, height, width / 2));
    }

    cv::Mat biasedFlatMaster;
    QVERIFY2(MasterBuilder::build(biasedFlatDir.path(), MasterBuilder::Type::FLAT, biasedFlatMaster, error,
                                  3.0, 3.0, masterBiasPath),
             qPrintable(error));
    const float bgAfter = biasedFlatMaster.at<float>(0, width - 1);
    const float brightAfter = biasedFlatMaster.at<float>(0, 0);
    QVERIFY2(std::abs(brightAfter / bgAfter - 2.0f) < 0.05f,
             qPrintable(QString("Bright/background ratio %1 after bias subtraction, expected ~2.0 (500 not "
                                 "properly removed before normalization)").arg(brightAfter / bgAfter)));

    // buildAndSave() round-trip: the saved file must be a valid, loadable FITS file with
    // the right dimensions — this is what gets fed straight into StackData::masterDark/
    // masterFlat, unchanged, per §1's chosen calling convention.
    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());
    const QString savedPath = outDir.filePath("master_dark.fits");
    QVERIFY2(MasterBuilder::buildAndSave(darkDir.path(), MasterBuilder::Type::DARK, savedPath, error),
             qPrintable(error));
    QVERIFY(QFile::exists(savedPath));

    FITSData reload(FITS_CALIBRATE);
    QFuture<bool> future = reload.loadFromFile(savedPath);
    future.waitForFinished();
    QVERIFY(future.result());
    QCOMPARE(static_cast<int>(reload.width()), width);
    QCOMPARE(static_cast<int>(reload.height()), height);
}

// Unit-tests CropOperation::apply()'s crop+WCS math directly, with a synthetic wcsprm
// built via plain wcslib calls — no plate solve, no live-stack session, no FITSData at
// all. This is the load-bearing correctness check: a crop must not change what sky
// coordinate any given pixel maps to.
void TestFitsData::testCropOperation()
{
    // 20x20 gradient image so cropped pixel values are independently verifiable (not
    // just dimensions).
    cv::Mat image(20, 20, CV_32FC1);
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++)
            image.at<float>(y, x) = static_cast<float>(y * 20 + x);

    struct wcsprm wcs;
    wcs.flag = -1;
    QCOMPARE(wcsini(1, 2, &wcs), 0);
    wcs.crval[0] = 83.633083;
    wcs.crval[1] = -5.391111;
    wcs.crpix[0] = 10.0;
    wcs.crpix[1] = 10.0;
    wcs.cdelt[0] = -0.001;
    wcs.cdelt[1] = 0.001;
    strncpy(wcs.ctype[0], "RA---TAN", 72);
    strncpy(wcs.ctype[1], "DEC--TAN", 72);
    QCOMPARE(wcsset(&wcs), 0);

    auto skyAt = [](struct wcsprm * w, double px, double py) -> QPointF
    {
        int stat[2];
        double imgcrd[2], phi, pixcrd[2], theta, world[2];
        pixcrd[0] = px;
        pixcrd[1] = py;
        wcsp2s(w, 1, 2, pixcrd, imgcrd, &phi, &theta, world, stat);
        return QPointF(world[0], world[1]);
    };

    // 1-indexed pixel coordinates, per FITS/WCS convention.
    const QPointF skyBefore = skyAt(&wcs, 15.0, 12.0);

    const QRect roi(5, 5, 10, 10);
    QString error;
    QVERIFY2(CropOperation::apply(image, roi, &wcs, error), qPrintable(error));

    QCOMPARE(image.cols, 10);
    QCOMPARE(image.rows, 10);
    // The 0-indexed pixel that was at (x=9,y=6) in the original 20x20 image is now at
    // (4,1) in the cropped 10x10 image — confirms the cv::Mat crop moved the right data,
    // not just resized.
    QCOMPARE(image.at<float>(1, 4), static_cast<float>(6 * 20 + 9));

    // The pixel that was at 1-indexed (15,12) is now at (15-5, 12-5) = (10,7). Its sky
    // coordinate must be unchanged by the crop.
    const QPointF skyAfter = skyAt(&wcs, 10.0, 7.0);
    QVERIFY2(std::abs(skyBefore.x() - skyAfter.x()) < 1e-9 && std::abs(skyBefore.y() - skyAfter.y()) < 1e-9,
             qPrintable(QString("Sky coord before crop (%1,%2) != after (%3,%4)")
                         .arg(skyBefore.x()).arg(skyBefore.y()).arg(skyAfter.x()).arg(skyAfter.y())));

    wcsfree(&wcs);
}

// Verifies AutoStretch::apply() against a synthetic
// image with a known, deliberately uniform background (so its MADN is exactly zero,
// isolating the targetBackground behavior from the shadowsClipping behavior) plus a
// bright "star" region — checks the background lands near targetBackground, the bright
// region lands near the top of [0,1], monotonicity is preserved, and everything stays
// within [0,1].
void TestFitsData::testAutoStretch()
{
    cv::Mat image(20, 20, CV_32FC1, cv::Scalar(1000.0f));
    // A small bright region, well below the assumed 65536 ADU ceiling so it stays in
    // the curve's normal (non-clipped) range.
    image(cv::Rect(5, 5, 3, 3)).setTo(50000.0f);

    QString error;
    const double targetBackground = 0.25;
    QVERIFY2(AutoStretch::apply(image, error, targetBackground, 2.8), qPrintable(error));

    double minVal, maxVal;
    cv::minMaxLoc(image, &minVal, &maxVal);
    QVERIFY2(minVal >= 0.0 && maxVal <= 1.0,
             qPrintable(QString("Stretched output out of [0,1]: min=%1 max=%2").arg(minVal).arg(maxVal)));

    const float background = image.at<float>(0, 0);
    const float star = image.at<float>(6, 6);

    QVERIFY2(std::abs(background - targetBackground) < 0.05,
             qPrintable(QString("Background stretched to %1, expected close to targetBackground %2")
                         .arg(background).arg(targetBackground)));
    QVERIFY2(star > background,
             qPrintable(QString("Star (%1) should be brighter than background (%2) after stretch")
                         .arg(star).arg(background)));
    QVERIFY2(star > 0.9f, qPrintable(QString("Star pixel only reached %1 after stretch, expected near 1.0").arg(star)));
}

// Verifies CurveOperation: an identity curve must leave
// pixels unchanged, a real curve must pass exactly through its control points and stay
// monotonic between them, and per-channel curves must act independently per channel.
void TestFitsData::testCurveOperation()
{
    // Identity: a 2-point curve (0,0)-(1,1) has matching boundary tangents (both equal
    // the single segment's own slope), so Hermite interpolation reduces to exact linear
    // interpolation — output must equal input.
    {
        cv::Mat image(4, 4, CV_32FC1);
        float v = 0.0f;
        for (int i = 0; i < 16; i++)
            image.at<float>(i / 4, i % 4) = (v = (i / 15.0f));

        cv::Mat original = image.clone();
        QString error;
        QVERIFY2(CurveOperation::apply(image, { QPointF(0, 0), QPointF(1, 1) }, error), qPrintable(error));

        for (int i = 0; i < 16; i++)
            QVERIFY(std::abs(image.at<float>(i / 4, i % 4) - original.at<float>(i / 4, i % 4)) < 1e-5f);
    }

    // A real curve: (0,0) -> (0.5,0.7) -> (1,1). Must pass exactly through (0.5,0.7),
    // and stay monotonically increasing between control points.
    {
        cv::Mat image(1, 3, CV_32FC1);
        image.at<float>(0, 0) = 0.25f;
        image.at<float>(0, 1) = 0.5f;
        image.at<float>(0, 2) = 0.75f;

        QString error;
        const QVector<QPointF> curve { QPointF(0, 0), QPointF(0.5, 0.7), QPointF(1, 1) };
        QVERIFY2(CurveOperation::apply(image, curve, error), qPrintable(error));

        QVERIFY2(std::abs(image.at<float>(0, 1) - 0.7f) < 1e-4f,
                 qPrintable(QString("Curve at its own control point x=0.5 gave %1, expected exactly 0.7")
                             .arg(image.at<float>(0, 1))));
        QVERIFY2(image.at<float>(0, 0) < image.at<float>(0, 1),
                 "Curve should be monotonically increasing between (0,0) and (0.5,0.7)");
        QVERIFY2(image.at<float>(0, 1) < image.at<float>(0, 2),
                 "Curve should be monotonically increasing between (0.5,0.7) and (1,1)");
    }

    // Error path: non-monotonic control points must be rejected.
    {
        cv::Mat image(1, 1, CV_32FC1, cv::Scalar(0.5f));
        QString error;
        QVERIFY(!CurveOperation::apply(image, { QPointF(0.5, 0), QPointF(0.2, 1) }, error));
        QVERIFY(!error.isEmpty());
    }

    // Per-channel: R gets identity, G gets a strong brightening curve, B gets a strong
    // darkening curve — same input value on all 3 channels must come out different.
    {
        cv::Mat image(1, 1, CV_32FC3, cv::Scalar(0.5f, 0.5f, 0.5f));
        QString error;
        const QVector<QVector<QPointF>> perChannel
        {
            { QPointF(0, 0), QPointF(1, 1) },       // R: identity
            { QPointF(0, 0), QPointF(0.5, 0.9), QPointF(1, 1) }, // G: brighten
            { QPointF(0, 0), QPointF(0.5, 0.1), QPointF(1, 1) }, // B: darken
        };
        QVERIFY2(CurveOperation::applyPerChannel(image, perChannel, error), qPrintable(error));

        const cv::Vec3f out = image.at<cv::Vec3f>(0, 0);
        QVERIFY2(std::abs(out[0] - 0.5f) < 1e-4f, "R (identity) should stay at 0.5");
        QVERIFY2(out[1] > 0.8f, qPrintable(QString("G (brighten) only reached %1, expected > 0.8").arg(out[1])));
        QVERIFY2(out[2] < 0.2f, qPrintable(QString("B (darken) only dropped to %1, expected < 0.2").arg(out[2])));

        // Wrong channel count must be rejected.
        QVERIFY(!CurveOperation::applyPerChannel(image, { { QPointF(0, 0), QPointF(1, 1) } }, error));
    }
}

// Verifies SaturationOperation: a mono image is a no-op,
// amt=0 desaturates a colored pixel to gray, amt>1 increases saturation, output always
// stays within valid HSV/RGB bounds, and out-of-range (un-stretched, raw-ADU-scale)
// input is rejected rather than silently producing garbage.
void TestFitsData::testSaturationOperation()
{
    // Mono: no-op.
    {
        cv::Mat mono(2, 2, CV_32FC1, cv::Scalar(0.5f));
        QString error;
        QVERIFY2(SaturationOperation::apply(mono, 2.0, error), qPrintable(error));
        QCOMPARE(mono.at<float>(0, 0), 0.5f);
    }

    // Out-of-range input (raw ADU scale, not stretched) must be rejected.
    {
        cv::Mat raw(2, 2, CV_32FC3, cv::Scalar(1000.0f, 1000.0f, 1000.0f));
        QString error;
        QVERIFY(!SaturationOperation::apply(raw, 1.0, error));
        QVERIFY(!error.isEmpty());
    }

    auto channelSpread = [](const cv::Vec3f & px) -> float
    {
        const float mx = std::max({px[0], px[1], px[2]});
        const float mn = std::min({px[0], px[1], px[2]});
        return mx - mn;
    };

    // A clearly-saturated reddish pixel.
    cv::Mat colorOriginal(1, 1, CV_32FC3, cv::Scalar(0.8f, 0.2f, 0.2f));
    const float originalSpread = channelSpread(colorOriginal.at<cv::Vec3f>(0, 0));
    QVERIFY(originalSpread > 0.1f);

    // amt=0 desaturates to gray (R==G==B).
    {
        cv::Mat image = colorOriginal.clone();
        QString error;
        QVERIFY2(SaturationOperation::apply(image, 0.0, error), qPrintable(error));
        QVERIFY2(channelSpread(image.at<cv::Vec3f>(0, 0)) < 0.01f,
                 qPrintable(QString("Expected near-gray after amt=0, spread=%1").arg(channelSpread(image.at<cv::Vec3f>(0,
                             0)))));
    }

    // amt>1 increases saturation (spread grows), staying within valid [0,1] bounds.
    {
        cv::Mat image = colorOriginal.clone();
        QString error;
        QVERIFY2(SaturationOperation::apply(image, 3.0, error), qPrintable(error));
        QVERIFY2(channelSpread(image.at<cv::Vec3f>(0, 0)) >= originalSpread,
                 "Saturation boost should not decrease channel spread");

        double minVal, maxVal;
        cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
        QVERIFY2(minVal >= -1e-4 && maxVal <= 1.0 + 1e-4,
                 qPrintable(QString("Saturated output out of bounds: min=%1 max=%2").arg(minVal).arg(maxVal)));
    }
}

// Verifies ContrastOperation (§5): amt=1 leaves the image unchanged, amt=0 flattens
// everything to the pivot (the image's own mean), amt>1 increases spread around that
// pivot, and output is clamped to [0,1].
void TestFitsData::testContrastOperation()
{
    cv::Mat original(1, 4, CV_32FC1);
    original.at<float>(0, 0) = 0.1f;
    original.at<float>(0, 1) = 0.3f;
    original.at<float>(0, 2) = 0.7f;
    original.at<float>(0, 3) = 0.9f;
    // Mean = 0.5, used as the pivot.

    {
        cv::Mat image = original.clone();
        QString error;
        QVERIFY2(ContrastOperation::apply(image, 1.0, error), qPrintable(error));
        for (int i = 0; i < 4; i++)
            QVERIFY(std::abs(image.at<float>(0, i) - original.at<float>(0, i)) < 1e-5f);
    }

    {
        cv::Mat image = original.clone();
        QString error;
        QVERIFY2(ContrastOperation::apply(image, 0.0, error), qPrintable(error));
        for (int i = 0; i < 4; i++)
            QVERIFY2(std::abs(image.at<float>(0, i) - 0.5f) < 1e-4f,
                     qPrintable(QString("amt=0 should flatten to the pivot 0.5, got %1").arg(image.at<float>(0, i))));
    }

    {
        cv::Mat image = original.clone();
        QString error;
        QVERIFY2(ContrastOperation::apply(image, 2.0, error), qPrintable(error));
        // Symmetric around the 0.5 pivot: values below the mean go lower, values above
        // go higher, spreading further apart than the original.
        QVERIFY(image.at<float>(0, 0) < original.at<float>(0, 0));
        QVERIFY(image.at<float>(0, 3) > original.at<float>(0, 3));
    }

    // Clamping: pushing far past the pivot must clamp to exactly [0,1], not overflow.
    {
        cv::Mat image = original.clone();
        QString error;
        QVERIFY2(ContrastOperation::apply(image, 10.0, error), qPrintable(error));
        QCOMPARE(image.at<float>(0, 0), 0.0f);
        QCOMPARE(image.at<float>(0, 3), 1.0f);
    }
}

// Verifies ChannelBlendOperation — the weighted narrowband-palette "pixel math" that
// lets e.g. a 2-filter (Ha/OIII) HOO blend be built from independently-stacked mono
// sessions, since the engine's own channel assignment only does 1:1, unweighted, and
// rejects exactly 2 input directories outright.
void TestFitsData::testChannelBlendOperation()
{
    cv::Mat ha(4, 4, CV_32FC1, cv::Scalar(1000.0f));
    cv::Mat oiii(4, 4, CV_32FC1, cv::Scalar(400.0f));

    QString error;

    // Plain passthrough (traditional unweighted 1:1 assignment) must reproduce the
    // input exactly.
    {
        cv::Mat out;
        QVERIFY2(ChannelBlendOperation::blendChannel({ {ha, 1.0} }, out, error), qPrintable(error));
        QCOMPARE(out.at<float>(0, 0), 1000.0f);
    }

    // HOO-style blend: red=Ha, green=0.7*OIII+0.3*Ha, blue=OIII.
    {
        cv::Mat rgb;
        QVERIFY2(ChannelBlendOperation::blendRGB(
        { {ha, 1.0} },
        { {oiii, 0.7}, {ha, 0.3} },
        { {oiii, 1.0} },
        rgb, error), qPrintable(error));

        const cv::Vec3f px = rgb.at<cv::Vec3f>(0, 0);
        QCOMPARE(px[0], 1000.0f);
        QVERIFY2(std::abs(px[1] - (0.7f * 400.0f + 0.3f * 1000.0f)) < 1e-3f,
                 qPrintable(QString("Green blend is %1, expected %2").arg(px[1]).arg(0.7 * 400.0 + 0.3 * 1000.0)));
        QCOMPARE(px[2], 400.0f);
    }

    // Mismatched sizes must be rejected.
    {
        cv::Mat small(2, 2, CV_32FC1, cv::Scalar(1.0f));
        cv::Mat out;
        QVERIFY(!ChannelBlendOperation::blendChannel({ {ha, 1.0}, {small, 1.0} }, out, error));
        QVERIFY(!error.isEmpty());
    }

    // A 3-channel (already-combined) input must be rejected — this operates on
    // independently-stacked mono sessions, not already-merged RGB results.
    {
        cv::Mat rgbInput(4, 4, CV_32FC3, cv::Scalar(1.0f, 1.0f, 1.0f));
        cv::Mat out;
        QVERIFY(!ChannelBlendOperation::blendChannel({ {rgbInput, 1.0} }, out, error));
        QVERIFY(!error.isEmpty());
    }
}

void TestFitsData::testDetectStarTrailing()
{
    const int size = 200;

    // Round stars (filled circles) on a noisy background should measure low elongation.
    {
        cv::Mat img(size, size, CV_32FC1);
        cv::randn(img, cv::Scalar(100.0), cv::Scalar(5.0));
        const std::vector<cv::Point> centers { {30, 30}, {80, 40}, {150, 60}, {60, 120}, {170, 170}, {20, 160} };
        for (const auto &c : centers)
            cv::circle(img, c, 4, cv::Scalar(2000.0), -1);

        double medianElongation = -1.0;
        int numSources = 0;
        QVERIFY(FITSData::detectStarTrailing(img, medianElongation, numSources));
        QVERIFY2(numSources >= 5, qPrintable(QString("Expected >=5 sources, got %1").arg(numSources)));
        QVERIFY2(medianElongation < 0.3,
                 qPrintable(QString("Round stars measured elongation %1, expected < 0.3").arg(medianElongation)));
    }

    // Trailed stars (thin streaks) should measure high elongation.
    {
        cv::Mat img(size, size, CV_32FC1);
        cv::randn(img, cv::Scalar(100.0), cv::Scalar(5.0));
        const std::vector<cv::Point> starts { {10, 30}, {10, 40}, {10, 60}, {10, 120}, {10, 170}, {10, 160} };
        for (const auto &s : starts)
            cv::line(img, s, cv::Point(s.x + 40, s.y), cv::Scalar(2000.0), 3);

        double medianElongation = -1.0;
        int numSources = 0;
        QVERIFY(FITSData::detectStarTrailing(img, medianElongation, numSources));
        QVERIFY2(numSources >= 5, qPrintable(QString("Expected >=5 sources, got %1").arg(numSources)));
        QVERIFY2(medianElongation > 0.6,
                 qPrintable(QString("Trailed stars measured elongation %1, expected > 0.6").arg(medianElongation)));
    }

    // Too few sources to trust a median must report false, not a misleading value.
    {
        cv::Mat img(size, size, CV_32FC1);
        cv::randn(img, cv::Scalar(100.0), cv::Scalar(5.0));
        cv::circle(img, cv::Point(50, 50), 4, cv::Scalar(2000.0), -1);

        double medianElongation = -1.0;
        int numSources = 0;
        QVERIFY(!FITSData::detectStarTrailing(img, medianElongation, numSources));
    }

    // An empty image must be rejected outright.
    {
        cv::Mat empty;
        double medianElongation = -1.0;
        int numSources = 0;
        QVERIFY(!FITSData::detectStarTrailing(empty, medianElongation, numSources));
    }
}

QTEST_GUILESS_MAIN(TestFitsData)
