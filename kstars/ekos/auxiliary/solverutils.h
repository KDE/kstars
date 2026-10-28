/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <stellarsolver.h>
#undef Const

#include <QObject>
#include <QString>
#include <QTimer>
#include <QFutureWatcher>
#include <mutex>
#include <memory>
#include <QSharedPointer>

#ifdef _WIN32
#undef Unused
#endif

class FITSData;

// This is a wrapper to make calling the StellarSolver solver a bit simpler.
// Must supply the imagedata and stellar solver parameters
// and connect to the signals. Remote solving not supported.
class SolverUtils : public QObject
{
        Q_OBJECT

    public:
        SolverUtils(const SSolver::Parameters &parameters, double timeoutSeconds = 15,
                    SSolver::ProcessType type = SSolver::SOLVE);
        ~SolverUtils();

        void runSolver(const QSharedPointer<FITSData> &data, const bool stack = false);
        void runSolver(const QString &filename);
        /**
         * @brief useScale Search for an image scale between scaleLow and scaleHigh. The range is widened by the scale
         * tolerance, see setScaleTolerance(), so pass the expected scale without a margin of your own.
         */
        SolverUtils &useScale(bool useIt, double scaleLow, double scaleHigh, SSolver::ScaleUnits units = ARCSEC_PER_PIX);
        /**
         * @brief setScaleTolerance Set how much the scale range of useScale() is widened, e.g. 0.05 searches from
         * 5% below scaleLow to 5% above scaleHigh. Defaults to 0.2.
         */
        SolverUtils &setScaleTolerance(double fraction);
        SolverUtils &usePosition(bool useIt, double raDegrees, double decDegrees);
        /**
         * @brief setExpectedFieldWidth Set the field width the image is expected to have, e.g. from the optical train.
         * When solving without a scale hint, the solver then searches field widths around it instead of the
         * profile's minwidth/maxwidth. Use 0 (default) to use the profile limits.
         * @param degrees expected field width in degrees.
         */
        SolverUtils &setExpectedFieldWidth(double degrees);
        bool isRunning() const;
        void abort(bool wait = false);

        void setHealpix(int indexToUse = -1, int healpixToUse = -1);
        void getSolutionHealpix(int *indexUsed, int *healpixUsed) const;

        const FITSImage::Background &getBackground() const
        {
            // Better leak than crash. Warn?
            if (!m_StellarSolver) return *new FITSImage::Background();
            return m_StellarSolver->getBackground();
        }
        const QList<FITSImage::Star> &getStarList() const
        {
            // Better leak than crash. Warn?
            if (!m_StellarSolver) return *new QList<FITSImage::Star>();
            return m_StellarSolver->getStarList();
        }
        int getNumStarsFound() const
        {
            if (!m_StellarSolver) return 0;
            return m_StellarSolver->getNumStarsFound();
        };

        static void patchMultiAlgorithm(StellarSolver *solver);

        /**
         * @brief selectMultiAlgorithm Resolve MULTI_AUTO based on the available hints.
         * With a scale or position hint, a single thread is fastest. Parallel solvers add overhead that the
         * narrow search does not recover. A blind solve searches on multiple scales in parallel, so it gives
         * up quickly on images that cannot be solved. Explicit choices are kept.
         */
        static SSolver::MultiAlgo selectMultiAlgorithm(SSolver::MultiAlgo requested, bool useScale, bool usePosition);

        /**
         * @brief applyExpectedFieldWidth Limit the blind scale search to 1/4x to 4x of the expected field width,
         * which covers a wrong reducer, barlow or focal length. Does nothing if the width is not positive.
         */
        static void applyExpectedFieldWidth(SSolver::Parameters &parameters, double degrees);

        /**
         * @brief scaleSearchRange The scale range searched for useScale(scaleLow, scaleHigh) with the given tolerance.
         */
        static void scaleSearchRange(double scaleLow, double scaleHigh, double tolerance, double &searchLow,
                                     double &searchHigh);

        // Override the multi-algorithm selection for testing.
        static void setMultiAlgorithmOverride(int algo)
        {
            s_MultiAlgorithmOverride = algo;
        }
        static void clearMultiAlgorithmOverride()
        {
            s_MultiAlgorithmOverride = -1;
        }

    Q_SIGNALS:
        void done(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void newLog(const QString &logText);

    private:
        void solverDone();
        void solverTimeout();
        void executeSolver();
        void prepareSolver(const bool stack = false);

        std::unique_ptr<StellarSolver> m_StellarSolver;

        qint64 m_StartTime;
        QTimer m_SolverTimer;
        // Copy of parameters
        SSolver::Parameters m_Parameters;
        // Solver timeout in milliseconds.
        const uint32_t m_TimeoutMilliseconds {0};
        // Temporary file name in case of external solver.
        QString m_TemporaryFilename;
        QFutureWatcher<bool> m_Watcher;
        double m_ScaleLow {0}, m_ScaleHigh {0};
        double m_ScaleTolerance {0.2};
        SSolver::ScaleUnits m_ScaleUnits { ARCSEC_PER_PIX };

        QSharedPointer<FITSData> m_ImageData;

        int m_IndexToUse { -1 };
        int m_HealpixToUse { -1 };

        bool m_UseScale { false };
        bool m_UsePosition { false };
        double m_raDegrees { 0.0 };
        double m_decDegrees { 0.0 };
        double m_ExpectedFieldWidth { 0.0 };

        SSolver::ProcessType m_Type = SSolver::SOLVE;
        std::mutex deleteSolverMutex;

        static int s_MultiAlgorithmOverride;
};
