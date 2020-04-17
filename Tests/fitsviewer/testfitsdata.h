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
    QString const m_FocusFixture1 { "ngc4535-autofocus1.fits" };
    QString const m_FocusFixture2 { "ngc4535-autofocus2.fits" };
    QString const m_FocusFixture3 { "ngc4535-autofocus3.fits" };
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
    void testFocusHFR();
    void runFocusHFR(const QString &filename, int nstars, float hfr);
};

#endif // TESTFITSDATA_H
