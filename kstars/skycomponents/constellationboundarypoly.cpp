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
    PolyList *polyList = ContainingPoly( p );
    if ( polyList ) return polyList->name();

 	return i18n("Unknown");
}

const QPolygonF* ConstellationBoundaryPoly::constellationPoly( const QString &name )
{
	if ( nameHash().contains( name ) )
		return nameHash().value( name )->poly();

	return 0;
}

const QPolygonF* ConstellationBoundaryPoly::constellationPoly( SkyPoint *p ) 
{
    PolyList *polyList = ContainingPoly( p );
    if ( polyList ) return polyList->poly();

 	return 0;
}


bool ConstellationBoundaryPoly::inConstellation( const QString &name, SkyPoint *p )
{
    PolyList* polyList = nameHash().value( name );
    if ( ! polyList ) return false;
    const QPolygonF* poly = polyList->poly();
	if ( poly->containsPoint( QPointF( p->ra()->Hours(), 
                             p->dec()->Degrees() ), Qt::OddEvenFill ) )
	    return true;

	return false;
}



