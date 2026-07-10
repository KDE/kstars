/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "solverutils.h"

#include "ekos_debug.h"
#include "fitsviewer/fitsdata.h"
#include "Options.h"
#include <QRegularExpression>
#include <QUuid>

int SolverUtils::s_MultiAlgorithmOverride = -1;

SolverUtils::SolverUtils(const SSolver::Parameters &parameters, double timeoutSeconds,
                         SSolver::ProcessType type) :
    m_Parameters(parameters), m_TimeoutMilliseconds(timeoutSeconds * 1000.0), m_Type(type)
{
    connect(&m_Watcher, &QFutureWatcher<bool>::finished, this, &SolverUtils::executeSolver, Qt::UniqueConnection);
    connect(&m_SolverTimer, &QTimer::timeout, this, &SolverUtils::solverTimeout, Qt::UniqueConnection);

    m_StellarSolver.reset(new StellarSolver());
    // connect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &SolverUtils::newLog);
}

SolverUtils::~SolverUtils()
{
    disconnect(&m_Watcher, &QFutureWatcher<bool>::finished, this, &SolverUtils::executeSolver);
    disconnect(&m_SolverTimer, &QTimer::timeout, this, &SolverUtils::solverTimeout);
    if (m_StellarSolver.get())
    {
        // disconnect(m_StellarSolver.get(), &StellarSolver::logOutput, this, &SolverUtils::newLog);
        disconnect(m_StellarSolver.get(), &StellarSolver::finished, this, &SolverUtils::solverDone);
    }
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

void SolverUtils::abort(bool wait)
{
    if (m_StellarSolver.get())
    {
        if (wait)
            m_StellarSolver->abortAndWait();
        else
            m_StellarSolver->abort();
    }
}

bool SolverUtils::isRunning() const
{
    if (!m_StellarSolver.get()) return false;
    return m_StellarSolver->isRunning();
}

void SolverUtils::getSolutionHealpix(int *indexUsed, int *healpixUsed) const
{
    *indexUsed = m_StellarSolver->getSolutionIndexNumber();
    *healpixUsed = m_StellarSolver->getSolutionHealpix();
}


void SolverUtils::prepareSolver(const bool stack)
{
    if (m_StellarSolver->isRunning())
        m_StellarSolver->abort();
    m_StellarSolver->setProperty("ProcessType", m_Type);
    if (stack)
        m_StellarSolver->loadNewImageBuffer(m_ImageData->getStackStatistics(), m_ImageData->getStackImageBuffer());
    else
        m_StellarSolver->loadNewImageBuffer(m_ImageData->getStatistics(), m_ImageData->getImageBuffer());
    m_StellarSolver->setProperty("ExtractorType", Options::solveSextractorType());
    m_StellarSolver->setProperty("SolverType", Options::solverType());
    connect(m_StellarSolver.get(), &StellarSolver::finished, this, &SolverUtils::solverDone, Qt::UniqueConnection);

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

    auto params = m_Parameters;
    params.partition = Options::stellarSolverPartition();
    m_StellarSolver->setParameters(params);

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
        // Extend search scale from 80% to 120%
        m_StellarSolver->setSearchScale(m_ScaleLow * 0.8, m_ScaleHigh * 1.2, m_ScaleUnits);
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

    patchMultiAlgorithm(m_StellarSolver.get());
}

void SolverUtils::runSolver(const QSharedPointer<FITSData> &data, const bool stack)
{
    // Limit the time the solver can run.
    m_SolverTimer.setSingleShot(true);
    m_SolverTimer.setInterval(m_TimeoutMilliseconds);
    m_SolverTimer.start();
    // Somehow m_SolverTimer's elapsed time can be greater than the interval,
    // so using this to get more exact times.
    m_StartTime = QDateTime::currentMSecsSinceEpoch();

    m_ImageData = data;
    prepareSolver(stack);
    m_StellarSolver->start();
}

SolverUtils &SolverUtils::useScale(bool useIt, double scaleLow, double scaleHigh, SSolver::ScaleUnits units)
{
    m_UseScale = useIt;
    m_ScaleLow = scaleLow;
    m_ScaleHigh = scaleHigh;
    m_ScaleUnits = units;
    return *this;
}

SolverUtils &SolverUtils::usePosition(bool useIt, double raDegrees, double decDegrees)
{
    m_UsePosition = useIt;
    m_raDegrees = raDegrees;
    m_decDegrees = decDegrees;
    return *this;
}

void SolverUtils::solverDone()
{
    const double elapsed = (QDateTime::currentMSecsSinceEpoch() - m_StartTime) / 1000.0;
    m_SolverTimer.stop();

    if (m_Type == SSolver::SOLVE)
    {
        FITSImage::Solution solution;
        const bool success = m_StellarSolver->solvingDone() && !m_StellarSolver->failed();
        if (success)
            solution = m_StellarSolver->getSolution();
        Q_EMIT done(false, success, solution, elapsed);
    }
    else
    {
        const bool success = m_StellarSolver->extractionDone() && !m_StellarSolver->failed();
        Q_EMIT done(false, success, FITSImage::Solution(), elapsed);
    }
    if (!m_TemporaryFilename.isEmpty())
        QFile::remove(m_TemporaryFilename);
    m_TemporaryFilename.clear();
}

void SolverUtils::solverTimeout()
{
    m_SolverTimer.stop();

    disconnect(m_StellarSolver.get(), &StellarSolver::finished, this, &SolverUtils::solverDone);
    abort();

    FITSImage::Solution empty;
    Q_EMIT done(true, false, empty, m_TimeoutMilliseconds / 1000.0);
    if (!m_TemporaryFilename.isEmpty())
        QFile::remove(m_TemporaryFilename);
    m_TemporaryFilename.clear();
}

// StellarSolver's MULTI_AUTO maps hints to algorithms backwards:
//   both hints -> NOT_MULTI, pos only -> MULTI_SCALES, scale only -> MULTI_DEPTHS.
// MULTI_DEPTHS needs position to narrow healpix search; without it, solve times
// blow up 5-14x. MULTI_SCALES wastes threads when scale is already known.
// Fix: select based on which hints are available.
void SolverUtils::patchMultiAlgorithm(StellarSolver *solver)
{
    if (!solver)
        return;

    auto params = solver->getCurrentParameters();

    if (s_MultiAlgorithmOverride >= 0)
    {
        params.multiAlgorithm = static_cast<SSolver::MultiAlgo>(s_MultiAlgorithmOverride);
        solver->setParameters(params);
        return;
    }

    if (params.multiAlgorithm == MULTI_AUTO)
    {
        bool usePosition = solver->property("UsePosition").toBool();
        // MULTI_DEPTHS partitions the star-depth axis across threads, which
        // helps when a position hint constrains the healpix search area but
        // only if the scale window is also narrow enough for each thread to
        // work a meaningful slice. With maxwidth at the full-sky default (180)
        // there is no useful scale partitioning, so fall back to MULTI_SCALES.
        bool scaleConstrained = params.maxwidth < 180.0;
        if (usePosition && !scaleConstrained)
        {
            static bool s_warnedMaxwidth = false;
            if (!s_warnedMaxwidth)
            {
                s_warnedMaxwidth = true;
                qCInfo(KSTARS_EKOS) << "Position hint available, but solver profile maxwidth is"
                                    << params.maxwidth
                                    << "deg (full sky) -- parallel solving cannot take advantage of the hint."
                                    << "For faster solves, set a narrower maxwidth (e.g. 10 deg) in your solver profile.";
            }
        }
        params.multiAlgorithm = (usePosition && scaleConstrained) ? MULTI_DEPTHS : MULTI_SCALES;
        solver->setParameters(params);
    }
}
