/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stackcontroller.h"
#include "fitsviewer/fitsdata.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "Options.h"

#include <algorithm>
#include <vector>

StackController::StackController(QObject *parent, FITSMode mode) : QObject(parent), m_Mode(mode)
{
}

StackController::~StackController()
{
}

void StackController::start(const QStringList &inDir, const StackData &params)
{
    m_ImageData.reset(new FITSData(m_Mode), &QObject::deleteLater);

    connect(m_ImageData.data(), &FITSData::plateSolveSub, this, &StackController::plateSolveSub);
    connect(m_ImageData.data(), &FITSData::plateSolveSub, this, &StackController::handlePlateSolveSub);
    connect(m_ImageData.data(), &FITSData::alignMasterChosen, this, &StackController::alignMasterChosen);
    connect(m_ImageData.data(), &FITSData::stackInProgress, this, &StackController::stackInProgress);
    connect(m_ImageData.data(), &FITSData::stackReady, this, &StackController::stackReady);
    connect(m_ImageData.data(), &FITSData::stackFailed, this, &StackController::stackFailed);
    connect(m_ImageData.data(), &FITSData::stackUpdateStats, this, &StackController::stackUpdateStats);

    // Mirrors FITSView::loadStack()'s placeholder-load workaround: loadStack() may
    // synchronously trigger stackReady() (when existing subs are present), which calls
    // loadStackBuffer(), launching a second background thread that writes fptr while a
    // still-in-flight loadFromFile() thread is inside calculateStats() — a race that
    // corrupts fptr and crashes cfitsio. Loading a placeholder first and waiting for it
    // to finish avoids that race.
    QString noImage = QStringLiteral(":/images/noimage.png");
    m_FitsWatcher.setFuture(m_ImageData->loadFromFile(noImage));
    m_FitsWatcher.waitForFinished();

    m_ImageData->loadStack(inDir, params);
}

void StackController::cancel()
{
    if (m_ImageData)
        m_ImageData->cancelStack();
}

void StackController::redoPostProcess(const StackPPData &ppParams)
{
    if (m_ImageData)
        m_ImageData->redoPostProcessStack(ppParams);
}

bool StackController::crop(const QRect &roi, QString &error)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->cropStack(roi, error);
}

bool StackController::applyAutoStretch(double targetBackground, double shadowsClipping, QString &error, bool linked)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->applyAutoStretch(targetBackground, shadowsClipping, error, linked);
}

bool StackController::applyCurve(const QVector<QPointF> &controlPoints, QString &error)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->applyCurve(controlPoints, error);
}

bool StackController::applyCurvePerChannel(const QVector<QVector<QPointF>> &channelPoints, QString &error)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->applyCurvePerChannel(channelPoints, error);
}

bool StackController::applySaturation(double amt, QString &error)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->applySaturation(amt, error);
}

bool StackController::applyContrast(double amt, QString &error)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->applyContrast(amt, error);
}

bool StackController::save(const QString &path, QString &error)
{
    if (!m_ImageData)
    {
        error = QStringLiteral("No active session — call start() first");
        return false;
    }
    return m_ImageData->saveStackedImage(path, error);
}

bool StackController::adopt(const cv::Mat &image, QString &error)
{
    if (!m_ImageData)
        m_ImageData.reset(new FITSData(m_Mode), &QObject::deleteLater);
    return m_ImageData->setStackedImage(image, error);
}

void StackController::handlePlateSolveSub(const double ra, const double dec, const double pixScale, const int index,
        const int healpix, const StackFrameWeighting &weighting)
{
    m_StackExtendedPlateSolve = (index == -1);
    m_PendingRa = ra;
    m_PendingDec = dec;
    m_PendingPixScale = pixScale;
    m_PendingIndex = index;
    m_PendingHealpix = healpix;
    if (weighting == StackFrameWeighting::HFR || weighting == StackFrameWeighting::NUM_STARS)
        // Star-quality weighting needs a star list first, so extract before solving.
        runExtract(ra, dec, pixScale, index, healpix);
    else
        runSolve(ra, dec, pixScale, index, healpix);
}

void StackController::runExtract(const double ra, const double dec, const double pixScale, const int index,
                                  const int healpix)
{
    auto parameters = Ekos::getDefaultAlignOptionsProfiles().at(Options::solveOptionsProfile());
    const double lowerPixScale = (index == -1) ? pixScale * 0.8 : pixScale * 0.95;
    const double upperPixScale = (index == -1) ? pixScale * 1.2 : pixScale * 1.05;
    if (index != -1)
        parameters.search_radius = 1;
    parameters.solverTimeLimit = std::min(parameters.solverTimeLimit, 20);

    m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::EXTRACT_WITH_HFR),
                   &QObject::deleteLater);
    connect(m_Solver.get(), &SolverUtils::done, this, &StackController::handleExtractDone, Qt::UniqueConnection);
    m_Solver->useScale(true, lowerPixScale, upperPixScale);
    m_Solver->usePosition(true, ra, dec);
    m_Solver->setHealpix(index, healpix);
    m_Solver->runSolver(m_ImageData, true);
}

