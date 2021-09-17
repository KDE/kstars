/*  KStars tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

    private:
        void initGenericDataFixture();

    private slots:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        void testLoadFits_data();
        void testLoadFits();

        void testCentroidAlgorithmBenchmark_data();
        void testCentroidAlgorithmBenchmark();

        void testGradientAlgorithmBenchmark_data();
        void testGradientAlgorithmBenchmark();

        void testThresholdAlgorithmBenchmark_data();
        void testThresholdAlgorithmBenchmark();

        void testSEPAlgorithmBenchmark_data();
        void testSEPAlgorithmBenchmark();

        void testComputeHFR_data();
        void testComputeHFR();

        void testBahtinovFocusHFR_data();
        void testBahtinovFocusHFR();
};

#endif // TESTFITSDATA_H
