/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidestars.h"

#include "ekos_guide_debug.h"
#include "../guideview.h"
#include "Options.h"

#include <math.h>
#include <stellarsolver.h>
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include <QTime>

// Keeps at most this many reference "neighbor" stars
#define MAX_GUIDE_STARS 10

// Then when looking for the guide star, gets this many candidates.
#define STARS_TO_SEARCH 250

// Don't use stars with SNR lower than this when computing multi-star drift.
#define MIN_DRIFT_SNR  8

// If there are fewer than this many stars, don't use this multi-star algorithm.
// It will instead back-off to a reticle-based algorithm.
#define MIN_STAR_CORRESPONDENCE_SIZE 5

// We limit the HFR for guide stars. When searching for the guide star, we relax this by the
// margin below (e.g. if a guide star was selected that was near the max guide-star hfr, the later
// the hfr increased a little, we still want to be able to find it.
constexpr double HFR_MARGIN = 2.0;
/*
 Start with a set of reference (x,y) positions from stars, where one is designated a guide star.
 Given these and a set of new input stars, determine a mapping of new stars to the references.
 As the star positions should mostly change via translation, the correspondences are determined by
 similar geometry. It is possible, though that there is noise in positions, and that
 some reference stars may not be in the new star group, or that the new stars may contain some
 stars not appearing in references. However, the guide star must appear in both for this to
 be successful.
 */

namespace
{

QString logHeader(const QString &label)
{
    return QString("%1 %2 %3 %4 %5 %6 %7")
           .arg(label, -9).arg("#", 3).arg("x   ", 6).arg("y   ", 6).arg("flux", 6)
           .arg("HFR", 6).arg("SNR ", 5);
}

QString logStar(const QString &label, int index, const SkyBackground &bg, const Edge &star)
{
    double snr = bg.SNR(star.sum, star.numPixels);
    return QString("%1 %2 %3 %4 %5 %6 %7")
           .arg(label, -9)
           .arg(index, 3)
           .arg(star.x, 6, 'f', 1)
           .arg(star.y, 6, 'f', 1)
           .arg(star.sum, 6, 'f', 0)
           .arg(star.HFR, 6, 'f', 2)
           .arg(snr, 5, 'f', 1);
}

void logStars(const QString &label, const QString &label2, const SkyBackground &bg,
              int size,
              std::function<const Edge & (int index)> stars,
              const QString &extraLabel,
              std::function<QString (int index)> extras)
{
    QString header = logHeader(label);
    if (extraLabel.size() > 0)
        header.append(QString("  %1").arg(extraLabel));
    qCDebug(KSTARS_EKOS_GUIDE) << header;
    for (int i = 0; i < size; ++i)
    {
        const auto &star = stars(i);
        QString line = logStar(label2, i, bg, star);
        if (extraLabel.size() > 0)
            line.append(QString(" %1").arg(extras(i)));
        qCDebug(KSTARS_EKOS_GUIDE) << line;
    }
}
}  //namespace

GuideStars::GuideStars()
{
}

// It's possible that we don't map all the stars, if there are too many.
int GuideStars::getStarMap(int index)
{
    if (index >= starMap.size() || (index < 0))
        return -1;
    return starMap[index];
}

void GuideStars::setupStarCorrespondence(const QList<Edge> &neighbors, int guideIndex)
{
    qCDebug(KSTARS_EKOS_GUIDE) << "setupStarCorrespondence: " << neighbors.size() << guideIndex;
    if (neighbors.size() >= MIN_STAR_CORRESPONDENCE_SIZE)
    {
        starMap.clear();
        for (int i = 0; i < neighbors.size(); ++i)
        {
            qCDebug(KSTARS_EKOS_GUIDE) << " adding ref: " << neighbors[i].x << neighbors[i].y;
            // We need to initialize starMap, because findGuideStar(), which normally
            // initializes starMap(), might call selectGuideStar() if it finds that
            // the starCorrespondence is empty.
            starMap.push_back(i);
        }
        starCorrespondence.initialize(neighbors, guideIndex);
    }
    else
        starCorrespondence.reset();
}

