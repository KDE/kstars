/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "solverutils.h"

#include "fitsviewer/fitsdata.h"
#include "Options.h"
#include <QRegularExpression>
#include <QUuid>

void SolverUtils::runSolver(FITSData * data, const SSolver::Parameters &parameters, double timeoutSeconds)
{
    if (m_StellarSolver)
    {
        auto *solver = m_StellarSolver.release();
        solver->disconnect(this);

        if (solver && solver->isRunning())
        {
            connect(solver, &StellarSolver::finished, solver, &StellarSolver::deleteLater);
            solver->abort();
        }
        else if (solver)
            solver->deleteLater();
    }

    m_SolverTimeoutSeconds = timeoutSeconds;
    m_StellarSolver.reset(new StellarSolver(SSolver::SOLVE, data->getStatistics(), data->getImageBuffer()));
    m_StellarSolver->setProperty("ExtractorType", Options::solveSextractorType());
    m_StellarSolver->setProperty("SolverType", Options::solverType());
    connect(m_StellarSolver.get(), &StellarSolver::ready, this, &SolverUtils::solverDone);
    connect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &SolverUtils::appendLogText);
    m_StellarSolver->setIndexFolderPaths(Options::astrometryIndexFolderList());
    m_StellarSolver->setParameters(parameters);

    const SSolver::SolverType type = static_cast<SSolver::SolverType>(m_StellarSolver->property("SolverType").toInt());
    m_TempFilename = "";
    if(type == SSolver::SOLVER_LOCALASTROMETRY || type == SSolver::SOLVER_ASTAP)
    {
        m_TempFilename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                             QRegularExpression("[-{}]")));
        data->saveImage(m_TempFilename);
        m_StellarSolver->setProperty("FileToProcess", m_TempFilename);
        m_StellarSolver->setProperty("SextractorBinaryPath", Options::sextractorBinary());
        m_StellarSolver->setProperty("SolverPath", Options::astrometrySolverBinary());
        m_StellarSolver->setProperty("ASTAPBinaryPath", Options::aSTAPExecutable());
        m_StellarSolver->setProperty("WCSPath", Options::astrometryWCSInfo());

        //No need for a conf file this way.
        m_StellarSolver->setProperty("AutoGenerateAstroConfig", true);
    }

    if(type == SSolver::SOLVER_ONLINEASTROMETRY )
    {
        // No online version. fail.
        emit appendLogText("Capture solver timed out. Can't check position.");
        return;
    }

    if (m_UseScale)
    {
        SSolver::ScaleUnits units = static_cast<SSolver::ScaleUnits>(Options::astrometryImageScaleUnits());
        // Extend search scale from 80% to 120%
        m_StellarSolver->setSearchScale(Options::astrometryImageScaleLow() * 0.8,
                                        Options::astrometryImageScaleHigh() * 1.2,
                                        units);
    }
    else
        m_StellarSolver->setProperty("UseScale", false);

    if (m_UsePosition)
        m_StellarSolver->setSearchPositionInDegrees(m_raDegrees, m_decDegrees);
    else
        m_StellarSolver->setProperty("UsePostion", false);

    m_StellarSolver->setLogLevel(SSolver::LOG_ALL);
    m_StellarSolver->setSSLogLevel(SSolver::LOG_NORMAL);
    m_StellarSolver->start();

    // Limit the time the solver can run.
    m_SolverTimer.setSingleShot(true);
    m_SolverTimer.setInterval(m_SolverTimeoutSeconds * 1000);
    connect(&m_SolverTimer, &QTimer::timeout, this, &SolverUtils::solverTimeout);
    m_SolverTimer.start();
}

SolverUtils &SolverUtils::useScale(bool useIt)
{
    m_UseScale = useIt;
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
    const double elapsed = m_SolverTimeoutSeconds - m_SolverTimer.remainingTime() / 1000.0;
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
    FITSImage::Solution empty;
    emit done(true, false, empty, m_SolverTimeoutSeconds);

    m_SolverTimer.stop();
    emit appendLogText("Capture solver timed out. Can't check position.");
    deleteSolver();
}

void SolverUtils::deleteSolver()
{
    m_SolverTimer.stop();
    auto solver = m_StellarSolver.release();

    disconnect(&m_SolverTimer, &QTimer::timeout, this, &SolverUtils::solverTimeout);
    disconnect(m_StellarSolver.get(), &StellarSolver::ready, this, &SolverUtils::solverDone);
    disconnect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &SolverUtils::appendLogText);

    solver->disconnect(this);
    if (solver && solver->isRunning())
    {
        connect(solver, &StellarSolver::finished, solver, &StellarSolver::deleteLater);
        solver->abort();
    }
    else if (solver)
        solver->deleteLater();

    // Would rather use QTemporaryFile but FITSData is writing the file.
    if (m_TempFilename.size() > 0)
    {
        QFile f(m_TempFilename);
        f.remove();
    }
}


