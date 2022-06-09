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

    m_StellarSolver.reset(new StellarSolver());
    connect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &SolverUtils::newLog);
}

SolverUtils::~SolverUtils()
{

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

void SolverUtils::setHealpix(int indexToUse, int healpixToUse)
{
    m_IndexToUse = indexToUse;
    m_HealpixToUse = healpixToUse;
}

void SolverUtils::getSolutionHealpix(int *indexUsed, int *healpixUsed)
{
    *indexUsed = m_StellarSolver->getSolutionIndexNumber();
    *healpixUsed = m_StellarSolver->getSolutionHealpix();
}


void SolverUtils::prepareSolver()
{
    if (m_StellarSolver->isRunning())
        m_StellarSolver->abort();
    m_StellarSolver->setProperty("ProcessType", SSolver::SOLVE);
    m_StellarSolver->loadNewImageBuffer(m_ImageData->getStatistics(), m_ImageData->getImageBuffer());
    m_StellarSolver->setProperty("ExtractorType", Options::solveSextractorType());
    m_StellarSolver->setProperty("SolverType", Options::solverType());
    connect(m_StellarSolver.get(), &StellarSolver::ready, this, &SolverUtils::solverDone);

    if (m_IndexToUse >= 0)
    {
        // The would only have an effect if Options::solverType() == SOLVER_STELLARSOLVER
        QStringList indexFiles = StellarSolver::getIndexFiles(
                                     Options::astrometryIndexFolderList(), m_IndexToUse, m_HealpixToUse);
        m_StellarSolver->setIndexFilePaths(indexFiles);
    }
    else
        m_StellarSolver->setIndexFolderPaths(Options::astrometryIndexFolderList());

    // External program paths
    ExternalProgramPaths externalPaths;
    externalPaths.sextractorBinaryPath = Options::sextractorBinary();
    externalPaths.solverPath = Options::astrometrySolverBinary();
    externalPaths.astapBinaryPath = Options::aSTAPExecutable();
    externalPaths.watneyBinaryPath = Options::watneyBinary();
    externalPaths.wcsPath = Options::astrometryWCSInfo();
    m_StellarSolver->setExternalFilePaths(externalPaths);

    //No need for a conf file this way.
    m_StellarSolver->setProperty("AutoGenerateAstroConfig", true);
    m_StellarSolver->setParameters(m_Parameters);

    m_TemporaryFilename.clear();

    const SSolver::SolverType type = static_cast<SSolver::SolverType>(m_StellarSolver->property("SolverType").toInt());
    if(type == SSolver::SOLVER_LOCALASTROMETRY || type == SSolver::SOLVER_ASTAP || type == SSolver::SOLVER_WATNEYASTROMETRY)
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
        m_StellarSolver->setProperty("UsePosition", false);

    // LOG_ALL is crashy now
    m_StellarSolver->setLogLevel(SSolver::LOG_NONE);
    m_StellarSolver->setSSLogLevel(SSolver::LOG_OFF);
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

    if (!m_TemporaryFilename.isEmpty())
        QFile::remove(m_TemporaryFilename);
    m_TemporaryFilename.clear();
    m_ImageData.reset();
}

void SolverUtils::solverTimeout()
{
    m_SolverTimer.stop();
    FITSImage::Solution empty;
    emit done(true, false, empty, m_Timeout);

    if (!m_TemporaryFilename.isEmpty())
        QFile::remove(m_TemporaryFilename);
    m_TemporaryFilename.clear();
    m_ImageData.reset();
}
