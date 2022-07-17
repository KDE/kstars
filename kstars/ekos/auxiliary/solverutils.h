/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <stellarsolver.h>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QFutureWatcher>
#include <mutex>
#include <memory>

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
        SolverUtils(const SSolver::Parameters &parameters, double timeoutSeconds = 15);
        ~SolverUtils();

        void runSolver(const QSharedPointer<FITSData> &data);
        void runSolver(const QString &filename);
        SolverUtils &useScale(bool useIt, double scaleLowArcsecPerPixel, double scaleHighArcsecPerPixel);
        SolverUtils &usePosition(bool useIt, double raDegrees, double decDegrees);
        bool isRunning() const;
        void abort();

        void setHealpix(int indexToUse = -1, int healpixToUse = -1);
        void getSolutionHealpix(int *indexUsed, int *healpixUsed) const;

        /**
         * @brief rotationToPositionAngle Convert from astrometry.net rotation to PA
         * @param value rotation in degrees (-180 to +180)
         * @return Position angle in degrees (-180 to +180)
         */
        static double rotationToPositionAngle(double value);

        /** @brief rotationToPositionAngle Convert from position angle to astrometry.net rotation
         * @param value Position angle in degrees (-180 to +180)
         * @return rotation in degrees (-180 to +180)
         */
        static double positionAngleToRotation(double value);

        /**
         * @brief rangePA Limit any angle to fall between -180 to +180 degrees for position angle
         * @param value angle
         * @return angle limited to -180 to +180
         */
        static double rangePA(double value);

    signals:
        void done(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void newLog(const QString &logText);

    private:
        void solverDone();
        void solverTimeout();
        void executeSolver();
        void prepareSolver();

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

        std::mutex deleteSolverMutex;
};

