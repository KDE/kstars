/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "starcorrespondence.h"

#include <math.h>
#include "ekos_guide_debug.h"

// Finds the star in sortedStars that's closest to x,y and within maxDistance pixels.
// Returns the index of the closest star in  sortedStars, or -1 if none satisfies the criteria.
// sortedStars must be sorted by x, from lower x to higher x.
// Fills distance to the pixel distance to the closest star.
int StarCorrespondence::findClosestStar(double x, double y, const QList<Edge> sortedStars,
                                        double maxDistance, double *distance) const
{
    if (x < -maxDistance || y < -maxDistance ||
            x > imageWidth + maxDistance || y > imageWidth + maxDistance)
        return -1;

    const int size = sortedStars.size();
    int startPoint = 0;

    // Efficiency improvement to quickly find the start point.
    // Increments by size/10 to find a better start point.
    const int coarseIncrement = size / 10;
    if (coarseIncrement > 1)
    {
        for (int i = 0; i < size; i += coarseIncrement)
        {
            if (sortedStars[i].x < x - maxDistance)
                startPoint = i;
            else
                break;
        }
    }

    // Iterate through the stars starting with the star with lowest x where x >= x - maxDistance
    // through the star with greatest x where x <= x + maxDistance.  Find the closest star to x,y.
    int bestIndex = -1;
    double bestSquaredDistance = maxDistance * maxDistance;
    for (int i = startPoint; i < size; ++i)
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

namespace
{
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

int StarCorrespondence::findInternal(const QList<Edge> &stars, double maxDistance, QVector<int> *starMap,
                                     int guideStarIndex, const QVector<Offsets> &offsets,
                                     int *numFound, int *numNotFound, double minFraction) const
{
    // This is the cost of not finding one of the reference stars.
    constexpr double missingRefStarCost = 100;
    // This weight multiplies the distance between a reference star position and the closest star in stars.
    constexpr double distanceWeight = 1.0;

    // Initialize all stars to not-corresponding to any reference star.
    *starMap = QVector<int>(stars.size(), -1);

    // We won't accept a solution worse than bestCost.
    // In the default case, we need to find about half the reference stars.
    // Another possibility is a constraint on the input stars being mapped
    // E.g. that the more likely solution is one where the stars are close to the references.
    // This can be an issue if the number of input stars is way less than the number of reference stars
    // but in that case we can fail and go to the default star-finding algorithm.
    int bestCost = offsets.size() * missingRefStarCost * (1 - minFraction);
    // Note that the above implies that if stars.size() < offsets.size() * minFraction
    // then it is impossible to succeed.
    if (stars.size() < minFraction * offsets.size())
        return -1;

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
        for (int offsetIndex = 0; offsetIndex < offsets.size(); ++offsetIndex)
        {
            // Of course, the guide star has offsets 0 to itself.
            if (offsetIndex == guideStarIndex) continue;

            // We're already worse than the best cost. No need to search any more.
            if (cost > bestCost) break;

            // Look for an input star at the offset position.
            // Note the index value returned is the sorted index, which is converted back later.
            const auto &offset = offsets[offsetIndex];
            double distance;
            const int closestIndex = findClosestStar(starX + offset.x, starY + offset.y,
                                     stars, maxDistance, &distance);
            if (closestIndex < 0)
            {
                // This reference star position had no corresponding input star.
                cost += missingRefStarCost;
                numNotFound++;
                continue;
            }
            numFound++;

            // If starIndex is the star that corresponds to guideStarIndex, then
            // stars[index] corresponds to references[offsetIndex]
            mapping[closestIndex] = offsetIndex;
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
    *numFound = bestNumFound;
    *numNotFound = bestNumNotFound;
    return bestStarIndex;
}

// Converts offsets that are from the point-of-view of the guidestar, to offsets from targetStar.
void StarCorrespondence::makeOffsets(const QVector<Offsets> &offsets, QVector<Offsets> *targetOffsets,
                                     int targetStar) const
{
    targetOffsets->resize(offsets.size());
    int xOffset = offsets[targetStar].x;
    int yOffset = offsets[targetStar].y;
    for (int i = 0; i < offsets.size(); ++i)
    {
        if (i == targetStar)
        {
            (*targetOffsets)[i] = Offsets(0, 0);
        }
        else
        {
            const double x = offsets[i].x - xOffset;
            const double y = offsets[i].y - yOffset;
            (*targetOffsets)[i] = Offsets(x, y);
        }
    }

}

// We create an imaginary star from the ones we did find.
GuiderUtils::Vector StarCorrespondence::inventStarPosition(const QList<Edge> &stars, QVector<int> &starMap,
        QVector<Offsets> offsets, Offsets offset) const
{
    QVector<double> xPositions, yPositions;
    for (int i = 0; i < starMap.size(); ++i)
    {
        int reference = starMap[i];
        if (reference >= 0)
        {
            xPositions.push_back(stars[i].x - offsets[reference].x);
            yPositions.push_back(stars[i].y - offsets[reference].y);
        }
    }
    if (xPositions.size() == 0)
        return GuiderUtils::Vector(-1, -1, -1);

    // Compute the median x and y values. After gathering the values above,
    // we sort them and use the middle positions.
    std::sort(xPositions.begin(),  xPositions.end(),  [] (double d1, double d2)
    {
        return d1 < d2;
    });
    std::sort(yPositions.begin(), yPositions.end(), [] (double d1, double d2)
    {
        return d1 < d2;
    });
    const int num = xPositions.size();
    double xVal = 0, yVal = 0;
    if (num % 2 == 0)
    {
        const int middle = num / 2;
        xVal = (xPositions[middle - 1] + xPositions[middle]) / 2.0;
        yVal = (yPositions[middle - 1] + yPositions[middle]) / 2.0;
    }
    else
    {
        const int middle = num / 2;  // because it's 0-based
        xVal = xPositions[middle];
        yVal = yPositions[middle];
    }
    return GuiderUtils::Vector(xVal - offset.x, yVal - offset.y, -1);
}

namespace
{
// This utility converts the starMap to refer to indexes in the original star QVector
// instead of the sorted-by-x version.
void unmapStarMap(const QVector<int> &sortedStarMap, const QVector<int> &sortedToOriginal, QVector<int> *starMap)
{
    for (int i = 0; i < sortedStarMap.size(); ++i)
        (*starMap)[sortedToOriginal[i]] = sortedStarMap[i];
}
}

GuiderUtils::Vector StarCorrespondence::find(const QList<Edge> &stars, double maxDistance,
        QVector<int> *starMap, bool adapt, double minFraction)
{
    *starMap = QVector<int>(stars.size(), -1);
    if (!initialized)  return GuiderUtils::Vector(-1, -1, -1);
    int numFound, numNotFound;

    // findClosestStar needs an input with stars sorted by their x.
    // Do this outside of the loops.
    QList<Edge> sortedStars;
    QVector<int> sortedToOriginal;
    sortByX(stars, &sortedStars, &sortedToOriginal);

    QVector<int> sortedStarMap;
    int bestStarIndex = findInternal(sortedStars, maxDistance, &sortedStarMap, guideStarIndex,
                                     guideStarOffsets, &numFound, &numNotFound, minFraction);

    GuiderUtils::Vector starPosition(-1, -1, -1);
    if (bestStarIndex > -1)
    {
        // Convert back to the unsorted index value.
        bestStarIndex = sortedToOriginal[bestStarIndex];
        unmapStarMap(sortedStarMap, sortedToOriginal, starMap);

        starPosition = GuiderUtils::Vector(stars[bestStarIndex].x, stars[bestStarIndex].y, -1);
        qCDebug(KSTARS_EKOS_GUIDE)
                << " StarCorrespondence found guideStar at " << bestStarIndex << "found/not"
                << numFound << numNotFound;
    }
    else if (allowMissingGuideStar && bestStarIndex == -1 &&
             stars.size() >= minFraction * guideStarOffsets.size())
    {
        // If we didn't find a good solution, perhaps the guide star was not detected.
        // See if we can get a reasonable solution from the other stars.
        int bestNumFound = 0;
        int bestNumNotFound = 0;
        GuiderUtils::Vector bestPosition(-1, -1, -1);
        for (int gStarIndex = 0; gStarIndex < guideStarOffsets.size(); gStarIndex++)
        {
            if (gStarIndex == guideStarIndex)
                continue;
            QVector<Offsets> gStarOffsets;
            makeOffsets(guideStarOffsets, &gStarOffsets, gStarIndex);
            QVector<int> newStarMap;
            int detectedStarIndex = findInternal(sortedStars, maxDistance, &newStarMap,
                                                 gStarIndex, gStarOffsets,
                                                 &numFound, &numNotFound, minFraction);
            if (detectedStarIndex >= 0 && numFound > bestNumFound)
            {
                GuiderUtils::Vector position = inventStarPosition(sortedStars, newStarMap, gStarOffsets,
                                               guideStarOffsets[gStarIndex]);
                if (position.x < 0 || position.y < 0)
                    continue;

                bestPosition = position;
                bestNumFound = numFound;
                bestNumNotFound = numNotFound;
                sortedStarMap = newStarMap;

                if (numNotFound <= 1)
                    // We can't do better than this.
                    break;
            }
        }
        if (bestNumFound > 0)
        {
            // Convert back to the unsorted index value.
            unmapStarMap(sortedStarMap, sortedToOriginal, starMap);
            qCDebug(KSTARS_EKOS_GUIDE)
                    << "StarCorrespondence found guideStar (invented) at "
                    << bestPosition.x << bestPosition.y << "found/not" << bestNumFound << bestNumNotFound;
            return bestPosition;
        }
        else qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence could not invent guideStar.";
    }
    else qCDebug(KSTARS_EKOS_GUIDE) << "StarCorrespondence could not find guideStar.";

    if (adapt && (bestStarIndex != -1))
        adaptOffsets(stars, *starMap);
    return starPosition;
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
