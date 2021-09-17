/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "starhopper.h"

#include "kstarsdata.h"
#include "ksutils.h"
#include "starcomponent.h"
#include "skyobjects/starobject.h"

#include <kstars_debug.h>

QList<StarObject *> *StarHopper::computePath(const SkyPoint &src, const SkyPoint &dest, float fov__, float maglim__,
                                             QStringList *metadata_)
{
    QList<const StarObject *> starHopList_const = computePath_const(src, dest, fov__, maglim__, metadata_);
    QList<StarObject *> *starHopList_unconst    = new QList<StarObject *>();
    foreach (const StarObject *so, starHopList_const)
    {
        starHopList_unconst->append(const_cast<StarObject *>(so));
    }
    return starHopList_unconst;
}

QList<const StarObject *> StarHopper::computePath_const(const SkyPoint &src, const SkyPoint &dest, float fov_,
                                                        float maglim_, QStringList *metadata)
{
    fov    = fov_;
    maglim = maglim_;
    start  = &src;
    end    = &dest;

    came_from.clear();
    result_path.clear();

    // Implements the A* search algorithm

    QList<SkyPoint const *> cSet;
    QList<SkyPoint const *> oSet;
    QHash<SkyPoint const *, double> g_score;
    QHash<SkyPoint const *, double> f_score;
    QHash<SkyPoint const *, double> h_score;

    qCDebug(KSTARS) << "StarHopper is trying to compute a path from source: " << src.ra().toHMSString()
             << src.dec().toDMSString() << " to destination: " << dest.ra().toHMSString() << dest.dec().toDMSString()
             << "; a starhop of " << src.angularDistanceTo(&dest).Degrees() << " degrees!";

    oSet.append(&src);
    g_score[&src] = 0;
    h_score[&src] = src.angularDistanceTo(&dest).Degrees() / fov;
    f_score[&src] = h_score[&src];

    while (!oSet.isEmpty())
    {
        qCDebug(KSTARS) << "Next step";
        // Find the node with the lowest f_score value
        SkyPoint const *curr_node = nullptr;
        double lowfscore          = 1.0e8;

        foreach (const SkyPoint *sp, oSet)
        {
            if (f_score[sp] < lowfscore)
            {
                lowfscore = f_score[sp];
                curr_node = sp;
            }
        }
        if (curr_node == nullptr)
            continue;

        qCDebug(KSTARS) << "Lowest fscore (vertex distance-plus-cost score) is " << lowfscore
                 << " with coords: " << curr_node->ra().toHMSString() << curr_node->dec().toDMSString()
                 << ". Considering this node now.";
        if (curr_node == &dest || (curr_node != &src && h_score[curr_node] < 0.5))
        {
            // We are at destination
            reconstructPath(came_from[curr_node]);
            qCDebug(KSTARS) << "We've arrived at the destination! Yay! Result path count: " << result_path.count();

            // Just a test -- try to print out useful instructions to the debug console. Once we make star hopper unexperimental, we should move this to some sort of a display
            qCDebug(KSTARS) << "Star Hopping Directions: ";
            const SkyPoint *prevHop = start;

            for (auto &hopStar : result_path)
            {
                QString direction;
                QString spectralChar = "";
                double pa; // should be 0 to 2pi

                dms angDist = prevHop->angularDistanceTo(hopStar, &pa);

                dms dmsPA;
                dmsPA.setRadians(pa);
                direction = KSUtils::toDirectionString(dmsPA);

                if (metadata)
                {
                    if (!patternNames.contains(hopStar))
                    {
                        spectralChar += hopStar->spchar();
                        starHopDirections = i18n(" Slew %1 degrees %2 to find an %3 star of mag %4 ", QString::number(angDist.Degrees(), 'f', 2),
                                                 direction, spectralChar, QString::number(hopStar->mag()));
                        qCDebug(KSTARS) << starHopDirections;
                    }
                    else
                    {
                        starHopDirections = i18n(" Slew %1 degrees %2 to find a(n) %3", QString::number(angDist.Degrees(), 'f', 2), direction,
                                                     patternNames.value(hopStar));
                        qCDebug(KSTARS) << starHopDirections;
                    }
                    metadata->append(starHopDirections);
                    qCDebug(KSTARS) << "Appended starHopDirections to metadata";
                }
                prevHop = hopStar;
            }
            qCDebug(KSTARS) << "  The destination is within a field-of-view";

            return result_path;
        }

        oSet.removeOne(curr_node);
        cSet.append(curr_node);

        // FIXME: Make sense. If current node ---> dest distance is
        // larger than src --> dest distance by more than 20%, don't
        // even bother considering it.

        if (h_score[curr_node] > h_score[&src] * 1.2)
        {
            qCDebug(KSTARS) << "Node under consideration has larger distance to destination (h-score) than start node! "
                        "Ignoring it.";
            continue;
        }

        // Get the list of stars that are neighbours of this node
        QList<StarObject *> neighbors;

        // FIXME: Actually, this should be done in
        // HorizontalToEquatorial, but we do it here because SkyPoint
        // needs a lot of fixing to handle unprecessed and precessed,
        // equatorial and horizontal coordinates nicely
        SkyPoint *CurrentNode = const_cast<SkyPoint *>(curr_node);
        //CurrentNode->deprecess(KStarsData::Instance()->updateNum());
        CurrentNode->catalogueCoord(KStarsData::Instance()->updateNum()->julianDay());
        //qCDebug(KSTARS) << "Calling starsInAperture";
        StarComponent::Instance()->starsInAperture(neighbors, *curr_node, fov, maglim);
        qCDebug(KSTARS) << "Choosing next node from a set of " << neighbors.count();
        // Look for the potential next node
        double curr_g_score = g_score[curr_node];

        for (auto &nhd_node : neighbors)
        {
            if (cSet.contains(nhd_node))
                continue;

            // Compute the tentative g_score
            double tentative_g_score = curr_g_score + cost(curr_node, nhd_node);
            bool tentative_better;
            if (!oSet.contains(nhd_node))
            {
                oSet.append(nhd_node);
                tentative_better = true;
            }
            else if (tentative_g_score < g_score[nhd_node])
                tentative_better = true;
            else
                tentative_better = false;

            if (tentative_better)
            {
                came_from[nhd_node] = curr_node;
                g_score[nhd_node]   = tentative_g_score;
                h_score[nhd_node]   = nhd_node->angularDistanceTo(&dest).Degrees() / fov;
                f_score[nhd_node]   = g_score[nhd_node] + h_score[nhd_node];
            }
        }
    }
    qCDebug(KSTARS) << "REGRET! Returning empty list!";
    return QList<StarObject const *>(); // Return an empty QList
}

