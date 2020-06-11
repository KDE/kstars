/*  Correspondence class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guidestars.h"

#include <math.h>
#include "ekos_guide_debug.h"

// Keeps at most this many reference "neighbor" stars
#define MAX_GUIDE_STARS 10

// Then when looking for the guide star, gets this many candidates.
#define STARS_TO_SEARCH 15

// Don't use stars with SNR lower than this when computing multi-star drift.
#define MIN_DRIFT_SNR  8

// If there are fewer than this many stars, don't use this multi-star algorithm.
// It will instead back-off to a reticle-based algorithm.
#define MIN_STAR_CORRESPONDENCE_SIZE 5

/*
 Start with a set of reference (x,y) positions from stars, where one is designated a guide star.
 Given these and a set of new input stars, determine a mapping of new stars to the references.
 As the star positions should mostly change via translation, the correspondences are determined by
 similar geometry. It is possible, though that there is noise in positions, and that
 some reference stars may not be in the new star group, or that the new stars may contain some
 stars not appearing in references. However, the guide star must appear in both for this to
 be successful.
 */

GuideStars::GuideStars()
{
    ROT_Z = Ekos::RotateZ(0.0);
}

void GuideStars::setupStarCorrespondence(const QList<Edge> &neighbors, int guideIndex)
{
    qCDebug(KSTARS_EKOS_GUIDE) << "setupStarCorrespondence: " << neighbors.size() << guideIndex;
    guideStarNeighbors.clear();
    if (neighbors.size() >= MIN_STAR_CORRESPONDENCE_SIZE)
    {
        for (int i = 0; i < neighbors.size(); ++i)
        {
            qCDebug(KSTARS_EKOS_GUIDE) << " adding ref: " << neighbors[i].x << neighbors[i].y;
            guideStarNeighbors.append(neighbors[i]);
        }
        guideStarNeighborIndex = guideIndex;
        starCorrespondence.initialize(guideStarNeighbors, guideStarNeighborIndex);
    }
}

// Calls SEP to generate a set of star detections and score them,
// then calls selectGuideStars(detections, scores) to select the guide star.
QVector3D GuideStars::selectGuideStar(FITSData *imageData)
{
    if (imageData == nullptr)
        return QVector3D(-1, -1, -1);

    QList<double> sepScores;
    findTopStars(imageData, MAX_GUIDE_STARS, &detectedStars, nullptr, &sepScores);

    int maxX = imageData->width();
    int maxY = imageData->height();
    return selectGuideStar(detectedStars, sepScores, maxX, maxY);
}

