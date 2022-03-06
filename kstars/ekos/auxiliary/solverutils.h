/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QFutureWatcher>
#include <mutex>
#include <stellarsolver.h>
#include <memory>

class FITSData;

// This is a wrapper to make calling the StellarSolver solver a bit simpler.
// Must supply the imagedata and stellar solver parameters
// and connect to the signals. Remote solving not supported.
class SolverUtils : public QObject
{
        Q_OBJECT

    public:
        SolverUtils(const SSolver::Parameters &parameters, double timeoutSeconds = 15);
        ~SolverUtils();

        void runSolver(const QSharedPointer<FITSData> &data);
        void runSolver(const QString &filename);
        SolverUtils &useScale(bool useIt, double scaleLowArcsecPerPixel, double scaleHighArcsecPerPixel);
        SolverUtils &usePosition(bool useIt, double raDegrees, double decDegrees);

    signals:
        void done(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void newLog(const QString &logText);

    private:
        void solverDone();
        void solverTimeout();
        void executeSolver();
        void prepareSolver();

        std::unique_ptr<StellarSolver> m_StellarSolver;

        QTimer m_SolverTimer;
        // Copy of parameters
        SSolver::Parameters m_Parameters;
        // Solver timeout in seconds.
        uint32_t m_Timeout {0};
        // Temporary file name in case of external solver.
        QString m_TemporaryFilename;
        QFutureWatcher<bool> m_Watcher;
        double m_ScaleLowArcsecPerPixel {0}, m_ScaleHighArcsecPerPixel {0};

        QSharedPointer<FITSData> m_ImageData;

        bool m_UseScale { false };
        bool m_UsePosition { false };
        double m_raDegrees { 0.0 };
        double m_decDegrees { 0.0 };

        std::mutex deleteSolverMutex;
};

