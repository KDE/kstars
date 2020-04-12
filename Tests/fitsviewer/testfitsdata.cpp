/*  KStars tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QtTest>

#include "testfitsdata.h"

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
    if(!QFile::exists(m_FitsFixture))
        QSKIP("Skipping load test because of missing fixture");

    cleanup();

    fd = new FITSData();
    QVERIFY(fd != nullptr);

    QFuture<bool> worker = fd->loadFITS(m_FitsFixture);
    QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 5000);
    QVERIFY(worker.result());
}

void TestFitsData::cleanup()
{
    if (fd != nullptr)
    {
        delete fd;
        fd = nullptr;
    }
}

void TestFitsData::testLoadFits()
{
    // Statistics computation
    QVERIFY(abs(fd->getADU() - 41.08) < 0.01);
    QVERIFY(abs(fd->getMean() - 41.08) < 0.01);
    QVERIFY(abs(fd->getStdDev() - 360.18) < 0.01);
    QVERIFY(abs(fd->getSNR() - 0.114) < 0.001);

    // Minmax
    QCOMPARE((int)fd->getMax(), 57832);
    QCOMPARE((int)fd->getMin(), 21);

    QWARN("No median calculation in the current implementation");
    QCOMPARE((int)fd->getMedian(), 0);

    // Without searching for stars, there are no stars found
    QCOMPARE(fd->getStarCenters().count(), 0);
    QCOMPARE(fd->getHFR(), -1.0);

    // Default algorithm is centroid, 80 stars with 1.495 as HFR
    QCOMPARE(fd->findStars(), 80);
    QCOMPARE(fd->getDetectedStars(), 80);
    QCOMPARE(fd->getStarCenters().count(), 80);
    QVERIFY(abs(fd->getHFR() - 1.49) < 0.01);

    // With the centroid algorithm, 80 stars with mean HFR 1.495
    QCOMPARE(fd->findStars(ALGORITHM_CENTROID), 80);
    QCOMPARE(fd->getDetectedStars(), 80);
    QCOMPARE(fd->getStarCenters().count(), 80);
    QVERIFY(abs(fd->getHFR() - 1.49) < 0.01);

    // With the gradient algorithm, one single star found with HFR 1.801
    QCOMPARE(fd->findStars(ALGORITHM_GRADIENT), 1);
    QCOMPARE(fd->getDetectedStars(), 1);
    QCOMPARE(fd->getStarCenters().count(), 1);
    QVERIFY(abs(fd->getHFR() - 1.80) < 0.01);

    // The threshold algorithm depends on a global option - skip until we know how to fiddle with that
    //QCOMPARE(fd->findStars(ALGORITHM_THRESHOLD), -1);
    //QCOMPARE(fd->getDetectedStars(), 0);
    //QCOMPARE(fd->getStarCenters().count(), 0);
    //QCOMPARE(fd->getHFR(), -1.0);

    // With the SEP algorithm, 100 stars with mean HFR 2.48
    QCOMPARE(fd->findStars(ALGORITHM_SEP), 100);
    QCOMPARE(fd->getDetectedStars(), 100);
    QCOMPARE(fd->getStarCenters().count(), 100);
    QVERIFY(abs(fd->getHFR() - 2.48) < 0.01);
}

void TestFitsData::testCentroidAlgorithmBenchmark()
{
    QBENCHMARK { fd->findStars(ALGORITHM_CENTROID); }
}

void TestFitsData::testGradientAlgorithmBenchmark()
{
    QBENCHMARK { fd->findStars(ALGORITHM_GRADIENT); }
}

void TestFitsData::testThresholdAlgorithmBenchmark()
{
    QSKIP("Skipping benchmark of Threshold Algorithm, which requires fiddling with a global option");
    QBENCHMARK { fd->findStars(ALGORITHM_THRESHOLD); }
}

void TestFitsData::testSEPAlgorithmBenchmark()
{
    QBENCHMARK { fd->findStars(ALGORITHM_SEP); }
}

QTEST_GUILESS_MAIN(TestFitsData)
