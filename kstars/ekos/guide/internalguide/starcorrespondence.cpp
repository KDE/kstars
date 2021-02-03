/*  Correspondence class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "starcorrespondence.h"

#include <math.h>
#include "ekos_guide_debug.h"

namespace
{

// Finds the star in sortedStars that's closest to x,y and within maxDistance pixels.
// Returns the index of the closest star in  sortedStars, or -1 if none satisfies the criteria.
// sortedStars must be sorted by x, from lower x to higher x.
// Fills distance to the pixel distance to the closest star.
int findClosestStar(double x, double y, const QList<Edge> sortedStars,
                    double maxDistance, double *distance)
{
    // Iterate through the stars starting with the star with lowest x where x >= x - maxDistance
    // through the star with greatest x where x <= x + maxDistance.  Find the closest star to x,y.
    int bestIndex = -1;
    double bestSquaredDistance = maxDistance * maxDistance;
    const int size = sortedStars.size();
    for (int i = 0; i < size; ++i)
    {
        auto &star = sortedStars[i];
        if (star.x < x - maxDistance) continue;
        if (star.x > x + maxDistance) break;
        const double xDiff = star.x - x;
        const double yDiff = star.y - y;
        const double squaredDistance = xDiff * xDiff + yDiff * yDiff;
        if (squaredDistance <= bestSquaredDistance)
        {
            bestIndex = i;
            bestSquaredDistance = squaredDistance;
        }
    }
    if (distance != nullptr) *distance = sqrt(bestSquaredDistance);
    return bestIndex;
}

// Sorts stars by their x values, places the sorted stars into sortedStars.
// sortedToOriginal is a map whose index is the index of a star in sortedStars and whose
// value is the index of a star in stars.
// Therefore, sortedToOrigial[a] = b, means that sortedStars[a] corresponds to stars[b].
void sortByX(const QList<Edge> &stars, QList<Edge> *sortedStars, QVector<int> *sortedToOriginal)
{
    sortedStars->clear();
    sortedToOriginal->clear();

    // Sort the stars by their x-positions, lower to higher.
    QVector<std::pair<int, float>> rx;
    for (int i = 0; i < stars.size(); ++i)
        rx.push_back(std::pair<int, float>(i, stars[i].x));
    std::sort(rx.begin(), rx.end(), [](const std::pair<int, float> &a, const std::pair<int, double> &b)
    {
        return a.second < b.second;
    });
    for (int i = 0; i < stars.size(); ++i)
    {
        sortedStars->append(stars[ rx[i].first ]);
        sortedToOriginal->push_back(rx[i].first);
    }
}

}  // namespace

StarCorrespondence::StarCorrespondence(const QList<Edge> &stars, int guideStar)
{
    initialize(stars, guideStar);
}

StarCorrespondence::StarCorrespondence()
{
}

// Initialize with the reference stars and the guide-star index.
void StarCorrespondence::initialize(const QList<Edge> &stars, int guideStar)
{
    qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence initialized with " << stars.size() << guideStar;
    references = stars;
    guideStarIndex = guideStar;
    float guideX = references[guideStarIndex].x;
    float guideY = references[guideStarIndex].y;
    guideStarOffsets.clear();

    // Compute the x and y offsets from the guide star to all reference stars
    // and store them in guideStarOffsets.
    const int numRefs = references.size();
    for (int i = 0; i < numRefs; ++i)
    {
        const auto &ref = references[i];
        guideStarOffsets.push_back(Offsets(ref.x - guideX, ref.y - guideY));
    }

    initializeAdaptation();

    initialized = true;
}

void StarCorrespondence::reset()
{
    references.clear();
    guideStarOffsets.clear();
    initialized = false;
}

void StarCorrespondence::find(const QList<Edge> &stars, double maxDistance, QVector<int> *starMap, bool adapt,
                              double minFraction)
{
    // This is the cost of not finding one of the reference stars.
    constexpr double missingRefStarCost = 100;
    // This weight multiplies the distance between a reference star position and the closest star in stars.
    constexpr double distanceWeight = 1.0;

    // Initialize all stars to not-corresponding to any reference star.
    *starMap = QVector<int>(stars.size(), -1);

    if (!initialized) return;

    // findClosestStar needs an input with stars sorted by their x.
    // Do this outside of the loop.
    QList<Edge> sortedStars;
    QVector<int> sortedToOriginal;
    sortByX(stars, &sortedStars, &sortedToOriginal);

    // We won't accept a solution worse than bestCost.
    // In the default case, we need to find about half the reference stars.
    // Another possibility is a constraint on the input stars being mapped
    // E.g. that the more likely solution is one where the stars are close to the references.
    // This can be an issue if the number of input stars is way less than the number of reference stars
    // but in that case we can fail and go to the default star-finding algorithm.
    int bestCost = guideStarOffsets.size() * missingRefStarCost * (1 - minFraction);

    // Assume the guide star corresponds to each of the stars.
    // Score the assignment, pick the best, and then assign the rest.
    const int numStars = stars.size();
    int bestStarIndex = -1, bestNumFound = 0, bestNumNotFound = 0;
    for (int starIndex = 0; starIndex < numStars; ++starIndex)
    {
        const float starX = stars[starIndex].x;
        const float starY = stars[starIndex].y;

        double cost = 0.0;
        QVector<int> mapping(stars.size(), -1);
        int numFound = 0, numNotFound = 0;
        for (int offsetIndex = 0; offsetIndex < guideStarOffsets.size(); ++offsetIndex)
        {
            // Of course, the guide star has offsets 0 to itself.
            if (offsetIndex == guideStarIndex) continue;

            // We're already worse than the best cost. No need to search any more.
            if (cost > bestCost) break;

            // Look for an input star at the offset position.
            // Note the index value returned is the sorted index, which is converted back later.
            const auto &offset = guideStarOffsets[offsetIndex];
            double distance;
            const int sortedIndex = findClosestStar(starX + offset.x, starY + offset.y,
                                                    sortedStars, maxDistance, &distance);
            if (sortedIndex < 0)
            {
                // This reference star position had no corresponding input star.
                cost += missingRefStarCost;
                numNotFound++;
                continue;
            }
            numFound++;
            // Convert to the unsorted index value.
            const int index = sortedToOriginal[sortedIndex];

            // If starIndex is the star that corresponds to guideStarIndex, then
            // stars[index] corresponds to references[offsetIndex]
            mapping[index] = offsetIndex;
            cost += distance * distanceWeight;
        }
        if (cost < bestCost)
        {
            bestCost = cost;
            bestStarIndex = starIndex;
            bestNumFound = numFound;
            bestNumNotFound = numNotFound;

            *starMap = QVector<int>(stars.size(), -1);
            for (int i = 0; i < mapping.size(); ++i)
                (*starMap)[i] = mapping[i];
            (*starMap)[starIndex] = guideStarIndex;
        }
    }
    if (bestStarIndex >= 0)
        qCDebug(KSTARS_EKOS_GUIDE)
                << " StarCorrespondence found guideStar at " << bestStarIndex << "found/not"
                << bestNumFound << bestNumNotFound;
    if (adapt)
        adaptOffsets(stars, *starMap);
}

// Compute the alpha cooeficient for a simple IIR filter used in adaptOffsets()
// that causes the offsets to be, roughtly, the moving average of the last timeConstant samples.
// See the discussion here:
// https://dsp.stackexchange.com/questions/378/what-is-the-best-first-order-iir-ar-filter-approximation-to-a-moving-average-f
void StarCorrespondence::initializeAdaptation()
{
    // Approximately average the last 25 samples.
    const double timeConstant = 25.0;
    alpha = 1.0 / pow(timeConstant, 0.865);

    // Don't let the adapted offsets move far from the original ones.
    originalGuideStarOffsets = guideStarOffsets;
}

void StarCorrespondence::adaptOffsets(const QList<Edge> &stars, const QVector<int> &starMap)
{
    const int numStars = stars.size();
    if (starMap.size() != numStars)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Adapt error: StarMap size != stars.size()" << starMap.size() << numStars;
        return;
    }
    // guideStar will be the index in stars corresponding to the reference guide star.
    int guideStar = -1;
    for (int i = 0; i < numStars; ++i)
    {
        if (starMap[i] == guideStarIndex)
        {
            guideStar = i;
            break;
        }
    }
    if (guideStar < 0)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Adapt error: no guide star in map";
        return;
    }

    for (int i = 0; i < numStars; ++i)
    {
        // We don't adapt if the ith star doesn't correspond to any of the references,
        // or if it corresponds to the guide star (whose offsets are, of course, always 0).
        const int refIndex = starMap[i];
        if (refIndex == -1 || refIndex == guideStarIndex)
            continue;

        if ((refIndex >= references.size()) || (refIndex < 0))
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "Adapt error: bad refIndex[" << i << "] =" << refIndex;
            return;
        }
        // Adapt the x and y offsets using the following IIR filter:
        // output[t] = alpha * offset[t] + (1-alpha) * output[t-1]
        const double xOffset = stars[i].x - stars[guideStar].x;
        const double yOffset = stars[i].y - stars[guideStar].y;
        const double currentXOffset = guideStarOffsets[refIndex].x;
        const double currentYOffset = guideStarOffsets[refIndex].y;
        const double newXOffset = alpha * xOffset + (1 - alpha) * currentXOffset;
        const double newYOffset = alpha * yOffset + (1 - alpha) * currentYOffset;

        // The adaptation is meant for small movements of at most a few pixels.
        // We don't want it to move enough to find a new star.
        constexpr double maxMovement = 2.5;  // pixels
        if ((fabs(newXOffset - originalGuideStarOffsets[refIndex].x) < maxMovement) &&
                (fabs(newYOffset - originalGuideStarOffsets[refIndex].y) < maxMovement))
        {
            guideStarOffsets[refIndex].x = newXOffset;
            guideStarOffsets[refIndex].y = newYOffset;
        }
    }
}
