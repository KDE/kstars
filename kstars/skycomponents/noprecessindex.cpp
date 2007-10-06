/***************************************************************************
                       noprecessindex.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-04
    copyright            : (C) 2007 James B. Bowlin
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "Options.h"
#include "skypoint.h"
#include "kstarsdata.h"
#include "noprecessindex.h"
#include "linelist.h"

NoPrecessIndex::NoPrecessIndex( SkyComponent *parent, const QString& name )
        : LineListIndex( parent, name ), lastZoom( true )
{}

// Don't precess the points, just account for the Earth's rotation
void NoPrecessIndex::JITupdate( KStarsData *data, LineList* lineList )
{
    lineList->updateID = data->updateID();
    SkyList* points = lineList->points();
    for (int i = 0; i < points->size(); i++ ) {
        points->at( i )->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

void NoPrecessIndex::draw( KStars *kstars, QPainter &psky )
{
    if ( ! selected() ) return;

    preDraw( kstars, psky );

    if ( skyMesh()->isZoomedIn() ) {
        drawLines( kstars, psky );
    }
    else {
        drawLines( kstars, psky );
    }

}