// Calls SEP to generate a set of star detections and score them,
// then calls selectGuideStars(detections, scores) to select the guide star.
QVector3D GuideStars::selectGuideStar(const QSharedPointer<FITSData> &imageData)
{
    if (imageData == nullptr)
        return QVector3D(-1, -1, -1);

    QList<double> sepScores;
    QList<double> minDistances;
    const double maxHFR = Options::guideMaxHFR();
    findTopStars(imageData, MAX_GUIDE_STARS, &detectedStars, maxHFR,
                 nullptr, &sepScores, &minDistances);

    int maxX = imageData->width();
    int maxY = imageData->height();
    return selectGuideStar(detectedStars, sepScores, maxX, maxY, minDistances);
}

// This selects the guide star by using the input scores, and
// downgrading stars too close to the edges of the image (so that calibration won't fail).
// It also removes from consideration stars with huge HFR values (most likely not stars).
QVector3D GuideStars::selectGuideStar(const QList<Edge> &stars,
                                      const QList<double> &sepScores,
                                      int maxX, int maxY,
                                      const QList<double> &minDistances)
{
    constexpr int maxStarDiameter = 32;
    int maxIndex = MAX_GUIDE_STARS < stars.count() ? MAX_GUIDE_STARS : stars.count();
    int scores[MAX_GUIDE_STARS];
    QList<Edge> guideStarNeighbors;
    for (int i = 0; i < maxIndex; i++)
    {
        int score = 100 + sepScores[i];
        const Edge &center = stars.at(i);
        guideStarNeighbors.append(center);

        // Severely reject stars close to edges
        // Worry about calibration? Make calibration distance parameter?
        constexpr int borderGuard = 35;
        if (center.x < borderGuard || center.y < borderGuard ||
                center.x > (maxX - borderGuard) || center.y > (maxY - borderGuard))
            score -= 1000;
        // Reject stars bigger than square
        if (center.HFR > float(maxStarDiameter) / 2)
            score -= 1000;

        // Add advantage to stars with SNRs between 40-100.
        auto bg = skybackground();
        double snr = bg.SNR(center.sum, center.numPixels);
        if (snr >= 40 && snr <= 100)
            score += 75;
        if (snr >= 100)
            score -= 50;

        // discourage stars that have close neighbors.
        // This isn't perfect, as we're not detecting all stars, just top 100 or so.
        const double neighborDistance = minDistances[i];
        constexpr double closeNeighbor = 25;
        constexpr double veryCloseNeighbor = 15;
        if (neighborDistance < veryCloseNeighbor)
            score -= 100;
        else if (neighborDistance < closeNeighbor)
            score -= 50;

        scores[i] = score;
    }

    logStars("Select", "Star", skyBackground,
             maxIndex,
             [&](int i) -> const Edge&
    {
        return stars[i];
    },
    "score", [&](int i) -> QString
    {
        return QString("%1").arg(scores[i], 5);
    });

    int maxScore      = -1;
    int maxScoreIndex = -1;
    for (int i = 0; i < maxIndex; i++)
    {
        if (scores[i] > maxScore)
        {
            maxScore      = scores[i];
            maxScoreIndex = i;
        }
    }

    if (maxScoreIndex < 0)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "No suitable star detected.";
        return QVector3D(-1, -1, -1);
    }
    setupStarCorrespondence(guideStarNeighbors, maxScoreIndex);
    QVector3D newStarCenter(stars[maxScoreIndex].x, stars[maxScoreIndex].y, 0);
    qCDebug(KSTARS_EKOS_GUIDE) << "new star center: " << maxScoreIndex << " x: "
                               << stars[maxScoreIndex].x << " y: " << stars[maxScoreIndex].y;
    return newStarCenter;
}

// Find the current target positions for the guide-star neighbors, and add them
// to the guideView.
void GuideStars::plotStars(GuideView *guideView, const QRect &trackingBox)
{
    if (guideView == nullptr) return;
    guideView->clearNeighbors();
    if (starCorrespondence.size() == 0) return;

    const Edge gStar = starCorrespondence.reference(starCorrespondence.guideStar());

    // Find the offset between the current reticle position and the original
    // guide star position (e.g. it might have moved due to dithering).
    double reticle_x = trackingBox.x() + trackingBox.width() / 2.0;
    double reticle_y = trackingBox.y() + trackingBox.height() / 2.0;

    // See which neighbor stars were associated with detected stars.
    QVector<bool> found(starCorrespondence.size(), false);
    QVector<int> detectedIndex(starCorrespondence.size(), -1);

    for (int i = 0; i < detectedStars.size(); ++i)
    {
        int refIndex = getStarMap(i);
        if (refIndex >= 0)
        {
            found[refIndex] = true;
            detectedIndex[refIndex] = i;
        }
    }
    // Add to the guideView the neighbor stars' target positions and whether they were found.
    for (int i = 0; i < starCorrespondence.size(); ++i)
    {
        bool isGuideStar = i == starCorrespondence.guideStar();
        const QVector2D offset = starCorrespondence.offset(i);
        const double detected_x = found[i] ? detectedStars[detectedIndex[i]].x : 0;
        const double detected_y = found[i] ? detectedStars[detectedIndex[i]].y : 0;
        guideView->addGuideStarNeighbor(offset.x() + reticle_x, offset.y() + reticle_y, found[i],
                                        detected_x, detected_y, isGuideStar);
    }
}

