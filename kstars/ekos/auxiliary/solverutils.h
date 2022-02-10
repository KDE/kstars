/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <stellarsolver.h>

class FITSData;

// This is a wrapper to make calling the StellarSolver solver a bit simpler.
// Must supply the imagedata and stellar solver parameters
// and connect to the signals. Remote solving not supported.
class SolverUtils : public QObject
{
        Q_OBJECT

    public:
        SolverUtils() {}

        void runSolver(FITSData * data, const SSolver::Parameters &parameters, double timeoutSeconds);

        SolverUtils &useScale(bool useIt);
        SolverUtils &usePosition(bool useIt, double raDegrees, double decDegrees);

    signals:
        void done(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void appendLogText(const QString &logText);

    private:
        void deleteSolver();
        void solverDone();
        void solverTimeout();

        std::unique_ptr<StellarSolver> m_StellarSolver;

        QTimer m_SolverTimer;
        double m_SolverTimeoutSeconds = 0;
        QString m_TempFilename;

        bool m_UseScale { false };
        bool m_UsePosition { false };
        double m_raDegrees { 0.0 };
        double m_decDegrees { 0.0 };
};