void StackController::runSolve(const double ra, const double dec, const double pixScale, const int index,
                                const int healpix)
{
    auto parameters = Ekos::getDefaultAlignOptionsProfiles().at(Options::solveOptionsProfile());
    double lowerPixScale, upperPixScale;
    if (index == -1)
    {
        // First solve (or a widened retry): looser criteria, full search radius.
        lowerPixScale = pixScale * 0.8;
        upperPixScale = pixScale * 1.2;
    }
    else
    {
        // We already know roughly where this landed (from an earlier sub in the same
        // stack) so tighten the search radius and pixel scale for a faster solve.
        parameters.search_radius = 1;
        lowerPixScale = pixScale * 0.95;
        upperPixScale = pixScale * 1.05;
    }
    parameters.solverTimeLimit = std::min(parameters.solverTimeLimit, 20);

    m_Solver.reset(new SolverUtils(parameters, parameters.solverTimeLimit, SSolver::SOLVE), &QObject::deleteLater);
    connect(m_Solver.get(), &SolverUtils::done, this, &StackController::handleSolveDone, Qt::UniqueConnection);
    m_Solver->useScale(true, lowerPixScale, upperPixScale);
    m_Solver->usePosition(true, ra, dec);
    m_Solver->setHealpix(index, healpix);
    m_Solver->runSolver(m_ImageData, true);
}

void StackController::handleExtractDone(bool timedOut, bool success, const FITSImage::Solution &solution,
        double elapsedSeconds)
{
    Q_UNUSED(solution);
    Q_UNUSED(elapsedSeconds);
    disconnect(m_Solver.get(), &SolverUtils::done, this, &StackController::handleExtractDone);

    if (timedOut || !success)
    {
        // Can't get star details, so nothing to weight by — report a clean failure
        // instead of attempting a solve with meaningless HFR/star-count data.
        m_StackMedianHFR = -1.0;
        m_StackNumStars = 0;
        m_ImageData->solverDone(false, false, m_StackMedianHFR, m_StackNumStars);
        return;
    }

    const QList<FITSImage::Star> &starList = m_Solver->getStarList();
    m_StackNumStars = starList.size();
    m_StackMedianHFR = -1.0;
    if (!starList.isEmpty())
    {
        std::vector<FITSImage::Star> stars(starList.constBegin(), starList.constEnd());
        std::nth_element(stars.begin(), stars.begin() + stars.size() / 2, stars.end(),
                          [](const FITSImage::Star & a, const FITSImage::Star & b)
        {
            return a.HFR < b.HFR;
        });
        m_StackMedianHFR = stars[stars.size() / 2].HFR;
    }

    // Now plate solve, using the same position/scale hint and known index/healpix
    // (if any) the extract phase used.
    runSolve(m_PendingRa, m_PendingDec, m_PendingPixScale, m_PendingIndex, m_PendingHealpix);
}

void StackController::handleSolveDone(bool timedOut, bool success, const FITSImage::Solution &solution,
                                       double elapsedSeconds)
{
    Q_UNUSED(elapsedSeconds);
    disconnect(m_Solver.get(), &SolverUtils::done, this, &StackController::handleSolveDone);

    if (!timedOut && success)
    {
        // Mirrors PlateSolve::subSolverDone(): record the index/healpix used (so the next
        // sub can search tightly instead of blind), and — critically — inject the solved
        // WCS into the header and re-parse it via stackLoadWCS() so m_StackWCSHandle holds
        // the real solution before solverDone() runs. Skipping this leaves the align-master
        // branch of FITSData::solverDone() calling wcssub() on a null/stale WCS handle.
        int indexUsed = -1, healpixUsed = -1;
        m_Solver->getSolutionHealpix(&indexUsed, &healpixUsed);
        m_ImageData->setStackSubSolution(solution.ra, solution.dec, solution.pixscale, indexUsed, healpixUsed);
        const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
        m_ImageData->injectStackWCS(solution.orientation, solution.ra, solution.dec, solution.pixscale,
                                     eastToTheRight);
        m_ImageData->stackLoadWCS();
        m_ImageData->solverDone(false, true, m_StackMedianHFR, m_StackNumStars);
        return;
    }

    if (m_StackExtendedPlateSolve)
    {
        // Already tried with widened criteria — give up on this sub.
        m_ImageData->solverDone(false, false, m_StackMedianHFR, m_StackNumStars);
        return;
    }

    // Failed on tight criteria (we guessed the wrong index/healpix, or the sub
    // genuinely doesn't match) — widen and retry once, using the original
    // position/scale hint (the failed solution carries no usable ra/dec/pixscale).
    m_StackExtendedPlateSolve = true;
    runSolve(m_PendingRa, m_PendingDec, m_PendingPixScale, -1, -1);
}