// Find the guide star using the starCorrespondence algorithm (looking for
// the other reference stars in the same relative position as when the guide star was selected).
// If this method fails, it backs off to looking in the tracking box for the highest scoring star.
GuiderUtils::Vector GuideStars::findGuideStar(const QSharedPointer<FITSData> &imageData, const QRect &trackingBox,
        GuideView *guideView)
{
    // Don't accept reference stars whose position is more than this many pixels from expected.
    constexpr double maxStarAssociationDistance = 10;

    if (imageData == nullptr)
        return GuiderUtils::Vector(-1, -1, -1);

    // If the guide star has not yet been set up, then establish it here.
    // Not thrilled doing this, but this is the way the internal guider is setup
    // when guiding is restarted by the scheduler (the normal establish guide star
    // methods are not called).
    if (starCorrespondence.size() == 0)
    {
        QVector3D v = selectGuideStar(imageData);
        qCDebug(KSTARS_EKOS_GUIDE) << QString("findGuideStar: Called without starCorrespondence. Refound guide star at %1 %2")
                                   .arg(QString::number(v.x(), 'f', 1)).arg(QString::number(v.y(), 'f', 1));
        return GuiderUtils::Vector(v.x(), v.y(), v.z());
    }

    // Allow a little margin above the max hfr for guide stars when searching for the guide star.
    const double maxHFR = Options::guideMaxHFR() + HFR_MARGIN;
    if (starCorrespondence.size() > 0)
    {

        findTopStars(imageData, STARS_TO_SEARCH, &detectedStars, maxHFR);
        if (detectedStars.empty())
            return GuiderUtils::Vector(-1, -1, -1);

        // Allow it to guide even if the main guide star isn't detected (as long as enough reference stars are).
        constexpr int maxMissingGuideStars = 5;
        starCorrespondence.setAllowMissingGuideStar(
            allowMissingGuideStar &&
            missedGuideStars < maxMissingGuideStars);

        // Star correspondence can run quicker if it knows the image size.
        starCorrespondence.setImageSize(imageData->width(), imageData->height());
        GuiderUtils::Vector position = starCorrespondence.find(detectedStars, maxStarAssociationDistance, &starMap);

        // Is there a correspondence to the guide star
        // Should we also weight distance to the tracking box?
        for (int i = 0; i < detectedStars.size(); ++i)
        {
            if (getStarMap(i) == starCorrespondence.guideStar())
            {
                auto &star = detectedStars[i];
                double SNR = skyBackground.SNR(star.sum, star.numPixels);
                guideStarSNR = SNR;
                guideStarMass = star.sum;
                missedGuideStars = 0;
                qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence found " << i << "at" << star.x << star.y << "SNR" << SNR;
                if (guideView != nullptr)
                    plotStars(guideView, trackingBox);
                return GuiderUtils::Vector(star.x, star.y, 0);
            }
        }
        // None of the stars matched the guide star, but it's possible star correspondence
        // invented a guide star position.
        if (position.x >= 0 && position.y >= 0)
        {
            // For now we're returning an snr of 0
            double SNR = 0;
            guideStarSNR = SNR;
            guideStarMass = 0;
            missedGuideStars++;
            qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence invented at" << position.x << position.y << "SNR" << SNR;
            if (guideView != nullptr)
                plotStars(guideView, trackingBox);
            return position;
        }
    }

    qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence not used. It failed to find the guide star.";
    logStars("findGuide", "Star", skyBackground,
             detectedStars.size(),
             [&](int i) -> const Edge&
    {
        return detectedStars[i];
    },
    "score", [&](int i) -> QString
    {
        return QString("%1").arg(getStarMap(i));
    });

    if (trackingBox.isValid() == false)
        return GuiderUtils::Vector(-1, -1, -1);

    // If we didn't find a star that way, then fall back
    findTopStars(imageData, 1, &detectedStars, maxHFR, &trackingBox);
    if (detectedStars.size() > 0)
    {
        auto &star = detectedStars[0];
        double SNR = skyBackground.SNR(star.sum, star.numPixels);
        guideStarSNR = SNR;
        guideStarMass = star.sum;
        qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence. Standard method found at " << star.x << star.y << "SNR" << SNR;
        return GuiderUtils::Vector(star.x, star.y, 0);
    }
    return GuiderUtils::Vector(-1, -1, -1);
}

