/***************************************************************************
                          trailobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Oct 27 2007
    copyright            : (C) 2007 by Jason Harris
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

#include <QPainter>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "kspopupmenu.h"
#include "trailobject.h"
#include "skypainter.h"

QSet<TrailObject*> TrailObject::trailObjects;

TrailObject::TrailObject( int t, dms r, dms d, float m, const QString &n )
  : SkyObject( t, r, d, m, n )
{}

TrailObject::TrailObject( int t, double r, double d, float m, const QString &n ) 
  : SkyObject( t, r, d, m, n )
{}

TrailObject::~TrailObject() {
    trailObjects.remove(this);
}

TrailObject* TrailObject::clone() const {
    return new TrailObject(*this);
}

void TrailObject::updateTrail( dms *LST, const dms *lat ) {
    for( int i=0; i < Trail.size(); ++i )
        Trail[i].EquatorialToHorizontal( LST, lat );
}

void TrailObject::initPopupMenu( KSPopupMenu *pmenu ) {
    pmenu->createPlanetMenu( this );
}

void TrailObject::addToTrail() {
    Trail.append( SkyPoint( *this ) );
    trailObjects.insert( this );
}

void TrailObject::clipTrail() {
    if( Trail.size() )
        Trail.removeFirst();
    if( Trail.size() )
        trailObjects.remove( this );
}

void TrailObject::clearTrail() {
    Trail.clear();
    trailObjects.remove( this );
}

void TrailObject::clearTrailsExcept(SkyObject* o) {
    TrailObject* keep = 0;
    foreach(TrailObject* tr, trailObjects) {
        if( tr != o )
            tr->clearTrail();
        else
            keep = tr;
    }

    trailObjects = QSet<TrailObject*>();
    if( keep )
        trailObjects.insert( keep );
}

void TrailObject::drawTrail(SkyPainter* skyp) const {
    if( !Trail.size() )
        return;

    KStarsData *data = KStarsData::Instance();

    QColor tcolor = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    skyp->setPen( QPen(tcolor, 1) );
    int n = Trail.size();
    for(int i = 1; i < n; ++i) {
        if ( Options::fadePlanetTrails() ) {
            tcolor.setAlphaF(static_cast<qreal>(i)/static_cast<qreal>(n));
            skyp->setPen( QPen( tcolor, 1 ) );
        }
        SkyPoint a = Trail[i-1];
        SkyPoint b = Trail[i];
        skyp->drawSkyLine(&a, &b);
    }
}
