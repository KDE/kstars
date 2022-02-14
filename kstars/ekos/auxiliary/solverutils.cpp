/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "solverutils.h"

#include "fitsviewer/fitsdata.h"
#include "Options.h"
#include <QRegularExpression>
#include <QUuid>

SolverUtils::SolverUtils(const SSolver::Parameters &parameters, double timeoutSeconds) : m_Parameters(parameters),
    m_Timeout(timeoutSeconds)
{
    connect(&m_Watcher, &QFutureWatcher<bool>::finished, this, &SolverUtils::executeSolver, Qt::UniqueConnection);
    connect(&m_SolverTimer, &QTimer::timeout, this, &SolverUtils::solverTimeout, Qt::UniqueConnection);
}

void SolverUtils::executeSolver()
{
    runSolver(m_ImageData);
}

void SolverUtils::runSolver(const QString &filename)
{
    m_ImageData.reset(new FITSData(), &QObject::deleteLater);
    QFuture<bool> response = m_ImageData->loadFromFile(filename);
    m_Watcher.setFuture(response);
}

void SolverUtils::prepareSolver()
{
    if (m_StellarSolver)
    {
        auto *solver = m_StellarSolver.release();
        if (solver)
        {
            solver->disconnect(this);
            if (solver->isRunning())
            {
                connect(solver, &StellarSolver::finished, solver, &StellarSolver::deleteLater);
                solver->abort();
            }
            else
                solver->deleteLater();
        }
    }

    m_StellarSolver.reset(new StellarSolver(SSolver::SOLVE, m_ImageData->getStatistics(), m_ImageData->getImageBuffer()));
    m_StellarSolver->setProperty("ExtractorType", Options::solveSextractorType());
    m_StellarSolver->setProperty("SolverType", Options::solverType());
    connect(m_StellarSolver.get(), &StellarSolver::ready, this, &SolverUtils::solverDone);
    connect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &SolverUtils::newLog);
    m_StellarSolver->setIndexFolderPaths(Options::astrometryIndexFolderList());
    m_StellarSolver->setProperty("SextractorBinaryPath", Options::sextractorBinary());
    m_StellarSolver->setProperty("SolverPath", Options::astrometrySolverBinary());
    m_StellarSolver->setProperty("ASTAPBinaryPath", Options::aSTAPExecutable());
    m_StellarSolver->setProperty("WCSPath", Options::astrometryWCSInfo());

    //No need for a conf file this way.
    m_StellarSolver->setProperty("AutoGenerateAstroConfig", true);
    m_StellarSolver->setParameters(m_Parameters);

    m_TemporaryFilename.clear();

    const SSolver::SolverType type = static_cast<SSolver::SolverType>(m_StellarSolver->property("SolverType").toInt());
    if(type == SSolver::SOLVER_LOCALASTROMETRY || type == SSolver::SOLVER_ASTAP)
    {
        m_TemporaryFilename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                  QRegularExpression("[-{}]")));
        m_ImageData->saveImage(m_TemporaryFilename);
        m_StellarSolver->setProperty("FileToProcess", m_TemporaryFilename);
    }
    else if (type == SSolver::SOLVER_ONLINEASTROMETRY )
    {
        m_TemporaryFilename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                  QRegularExpression("[-{}]")));
        m_ImageData->saveImage(m_TemporaryFilename);
        m_StellarSolver->setProperty("FileToProcess", m_TemporaryFilename);
        m_StellarSolver->setProperty("AstrometryAPIKey", Options::astrometryAPIKey());
        m_StellarSolver->setProperty("AstrometryAPIURL", Options::astrometryAPIURL());
    }

    if (m_UseScale)
    {
        SSolver::ScaleUnits units = static_cast<SSolver::ScaleUnits>(Options::astrometryImageScaleUnits());
        // Extend search scale from 80% to 120%
        m_StellarSolver->setSearchScale(m_ScaleLowArcsecPerPixel * 0.8,
                                        m_ScaleHighArcsecPerPixel * 1.2,
                                        units);
    }
    else
        m_StellarSolver->setProperty("UseScale", false);

    if (m_UsePosition)
        m_StellarSolver->setSearchPositionInDegrees(m_raDegrees, m_decDegrees);
    else
        m_StellarSolver->setProperty("UsePostion", false);

    // LOG_ALL is crashy now
    m_StellarSolver->setLogLevel(SSolver::LOG_ERROR);
    m_StellarSolver->setSSLogLevel(SSolver::LOG_NORMAL);
}

void SolverUtils::runSolver(const QSharedPointer<FITSData> &data)
{
    m_ImageData = data;
    prepareSolver();
    m_StellarSolver->start();

    // Limit the time the solver can run.
    m_SolverTimer.setSingleShot(true);
    m_SolverTimer.setInterval(m_Timeout * 1000);
    m_SolverTimer.start();
}

SolverUtils &SolverUtils::useScale(bool useIt, double scaleLowArcsecPerPixel, double scaleHighArcsecPerPixel)
{
    m_UseScale = useIt;
    m_ScaleLowArcsecPerPixel = scaleLowArcsecPerPixel;
    m_ScaleHighArcsecPerPixel = scaleHighArcsecPerPixel;
    return *this;
}

SolverUtils &SolverUtils::usePosition(bool useIt, double raDegrees, double decDegrees)
{
    m_UsePosition = useIt;
    m_raDegrees = raDegrees;
    m_decDegrees = decDegrees;
    return *this;
}

SolverUtils &usePosition(bool useIt, double raDegrees, double decDegrees);

void SolverUtils::solverDone()
{
    const double elapsed = m_Timeout - m_SolverTimer.remainingTime() / 1000.0;
    m_SolverTimer.stop();

    FITSImage::Solution solution;
    bool success = m_StellarSolver->solvingDone() && !m_StellarSolver->failed();
    if (success)
        solution = m_StellarSolver->getSolution();
    emit done(false, success, solution, elapsed);
    deleteSolver();
}

void SolverUtils::solverTimeout()
{
    m_SolverTimer.stop();
    FITSImage::Solution empty;
    emit done(true, false, empty, m_Timeout);
    deleteSolver();
}

void SolverUtils::deleteSolver()
{
    auto *solver = m_StellarSolver.release();
    if (solver)
    {
        solver->disconnect(this);
        if (solver->isRunning())
        {
            connect(solver, &StellarSolver::finished, solver, &StellarSolver::deleteLater);
            solver->abort();
        }
        else
            solver->deleteLater();
    }

    if (!m_TemporaryFilename.isEmpty())
        QFile::remove(m_TemporaryFilename);

    m_ImageData.reset();
}