void StarHopper::reconstructPath(SkyPoint const *curr_node)
{
    if (curr_node != start)
    {
        StarObject const *s = dynamic_cast<StarObject const *>(curr_node);
        Q_ASSERT(s);
        result_path.prepend(s);
        reconstructPath(came_from[s]);
    }
}

float StarHopper::cost(const SkyPoint *curr, const SkyPoint *next)
{
    // This is a very heuristic method, that tries to produce a cost
    // for each hop.

    float netcost = 0;

    // Convert 'next' into a StarObject
    if (next == start)
    {
        // If the next hop is back to square one, junk it
        return 1e8;
    }
    bool isThisTheEnd = (next == end);

    float magcost, speccost;
    magcost = speccost = 0;

    if (!isThisTheEnd)
    {
        // We ought to be dealing with a star
        StarObject const *nextstar = dynamic_cast<StarObject const *>(next);
        Q_ASSERT(nextstar);

        // Test 1: How bright is the star?
        magcost =
            nextstar->mag() - 7.0 +
            5 * log10(
                    fov); // The brighter, the better. FIXME: 8.0 is now an arbitrary reference to the average faint star. Should actually depend on FOV, something like log( FOV ).

        // Test 2: Is the star strikingly red / yellow coloured?
        QString SpType = nextstar->sptype();
        char spclass   = SpType.at(0).toLatin1();
        speccost       = (spclass == 'G' || spclass == 'K' || spclass == 'M') ? -0.3 : 0;
        /*
        // Test 3: Is the star in the general direction of the object?
        // We use the cosine rule to find the angle between the hop direction, and the direction to destination
        // a = side joining curr to end
        // b = side joining curr to next
        // c = side joining next to end
        // C = angle between curr-next and curr-end
        double sina, sinb, cosa, cosb;
        curr->angularDistanceTo(dest).SinCos( &sina, &cosa );
        curr->angularDistanceTo(next).SinCos( &sinb, &cosb );
        double cosc = cos(next->angularDistanceTo(end).radians());
        double cosC = ( cosc - cosa * cosb ) / (sina * sinb);
        float dircost;
        if( cosC < 0 ) // Wrong direction!
        dircost = 1e8; // Some humongous number;
        else
        dircost = sqrt( 1 - cosC * cosC ) / cosC; // tan( C )
        */
    }

    // Test 4: How far is the hop?
    double distcost =
        (curr->angularDistanceTo(next).Degrees() /
         fov); // 1 "magnitude" incremental cost for 1 FOV. Is this even required, or is it just equivalent to halving our distance unit? I think it is required since the hop is not necessarily in the direction of the object -- asimha

    // Test 5: How effective is the hop? [Might not be required with A*]
    //    double distredcost = -((src->angularDistanceTo( dest ).Degrees() - next->angularDistanceTo( dest ).Degrees()) * 60 / fov)*3; // 3 "magnitudes" for 1 FOV closer

    // Test 6: Is the destination an asterism? Are there bright stars clustered nearby?
    QList<StarObject *> localNeighbors;
    StarComponent::Instance()->starsInAperture(localNeighbors, *next, fov / 10, maglim + 1.0);
    double stardensitycost = 1 - localNeighbors.count(); // -1 "magnitude" for every neighbouring star

// Test 7: Identify star patterns

#define RIGHT_ANGLE_THRESHOLD 0.05
#define EQUAL_EDGE_THRESHOLD  0.025

    double patterncost = 0;
    QString patternName;
    if (!isThisTheEnd)
    {
        // We ought to be dealing with a star
        StarObject const *nextstar = dynamic_cast<StarObject const *>(next);
        Q_ASSERT(nextstar);

        float factor = 1.0;
        while (factor <= 10.0)
        {
            localNeighbors.clear();
            StarComponent::Instance()->starsInAperture(
                localNeighbors, *next, fov / factor,
                nextstar->mag() + 1.0); // Use a larger aperture for pattern identification; max 1.0 mag difference
            foreach (StarObject *star, localNeighbors)
            {
                if (star == nextstar)
                    localNeighbors.removeOne(star);
                else if (fabs(star->mag() - nextstar->mag()) > 1.0)
                    localNeighbors.removeOne(star);
            } // Now, we should have a pruned list
            factor += 1.0;
            if (localNeighbors.size() == 2)
                break;
        }
        factor -= 1.0;
        if (localNeighbors.size() == 2)
        {
            patternName = i18n("triangle (of similar magnitudes)"); // any three stars form a triangle!
            // Try to find triangles. Note that we assume that the standard Euclidian metric works on a sphere for small angles, i.e. the celestial sphere is nearly flat over our FOV.
            StarObject *star1 = localNeighbors[0];
            double dRA1       = nextstar->ra().radians() - star1->ra().radians();
            double dDec1      = nextstar->dec().radians() - star1->dec().radians();
            double dist1sqr   = dRA1 * dRA1 + dDec1 * dDec1;

            StarObject *star2 = localNeighbors[1];
            double dRA2       = nextstar->ra().radians() - star2->ra().radians();
            double dDec2      = nextstar->dec().radians() - star2->dec().radians();
            double dist2sqr   = dRA2 * dRA2 + dDec2 * dDec2;

            // Check for right-angled triangles (without loss of generality, right angle is at this vertex)
            if (fabs((dRA1 * dRA2 - dDec1 * dDec2) / sqrt(dist1sqr * dist2sqr)) < RIGHT_ANGLE_THRESHOLD)
            {
                // We have a right angled triangle! Give -3 magnitudes!
                patterncost += -3;
                patternName = i18n("right-angled triangle");
            }

            // Check for isosceles triangles (without loss of generality, this is the vertex)
            if (fabs((dist1sqr - dist2sqr) / (dist1sqr)) < EQUAL_EDGE_THRESHOLD)
            {
                patterncost += -1;
                patternName = i18n("isosceles triangle");
                if (fabs((dRA2 * dDec1 - dRA1 * dDec2) / sqrt(dist1sqr * dist2sqr)) < RIGHT_ANGLE_THRESHOLD)
                {
                    patterncost += -1;
                    patternName = i18n("straight line of 3 stars");
                }
                // Check for equilateral triangles
                double dist3    = star1->angularDistanceTo(star2).radians();
                double dist3sqr = dist3 * dist3;
                if (fabs((dist3sqr - dist1sqr) / dist1sqr) < EQUAL_EDGE_THRESHOLD)
                {
                    patterncost += -1;
                    patternName = i18n("equilateral triangle");
                }
            }
        }
        // TODO: Identify squares.
        if (!patternName.isEmpty())
        {
            patternName += i18n(" within %1% of FOV of the marked star", (int)(100.0 / factor));
            patternNames.insert(nextstar, patternName);
        }
    }

    netcost = magcost + speccost + distcost + stardensitycost + patterncost;
    if (netcost < 0)
        netcost = 0.1; // FIXME: Heuristics aren't supposed to be entirely random. This one is.
    qCDebug(KSTARS) << "Mag cost: " << magcost << "; Spec Cost: " << speccost << "; Dist Cost: " << distcost
             << "; Density cost: " << stardensitycost << "; Pattern cost: " << patterncost << "; Net cost: " << netcost
             << "; Pattern: " << patternName;
    return netcost;
}
