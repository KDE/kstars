/*  KStars tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#include <memory>
#include "testfitsdata.h"
#include "Options.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include <QtGlobal>

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

    QCOMPARE(d->getDetectedStars(), NSTARS);
    QCOMPARE(d->getStarCenters().count(), NSTARS);
    QVERIFY2(abs(d->getHFR() - HFR) <= 0.1, qPrintable(QString("HFR expected(measured): %1(%2)").arg(HFR).arg(d->getHFR())));
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
    QVERIFY(abs(d->getHFR() - HFR) < 0.01);
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
            << 360.18   // StdDev
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
    QVERIFY(abs(fd->getADU() - ADU) < 0.01);
    QVERIFY(abs(fd->getMean() - MEAN) < 0.01);
    QVERIFY(abs(fd->getStdDev() - STDDEV) < 0.01);
    QVERIFY(abs(fd->getSNR() - SNR) < 0.001);

    // Minmax
    QCOMPARE((long)fd->getMax(), MAXIMUM);
    QCOMPARE((long)fd->getMin(), MINIMUM);

    QVERIFY(abs(fd->getMedian() - MEDIAN) < 0.01);

    // Without searching for stars, there are no stars found
    QCOMPARE(fd->getStarCenters().count(), 0);
    QCOMPARE(fd->getHFR(), -1.0);

    // Default algorithm is centroid, 80 stars with 1.495 as HFR
    fd->findStars().waitForFinished();
    QCOMPARE(fd->getDetectedStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_CENTROID);
    QVERIFY(abs(fd->getHFR() - HFR_CENTROID) < 0.01);

    // With the centroid algorithm, 80 stars with MEAN HFR 1.495
    fd->findStars(ALGORITHM_CENTROID).waitForFinished();
    QCOMPARE(fd->getDetectedStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_CENTROID);
    QVERIFY(abs(fd->getHFR() - HFR_CENTROID) < 0.01);

    // With the gradient algorithm, one single star found with HFR 1.801
    fd->findStars(ALGORITHM_GRADIENT).waitForFinished();
    QCOMPARE(fd->getDetectedStars(), 1);
    QCOMPARE(fd->getStarCenters().count(), 1);
    QVERIFY(abs(fd->getHFR() - HFR_GRADIENT) < 0.01);

    // The threshold algorithm depends on a global option - skip until we know how to fiddle with that
    //QCOMPARE(fd->findStars(ALGORITHM_THRESHOLD), -1);
    //QCOMPARE(fd->getDetectedStars(), 0);
    //QCOMPARE(fd->getStarCenters().count(), 0);
    //QCOMPARE(fd->getHFR(), -1.0);

    // With the SEP algorithm, 100 stars with MEAN HFR 2.08
    fd->findStars(ALGORITHM_SEP).waitForFinished();
    QCOMPARE(fd->getDetectedStars(), NSTARS_STELLARSOLVER);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_STELLARSOLVER);
    QVERIFY(abs(fd->getHFR() - HFR_STELLARSOLVER) < 0.1);

    // Test the SEP algorithm with a tracking box, as used by the internal guider and subframe focus.
    fd->findStars(ALGORITHM_SEP, TRACKING_BOX).waitForFinished();
    auto centers = fd->getStarCenters();
    QCOMPARE(centers.count(), 1);
    // QWARN(QString("Center    %1,%2").arg(centers[0]->x).arg(centers[0]->y).toStdString().c_str());
    // QWARN(QString("TB Center %1,%2").arg(TRACKING_BOX.center().x()).arg(TRACKING_BOX.center().y()).toStdString().c_str());
    QVERIFY(abs(centers[0]->x - TRACKING_BOX.center().x()) <= 5);
    QVERIFY(abs(centers[0]->y - TRACKING_BOX.center().y()) <= 5);
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

        if (numDetects++ < repetitions)
            startDetect(numDetects % filenames.size());
    }
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

// This tests how well we can detect stars and/or plate-solve a number of images
// at the same time. Mostly a memory test--a failure would be a segv.
// I have not provided the fits files as part of the source code, so you need to
// provide them, add them the QVector<QString> variables, and make sure the directory
// below points to the one holding your files.
//
// You may wish to reconfigure the section with SolverLoop at the bottom to have more
// or less parallelism and switch between star detection and plate solves.
//
// You can want to run this as follows:
//   export QTEST_FUNCTION_TIMEOUT=1000000000; testfitsdata testParallelSolvers -maxwarnings 1000000
void TestFitsData::testParallelSolvers()
{
#define SKIP_PARALLEL_SOLVERS_TEST
#ifdef SKIP_PARALLEL_SOLVERS_TEST
    QSKIP("Skipping testParallelSolvers");
    return;
#else
    Options::setAutoDebayer(false);

    const QString dir = "/home/hy/Desktop/SharedFolder/DEBUG-solver";
    const QString dir1 = dir;
    const QVector<QString> files1 =
    {
        "guide_frame_00-20-08.fits",
        "guide_frame_00-20-12.fits",
        "guide_frame_00-20-15.fits",
        "guide_frame_00-20-18.fits",
        "guide_frame_00-20-21.fits",
        "guide_frame_00-20-24.fits",
        "guide_frame_00-20-27.fits",
    };

    const QString dir2 = dir;
    const QVector<QString> files2 =
    {
        "guide_frame_00-20-30.fits",
        "guide_frame_00-20-34.fits",
        "guide_frame_00-20-37.fits",
        "guide_frame_00-20-40.fits",
        "guide_frame_00-20-43.fits",
        "guide_frame_00-20-46.fits"
    };

    const QString dir3 = dir;
    const QVector<QString> files3 =
    {
        "m5_Light_LPR_120_secs_2022-03-12T04-44-56_201.fits",
        "m5_Light_LPR_120_secs_2022-03-12T04-47-02_202.fits",
        "m5_Light_LPR_120_secs_2022-03-12T04-49-04_203.fits",
        "m5_Light_LPR_120_secs_2022-03-12T04-51-06_204.fits",
        "m5_Light_LPR_120_secs_2022-03-12T04-53-07_205.fits"
    };

    const QString dir5 = dir;
    const QVector<QString> files5 =
    {
        "m74_Light_LPR_60_secs_2021-10-11T04-48-41_301.fits",
        "m74_Light_LPR_60_secs_2021-10-11T04-49-43_302.fits",
        "m74_Light_LPR_60_secs_2021-10-11T04-50-55_303.fits",
        "m74_Light_LPR_60_secs_2021-10-11T04-51-57_304.fits"
    };

    // Set the number of iterations here. THe more the better, e.g. 10000.
    constexpr int numIterations = 10000;

    // In the below declarations of SolverLoop,
    // for the 3rd arg: true means detect stars, false means plate solve them.

    // Detect stars in guide files
    SolverLoop loop1(files1, dir1, true, numIterations);
    loop1.setRandomAbort(1);

    // Detect stars in other guide files
    SolverLoop loop2(files2, dir2, true, numIterations);
    loop2.setRandomAbort(1);

    // Detect stars in subs
    SolverLoop loop3(files3, dir3, true, numIterations / 15);
    loop3.setRandomAbort(3);

    // This one solves the fits files
    SolverLoop loop4(files1, dir1, false, numIterations / 50);

    // This one solves the fits files
    SolverLoop loop5(files5, dir5, false, numIterations / 50);

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
        // The qWait is needed to allow message passing.
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
#endif
}

QTEST_GUILESS_MAIN(TestFitsData)
