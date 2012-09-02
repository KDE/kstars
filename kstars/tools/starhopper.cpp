/***************************************************************************
               starhopper.cpp  -  Star Hopping Guide for KStars
                             -------------------
    begin                : Mon 23rd Aug 2010
    copyright            : (C) 2010 Akarsh Simha
    email                : akarshsimha@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "starhopper.h"

#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "starcomponent.h"

#include "kstarsdata.h"

#include <QList>


QList<const StarObject *> StarHopper::computePath( const SkyPoint &src, const SkyPoint &dest, float fov_, float maglim_ ) {

    fov = fov_;
    maglim = maglim_;
    start = &src;
    end = &dest;

    came_from.clear();
    result_path.clear();
    

    // Implements the A* search algorithm
    
    QList<SkyPoint const *> cSet;
    QList<SkyPoint const *> oSet;
    QHash<SkyPoint const *, double> g_score;
    QHash<SkyPoint const *, double> f_score;
    QHash<SkyPoint const *, double> h_score;

    kDebug() << "StarHopper is trying to compute a path from source: " << src.ra().toHMSString() << src.dec().toDMSString() << " to destination: " << dest.ra().toHMSString() << dest.dec().toDMSString() << "; a starhop of " << src.angularDistanceTo( &dest ).Degrees() << " degrees!";

    oSet.append( &src );
    g_score[ &src ] = 0;
    h_score[ &src ] = src.angularDistanceTo( &dest ).Degrees()/fov;
    f_score[ &src ] = h_score[ &src ];
    
    while( !oSet.isEmpty() ) {
        kDebug() << "Next step";
        // Find the node with the lowest f_score value
        SkyPoint const *curr_node = NULL;
        double lowfscore = 1.0e8;
        foreach( const SkyPoint *sp, oSet ) {
            if( f_score[ sp ] < lowfscore ) {
                lowfscore = f_score[ sp ];
                curr_node = sp;
            }
        }
        kDebug() << "Lowest fscore (vertex distance-plus-cost score) is " << lowfscore << " with coords: " << curr_node->ra().toHMSString() << curr_node->dec().toDMSString() << ". Considering this node now.";
        if( curr_node == &dest || (curr_node != &src && h_score[ curr_node ] < 0.5) ) {
            // We are at destination
            reconstructPath( came_from[ curr_node ] );
            kDebug() << "We've arrived at the destination! Yay! Result path count: " << result_path.count();

            // Just a test -- try to print out useful instructions to the debug console. Once we make star hopper unexperimental, we should move this to some sort of a display
            kDebug() << "Star Hopping Directions: ";
            const SkyPoint *prevHop = start;
            foreach( const StarObject *hopStar, result_path ) {
                QString direction;
                double pa; // should be 0 to 2pi
                dms angDist = prevHop->angularDistanceTo( hopStar, &pa );
                Q_ASSERT( pa >= 0 && pa <= 2 * M_PI );
                if( pa < M_PI/4 || pa > 7*M_PI/4)
                    direction = "North";
                else if( pa >= M_PI/4 && pa < 3*M_PI/4 )
                    direction = "East";
                else if( pa >= 3* M_PI/4 && pa < 5*M_PI/4 )
                    direction = "South";
                else
                    direction = "West";
                kDebug() << "  Slew " << angDist.Degrees() << " degrees " << direction << " to find a " << hopStar->spchar() << " star of mag " << hopStar->mag();
                prevHop = hopStar;
            }
            kDebug() << "  The destination is within a field-of-view";

            return result_path;
        }
        
        oSet.removeOne( curr_node );
        cSet.append( curr_node );

        // FIXME: Make sense. If current node ---> dest distance is
        // larger than src --> dest distance by more than 20%, don't
        // even bother considering it.

        if( h_score[ curr_node ] > h_score[ &src ] * 1.2 ) {
            kDebug() << "Node under consideration has larger distance to destination (h-score) than start node! Ignoring it.";
            continue;
        }

        SkyPoint const *nhd_node;

        // Get the list of stars that are neighbours of this node
        QList<StarObject *> neighbors;

        // FIXME: Actually, this should be done in
        // HorizontalToEquatorial, but we do it here because SkyPoint
        // needs a lot of fixing to handle unprecessed and precessed,
        // equatorial and horizontal coordinates nicely
        SkyPoint *CurrentNode = const_cast<SkyPoint *>(curr_node);
        CurrentNode->deprecess( KStarsData::Instance()->updateNum() );
        kDebug() << "Calling starsInAperture";
        StarComponent::Instance()->starsInAperture( neighbors, *curr_node, fov, maglim );
        kDebug() << "Choosing next node from a set of " << neighbors.count();
        // Look for the potential next node
        double curr_g_score = g_score[ curr_node ];
        foreach( nhd_node, neighbors ) {
            if( cSet.contains( nhd_node ) )
                continue;
            
            // Compute the tentative g_score
            double tentative_g_score = curr_g_score + cost( curr_node, nhd_node );
            bool tentative_better;
            if( !oSet.contains( nhd_node ) ) {
                oSet.append( nhd_node );
                tentative_better = true;
            }
            else if( tentative_g_score < g_score[ nhd_node ] )
                tentative_better = true;
            else
                tentative_better = false;
            
            if( tentative_better ) {
                came_from[ nhd_node ] = curr_node;
                g_score[ nhd_node ] = tentative_g_score;
                h_score[ nhd_node ] = nhd_node->angularDistanceTo( &dest ).Degrees() / fov;
                f_score[ nhd_node ] = g_score[ nhd_node ] + h_score[ nhd_node ];
            }
        }
    }
    kDebug() << "REGRET! Returning empty list!";
    return QList<StarObject const *>(); // Return an empty QList
}

void StarHopper::reconstructPath( SkyPoint const *curr_node ) {
    if( curr_node != start ) {
        StarObject const *s = dynamic_cast<StarObject const *>(curr_node);
        Q_ASSERT( s );
        result_path.prepend( s );
        reconstructPath( came_from[ s ] );
    }
}

float StarHopper::cost( const SkyPoint *curr, const SkyPoint *next ) {

    // This is a very heuristic method, that tries to produce a cost
    // for each hop.

    float netcost = 0;

    // Convert 'next' into a StarObject
    if( next == start ) {
        // If the next hop is back to square one, junk it
        return 1e8;
    }
    bool isThisTheEnd = (next == end);

    float magcost, speccost;
    magcost = speccost = 0;

    if( !isThisTheEnd ) {
        // We ought to be dealing with a star
        StarObject const *nextstar = dynamic_cast<StarObject const *>(next);
        Q_ASSERT( nextstar );

        // Test 1: How bright is the star?
        magcost = nextstar->mag() - 7.0 + 5 * log10( fov ); // The brighter, the better. FIXME: 8.0 is now an arbitrary reference to the average faint star. Should actually depend on FOV, something like log( FOV ).
    
        // Test 2: Is the star strikingly red / yellow coloured?
        QString SpType = nextstar->sptype();
        char spclass = SpType.at( 0 ).toAscii();
        speccost = ( spclass == 'G' || spclass == 'K' || spclass == 'M' ) ? -1 : 0;
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
    double distcost = (curr->angularDistanceTo( next ).Degrees() / fov); // 1 "magnitude" incremental cost for 1 FOV. Is this even required, or is it just equivalent to halving our distance unit? I think it is required since the hop is not necessarily in the direction of the object -- asimha

    // Test 5: How effective is the hop? [Might not be required with A*]
    //    double distredcost = -((src->angularDistanceTo( dest ).Degrees() - next->angularDistanceTo( dest ).Degrees()) * 60 / fov)*3; // 3 "magnitudes" for 1 FOV closer

    // Test 5: Is this an asterism, or are there bright stars clustered nearby?
    QList<StarObject *> localNeighbors;
    StarComponent::Instance()->starsInAperture( localNeighbors, *curr, fov/10, maglim + 1.0 );
    double stardensitycost = 1 - localNeighbors.count(); // -1 "magnitude" for every neighbouring star

    netcost = magcost /*+ speccost*/ + distcost + stardensitycost;
    if( netcost < 0 )
        netcost = 0.1; // FIXME: Heuristics aren't supposed to be entirely random. This one is.
    kDebug() << "Mag cost: " << magcost << "; Spec Cost: " << speccost << "; Dist Cost: " << distcost << "; Net cost: " << netcost;

    return netcost;
}
