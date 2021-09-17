/*  KStars tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>
#include <memory>
#include "testfitsdata.h"

Q_DECLARE_METATYPE(FITSMode);

TestFitsData::TestFitsData(QObject *parent) : QObject(parent)
{
}

void TestFitsData::initTestCase()
{
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
    QTest::newRow("NGC4535-3-FOCUS") << "ngc4535-autofocus3.fits" << FITS_FOCUS << 125 << 1.30;

    // Focus HFR tests
    QTest::newRow("NGC4535-1-NORMAL") << "ngc4535-autofocus1.fits" << FITS_NORMAL << 3 << 3.02;
    QTest::newRow("NGC4535-2-NORMAL") << "ngc4535-autofocus2.fits" << FITS_NORMAL << 4 << 2.02;
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
    qDebug() << "Expected HFR:" << HFR << "Calculated:" << d->getHFR();
    QVERIFY(abs(d->getHFR() - HFR) <= 0.1);
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
            << 94       // Stars found with the StellarSolver detection - default profile limits count
            << 1.49     // HFR found with the Centroid detection
            << 1.80     // HFR found with the Gradient detection
            << 0.0      // HFR found with the Threshold detection - not used
            << 2.08     // HFR found with the StellarSolver detection
            << 41.08    // ADU
            << 41.08    // Mean
            << 360.18   // StdDev
            << 0.114    // SNR
            << 57832L   // Max
            << 21L      // Min
            << 0.0      // Median
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

    QWARN("No MEDIAN calculation in the current implementation");
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
    QWARN(QString("Center    %1,%2").arg(centers[0]->x).arg(centers[0]->y).toStdString().c_str());
    QWARN(QString("TB Center %1,%2").arg(TRACKING_BOX.center().x()).arg(TRACKING_BOX.center().y()).toStdString().c_str());
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

QTEST_GUILESS_MAIN(TestFitsData)