SSolver::Parameters GuideStars::getStarExtractionParameters(int num)
{
    SSolver::Parameters params;
    params.listName = "Guider";
    params.apertureShape = SSolver::SHAPE_CIRCLE;
    params.minarea = 10; // changed from 5 --> 10
    params.deblend_thresh = 32;
    params.deblend_contrast = 0.005;
    params.initialKeep = num * 2;
    params.keepNum = num;
    params.removeBrightest = 0;
    params.removeDimmest = 0;
    params.saturationLimit = 0;
    return params;
}

// This is the interface to star detection.
int GuideStars::findAllSEPStars(const QSharedPointer<FITSData> &imageData, QList<Edge *> *sepStars, int num)
{
    if (imageData == nullptr)
        return 0;

    QVariantMap settings;
    settings["optionsProfileIndex"] = Options::guideOptionsProfile();
    settings["optionsProfileGroup"] = static_cast<int>(Ekos::GuideProfiles);
    imageData->setSourceExtractorSettings(settings);
    imageData->findStars(ALGORITHM_SEP).waitForFinished();
    skyBackground = imageData->getSkyBackground();

    QList<Edge *> edges = imageData->getStarCenters();
    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->HFR > edge2->HFR;});

    // Take only the first maxNumCenters stars
    {
        int starCount = qMin(num, edges.count());
        for (int i = 0; i < starCount; i++)
            sepStars->append(edges[i]);
    }

    edges.clear();
    return sepStars->count();
}

double GuideStars::findMinDistance(int index, const QList<Edge*> &stars)
{
    double closestDistanceSqr = 1e10;
    const Edge &star = *stars[index];
    for (int i = 0; i < stars.size(); ++i)
    {
        if (i == index) continue;
        const Edge &thisStar = *stars[i];
        const double xDist = star.x - thisStar.x;
        const double yDist = star.y - thisStar.y;
        const double distanceSqr = xDist * xDist + yDist * yDist;
        if (distanceSqr < closestDistanceSqr)
            closestDistanceSqr = distanceSqr;
    }
    return sqrt(closestDistanceSqr);
}

// Returns a list of 'num' stars, sorted according to evaluateSEPStars().
// If the region-of-interest rectange is not null, it only returns scores in that area.
void GuideStars::findTopStars(const QSharedPointer<FITSData> &imageData, int num, QList<Edge> *stars,
                              const double maxHFR, const QRect *roi,
                              QList<double> *outputScores, QList<double> *minDistances)
{
    if (roi == nullptr)
        qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: findTopStars" << num;
    else
        qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: findTopStars" << num <<
                                   QString("Roi(%1,%2 %3x%4)").arg(roi->x()).arg(roi->y()).arg(roi->width()).arg(roi->height());

    stars->clear();
    if (imageData == nullptr)
        return;

    QTime timer;
    timer.restart();
    QList<Edge*> sepStars;
    int count = findAllSEPStars(imageData, &sepStars, num * 2);
    if (count == 0)
        return;

    if (sepStars.empty())
        return;

    QVector<double> scores;
    evaluateSEPStars(sepStars, &scores, roi, maxHFR);
    // Sort the sepStars by score, higher score to lower score.
    QVector<std::pair<int, double>> sc;
    for (int i = 0; i < scores.size(); ++i)
        sc.push_back(std::pair<int, double>(i, scores[i]));
    std::sort(sc.begin(), sc.end(), [](const std::pair<int, double> &a, const std::pair<int, double> &b)
    {
        return a.second > b.second;
    });
    // Copy the top num results.
    for (int i = 0; i < std::min(num, scores.size()); ++i)
    {
        const int starIndex = sc[i].first;
        const double starScore = sc[i].second;
        if (starScore >= 0)
        {
            stars->append(*(sepStars[starIndex]));
            if (outputScores != nullptr)
                outputScores->append(starScore);
            if (minDistances != nullptr)
                minDistances->append(findMinDistance(starIndex, sepStars));
        }
    }
    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("Multistar: findTopStars returning: %1 stars, %2s")
            .arg(stars->size()).arg(timer.elapsed() / 1000.0, 4, 'f', 2);
}

