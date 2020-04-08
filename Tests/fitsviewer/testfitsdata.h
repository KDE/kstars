/*  KStars tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTFITSDATA_H
#define TESTFITSDATA_H

#include <QObject>
#include "fitsviewer/fitsdata.h"

class TestFitsData : public QObject
{
    Q_OBJECT
public:
    explicit TestFitsData(QObject *parent = nullptr);

public:
    QString const m_FitsFixture { "m47_sim_stars.fits" };
    FITSData * fd { nullptr };

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testLoadFits();
    void testCentroidAlgorithmBenchmark();
    void testGradientAlgorithmBenchmark();
    void testThresholdAlgorithmBenchmark();
    void testSEPAlgorithmBenchmark();
};

#endif // TESTFITSDATA_H
