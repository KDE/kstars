/*  Ekos Polar Alignment Assistant Tool
    SPDX-FileCopyrightText: 2018-2021 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020-2021 Hy Murveit

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "polaralignmentassistant.h"

#include "align.h"
#include "alignview.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksmessagebox.h"
#include "ekos/auxiliary/stellarsolverprofile.h"
#include "ekos/auxiliary/solverutils.h"
#include "Options.h"
#include "polaralignwidget.h"
#include <ekos_align_debug.h>

#define PAA_VERSION "v3.0"

namespace Ekos
{

const QMap<PolarAlignmentAssistant::Stage, KLocalizedString> PolarAlignmentAssistant::PAHStages =
{
    {PAH_IDLE, ki18n("Idle")},
    {PAH_FIRST_CAPTURE, ki18n("First Capture")},
    {PAH_FIRST_SOLVE, ki18n("First Solve")},
    {PAH_FIND_CP, ki18n("Finding CP")},
    {PAH_FIRST_ROTATE, ki18n("First Rotation")},
    {PAH_FIRST_SETTLE, ki18n("First Settle")},
    {PAH_SECOND_CAPTURE, ki18n("Second Capture")},
    {PAH_SECOND_SOLVE, ki18n("Second Solve")},
    {PAH_SECOND_ROTATE, ki18n("Second Rotation")},
    {PAH_SECOND_SETTLE, ki18n("Second Settle")},
    {PAH_THIRD_CAPTURE, ki18n("Third Capture")},
    {PAH_THIRD_SOLVE, ki18n("Third Solve")},
    {PAH_STAR_SELECT, ki18n("Select Star")},
    {PAH_REFRESH, ki18n("Refreshing")},
    {PAH_POST_REFRESH, ki18n("Refresh Complete")},
};

PolarAlignmentAssistant::PolarAlignmentAssistant(Align *parent, const QSharedPointer<AlignView> &view) : QWidget(parent)
{
    setupUi(this);
    polarAlignWidget = new PolarAlignWidget();
    mainPALayout->insertWidget(0, polarAlignWidget);

    m_AlignInstance = parent;
    m_AlignView = view;

    showUpdatedError((pAHRefreshAlgorithm->currentIndex() == PLATE_SOLVE_ALGORITHM) ||
                     (pAHRefreshAlgorithm->currentIndex() == MOVE_STAR_UPDATE_ERR_ALGORITHM));

    connect(pAHRefreshAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        setPAHRefreshAlgorithm(static_cast<RefreshAlgorithm>(index));
    });
    starCorrespondencePAH.reset();

    // PAH Connections
    PAHWidgets->setCurrentWidget(PAHIntroPage);
    connect(this, &PolarAlignmentAssistant::PAHEnabled, [&](bool enabled)
    {
        PAHStartB->setEnabled(enabled);
        directionLabel->setEnabled(enabled);
        pAHDirection->setEnabled(enabled);
        pAHRotation->setEnabled(enabled);
        pAHMountSpeed->setEnabled(enabled);
        pAHManualSlew->setEnabled(enabled);
    });
    connect(PAHStartB, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::startPAHProcess);
    // PAH StopB is just a shortcut for the regular stop
    connect(PAHStopB, &QPushButton::clicked, this, &PolarAlignmentAssistant::stopPAHProcess);
    connect(PAHRefreshB, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::startPAHRefreshProcess);
    // done button for manual slewing during polar alignment:
    connect(PAHManualDone, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::setPAHSlewDone);

    hemisphere = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? NORTH_HEMISPHERE : SOUTH_HEMISPHERE;

    label_PAHOrigErrorAz->setText("Az: ");
    label_PAHOrigErrorAlt->setText("Alt: ");
}

PolarAlignmentAssistant::~PolarAlignmentAssistant()
{
}

void PolarAlignmentAssistant::showUpdatedError(bool show)
{
    label_PAHUpdatedErrorTotal->setVisible(show);
    PAHUpdatedErrorTotal->setVisible(show);
    label_PAHUpdatedErrorAlt->setVisible(show);
    PAHUpdatedErrorAlt->setVisible(show);
    label_PAHUpdatedErrorAz->setVisible(show);
    PAHUpdatedErrorAz->setVisible(show);
}

void PolarAlignmentAssistant::syncMountSpeed(const QString &speed)
{
    pAHMountSpeed->blockSignals(true);
    pAHMountSpeed->clear();
    pAHMountSpeed->addItems(m_CurrentTelescope->slewRates());
    pAHMountSpeed->setCurrentText(speed);
    pAHMountSpeed->blockSignals(false);
}

void PolarAlignmentAssistant::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);

    emit PAHEnabled(enabled);
    if (enabled)
    {
        PAHWidgets->setToolTip(QString());
        FOVDisabledLabel->hide();
    }
    else
    {
        PAHWidgets->setToolTip(i18n("<p>Polar Alignment tool requires a German Equatorial Mount.</p>"));
        FOVDisabledLabel->show();
    }

}

// This solver is only used by the refresh plate-solving scheme.
void PolarAlignmentAssistant::startSolver()
{
    auto profiles = getDefaultAlignOptionsProfiles();
    SSolver::Parameters parameters;
    // Get solver parameters
    // In case of exception, use first profile
    try
    {
        parameters = profiles.at(Options::solveOptionsProfile());
    }
    catch (std::out_of_range const &)
    {
        parameters = profiles[0];
    }

    // Double search radius
    parameters.search_radius = parameters.search_radius * 2;
    constexpr double solverTimeout = 10.0;

    m_Solver.reset(new SolverUtils(parameters, solverTimeout), &QObject::deleteLater);
    connect(m_Solver.get(), &SolverUtils::done, this, &PolarAlignmentAssistant::solverDone, Qt::UniqueConnection);

    // Use the scale and position from the most recent solve.
    m_Solver->useScale(Options::astrometryUseImageScale(), m_LastPixscale * 0.9, m_LastPixscale * 1.1);
    m_Solver->usePosition(true, m_LastRa, m_LastDec);
    m_Solver->setHealpix(m_IndexToUse, m_HealpixToUse);
    m_Solver->runSolver(m_ImageData);
}

void PolarAlignmentAssistant::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution,
        double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &PolarAlignmentAssistant::solverDone);

    if (m_PAHStage != PAH_REFRESH)
        return;

    if (timedOut || !success)
    {
        // I'm torn between 1 and 2 for this constant.
        // 1: If a solve failed, open up the search next time.
        // 2: A common reason for a solve to fail is because a knob was adjusted during the exposure.
        //    Let it try one more time.
        constexpr int MAX_NUM_HEALPIX_FAILURES = 2;
        if (++m_NumHealpixFailures >= MAX_NUM_HEALPIX_FAILURES)
        {
            // Don't use the previous index and healpix next time we solve.
            m_IndexToUse = -1;
            m_HealpixToUse = -1;
        }
    }
    else
    {
        // Get the index and healpix from the successful solve.
        m_Solver->getSolutionHealpix(&m_IndexToUse, &m_HealpixToUse);
    }

    if (timedOut)
    {
        emit newLog(i18n("Refresh solver timed out: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1)));
        emit captureAndSolve();
    }
    else if (!success)
    {
        emit newLog(i18n("Refresh solver failed: %1s", QString("%L1").arg(elapsedSeconds, 0, 'f', 1)));
        emit updatedErrorsChanged(-1, -1, -1);
        emit captureAndSolve();
    }
    else
    {
        m_NumHealpixFailures = 0;
        refreshIteration++;
        const double ra = solution.ra;
        const double dec = solution.dec;
        m_LastRa = solution.ra;
        m_LastDec = solution.dec;
        m_LastOrientation = solution.orientation;
        m_LastPixscale = solution.pixscale;

        emit newLog(QString("Refresh solver success %1s: ra %2 dec %3 scale %4")
                    .arg(elapsedSeconds, 0, 'f', 1).arg(ra, 0, 'f', 3)
                    .arg(dec, 0, 'f', 3).arg(solution.pixscale));

        // RA is input in hours, not degrees!
        SkyPoint refreshCoords(ra / 15.0, dec);
        double azError = 0, altError = 0;
        if (polarAlign.processRefreshCoords(refreshCoords, m_ImageData->getDateTime(), &azError, &altError))
        {
            updateRefreshDisplay(azError, altError);

            const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
            // The 2nd false means don't block. The code below doesn't work if we block
            // because wcsToPixel in updateTriangle() depends on the injectWCS being finished.
            m_AlignView->injectWCS(solution.orientation, ra, dec, solution.pixscale, eastToTheRight, false);
            updatePlateSolveTriangle(m_ImageData);
        }
        else
            emit newLog(QString("Could not estimate mount rotation"));
    }
    // Start the next refresh capture.
    emit captureAndSolve();
}

void PolarAlignmentAssistant::updatePlateSolveTriangle(const QSharedPointer<FITSData> &image)
{
    if (image.isNull())
        return;

    // To render the triangle for the plate-solve refresh, we need image coordinates for
    // - the original (3rd image) center point (originalPoint)
    // - the solution point (solutionPoint),
    // - the alt-only solution point (altOnlyPoint),
    // - the current center of the image (centerPixel),
    // The triangle is made from the first 3 above.
    const SkyPoint &originalCoords = polarAlign.getPoint(2);
    QPointF originalPixel, solutionPixel, altOnlyPixel, dummy;
    QPointF centerPixel(image->width() / 2, image->height() / 2);
    if (image->wcsToPixel(originalCoords, originalPixel, dummy) &&
            image->wcsToPixel(refreshSolution, solutionPixel, dummy) &&
            image->wcsToPixel(altOnlyRefreshSolution, altOnlyPixel, dummy))
    {
        m_AlignView->setCorrectionParams(originalPixel, solutionPixel, altOnlyPixel);
        m_AlignView->setStarCircle(centerPixel);
    }
    else
    {
        qCDebug(KSTARS_EKOS_ALIGN) << "wcs failed";
    }
}

namespace
{

// These functions paint up/down/left/right-pointing arrows in a box of size w x h.

void upArrow(QPainter *painter, int w, int h)
{
    const double wCenter = w / 2, lineTop = 0.38, lineBottom = 0.9;
    const double lineLength = h * (lineBottom - lineTop);
    painter->drawRect(wCenter - w * .1, h * lineTop, w * .2, lineLength);
    QPolygonF arrowHead;
    arrowHead << QPointF(wCenter, h * .1) << QPointF(w * .2, h * .4) << QPointF(w * .8, h * .4);
    painter->drawConvexPolygon(arrowHead);
}
void downArrow(QPainter *painter, int w, int h)
{
    const double wCenter = w / 2, lineBottom = 0.62, lineTop = 0.1;
    const double lineLength = h * (lineBottom - lineTop);
    painter->drawRect(wCenter - w * .1, h * lineTop, w * .2, lineLength);
    QPolygonF arrowHead;
    arrowHead << QPointF(wCenter, h * .9) << QPointF(w * .2, h * .6) << QPointF(w * .8, h * .6);
    painter->drawConvexPolygon(arrowHead);
}
void leftArrow(QPainter *painter, int w, int h)
{
    const double hCenter = h / 2, lineLeft = 0.38, lineRight = 0.9;
    const double lineLength = w * (lineRight - lineLeft);
    painter->drawRect(h * lineLeft, hCenter - h * .1, lineLength, h * .2);
    QPolygonF arrowHead;
    arrowHead << QPointF(w * .1, hCenter) << QPointF(w * .4, h * .2) << QPointF(w * .4, h * .8);
    painter->drawConvexPolygon(arrowHead);
}
void rightArrow(QPainter *painter, int w, int h)
{
    const double hCenter = h / 2, lineLeft = .1, lineRight = .62;
    const double lineLength = w * (lineRight - lineLeft);
    painter->drawRect(h * lineLeft, hCenter - h * .1, lineLength, h * .2);
    QPolygonF arrowHead;
    arrowHead << QPointF(w * .9, hCenter) << QPointF(w * .6, h * .2) << QPointF(w * .6, h * .8);
    painter->drawConvexPolygon(arrowHead);
}
}  // namespace

// Draw arrows that give the user a hint of how he/she needs to adjust the azimuth and altitude
// knobs. The arrows' size changes depending on the azimuth and altitude error.
void PolarAlignmentAssistant::drawArrows(double azError, double altError)
{
    constexpr double minError = 20.0 / 3600.0;  // 20 arcsec
    double absError = fabs(altError);

    // these constants are worked out so a 10' error gives a size of 50
    // and a 1' error gives a size of 20.
    constexpr double largeErr = 10.0 / 60.0, smallErr = 1.0 / 60.0, largeSize = 50, smallSize = 20, c1 = 200, c2 = 100 / 6;

    int size = 0;
    if (absError > largeErr)
        size = largeSize;
    else if (absError < smallErr)
        size = smallSize;
    else size = absError * c1 + c2;

    QPixmap altPixmap(size, size);
    altPixmap.fill(QColor("transparent"));
    QPainter altPainter(&altPixmap);
    altPainter.setBrush(arrowAlt->palette().color(QPalette::WindowText));

    if (altError > minError)
        downArrow(&altPainter, size, size);
    else if (altError < -minError)
        upArrow(&altPainter, size, size);
    arrowAlt->setPixmap(altPixmap);

    absError = fabs(azError);
    if (absError > largeErr)
        size = largeSize;
    else if (absError < smallErr)
        size = smallSize;
    else size = absError * c1 + c2;

    QPixmap azPixmap(size, size);
    azPixmap.fill(QColor("transparent"));
    QPainter azPainter(&azPixmap);
    azPainter.setBrush(arrowAz->palette().color(QPalette::WindowText));

    if (azError > minError)
        leftArrow(&azPainter, size, size);
    else if (azError < -minError)
        rightArrow(&azPainter, size, size);
    arrowAz->setPixmap(azPixmap);
}

bool PolarAlignmentAssistant::detectStarsPAHRefresh(QList<Edge> *stars, int num, int x, int y, int *starIndex)
{
    stars->clear();
    *starIndex = -1;

    // Use the solver settings from the align tab for for "polar-align refresh" star detection.
    QVariantMap settings;
    settings["optionsProfileIndex"] = Options::solveOptionsProfile();
    settings["optionsProfileGroup"] = static_cast<int>(Ekos::AlignProfiles);
    m_ImageData->setSourceExtractorSettings(settings);

    QElapsedTimer timer;
    m_ImageData->findStars(ALGORITHM_SEP).waitForFinished();

    QString debugString = QString("PAA Refresh: Detected %1 stars (%2s)")
                          .arg(m_ImageData->getStarCenters().size()).arg(timer.elapsed() / 1000.0, 5, 'f', 3);
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;

    QList<Edge *> detectedStars = m_ImageData->getStarCenters();
    // Let's sort detectedStars by flux, starting with widest
    std::sort(detectedStars.begin(), detectedStars.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->sum > edge2->sum;});

    // Find the closest star to the x,y position, which is where the user clicked on the alignView.
    double bestDist = 1e9;
    int bestIndex = -1;
    for (int i = 0; i < detectedStars.size(); i++)
    {
        double dx = detectedStars[i]->x - x;
        double dy = detectedStars[i]->y - y;
        double dist = dx * dx + dy * dy;
        if (dist < bestDist)
        {
            bestDist = dist;
            bestIndex = i;
        }
    }

    int starCount = qMin(num, detectedStars.count());
    for (int i = 0; i < starCount; i++)
        stars->append(*(detectedStars[i]));
    if (bestIndex >= starCount)
    {
        // If we found the star, but requested 'num' stars, and the user's star
        // is lower down in the list, add it and return num+1 stars.
        stars->append(*(detectedStars[bestIndex]));
        *starIndex = starCount;
    }
    else
    {
        *starIndex = bestIndex;
    }
    debugString = QString("PAA Refresh: User's star(%1,%2) is index %3").arg(x).arg(y).arg(*starIndex);
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;

    detectedStars.clear();

    return stars->count();
}

void PolarAlignmentAssistant::updateRefreshDisplay(double azE, double altE)
{
    drawArrows(azE, altE);
    const double errDegrees = hypot(azE, altE);
    dms totalError(errDegrees), azError(azE), altError(altE);
    PAHUpdatedErrorTotal->setText(totalError.toDMSString());
    PAHUpdatedErrorAlt->setText(altError.toDMSString());
    PAHUpdatedErrorAz->setText(azError.toDMSString());

    QString debugString = QString("PAA Refresh(%1): Corrected az: %2 alt: %4 total: %6")
                          .arg(refreshIteration).arg(azError.toDMSString())
                          .arg(altError.toDMSString()).arg(totalError.toDMSString());
    emit newLog(debugString);
    emit updatedErrorsChanged(totalError.Degrees(), azError.Degrees(), altError.Degrees());
}

void PolarAlignmentAssistant::processPAHRefresh()
{
    m_AlignView->setStarCircle();
    PAHUpdatedErrorTotal->clear();
    PAHIteration->clear();
    PAHUpdatedErrorAlt->clear();
    PAHUpdatedErrorAz->clear();
    QString debugString;
    imageNumber++;
    PAHIteration->setText(QString("Image %1").arg(imageNumber));

    if (pAHRefreshAlgorithm->currentIndex() == PLATE_SOLVE_ALGORITHM)
    {
        startSolver();
        return;
    }

    // Always run on the initial iteration to setup the user's star,
    // so that if it is enabled later the star could be tracked.
    // Flaw here is that if enough stars are not detected, iteration is not incremented,
    // so it may repeat.
    if ((pAHRefreshAlgorithm->currentIndex() == MOVE_STAR_UPDATE_ERR_ALGORITHM) || (refreshIteration == 0))
    {
        constexpr int MIN_PAH_REFRESH_STARS = 10;

        QList<Edge> stars;
        // Index of user's star in the detected stars list. In the first iteration
        // the stars haven't moved and we can just use the location of the click.
        // Later we'll need to find the star with starCorrespondence.
        int clickedStarIndex;
        detectStarsPAHRefresh(&stars, 100, correctionFrom.x(), correctionFrom.y(), &clickedStarIndex);
        if (clickedStarIndex < 0)
        {
            debugString = QString("PAA Refresh(%1): Didn't find the clicked star near %2,%3")
                          .arg(refreshIteration).arg(correctionFrom.x()).arg(correctionFrom.y());
            qCDebug(KSTARS_EKOS_ALIGN) << debugString;

            emit newAlignTableResult(Align::ALIGN_RESULT_FAILED);
            emit captureAndSolve();
            return;
        }

        debugString = QString("PAA Refresh(%1): Refresh star(%2,%3) is index %4 with offset %5 %6")
                      .arg(refreshIteration + 1).arg(correctionFrom.x(), 4, 'f', 0)
                      .arg(correctionFrom.y(), 4, 'f', 0).arg(clickedStarIndex)
                      .arg(stars[clickedStarIndex].x - correctionFrom.x(), 4, 'f', 0)
                      .arg(stars[clickedStarIndex].y - correctionFrom.y(), 4, 'f', 0);
        qCDebug(KSTARS_EKOS_ALIGN) << debugString;

        if (stars.size() > MIN_PAH_REFRESH_STARS)
        {
            int dx = 0;
            int dy = 0;
            int starIndex = -1;

            if (refreshIteration++ == 0)
            {
                // First iteration. Setup starCorrespondence so we can find the user's star.
                // clickedStarIndex should be the index of a detected star near where the user clicked.
                starCorrespondencePAH.initialize(stars, clickedStarIndex);
                if (clickedStarIndex >= 0)
                {
                    setupCorrectionGraphics(QPointF(stars[clickedStarIndex].x, stars[clickedStarIndex].y));
                    emit newCorrectionVector(QLineF(correctionFrom, correctionTo));
                    emit newFrame(m_AlignView);
                }
            }
            else
            {
                // Or, in other iterations find the movement of the "user's star".
                // The 0.40 means it's OK if star correspondence only finds 40% of the
                // reference stars (as we'd have more issues near the image edge otherwise).
                QVector<int> starMap;
                starCorrespondencePAH.find(stars, 200.0, &starMap, false, 0.40);

                // Go through the starMap, and find the user's star, and compare its position
                // to its initial position.
                for (int i = 0; i < starMap.size(); ++i)
                {
                    if (starMap[i] == starCorrespondencePAH.guideStar())
                    {
                        dx = stars[i].x - correctionFrom.x();
                        dy = stars[i].y - correctionFrom.y();
                        starIndex = i;
                        break;
                    }
                }
                if (starIndex == -1)
                {
                    bool allOnes = true;
                    for (int i = 0; i < starMap.size(); ++i)
                    {
                        if (starMap[i] != -1)
                            allOnes = false;
                    }
                    debugString = QString("PAA Refresh(%1): starMap %2").arg(refreshIteration).arg(allOnes ? "ALL -1's" : "not all -1's");
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
            }

            if (starIndex >= 0)
            {
                // Annotate the user's star on the alignview.
                m_AlignView->setStarCircle(QPointF(stars[starIndex].x, stars[starIndex].y));
                debugString = QString("PAA Refresh(%1): User's star is now at %2,%3, with movement = %4,%5").arg(refreshIteration)
                              .arg(stars[starIndex].x, 4, 'f', 0).arg(stars[starIndex].y, 4, 'f', 0).arg(dx).arg(dy);
                qCDebug(KSTARS_EKOS_ALIGN) << debugString;

                double azE, altE;
                if (polarAlign.pixelError(m_AlignView->keptImage(), QPointF(stars[starIndex].x, stars[starIndex].y),
                                          correctionTo, &azE, &altE))
                {
                    updateRefreshDisplay(azE, altE);
                    debugString = QString("PAA Refresh(%1): %2,%3 --> %4,%5 @ %6,%7")
                                  .arg(refreshIteration).arg(correctionFrom.x(), 4, 'f', 0).arg(correctionFrom.y(), 4, 'f', 0)
                                  .arg(correctionTo.x(), 4, 'f', 0).arg(correctionTo.y(), 4, 'f', 0)
                                  .arg(stars[starIndex].x, 4, 'f', 0).arg(stars[starIndex].y, 4, 'f', 0);
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
                else
                {
                    debugString = QString("PAA Refresh(%1): pixelError failed to estimate the remaining correction").arg(refreshIteration);
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
            }
            else
            {
                if (refreshIteration > 1)
                {
                    debugString = QString("PAA Refresh(%1): Didn't find the user's star").arg(refreshIteration);
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
            }
        }
        else
        {
            debugString = QString("PAA Refresh(%1): Too few stars detected (%2)").arg(refreshIteration).arg(stars.size());
            qCDebug(KSTARS_EKOS_ALIGN) << debugString;
            emit updatedErrorsChanged(-1, -1, -1);
        }
    }
    // Finally start the next capture
    emit captureAndSolve();
}

bool PolarAlignmentAssistant::processSolverFailure()
{
    if ((m_PAHStage == PAH_FIRST_CAPTURE ||
            m_PAHStage == PAH_SECOND_CAPTURE ||
            m_PAHStage == PAH_THIRD_CAPTURE ||
            m_PAHStage == PAH_FIRST_SOLVE ||
            m_PAHStage == PAH_SECOND_SOLVE ||
            m_PAHStage == PAH_THIRD_SOLVE)
            && ++m_PAHRetrySolveCounter < 4)
    {
        emit newLog(i18n("PAA: Solver failed, retrying."));
        emit captureAndSolve();
        return true;
    }

    if (m_PAHStage != PAH_IDLE)
    {
        emit newLog(i18n("PAA: Stopping, solver failed too many times."));
        stopPAHProcess();
    }

    return false;
}

void PolarAlignmentAssistant::setPAHStage(Stage stage)
{
    if (stage != m_PAHStage)
    {
        m_PAHStage = stage;
        polarAlignWidget->updatePAHStage(stage);
        emit newPAHStage(m_PAHStage);
    }
}

void PolarAlignmentAssistant::processMountRotation(const dms &ra, double settleDuration)
{
    // Check how many degrees travelled so far from starting point
    double traveledAngle = ra.deltaAngle(m_StartCoord.ra()).Degrees();
    bool goingWest = pAHDirection->currentIndex() == 0;

    // Handle RA wrap-around
    // When going west: If current RA is much larger than start RA, we crossed the boundary
    // When going east: If current RA is much smaller than start RA, we crossed the boundary
    if (goingWest && (ra.Hours() - m_StartCoord.ra().Hours() > 12))
        traveledAngle = (ra.Hours() - 24 - m_StartCoord.ra().Hours()) * 15;
    else if (!goingWest && (m_StartCoord.ra().Hours() - ra.Hours() > 12))
        traveledAngle = ((ra.Hours() + 24) - m_StartCoord.ra().Hours()) * 15;

    QString rotProgressMessage;
    QString rotDoneMessage;
    Stage nextCapture;
    Stage nextSettle;

    if (m_PAHStage == PAH_FIRST_ROTATE)
    {
        rotProgressMessage = "First mount rotation completed degrees:";
        rotDoneMessage = i18n("Mount first rotation is complete.");
        nextCapture = PAH_SECOND_CAPTURE;
        nextSettle = PAH_FIRST_SETTLE;
    }
    else if (m_PAHStage == PAH_SECOND_ROTATE)
    {
        rotProgressMessage = "Second mount rotation completed degrees:";
        rotDoneMessage = i18n("Mount second rotation is complete.");
        nextCapture = PAH_THIRD_CAPTURE;
        nextSettle = PAH_SECOND_SETTLE;
    }
    else return;

    auto settle = [this, rotDoneMessage, settleDuration, nextCapture, nextSettle]()
    {
        m_CurrentTelescope->StopWE();
        emit newLog(rotDoneMessage);

        int settleDurationMsec = settleDuration;

        // Always settle at least a second!
        // It is not obvious to users that the align settle field is also used for polar alignment.
        if (settleDurationMsec <= 1000)
            settleDurationMsec = 2000;

        setPAHStage(nextSettle);
        updateDisplay(m_PAHStage, getPAHMessage());

        emit newLog(i18n("Settling..."));
        QTimer::singleShot(settleDurationMsec, [nextCapture, this]()
        {
            setPAHStage(nextCapture);
            updateDisplay(m_PAHStage, getPAHMessage());
        });
    };

    if (m_PAHStage == PAH_FIRST_ROTATE || m_PAHStage == PAH_SECOND_ROTATE)
    {
        // only wait for telescope to slew to new position if manual slewing is switched off
        if(!pAHManualSlew->isChecked())
        {
            qCDebug(KSTARS_EKOS_ALIGN) << rotProgressMessage << traveledAngle;

            // Allow for some travel angle to get started first
            if (std::abs(traveledAngle) > 0.5)
            {
                bool goingWest = pAHDirection->currentIndex() == 0;
                // traveledAngle < 0 --> Going West
                // traveledAngle > 0 --> Going East
                // #1 Check for reversed direction
                if ( (traveledAngle < 0 && !goingWest) || (traveledAngle > 0 && goingWest))
                {
                    m_CurrentTelescope->abort();
                    emit newLog(i18n("Mount aborted. Reverse RA axis direction and try again."));
                    stopPAHProcess();
                }
                // #2 Check if we travelled almost 90% of the desired rotation or more
                // then stop
                else if (std::abs(traveledAngle) > pAHRotation->value() * 0.90)
                    settle();
            }
        }
    }
}

bool PolarAlignmentAssistant::checkPAHForMeridianCrossing()
{
    // Make sure using -180 to 180 for hourAngle and DEC. (Yes dec should be between -90 and 90).
    double hourAngle = m_CurrentTelescope->hourAngle().Degrees();
    while (hourAngle < -180)
        hourAngle += 360;
    while (hourAngle > 180)
        hourAngle -= 360;
    double ra = 0, dec = 0;
    m_CurrentTelescope->getEqCoords(&ra, &dec);
    while (dec < -180)
        dec += 360;
    while (dec > 180)
        dec -= 360;

    // Don't do this check within 2 degrees of the poles.
    bool nearThePole = fabs(dec) > 88;
    if (nearThePole)
        return false;

    double degreesPerSlew = pAHRotation->value();
    bool closeToMeridian = fabs(hourAngle) < 2.0 * degreesPerSlew;
    bool goingWest = pAHDirection->currentIndex() == 0;

    // If the pier is on the east side (pointing west) and will slew west and is within 2 slews of the HA=0,
    // or on the west side (pointing east) and will slew east, and is within 2 slews of HA=0
    // then warn and give the user a chance to cancel.
    bool wouldCrossMeridian =
        ((m_CurrentTelescope->pierSide() == ISD::Mount::PIER_EAST && !goingWest && closeToMeridian) ||
         (m_CurrentTelescope->pierSide() == ISD::Mount::PIER_WEST && goingWest && closeToMeridian) ||
         (m_CurrentTelescope->pierSide() == ISD::Mount::PIER_UNKNOWN && closeToMeridian));

    return wouldCrossMeridian;
}

void PolarAlignmentAssistant::updateDisplay(Stage stage, const QString &message)
{
    switch(stage)
    {
        case PAH_FIRST_ROTATE:
        case PAH_SECOND_ROTATE:
            if (pAHManualSlew->isChecked())
            {
                polarAlignWidget->updatePAHStage(stage);
                PAHWidgets->setCurrentWidget(PAHManualRotatePage);
                manualRotateText->setText(message);
                emit newPAHMessage(message);
                return;
            }
        // fall through
        case PAH_FIRST_CAPTURE:
        case PAH_SECOND_CAPTURE:
        case PAH_THIRD_CAPTURE:
        case PAH_FIRST_SOLVE:
        case PAH_SECOND_SOLVE:
        case PAH_THIRD_SOLVE:
            polarAlignWidget->updatePAHStage(stage);
            PAHWidgets->setCurrentWidget(PAHMessagePage);
            PAHMessageText->setText(message);
            emit newPAHMessage(message);
            break;

        default:
            break;

    }
}

void PolarAlignmentAssistant::startPAHProcess()
{
    qCInfo(KSTARS_EKOS_ALIGN) << QString("Starting Polar Alignment Assistant process %1 ...").arg(PAA_VERSION);

    auto executePAH = [ this ]()
    {
        setPAHStage(PAH_FIRST_CAPTURE);

        if (Options::limitedResourcesMode())
            emit newLog(i18n("Warning: Equatorial Grid Lines will not be drawn due to limited resources mode."));

        if (m_CurrentTelescope->hasAlignmentModel())
        {
            emit newLog(i18n("Clearing mount Alignment Model..."));
            m_CurrentTelescope->clearAlignmentModel();
        }

        // Unpark
        m_CurrentTelescope->unpark();

        // Set tracking ON if not already
        if (m_CurrentTelescope->canControlTrack() && m_CurrentTelescope->isTracking() == false)
            m_CurrentTelescope->setTrackEnabled(true);

        PAHStartB->setEnabled(false);
        PAHStopB->setEnabled(true);

        PAHUpdatedErrorTotal->clear();
        PAHUpdatedErrorAlt->clear();
        PAHUpdatedErrorAz->clear();
        PAHOrigErrorTotal->clear();
        PAHOrigErrorAlt->clear();
        PAHOrigErrorAz->clear();
        PAHIteration->setText("");

        updateDisplay(m_PAHStage, getPAHMessage());

        m_AlignView->setCorrectionParams(QPointF(), QPointF(), QPointF());

        m_PAHRetrySolveCounter = 0;
        emit captureAndSolve();
    };

    // Right off the bat, check if this alignment might cause a pier crash.
    // If we're crossing the meridian, warn unless within 5-degrees from the pole.
    if (checkPAHForMeridianCrossing())
    {
        // Continue
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executePAH]()
        {
            KSMessageBox::Instance()->disconnect(this);
            executePAH();
        });

        KSMessageBox::Instance()->warningContinueCancel(i18n("This could cause the telescope to cross the meridian."),
                i18n("Warning"), 15);
    }
    else
        executePAH();
}

void PolarAlignmentAssistant::stopPAHProcess()
{
    if (m_PAHStage == PAH_IDLE)
        return;

    // Only display dialog if user clicked.
    //    if ((static_cast<QPushButton *>(sender()) == PAHStopB) && KMessageBox::questionYesNo(KStars::Instance(),
    //            i18n("Are you sure you want to stop the polar alignment process?"),
    //            i18n("Polar Alignment Assistant"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
    //            "restart_PAA_process_dialog") == KMessageBox::No)
    //        return;

    if (m_PAHStage == PAH_REFRESH)
    {
        setPAHStage(PAH_POST_REFRESH);
        polarAlignWidget->updatePAHStage(m_PAHStage);
    }
    qCInfo(KSTARS_EKOS_ALIGN) << "Stopping Polar Alignment Assistant process...";


    if (m_CurrentTelescope && m_CurrentTelescope->isInMotion())
        m_CurrentTelescope->abort();

    setPAHStage(PAH_IDLE);
    polarAlignWidget->updatePAHStage(m_PAHStage);

    PAHStartB->setEnabled(true);
    PAHStopB->setEnabled(false);
    PAHRefreshB->setEnabled(true);
    PAHWidgets->setCurrentWidget(PAHIntroPage);
    emit newPAHMessage(introText->text());

    m_AlignView->reset();
    m_AlignView->setRefreshEnabled(false);

    emit newFrame(m_AlignView);
    disconnect(m_AlignView.get(), &AlignView::trackingStarSelected, this,
               &Ekos::PolarAlignmentAssistant::setPAHCorrectionOffset);
    disconnect(m_AlignView.get(), &AlignView::newCorrectionVector, this, &Ekos::PolarAlignmentAssistant::newCorrectionVector);

    if (Options::pAHAutoPark())
    {
        m_CurrentTelescope->park();
        emit newLog(i18n("Parking the mount..."));
    }
}

void PolarAlignmentAssistant::rotatePAH()
{
    double TargetDiffRA = pAHRotation->value();
    bool westMeridian = pAHDirection->currentIndex() == 0;

    // West
    if (westMeridian)
        TargetDiffRA *= -1;
    // East
    else
        TargetDiffRA *= 1;

    // JM 2018-05-03: Hemispheres shouldn't affect rotation direction in RA

    // if Manual slewing is selected, don't move the mount
    if (pAHManualSlew->isChecked())
    {
        return;
    }

    const SkyPoint telescopeCoord = m_CurrentTelescope->currentCoordinates();

    // Record starting point
    m_StartCoord = telescopeCoord;

    // TargetDiffRA is in degrees
    dms newTelescopeRA = (telescopeCoord.ra() + dms(TargetDiffRA)).reduce();

    targetPAH.setRA(newTelescopeRA);
    targetPAH.setDec(telescopeCoord.dec());

    //m_CurrentTelescope->Slew(&targetPAH);
    // Set Selected Speed
    if (pAHMountSpeed->currentIndex() >= 0)
        m_CurrentTelescope->setSlewRate(pAHMountSpeed->currentIndex());
    // Go to direction
    m_CurrentTelescope->MoveWE(westMeridian ? ISD::Mount::MOTION_WEST : ISD::Mount::MOTION_EAST,
                               ISD::Mount::MOTION_START);

    emit newLog(i18n("Please wait until mount completes rotating to RA (%1) DE (%2)", targetPAH.ra().toHMSString(),
                     targetPAH.dec().toDMSString()));
}

void PolarAlignmentAssistant::setupCorrectionGraphics(const QPointF &pixel)
{
    polarAlign.refreshSolution(&refreshSolution, &altOnlyRefreshSolution);

    // We use the previously stored image (the 3rd PAA image)
    // so we can continue to estimate the correction even after
    // capturing new images during the refresh stage.
    const QSharedPointer<FITSData> &imageData = m_AlignView->keptImage();

    // Just the altitude correction
    if (!polarAlign.findCorrectedPixel(imageData, pixel, &correctionAltTo, true))
    {
        qCInfo(KSTARS_EKOS_ALIGN) << QString(i18n("PAA: Failed to findCorrectedPixel."));
        return;
    }
    // The whole correction.
    if (!polarAlign.findCorrectedPixel(imageData, pixel, &correctionTo))
    {
        qCInfo(KSTARS_EKOS_ALIGN) << QString(i18n("PAA: Failed to findCorrectedPixel."));
        return;
    }
    QString debugString = QString("PAA: Correction: %1,%2 --> %3,%4 (alt only %5,%6)")
                          .arg(pixel.x(), 4, 'f', 0).arg(pixel.y(), 4, 'f', 0)
                          .arg(correctionTo.x(), 4, 'f', 0).arg(correctionTo.y(), 4, 'f', 0)
                          .arg(correctionAltTo.x(), 4, 'f', 0).arg(correctionAltTo.y(), 4, 'f', 0);
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
    correctionFrom = pixel;

    if (pAHRefreshAlgorithm->currentIndex() == PLATE_SOLVE_ALGORITHM)
        updatePlateSolveTriangle(imageData);
    else
        m_AlignView->setCorrectionParams(correctionFrom, correctionTo, correctionAltTo);

    return;
}


bool PolarAlignmentAssistant::calculatePAHError()
{
    // Hold on to the imageData so we can use it during the refresh phase.
    m_AlignView->holdOnToImage();

    if (!polarAlign.findAxis())
    {
        emit newLog(i18n("PAA: Failed to find RA Axis center."));
        stopPAHProcess();
        return false;
    }

    double azimuthError, altitudeError;
    polarAlign.calculateAzAltError(&azimuthError, &altitudeError);
    drawArrows(azimuthError, altitudeError);
    dms polarError(hypot(altitudeError, azimuthError));
    dms azError(azimuthError), altError(altitudeError);

    // No need to toggle EQGrid
    //    if (m_AlignView->isEQGridShown() == false && !Options::limitedResourcesMode())
    //        m_AlignView->toggleEQGrid();

    QString msg = QString("%1. Azimuth: %2  Altitude: %3")
                  .arg(polarError.toDMSString()).arg(azError.toDMSString())
                  .arg(altError.toDMSString());
    emit newLog(QString("Polar Alignment Error: %1").arg(msg));

    polarAlign.setMaxPixelSearchRange(polarError.Degrees() + 1);

    // These are viewed during the refresh phase.
    PAHOrigErrorTotal->setText(polarError.toDMSString());
    PAHOrigErrorAlt->setText(altError.toDMSString());
    PAHOrigErrorAz->setText(azError.toDMSString());

    setupCorrectionGraphics(QPointF(m_ImageData->width() / 2, m_ImageData->height() / 2));

    // Find Celestial pole location and mount's RA axis
    SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);
    QPointF imagePoint, celestialPolePoint;
    m_ImageData->wcsToPixel(CP, celestialPolePoint, imagePoint);
    if (m_ImageData->contains(celestialPolePoint))
    {
        m_AlignView->setCelestialPole(celestialPolePoint);
        QPointF raAxis;
        if (polarAlign.findCorrectedPixel(m_ImageData, celestialPolePoint, &raAxis))
            m_AlignView->setRaAxis(raAxis);
    }

    connect(m_AlignView.get(), &AlignView::trackingStarSelected, this, &Ekos::PolarAlignmentAssistant::setPAHCorrectionOffset);
    emit polarResultUpdated(QLineF(correctionFrom, correctionTo), polarError.Degrees(), azError.Degrees(), altError.Degrees());

    connect(m_AlignView.get(), &AlignView::newCorrectionVector, this, &Ekos::PolarAlignmentAssistant::newCorrectionVector,
            Qt::UniqueConnection);
    syncCorrectionVector();
    emit newFrame(m_AlignView);

    return true;
}

void PolarAlignmentAssistant::syncCorrectionVector()
{
    if (pAHRefreshAlgorithm->currentIndex() == PLATE_SOLVE_ALGORITHM)
        return;
    emit newCorrectionVector(QLineF(correctionFrom, correctionTo));
    m_AlignView->setCorrectionParams(correctionFrom, correctionTo, correctionAltTo);
}

void PolarAlignmentAssistant::setPAHCorrectionOffsetPercentage(double dx, double dy)
{
    double x = dx * m_AlignView->zoomedWidth();
    double y = dy * m_AlignView->zoomedHeight();
    setPAHCorrectionOffset(static_cast<int>(round(x)), static_cast<int>(round(y)));
}

void PolarAlignmentAssistant::setPAHCorrectionOffset(int x, int y)
{
    if (m_PAHStage == PAH_REFRESH)
    {
        emit newLog(i18n("Polar-alignment star cannot be updated during refresh phase as it might affect error measurements."));
    }
    else
    {
        setupCorrectionGraphics(QPointF(x, y));
        emit newCorrectionVector(QLineF(correctionFrom, correctionTo));
        emit newFrame(m_AlignView);
    }
}

void PolarAlignmentAssistant::setPAHSlewDone()
{
    emit newPAHMessage("Manual slew done.");
    switch(m_PAHStage)
    {
        case PAH_FIRST_ROTATE :
            setPAHStage(PAH_SECOND_CAPTURE);
            emit newLog(i18n("First manual rotation done."));
            updateDisplay(m_PAHStage, getPAHMessage());
            break;
        case PAH_SECOND_ROTATE :
            setPAHStage(PAH_THIRD_CAPTURE);
            emit newLog(i18n("Second manual rotation done."));
            updateDisplay(m_PAHStage, getPAHMessage());
            break;
        default :
            return; // no other stage should be able to trigger this event
    }
}



void PolarAlignmentAssistant::startPAHRefreshProcess()
{
    qCInfo(KSTARS_EKOS_ALIGN) << "Starting Polar Alignment Assistant refreshing...";

    refreshIteration = 0;
    imageNumber = 0;
    m_NumHealpixFailures = 0;

    setPAHStage(PAH_REFRESH);
    polarAlignWidget->updatePAHStage(m_PAHStage);
    auto message = getPAHMessage();
    refreshText->setText(message);
    emit newPAHMessage(message);

    PAHRefreshB->setEnabled(false);

    // Hide EQ Grids if shown
    if (m_AlignView->isEQGridShown())
        m_AlignView->toggleEQGrid();

    m_AlignView->setRefreshEnabled(true);

    Options::setAstrometrySolverWCS(false);
    Options::setAutoWCS(false);

    // We for refresh, just capture really
    emit captureAndSolve();
}

void PolarAlignmentAssistant::processPAHStage(double orientation, double ra, double dec, double pixscale,
        bool eastToTheRight, short healpix, short index)
{
    if (m_PAHStage == PAH_FIND_CP)
    {
        emit newLog(
            i18n("Mount is synced to celestial pole. You can now continue Polar Alignment Assistant procedure."));
        setPAHStage(PAH_FIRST_CAPTURE);
        polarAlignWidget->updatePAHStage(m_PAHStage);
        return;
    }

    if (m_PAHStage == PAH_FIRST_SOLVE || m_PAHStage == PAH_SECOND_SOLVE || m_PAHStage == PAH_THIRD_SOLVE)
    {
        // Used by refresh, when looking at the 3rd image.
        m_LastRa = ra;
        m_LastDec = dec;
        m_LastOrientation = orientation;
        m_LastPixscale = pixscale;
        m_HealpixToUse = healpix;
        m_IndexToUse = index;

        bool doWcs = (m_PAHStage == PAH_THIRD_SOLVE) || !Options::limitedResourcesMode();
        if (doWcs)
        {
            emit newLog(i18n("Please wait while WCS data is processed..."));
            PAHMessageText->setText(
                m_PAHStage == PAH_FIRST_SOLVE
                ? "Calculating WCS for the first image...</p>"
                : (m_PAHStage == PAH_SECOND_SOLVE ? "Calculating WCS for the second image...</p>"
                   : "Calculating WCS for the third image...</p>"));
        }
        connect(m_AlignView.get(), &AlignView::wcsToggled, this, &Ekos::PolarAlignmentAssistant::setWCSToggled,
                Qt::UniqueConnection);
        m_AlignView->injectWCS(orientation, ra, dec, pixscale, eastToTheRight);
        return;
    }
}

void PolarAlignmentAssistant::setImageData(const QSharedPointer<FITSData> &image)
{
    m_ImageData = image;

    if (m_PAHStage == PAH_FIRST_CAPTURE)
        setPAHStage(PAH_FIRST_SOLVE);
    else if (m_PAHStage == PAH_SECOND_CAPTURE)
        setPAHStage(PAH_SECOND_SOLVE);
    else if (m_PAHStage == PAH_THIRD_CAPTURE)
        setPAHStage(PAH_THIRD_SOLVE);
    else
        return;
    updateDisplay(m_PAHStage, getPAHMessage());
}

void PolarAlignmentAssistant::setWCSToggled(bool result)
{
    emit newLog(i18n("WCS data processing is complete."));

    disconnect(m_AlignView.get(), &AlignView::wcsToggled, this, &Ekos::PolarAlignmentAssistant::setWCSToggled);

    if (m_PAHStage == PAH_FIRST_CAPTURE || m_PAHStage == PAH_FIRST_SOLVE)
    {
        // We need WCS to be synced first
        if (result == false && m_AlignInstance->wcsSynced() == true)
        {
            emit newLog(i18n("WCS info is now valid. Capturing next frame..."));
            emit captureAndSolve();
            return;
        }

        polarAlign.reset();
        polarAlign.addPoint(m_ImageData);

        setPAHStage(PAH_FIRST_ROTATE);
        auto msg = getPAHMessage();
        if (pAHManualSlew->isChecked())
        {
            msg = QString("Please rotate your mount about %1 deg in RA")
                  .arg(pAHRotation->value());
            emit newLog(msg);
        }
        updateDisplay(m_PAHStage, msg);

        rotatePAH();
    }
    else if (m_PAHStage == PAH_SECOND_CAPTURE || m_PAHStage == PAH_SECOND_SOLVE)
    {
        setPAHStage(PAH_SECOND_ROTATE);
        auto msg = getPAHMessage();

        if (pAHManualSlew->isChecked())
        {
            msg = QString("Please rotate your mount about %1 deg in RA")
                  .arg(pAHRotation->value());
            emit newLog(msg);
        }
        updateDisplay(m_PAHStage, msg);

        polarAlign.addPoint(m_ImageData);

        rotatePAH();
    }
    else if (m_PAHStage == PAH_THIRD_CAPTURE || m_PAHStage == PAH_THIRD_SOLVE)
    {
        // Critical error
        if (result == false)
        {
            emit newLog(i18n("Failed to process World Coordinate System: %1. Try again.", m_ImageData->getLastError()));
            return;
        }

        polarAlign.addPoint(m_ImageData);

        // We have 3 points which uniquely defines a circle with its center representing the RA Axis
        // We have celestial pole location. So correction vector is just the vector between these two points
        if (calculatePAHError())
        {
            setPAHStage(PAH_STAR_SELECT);
            polarAlignWidget->updatePAHStage(m_PAHStage);
            PAHWidgets->setCurrentWidget(PAHRefreshPage);
            refreshText->setText(getPAHMessage());
            emit newPAHMessage(getPAHMessage());
        }
        else
        {
            emit newLog(i18n("PAA: Failed to find the RA axis. Quitting."));
            stopPAHProcess();
        }
    }
}

void PolarAlignmentAssistant::setMountStatus(ISD::Mount::Status newState)
{
    switch (newState)
    {
        case ISD::Mount::MOUNT_PARKING:
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_MOVING:
            PAHStartB->setEnabled(false);
            break;

        default:
            if (m_PAHStage == PAH_IDLE)
                PAHStartB->setEnabled(true);
            break;
    }
}

QString PolarAlignmentAssistant::getPAHMessage() const
{
    switch (m_PAHStage)
    {
        case PAH_IDLE:
        case PAH_FIND_CP:
            return introText->text();
        case PAH_FIRST_CAPTURE:
            return i18n("<p>The assistant requires three images to find a solution.  Ekos is now capturing the first image...</p>");
        case PAH_FIRST_SOLVE:
            return i18n("<p>Solving the <i>first</i> image...</p>");
        case PAH_FIRST_ROTATE:
            return i18n("<p>Executing the <i>first</i> mount rotation...</p>");
        case PAH_FIRST_SETTLE:
            return i18n("<p>Settling after the <i>first</i> mount rotation.</p>");
        case PAH_SECOND_SETTLE:
            return i18n("<p>Settling after the <i>second</i> mount rotation.</p>");
        case PAH_SECOND_CAPTURE:
            return i18n("<p>Capturing the second image...</p>");
        case PAH_SECOND_SOLVE:
            return i18n("<p>Solving the <i>second</i> image...</p>");
        case PAH_SECOND_ROTATE:
            return i18n("<p>Executing the <i>second</i> mount rotation...</p>");
        case PAH_THIRD_CAPTURE:
            return i18n("<p>Capturing the <i>third</i> and final image...</p>");
        case PAH_THIRD_SOLVE:
            return i18n("<p>Solving the <i>third</i> image...</p>");
        case PAH_STAR_SELECT:
            if (pAHRefreshAlgorithm->currentIndex() == PLATE_SOLVE_ALGORITHM)
                return i18n("<p>Choose your exposure time & select an adjustment method. Then click <i>refresh</i> to begin adjustments.</p>");
            else
                return i18n("<p>Choose your exposure time & select an adjustment method. Click <i>Refresh</i> to begin.</p><p>Correction triangle is plotted above. <i>Zoom in and select a bright star</i> to reposition the correction vector. Use the <i>MoveStar & Calc Error</i> method to estimate the remaining error.</p>");
        case PAH_REFRESH:
            if (pAHRefreshAlgorithm->currentIndex() == PLATE_SOLVE_ALGORITHM)
                return i18n("<p>Adjust mount's <i>Altitude and Azimuth knobs</i> to reduce the polar alignment error.</p><p>Be patient, plate solving can be affected by knob movement. Consider using results after 2 images.  Click <i>Stop</i> when you're finished.</p>");
            else
                return i18n("<p>Adjust mount's <i>Altitude knob</i> to move the star along the yellow line, then adjust the <i>Azimuth knob</i> to move it along the Green line until the selected star is centered within the crosshair.</p><p>Click <i>Stop</i> when the star is centered.</p>");
        default:
            break;
    }

    return QString();
}

void PolarAlignmentAssistant::setPAHRefreshAlgorithm(RefreshAlgorithm value)
{
    // If the star-correspondence method of tracking polar alignment error wasn't initialized,
    // at the start, it can't be turned on mid process.
    if ((m_PAHStage == PAH_REFRESH) && refreshIteration > 0 && (value != PLATE_SOLVE_ALGORITHM)
            && !starCorrespondencePAH.size())
    {
        pAHRefreshAlgorithm->setCurrentIndex(PLATE_SOLVE_ALGORITHM);
        emit newLog(i18n("Cannot change to MoveStar algorithm once refresh has begun"));
        return;
    }
    if (m_PAHStage == PAH_REFRESH || m_PAHStage == PAH_STAR_SELECT)
    {
        refreshText->setText(getPAHMessage());
        emit newPAHMessage(getPAHMessage());
    }

    showUpdatedError((value == PLATE_SOLVE_ALGORITHM) ||
                     (value == MOVE_STAR_UPDATE_ERR_ALGORITHM));
    if (value == PLATE_SOLVE_ALGORITHM)
        updatePlateSolveTriangle(m_ImageData);
    else
        m_AlignView->setCorrectionParams(correctionFrom, correctionTo, correctionAltTo);

}

}
