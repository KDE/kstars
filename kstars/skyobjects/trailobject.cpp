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

void TrailObject::drawTrail(QPainter& psky) const {
    if( !Trail.size() )
        return;

    SkyMap     *map  = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width  = map->scale() * map->width();
    float Height = map->scale() * map->height();

    SkyPoint p = Trail.first();
    QPointF o = map->toScreen( &p );
    QPointF oLast( o );

    int i = 0;
    int n = Trail.size();

    bool doDrawLine =
        o.x() >= -1000.0      &&
        o.x() <= Width+1000.0 &&
        o.y() >= -1000.       &&
        o.y() <= Height+1000.;
    bool firstPoint = true;

    QColor tcolor = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    psky.setPen( QPen( tcolor, 1 ) );
    foreach( p, Trail ) {
        //skip first point
        if( firstPoint ) {
            firstPoint = false;
            continue;
        }

        if ( Options::fadePlanetTrails() ) {
            tcolor.setAlphaF(static_cast<qreal>(i)/static_cast<qreal>(n));
            ++i;
            psky.setPen( QPen( tcolor, 1 ) );
        }

        o = map->toScreen( &p );
        if( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

            //Want to disable line-drawing if this point and the last are both outside bounds of display.
            //FIXME: map->rect() should return QRectF
            if( !map->rect().contains( o.toPoint() ) && ! map->rect().contains( oLast.toPoint() ) )
                doDrawLine = false;

            if ( doDrawLine )
                psky.drawLine( oLast, o );
            else
                doDrawLine = true;
        }
        oLast = o;
    }
}
