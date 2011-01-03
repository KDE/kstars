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
 
    oSet.append( &src );
    g_score[ &src ] = 0;
    h_score[ &src ] = src.angularDistanceTo( &dest ).Degrees();
    f_score[ &src ] = h_score[ &src ];
    
    while( !oSet.isEmpty() ) {
        
        // Find the node with the lowest f_score value
        SkyPoint const *curr_node = NULL;
        double lowfscore = 1e8;
        foreach( const SkyPoint *sp, oSet ) {
            if( f_score[ sp ] < lowfscore ) {
                lowfscore = f_score[ sp ];
                curr_node = sp;
            }
        }
        
        if( curr_node == &dest ) {
            // We are at destination
            reconstructPath( came_from[ curr_node ] );
            return result_path;
        }
        
        oSet.removeOne( curr_node );
        cSet.append( curr_node );
        SkyPoint const *nhd_node;

        // Get the list of stars that are neighbours of this node
        QList<StarObject *> neighbors;
        StarComponent::Instance()->starsInAperture( neighbors, *curr_node, fov, maglim );

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
                h_score[ nhd_node ] = nhd_node->angularDistanceTo( &dest ).Degrees();
            }
        }
    }
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
        nextstar->mag(); // The brighter, the better
    
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
    double distcost = (curr->angularDistanceTo( next ).Degrees()*60 / fov); // 1 "magnitude" inc. for 1 FOV

    // Test 5: How effective is the hop? [Might not be required with A*]
    //    double distredcost = -((src->angularDistanceTo( dest ).Degrees() - next->angularDistanceTo( dest ).Degrees()) * 60 / fov)*3; // 3 "magnitudes" for 1 FOV closer

    netcost = magcost + speccost + distcost;
    return netcost;
}
