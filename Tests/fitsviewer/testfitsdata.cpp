/*  KStars tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
    QTest::newRow("NGC4535-1-FOCUS") << "ngc4535-autofocus1.fits" << FITS_FOCUS << 11 << 3.89;
    QTest::newRow("NGC4535-2-FOCUS") << "ngc4535-autofocus2.fits" << FITS_FOCUS << 17 << 2.16;
    QTest::newRow("NGC4535-3-FOCUS") << "ngc4535-autofocus3.fits" << FITS_FOCUS << 100 << 1.23;

    // Focus HFR tests
    QTest::newRow("NGC4535-1-NORMAL") << "ngc4535-autofocus1.fits" << FITS_NORMAL << 4 << 3.05;
    QTest::newRow("NGC4535-2-NORMAL") << "ngc4535-autofocus2.fits" << FITS_NORMAL << 6 << 1.83;
    QTest::newRow("NGC4535-3-NORMAL") << "ngc4535-autofocus3.fits" << FITS_NORMAL << 30 << 1.25;
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

    QFuture<bool> worker = d->loadFITS(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 60000);
    QVERIFY(worker.result());

    QCOMPARE(d->findStars(ALGORITHM_SEP), NSTARS);
    QCOMPARE(d->getDetectedStars(), NSTARS);
    QCOMPARE(d->getStarCenters().count(), NSTARS);
    QVERIFY(abs(d->getHFR() - HFR) <= 0.01);
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

    QFuture<bool> worker = d->loadFITS(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    // The bahtinov algorithm depends on which star is selected and number of average rows - not sure how to fiddle with that yet
    const QRect trackingBox(204, 240, 128, 128);

    QCOMPARE(d->findStars(ALGORITHM_BAHTINOV, trackingBox), 1);
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
    QTest::addColumn<int>("NSTARS_SEP");

    // HFRs using variouls methods
    QTest::addColumn<double>("HFR_CENTROID");
    QTest::addColumn<double>("HFR_GRADIENT");
    QTest::addColumn<double>("HFR_THRESHOLD");
    QTest::addColumn<double>("HFR_SEP");

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
            << 100      // Stars found with the SEP detection
            << 1.49     // HFR found with the Centroid detection
            << 1.80     // HFR found with the Gradient detection
            << 0.0      // HFR found with the Threshold detection - not used
            << 2.09     // HFR found with the SEP detection
            << 41.08    // ADU
            << 41.08    // Mean
            << 360.18   // StdDev
            << 0.114    // SNR
            << 57832L   // Max
            << 21L      // Min
            << 0.0      // Median
            << QRect(591 - 16/2, 482 - 16/2, 16, 16);
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
    QFETCH(int, NSTARS_SEP);
    QFETCH(double, HFR_CENTROID);
    QFETCH(double, HFR_GRADIENT);
    QFETCH(double, HFR_THRESHOLD); Q_UNUSED(HFR_THRESHOLD);
    QFETCH(double, HFR_SEP);
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

    QFuture<bool> worker = fd->loadFITS(NAME);
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
    QCOMPARE(fd->findStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getDetectedStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_CENTROID);
    QVERIFY(abs(fd->getHFR() - HFR_CENTROID) < 0.01);

    // With the centroid algorithm, 80 stars with MEAN HFR 1.495
    QCOMPARE(fd->findStars(ALGORITHM_CENTROID), NSTARS_CENTROID);
    QCOMPARE(fd->getDetectedStars(), NSTARS_CENTROID);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_CENTROID);
    QVERIFY(abs(fd->getHFR() - HFR_CENTROID) < 0.01);

    // With the gradient algorithm, one single star found with HFR 1.801
    QCOMPARE(fd->findStars(ALGORITHM_GRADIENT), 1);
    QCOMPARE(fd->getDetectedStars(), 1);
    QCOMPARE(fd->getStarCenters().count(), 1);
    QVERIFY(abs(fd->getHFR() - HFR_GRADIENT) < 0.01);

    // The threshold algorithm depends on a global option - skip until we know how to fiddle with that
    //QCOMPARE(fd->findStars(ALGORITHM_THRESHOLD), -1);
    //QCOMPARE(fd->getDetectedStars(), 0);
    //QCOMPARE(fd->getStarCenters().count(), 0);
    //QCOMPARE(fd->getHFR(), -1.0);

    // With the SEP algorithm, 100 stars with MEAN HFR 2.09
    QCOMPARE(fd->findStars(ALGORITHM_SEP), NSTARS_SEP);
    QCOMPARE(fd->getDetectedStars(), NSTARS_SEP);
    QCOMPARE(fd->getStarCenters().count(), NSTARS_SEP);
    QVERIFY(abs(fd->getHFR() - HFR_SEP) < 0.01);

    // Test the SEP algorithm with a tracking box, as used by the internal guider and subframe focus.
    QCOMPARE(fd->findStars(ALGORITHM_SEP, TRACKING_BOX), 1);
    auto centers = fd->getStarCenters();
    QCOMPARE(centers.count(), 1);
    QWARN(QString("Center    %1,%2").arg(centers[0]->x).arg(centers[0]->y).toStdString().c_str());
    QWARN(QString("TB Center %1,%2").arg(TRACKING_BOX.center().x()).arg(TRACKING_BOX.center().y()).toStdString().c_str());
    QVERIFY(abs(centers[0]->x - TRACKING_BOX.center().x()) <= 2);
    QVERIFY(abs(centers[0]->y - TRACKING_BOX.center().y()) <= 2);
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

    QFuture<bool> worker = d->loadFITS(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_CENTROID); }
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

    QFuture<bool> worker = d->loadFITS(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_GRADIENT); }
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

    QFuture<bool> worker = d->loadFITS(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_THRESHOLD); }
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

    QFuture<bool> worker = d->loadFITS(NAME);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000);
    QVERIFY(worker.result());

    QBENCHMARK { d->findStars(ALGORITHM_SEP); }
#endif
}

QTEST_GUILESS_MAIN(TestFitsData)
