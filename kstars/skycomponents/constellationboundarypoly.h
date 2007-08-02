/***************************************************************************
               constellationboundarypoly.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-10
    copyright            : (C) 2007 James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATIONBOUNDARYPOLY_H
#define CONSTELLATIONBOUNDARYPOLY_H

#include "polylistindex.h"

#include <QHash>
#include <QPolygonF>

class PolyList;

typedef QVector<PolyList*> PolyListList;


/* @class ConstellationBoundaryPoly
 * Holds the outlines of every constellation boundary in PolygonF's.
 *
 * @author James B. Bowlin
 * @version 0.1
*/

class ConstellationBoundaryPoly : public PolyListIndex
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		ConstellationBoundaryPoly( SkyComponent *parent );

        /* @short this is now required to precess the boundaries.
         */
        void update( KStarsData *data, KSNumbers *num );

		const QPolygonF& boundary( const QString &name ) const;

		QString constellationName( SkyPoint *p );

    	bool inConstellation( const QString &name, SkyPoint *p );

    private:

};


#endif