// This selects the guide star by using the input scores, and
// downgrading stars too close to the edges of the image (so that calibration won't fail).
// It also removes from consideration stars with huge HFR values (most likely not stars).
QVector3D GuideStars::selectGuideStar(const QList<Edge> &stars,
                                      const QList<double> &sepScores,
                                      int maxX, int maxY)
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

        // Moderately reject stars close to other stars
        // This won't work because we're strongly limiting the number of stars.
        // Skip for now, should not be important with star-correspondence anyway.

        scores[i] = score;
    }

    int maxScore      = -1;
    int maxScoreIndex = -1;

    qCDebug(KSTARS_EKOS_GUIDE) << qSetFieldWidth(6) << "#" << "X" << "Y" << "Flux"
                               << "HFR" << "sepSc" << "SCORE" << "SNR";
    for (int i = 0; i < maxIndex; i++)
    {
        auto bg = skybackground();
        double snr = bg.SNR(stars[i].sum, stars[i].numPixels);
        qCDebug(KSTARS_EKOS_GUIDE) << qSetFieldWidth(6) << i << stars[i].x << stars[i].y
                                   << stars[i].sum << stars[i].HFR
                                   << sepScores[i] << scores[i] << snr;
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

// Find the guide star using the starCorrespondence algorithm (looking for
// the other reference stars in the same relative position as when the guide star was selected).
// If this method fails, it backs off to looking in the tracking box for the highest scoring star.
Vector GuideStars::findGuideStar(FITSData *imageData, const QRect &trackingBox)
{
    // Don't accept reference stars whose position is more than this many pixels from expected.
    constexpr double maxStarAssociationDistance = 10;

    if (imageData == nullptr)
        return Vector(-1, -1, -1);

    if (guideStarNeighbors.size() > 0)
    {
        findTopStars(imageData, STARS_TO_SEARCH, &detectedStars);
        if (detectedStars.empty())
            return Vector(-1, -1, -1);
        starCorrespondence.find(detectedStars, maxStarAssociationDistance, &starMap);
        // Is there a correspondence to the guide star
        // Should we also weight distance to the tracking box?
        for (int i = 0; i < detectedStars.size(); ++i)
        {
            if (starMap[i] == guideStarNeighborIndex)
            {
                auto &star = detectedStars[i];
                double SNR = skyBackground.SNR(star.sum, star.numPixels);
                guideStarSNR = SNR;
                guideStarMass = star.sum;
                qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence found " << i << "at" << star.x << star.y << "SNR" << SNR;
                return Vector(star.x, star.y, 0);
            }
        }
    }

    if (trackingBox.isValid() == false)
        return Vector(-1, -1, -1);

    // If we didn't find a star that way, then fall back
    qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence not used";
    findTopStars(imageData, 1, &detectedStars, &trackingBox);
    if (detectedStars.size() > 0)
    {
        auto &star = detectedStars[0];
        double SNR = skyBackground.SNR(star.sum, star.numPixels);
        guideStarSNR = SNR;
        guideStarMass = star.sum;
        qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence. Standard method found at " << star.x << star.y << "SNR" << SNR;
        return Vector(star.x, star.y, 0);
    }
    return Vector(-1, -1, -1);
}

// This is the interface to star detection.
int GuideStars::findAllSEPStars(FITSData *imageData, QList<Edge*> *sepStars)
{
    qDeleteAll(*sepStars);
    sepStars->clear();

    QRect nullBox;
    int count = FITSSEPDetector(imageData)
                .configure("numStars", 100)
                .configure("fractionRemoved", 0.0)
                .configure("deblendMincont", 1.0)
                .findSourcesAndBackground(*sepStars, nullBox, &skyBackground);
    return count;
}

// Returns a list of 'num' stars, sorted according to evaluateSEPStars().
// If the region-of-interest rectange is not null, it only returns scores in that area.
void GuideStars::findTopStars(FITSData *imageData, int num, QList<Edge> *stars, const QRect *roi,
                              QList<double> *outputScores)
{
    if (roi == nullptr)
        qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: findTopStars" << num;
    else
        qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: findTopStars" << num <<
                                   QString("Roi(%1,%2 %3x%4)").arg(roi->x()).arg(roi->y()).arg(roi->width()).arg(roi->height());

    stars->clear();
    if (imageData == nullptr)
        return;

    QList<Edge*> sepStars;
    int count = findAllSEPStars(imageData, &sepStars);
    if (count == 0)
        return;

    if (sepStars.empty())
        return;

    QVector<double> scores;
    evaluateSEPStars(sepStars, &scores, roi);
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
        if (sc[i].second >= 0)
        {
            stars->append(*(sepStars[sc[i].first]));
            if (outputScores != nullptr)
                outputScores->append(sc[i].second);
        }
    }
    qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: findTopStars returning: " << stars->size();
}

