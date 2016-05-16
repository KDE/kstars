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

#include "trailobject.h"

#include "kstarsdata.h"
#include "skymap.h"
#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif
#include "skycomponents/skylabeler.h"
#include "skypainter.h"
#include "projections/projector.h"
#include "Options.h"

#include <QPainter>

#include <typeinfo>

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
    Q_ASSERT( typeid( this ) == typeid( static_cast<const TrailObject *>( this ) ) ); // Ensure we are not slicing a derived class
    return new TrailObject(*this);
}

void TrailObject::updateTrail( dms *LST, const dms *lat ) {
    for( int i=0; i < Trail.size(); ++i )
        Trail[i].EquatorialToHorizontal( LST, lat );
}

void TrailObject::initPopupMenu( KSPopupMenu *pmenu ) {
#ifndef KSTARS_LITE
    pmenu->createPlanetMenu( this );
#endif
}

void TrailObject::addToTrail( const QString &label ) {
    Trail.append( SkyPoint( *this ) );
    m_TrailLabels.append( label );
    trailObjects.insert( this );
}

void TrailObject::clipTrail() {
    if( Trail.size() ) {
        Trail.removeFirst();
        Q_ASSERT( m_TrailLabels.size() );
        m_TrailLabels.removeFirst();
    }
    if( Trail.size() ) // Eh? Shouldn't this be if( !Trail.size() ) -- asimha
        trailObjects.remove( this );
}

void TrailObject::clearTrail() {
    Trail.clear();
    m_TrailLabels.clear();
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
#ifndef KSTARS_LITE
    if( !Trail.size() )
        return;

    KStarsData *data = KStarsData::Instance();

    QColor tcolor = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    skyp->setPen( QPen(tcolor, 1) );
    SkyLabeler *labeler = SkyLabeler::Instance();
    labeler->setPen( tcolor );
    int n = Trail.size();
    for(int i = 1; i < n; ++i) {
        if ( Options::fadePlanetTrails() ) {
            tcolor.setAlphaF(static_cast<qreal>(i)/static_cast<qreal>(n));
            skyp->setPen( QPen( tcolor, 1 ) );
        }
        SkyPoint a = Trail[i-1];
        SkyPoint b = Trail[i];
        skyp->drawSkyLine(&a, &b);
        if( i % 5 == 1 ) { // TODO: Make drawing of labels configurable, incl. frequency etc.
            QPointF pt = SkyMap::Instance()->projector()->toScreen( &a );
            labeler->drawGuideLabel( pt, m_TrailLabels[i - 1], 0.0 );
        }
    }
#endif
}