// Scores star detection relative to each other. Uses the star's SNR as the main measure.
void GuideStars::evaluateSEPStars(const QList<Edge *> &starCenters, QVector<double> *scores,
                                  const QRect *roi, const double maxHFR) const
{
    auto centers = starCenters;
    scores->clear();
    for (int i = 0; i < centers.size(); ++i) scores->push_back(0);
    if (centers.empty()) return;

    // Rough constants used by this weighting.
    // If the center pixel is above this, assume it's clipped and don't emphasize.
    constexpr double snrWeight = 20;  // Measure weight if/when multiple measures are used.
    auto bg = skybackground();

    // Sort by SNR in increasing order so the weighting goes up.
    // Assign score based on the sorted position.
    std::sort(centers.begin(), centers.end(), [&bg](const Edge * a, const Edge * b)
    {
        double snrA = bg.SNR(a->sum, a->numPixels);
        double snrB = bg.SNR(b->sum, b->numPixels);
        return snrA < snrB;
    });
    for (int i = 0; i < centers.size(); ++i)
    {
        for (int j = 0; j < starCenters.size(); ++j)
        {
            if (starCenters.at(j) == centers.at(i))
            {
                // Don't emphasize stars that are too wide.
                if (centers.at(i)->HFR > maxHFR)
                    (*scores)[j] = -1;
                else
                    (*scores)[j] += snrWeight * i;
                break;
            }
        }
    }

    // If we are insisting on a star in the tracking box.
    if (roi != nullptr)
    {
        // Reset the score of anything outside the tracking box.
        for (int j = 0; j < starCenters.size(); ++j)
        {
            const auto &star = starCenters.at(j);
            if (star->x < roi->x() || star->x >= roi->x() + roi->width() ||
                    star->y < roi->y() || star->y >= roi->y() + roi->height())
                (*scores)[j] = -1;
        }
    }
}

// Given a star detection and a reference star compute the RA and DEC drifts between the two.
void GuideStars::computeStarDrift(const Edge &star, const Edge &reference, double *driftRA, double *driftDEC) const
{
    if (!calibrationInitialized) return;

    GuiderUtils::Vector position(star.x, star.y, 0);
    GuiderUtils::Vector reference_position(reference.x, reference.y, 0);
    GuiderUtils::Vector arc_position, arc_reference_position;
    arc_position = calibration.convertToArcseconds(position);
    arc_reference_position = calibration.convertToArcseconds(reference_position);

    // translate into sky coords.
    GuiderUtils::Vector sky_coords = arc_position - arc_reference_position;
    sky_coords = calibration.rotateToRaDec(sky_coords);

    // Save the drifts in RA and DEC.
    *driftRA   = sky_coords.x;
    *driftDEC = sky_coords.y;
}

