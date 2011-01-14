/***************************************************************************
                starhopper.h  -  Star Hopping Guide for KStars
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

#ifndef _STARHOPPER_H_
#define _STARHOPPER_H_

/**
 *@class StarHopper
 *@short Helps planning star hopping
 *@version 1.0
 *@author Akarsh Simha
 */

#include "skypoint.h"
#include "skyobject.h"
#include "starobject.h"

class StarHopper {

 public:

    QList<StarObject const *> computePath( const SkyPoint &src, const SkyPoint &dest, float fov_, float maglim_ );

 private:
    float fov;
    float maglim;

    // Useful for internal computations
    SkyPoint const *start;
    SkyPoint const *end;
    QHash<const SkyPoint *, const SkyPoint *> came_from; // Used by the A* search algorithm
    QList<StarObject const *> result_path;

    /**
     *@short The cost function for hopping from current position to the a given star, in view of the final destination
     *@param curr Source SkyPoint
     *@param next Next point in the hop.
     *@note If 'next' is neither the starting point of the hop, nor
     * the ending point, it _has_ to be a StarObject. A dynamic cast
     * followed by a Q_ASSERT will ensure this.
     */
    float cost( const SkyPoint *curr, const SkyPoint *next );

    /**
     *@short For internal use by the A* Search Algorithm. Completes
     * the star-hop path. See
     * http://en.wikipedia.org/wiki/A*_search_algorithm for details
     */
    void reconstructPath( SkyPoint const *curr_node );

};

#endif
