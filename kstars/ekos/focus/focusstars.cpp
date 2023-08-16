#include "focusstars.h"
#include "ekos/guide/internalguide/starcorrespondence.h"
#include <ekos_focus_debug.h>

namespace Ekos
{

namespace
{

using Ekos::FocusStars;

// Debug code to print out how well the star correspondence matched.
// Compares the star correspondence offsets from the detected "guide star position"
// with the actual detected stars. Note, those offsets were just taken from the
// first star set's detected stars.
// Move to star correspondence?
double correspondenceRmsError(const StarCorrespondence &corr, const QList<Edge> &stars2,
                              const QVector<int> &starMap, GuiderUtils::Vector gsPosition)
{
    const int s2Size = starMap.size();
    double sumSq2 = 0;
    int num = 0;
    for (int s2 = 0; s2 < s2Size; s2++)
    {
        const int s1 = starMap[s2];
        if (s1 >= 0)
        {
            // Compute the RMS association distance.
            double x2 = stars2[s2].x;
            double y2 = stars2[s2].y;

            QVector2D offset = corr.offset(s1);
            double x1 = gsPosition.x + offset.x();
            double y1 = gsPosition.y + offset.y();
            double dSq = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
            sumSq2 += dSq;
            num++;
        }
    }
    return sqrt(sumSq2 / num);
}

// Use StarCorrespondence to find a common set of stars between stars1 and stars2.
// Use mainStar1 as the "guideStar" or really the central star. We look for the pattern
// of stars around that star. It is an index into stars1. If this star is missing from stars2,
// StarCorrespondence could recover, but ideally it exists in both.
// Returns two FocusStars objects containing the sets of corresponding stars.
int matchStars(const QList<Edge> &stars1, const QList<Edge> &stars2, int mainStar1,
               double maxDistance, FocusStars *stars1Filtered, FocusStars *stars2Filtered)
{
    // minFraction is the fraction of the stars passed into StarCorrespondence (i.e. in stars1)
    // that must be found in stars2. Ideally, star1 would be a smaller list, but instead we simply
    // adjust minFraction.
    // Goal is to get 45% of the stars in the smaller star set (or more).
    int minStars = std::min(stars1.size(), stars2.size());
    int desiredStars = 0.45 * minStars;
    const double minFraction = static_cast<double>(desiredStars) / stars1.size();

    // Do the star correspondence
    QVector<int> starMap;
    StarCorrespondence corr(stars1, mainStar1);
    corr.setAllowMissingGuideStar(true);
    Edge gStar = corr.find(stars2, maxDistance, &starMap, false, minFraction);
    GuiderUtils::Vector gsPosition(gStar.x, gStar.y, 0);

    // Grab the two sets of stars.
    const int s2Size = starMap.size();
    int numAssociated = 0;
    QList<Edge> edges1, edges2;
    for (int s2 = 0; s2 < s2Size; s2++)
    {
        const int s1 = starMap[s2];
        if (s1 >= 0)
        {
            numAssociated++;
            edges1.append(stars1[s1]);
            edges2.append(stars2[s2]);
        }
    }
    *stars1Filtered = FocusStars(edges1);
    *stars2Filtered = FocusStars(edges2);

    // debug
    double corrError = correspondenceRmsError(corr, stars2, starMap, gsPosition);
    QString debugStr = QString("matchStars: Inputs sized %1 %2 found %3 matches, RMS dist %4")
                       .arg(stars1.size()).arg(stars2.size()).arg(numAssociated).arg(corrError, 0, 'f', 1);
    qCDebug(KSTARS_EKOS_FOCUS) << debugStr;

    return numAssociated;
}

bool filterFocusStars(const FocusStars &stars1, const FocusStars &stars2, double maxDistance, FocusStars *filtered1,
                      FocusStars *filtered2)
{
    int size1 = stars1.getStars().size();
    int size2 = stars2.getStars().size();

    // This section limits the process to the first 100 stars, for computational reasons,
    // just in case there is an input with a large number of stars.
    constexpr int maxNumStars = 100;
    QList<Edge> s1;
    const QList<Edge> *s1ptr = &(stars1.getStars());
    if (size1 > maxNumStars)
    {
        for (int i = 0; i < maxNumStars; i++)
            s1.append(stars1.getStars()[i]);
        s1ptr = &s1;
        size1 = maxNumStars;
    }
    QList<Edge> s2;
    const QList<Edge> *s2ptr = &(stars2.getStars());
    if (size2 > maxNumStars)
    {
        for (int i = 0; i < maxNumStars; i++)
            s2.append(stars2.getStars()[i]);
        s2ptr = &s2;
        size2 = maxNumStars;
    }

    // If we have at least 10 matching stars, or 45% of the smallest star set, that's fine.
    const int sufficientMatches = std::min(10, static_cast<int>(0.45 * std::min(size1, size2)));

    // Pick upto 5 different seed stars, if we need them.
    // Those are the stars from stars1 that are used in the StarCorrespondence algorithm.
    int maxMatches = 0;
    srand(time(0));
    FocusStars f1, f2;
    for (int i = 0; i < 5; ++i)
    {
        const int seed = rand() % size1;
        int numMatches = matchStars(*s1ptr, *s2ptr, seed, maxDistance, &f1, &f2);
        if (numMatches > maxMatches)
        {
            *filtered1 = f1;
            *filtered2 = f2;
            maxMatches = numMatches;
            if (numMatches >= sufficientMatches) break;
        }
    }
    return (maxMatches > 1);
}

}  // namespace


FocusStars::FocusStars(const QList<Edge *> edges, double maxDist)
    : maxDistance(maxDist), initialized(true)
{
    // Copy the memory--can't depend on it sticking around.
    for (const auto &edge : edges)
        stars.append(*edge);
}

FocusStars::FocusStars(const QList<Edge> edges, double maxDist)
    : maxDistance(maxDist), initialized(true)
{
    // Copy the memory--can't depend on it sticking around.
    for (const auto &edge : edges)
        stars.append(edge);
}

// Fitsdata does a bit of processing, removing saturated stars,
// removing extremes, but all this is now being done in stellarsolver.
// Just computing the median HFR.
double FocusStars::getHFR() const
{
    if (!initialized)
        return -1;

    if (computedHFR >= 0)
        return computedHFR;
    if (stars.size() == 0) return -1;
    std::vector<double> values;
    for (auto &star : stars)
        values.push_back(star.HFR);
    const int middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    computedHFR = values[middle];
    return computedHFR;
}

bool FocusStars::commonHFR(const FocusStars &referenceStars, double *thisHFR, double *referenceHFR) const
{
    if (!initialized)
        return -1;

    FocusStars commonStars, commonReferenceStars;
    bool ok = filterFocusStars(*this, referenceStars, maxDistance, &commonStars, &commonReferenceStars);
    if (!ok) return false;
    *thisHFR  = commonStars.getHFR();
    *referenceHFR = commonReferenceStars.getHFR();

    if (*thisHFR < 0 || *referenceHFR < 0 || commonStars.getStars().size() == 0 ||
            commonReferenceStars.getStars().size() == 0)
        return false;

    return true;
}

void FocusStars::printStars(const QString &label) const
{
    if (label.size() > 0) fprintf(stderr, "%s\n{", label.toLatin1().data());
    bool first = true;
    for (const auto &star : getStars())
    {
        fprintf(stderr, "%s{%6.1f,%6.1f, %5.2f}", first ? "" : ", ", star.x, star.y, star.HFR);
        first = false;
    }
    fprintf(stderr, "}\n");
}

double FocusStars::relativeHFR(const FocusStars &referenceStars, double referenceHFR) const
{
    if (!initialized)
        return -1;

    double hfrCommon, refHfrCommon;
    if (!commonHFR(referenceStars, &hfrCommon, &refHfrCommon))
    {
        qCDebug(KSTARS_EKOS_FOCUS) << "Couldn't get a relative HFR, punted!";
        // Scale the reference hfr by the HFR ratio when we can't find common stars.
        return  (getHFR() / referenceStars.getHFR()) * referenceHFR;
    }

    if (hfrCommon < 0 || refHfrCommon < 0) return -1;

    QString debugStr = QString("RelativeHFR: sizes: %7 %8 hfr: (%1 (%2) / %3 (%4)) * %5 = %6")
                       .arg(hfrCommon, 0, 'f', 2).arg(getHFR(), 0, 'f', 2).arg(refHfrCommon, 0, 'f', 2)
                       .arg(referenceStars.getHFR(), 0, 'f', 2).arg(referenceHFR, 0, 'f', 2)
                       .arg((hfrCommon / refHfrCommon) * referenceHFR, 0, 'f', 2)
                       .arg(getStars().size()).arg(referenceStars.getStars().size());
    qCDebug(KSTARS_EKOS_FOCUS) << debugStr;

    return (hfrCommon / refHfrCommon) * referenceHFR;
}

}  // namespace
