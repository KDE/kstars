/*  KStars tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTFITSDATA_H
#define TESTFITSDATA_H

#include <QObject>
#include "fitsviewer/fitsdata.h"
#include "ekos/auxiliary/solverutils.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QRandomGenerator>
#include <memory>

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

        void testParallelSolvers();
    private:
        void startGuideDetect(const QString &filename);
        void guideLoadFinished();
        void guideDetectFinished();

        std::unique_ptr<FITSData> guideFits;
        QFuture<bool> guideFuture;
        QFutureWatcher<bool> guideWatcher;
        int numGuideDetects { 0 };
};

class SolverLoop : public QObject
{
        Q_OBJECT
    public:
        SolverLoop(const QVector<QString> &files, const QString &dir, bool isDetecting, int numReps);
        void start();
        bool done() const;
        int upto() const;
        QString status() const;
        void setRandomAbort(double secs)
        {
            randomAbortSecs = secs;
        };

    private:
        void detectFinished();
        void startDetect(int index);
        void solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void timeout();
        void randomTimeout();

        QVector<QString> filenames;
        QString directory;
        QFuture<bool> future;
        QFutureWatcher<bool> watcher;
        int numDetects { 0 };
        int repetitions { 0 };
        bool detecting { true };
        QSharedPointer<SolverUtils> solver;

        int currentIndex { 0 };
        QVector<QSharedPointer<FITSData>> images;
        QSharedPointer<FITSData>thisImage;
        // Timer to measure how long detections take.
        QElapsedTimer dTimer;
        // generate a random timeout between 0 and this many elapsed seconds.
        double randomAbortSecs { 0 };
        // Timer that can randomly interrupt detections.
        QTimer randomAbortTimer;
        QRandomGenerator rand;
        // the random timeout being used now.
        double thisRandomTimeout { 0 };

        QTimer timer;
        int timeoutSecs { 30 };
};

#endif // TESTFITSDATA_H