// Scores star detection relative to each other. Uses the star's SNR as the main measure.
void GuideStars::evaluateSEPStars(const QList<Edge *> &starCenters, QVector<double> *scores, const QRect *roi) const
{
    auto centers = starCenters;
    scores->clear();
    for (int i = 0; i < centers.size(); ++i) scores->push_back(0);

    // Rough constants used by this weighting.
    // If the center pixel is above this, assume it's clipped and don't emphasize.
    constexpr double maxHFR = 10.0;
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
        // Don't emphasize stars that are too wide.
        if (centers.at(i)->HFR > maxHFR) continue;
        for (int j = 0; j < starCenters.size(); ++j)
        {
            if (starCenters.at(j) == centers.at(i))
            {
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

    Vector position(star.x, star.y, 0);
    Vector reference_position(reference.x, reference.y, 0);
    Vector arc_position, arc_reference_position;
    arc_position = point2arcsec(position);
    arc_reference_position = point2arcsec(reference_position);

    // translate into sky coords.
    Vector sky_coords = arc_position - arc_reference_position;
    sky_coords.y = -sky_coords.y; // invert y-axis as y picture axis is inverted

    // Rotate so RA aligns with x-axis and DEC with y-axis.
    sky_coords = sky_coords * ROT_Z;

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
    if (guideStarNeighbors.empty())
        return false;
    qCDebug(KSTARS_EKOS_GUIDE) << "Multistar getDrift, reticle:" << reticle_x << reticle_y
                               << "guidestar" << guideStarNeighbors[guideStarNeighborIndex].x
                               << guideStarNeighbors[guideStarNeighborIndex].y
                               << "so offsets:"
                               << (reticle_x - guideStarNeighbors[guideStarNeighborIndex].x)
                               << (reticle_y - guideStarNeighbors[guideStarNeighborIndex].y);
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

    // The snapshot we took when we found the guidestars has been offset.
    double offset_x = reticle_x - guideStarNeighbors[guideStarNeighborIndex].x;
    double offset_y = reticle_y - guideStarNeighbors[guideStarNeighborIndex].y;

    int numStarsProcessed = 0;
    bool guideStarProcessed = false;
    qCDebug(KSTARS_EKOS_GUIDE) << "Multistar: associated";
    double driftRASum = 0, driftDECSum = 0;
    double guideStarRADrift = 0, guideStarDECDrift = 0;
    QVector<double> raDrifts, decDrifts;
    for (int i = 0; i < detectedStars.size(); ++i)
    {
        const auto &star = detectedStars[i];
        auto bg = skybackground();
        double snr = bg.SNR(detectedStars[i].sum, detectedStars[i].numPixels);
        // Probably should test SNR on the reference as well.
        if (starMap[i] >= 0 && snr >= MIN_DRIFT_SNR)
        {
            auto ref = starCorrespondence.reference(starMap[i]);
            ref.x += offset_x;
            ref.y += offset_y;
            double driftRA, driftDEC;
            computeStarDrift(star, ref, &driftRA, &driftDEC);
            if (starMap[i] == guideStarNeighborIndex)
            {
                guideStarRADrift = driftRA;
                guideStarDECDrift = driftDEC;
                guideStarProcessed = true;
            }

            raDrifts.push_back(driftRA);
            decDrifts.push_back(driftDEC);
            numStarsProcessed++;

            qCDebug(KSTARS_EKOS_GUIDE) << "MultiStar: " << i << star.x << star.y
                                       << star.sum << star.HFR << snr
                                       << "<>" << starMap[i] << ref.x << ref.y << ref.sum
                                       << ref.HFR << " Dr: " << driftRA << driftDEC;
        }
        else
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "MultiStar: " << i << star.x << star.y
                                       << star.sum << star.HFR << snr << starMap[i] << " NOT ASSOCIATED";
        }
    }

    if (numStarsProcessed == 0 || !guideStarProcessed)
        return false;

    // Filter out reference star drifts that are too different from the guide-star drift.
    QVector<double> raDriftsKeep, decDriftsKeep;
    for (int i = 0; i < raDrifts.size(); ++i)
    {
        if ((fabs(raDrifts[i] - guideStarRADrift) < 2.0) &&
                (fabs(decDrifts[i] - guideStarDECDrift) < 2.0))
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

// Converts from image coordinates to arc-second coordinates. Doesn't rotate to RA/DEC.
Vector GuideStars::point2arcsec(const Vector &p) const
{
    Vector arcs;

    // arcs = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    arcs.x = 206264.8062470963552 * p.x * binning * pixel_size / focal_length;
    arcs.y = 206264.8062470963552 * p.y * binning * pixel_size / focal_length;
    return arcs;
}

void GuideStars::setCalibration(double angle_, int binning_,
                                double pixel_size_, double focal_length_)
{
    pixel_size = pixel_size_;
    focal_length = focal_length_;
    binning = binning_;
    calibration_angle = angle_;
    ROT_Z = Ekos::RotateZ(-M_PI * calibration_angle / 180.0); // NOTE!!! sing '-' derotates star coordinate system
    calibrationInitialized = true;
}
