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

Q_DECLARE_METATYPE(FITSMode);

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

QTEST_GUILESS_MAIN(TestFitsData)