// Computes the overall drift given the input image that was analyzed in findGuideStar().
// Returns true if this can be done--there are already guide stars and the current detections
// can be compared to them. In that case, fills in RADRift and DECDrift with arc-second drifts.
// Reticle_x,y is the target position. It may be different from when we
// originally selected the guide star, e.g. due to dithering, etc. We compute an offset from the
// original guide-star position and the reticle position and offset all the reference star
// positions by that.
bool GuideStars::getDrift(double oneStarDrift, double reticle_x, double reticle_y,
                          double *RADrift, double *DECDrift)
{
    if (starCorrespondence.size() == 0)
        return false;
    const Edge gStar = starCorrespondence.reference(starCorrespondence.guideStar());
    qCDebug(KSTARS_EKOS_GUIDE) << "Multistar getDrift, reticle:" << reticle_x << reticle_y
                               << "guidestar" << gStar.x << gStar.y
                               << "so offsets:" << (reticle_x - gStar.x) << (reticle_y - gStar.y);
    // Revoke multistar if we're that far away.
    constexpr double maxDriftForMultistar = 4.0;  // arc-seconds

    if (!calibrationInitialized)
        return false;

    // Don't run multistar if the 1-star approach is too far off.
    if (oneStarDrift > maxDriftForMultistar)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "MultiStar: 1-star drift too high: " << oneStarDrift;
        return false;
    }
    if (starCorrespondence.size() < 2)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: no guide stars";
        return false;
    }

    // The original guide star position has been offset.
    double offset_x = reticle_x - gStar.x;
    double offset_y = reticle_y - gStar.y;

    int numStarsProcessed = 0;
    bool guideStarProcessed = false;
    double driftRASum = 0, driftDECSum = 0;
    double guideStarRADrift = 0, guideStarDECDrift = 0;
    QVector<double> raDrifts, decDrifts;
    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("%1 %2  dRA   dDEC").arg(logHeader("")).arg(logHeader("    Ref:"));
    for (int i = 0; i < detectedStars.size(); ++i)
    {
        const auto &star = detectedStars[i];
        auto bg = skybackground();
        double snr = bg.SNR(detectedStars[i].sum, detectedStars[i].numPixels);
        // Probably should test SNR on the reference as well.
        if (getStarMap(i) >= 0 && snr >= MIN_DRIFT_SNR)
        {
            auto ref = starCorrespondence.reference(getStarMap(i));
            ref.x += offset_x;
            ref.y += offset_y;
            double driftRA, driftDEC;
            computeStarDrift(star, ref, &driftRA, &driftDEC);
            if (getStarMap(i) == starCorrespondence.guideStar())
            {
                guideStarRADrift = driftRA;
                guideStarDECDrift = driftDEC;
                guideStarProcessed = true;
            }

            raDrifts.push_back(driftRA);
            decDrifts.push_back(driftDEC);
            numStarsProcessed++;

            qCDebug(KSTARS_EKOS_GUIDE)
                    << QString("%1 %2 %3 %4").arg(logStar("MultiStar", i, bg, star))
                    .arg(logStar("    Ref:", getStarMap(i), bg, ref))
                    .arg(driftRA, 5, 'f', 2).arg(driftDEC, 5, 'f', 2);
        }
    }

    if (numStarsProcessed == 0 || (!allowMissingGuideStar && !guideStarProcessed))
        return false;

    // Filter out reference star drifts that are too different from the guide-star drift.
    QVector<double> raDriftsKeep, decDriftsKeep;
    for (int i = 0; i < raDrifts.size(); ++i)
    {
        if ((allowMissingGuideStar && !guideStarProcessed) ||
                ((fabs(raDrifts[i] - guideStarRADrift) < 2.0) &&
                 (fabs(decDrifts[i] - guideStarDECDrift) < 2.0)))
        {
            driftRASum += raDrifts[i];
            driftDECSum += decDrifts[i];
            raDriftsKeep.push_back(raDrifts[i]);
            decDriftsKeep.push_back(decDrifts[i]);
        }
    }
    if (raDriftsKeep.empty() || raDriftsKeep.empty())
        return false;  // Shouldn't happen.

    numStarsProcessed = raDriftsKeep.size();

    // Generate the drift either from the median or the average of the usable reference drifts.
    bool useMedian = true;
    if (useMedian)
    {
        std::sort(raDriftsKeep.begin(),  raDriftsKeep.end(),  [] (double d1, double d2)
        {
            return d1 < d2;
        });
        std::sort(decDriftsKeep.begin(), decDriftsKeep.end(), [] (double d1, double d2)
        {
            return d1 < d2;
        });
        if (numStarsProcessed % 2 == 0)
        {
            const int middle = numStarsProcessed / 2;
            *RADrift  = ( raDriftsKeep[middle - 1] +  raDriftsKeep[middle]) / 2.0;
            *DECDrift = (decDriftsKeep[middle - 1] + decDriftsKeep[middle]) / 2.0;
        }
        else
        {
            const int middle = numStarsProcessed / 2;  // because it's 0-based
            *RADrift  = raDriftsKeep[middle];
            *DECDrift = decDriftsKeep[middle];
        }
        qCDebug(KSTARS_EKOS_GUIDE) << "MultiStar: Drift median " << *RADrift << *DECDrift << numStarsProcessed << " of " <<
                                   detectedStars.size() << "#guide" << starCorrespondence.size();
    }
    else
    {
        *RADrift  = driftRASum / raDriftsKeep.size();
        *DECDrift = driftDECSum / decDriftsKeep.size();
        qCDebug(KSTARS_EKOS_GUIDE) << "MultiStar: Drift " << *RADrift << *DECDrift << numStarsProcessed << " of " <<
                                   detectedStars.size() << "#guide" << starCorrespondence.size();
    }
    return true;
}

void GuideStars::setCalibration(const Calibration &cal)
{
    calibration = cal;
    calibrationInitialized = true;
}
