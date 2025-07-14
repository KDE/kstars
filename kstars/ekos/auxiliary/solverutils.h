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
        SolverUtils &useScale(bool useIt, double scaleLowArcsecPerPixel, double scaleHighArcsecPerPixel);
        SolverUtils &usePosition(bool useIt, double raDegrees, double decDegrees);
        bool isRunning() const;
        void abort();

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

        // We don't trust StellarSolver's mutli-processing algorithm MULTI_DEPTHS which is used
        // with multiAlgorithm==MULTI_AUTO && use_scale && !use_position. This disables that.
        static void patchMultiAlgorithm(StellarSolver *solver);

    signals:
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
        double m_ScaleLowArcsecPerPixel {0}, m_ScaleHighArcsecPerPixel {0};

        QSharedPointer<FITSData> m_ImageData;

        int m_IndexToUse { -1 };
        int m_HealpixToUse { -1 };

        bool m_UseScale { false };
        bool m_UsePosition { false };
        double m_raDegrees { 0.0 };
        double m_decDegrees { 0.0 };

        SSolver::ProcessType m_Type = SSolver::SOLVE;
        std::mutex deleteSolverMutex;
};

