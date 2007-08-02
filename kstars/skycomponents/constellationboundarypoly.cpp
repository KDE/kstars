/***************************************************************************
                 constellationboundarypoly.cpp -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-10
    copyright            : (C) 2007 by James B. Bowlin
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

#include <stdio.h>

#include <kdebug.h>
#include <klocale.h>
#include <QPolygonF>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobject.h"

#include "polylist.h"
#include "constellationboundarypoly.h"

#include "skymesh.h"

ConstellationBoundaryPoly::ConstellationBoundaryPoly( SkyComponent *parent )
  : PolyListIndex( parent )
{}


//-------------------------------------------------------------------
// The routines for providing public access to the the boundary index
// start here.  (Some of them may not be needed (or working)).
//-------------------------------------------------------------------

QString ConstellationBoundaryPoly::constellationName( SkyPoint *p ) 
{
    PolyList *poly = ContainingPoly( p );
    if ( poly ) return poly->name();

 	return i18n("Unknown");
}


bool ConstellationBoundaryPoly::inConstellation( const QString &name, SkyPoint *p )
{
    PolyList* polyList = nameHash().value( name );
    if ( ! polyList ) return false;
    const QPolygonF& poly = polyList->poly();
	if ( poly.containsPoint( QPointF( p->ra()->Hours(), 
                             p->dec()->Degrees() ), Qt::OddEvenFill ) )
	    return true;

	return false;
}

// We don't need the rotation update but we still need to precess (I think).
// This is a minor nightmare.  If we need to precess the boundaries then
// we may want to rethink the data storage.
void ConstellationBoundaryPoly::update( KStarsData *data, KSNumbers *num )
{
    /** -jbb need to use an iterator over the PolgonF in order to
     * update then inclosed ra, dec values.

    if ( ! num ) return;
    PolyIndex* index = polyIndex();
    for (int i = 0; i < index->size(); i++ ) {
        PolyListList* listList = index->at( i );
        for ( int j = 0; j < listList->size(); j++ ) {
            QPolygonF poly = listList->at( j )->poly();
            for ( int k = 0; k < poly->size(); k++ ) {
                QPointF p = 
                double ra = poly->at( k ).x();
                double dec = poly->at( k ).y();
                SkyPoint p( ra, dec );
                p->updateCoords( num );

            }
        }
    }
    **/
}

/**
const QPolygonF& ConstellationBoundaryPoly::boundary( const QString &name ) const
{
	if ( nameHash.contains( name ) )
		return nameHash.value( name )->poly();
	else
		return QPolygonF();  // FIXME: ref to temp.  -jbb
}
**/
