/***************************************************************************
              solarsystemlistcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/22/09
    copyright            : (C) 2005 by Jason Harris
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

#include "solarsystemlistcomponent.h"

SolarSystemListComponent::SolarSystemListComponent( SolarSystemComposite *parent, bool (*visibleMethod)(), int msize )
: ListComponent( parent, visibleMethod )
{
	m_Earth = parent->earth();
}

SolarSystemListComponent::~SolarSystemListComponent()
{
	//Object deletes handled by parent class (ListComponent)
}

void SolarSystemListComponent::updatePlanets(KStarsData *data, KSNumbers *num ) {
	if ( visible() ) {
		foreach ( SkyObject *o, objectList() ) {
			KSPlanetBase *p = (KSPlanetBase*)o;
			KSPlanet Earth( data, I18N_NOOP( "Earth" ) );
			Earth.findPosition( num );
			p->findPosition( num, data->geo()->lat(), data->lst(), &Earth );
			p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

			if ( hasTrail() ) 
				planet()->updateTrail( data->lst(), data->geo()->lat() );
		}
	}
}

bool SolarSystemListComponent::addTrail( SkyObject *o ) {
	foreach( SkyObject *o, objectList() ) {
		if ( o == storedObject() ) {
			(KSPlanetBase*)(storedObject())->addToTrail();
			m_TrailList.append( o );
			return true;
		}
	}
	return false;
}

bool SolarSystemListComponent::hasTrail( SkyObject *o ) {
	foreach( SkyObject *o, m_TrailList() ) {
		if ( o == storedObject() ) {
			(KSPlanetBase*)(storedObject())->hasTrail();
			return true;
		}
	}
	return false;
}

bool SolarSystemListComponent::removeTrail( SkyObject *o ) {
	foreach( SkyObject *o, m_TrailList() ) {
		if ( o == storedObject() ) {
			(KSPlanetBase*)(storedObject())->clearTrail();
			if ( m_TrailList.indexOf( o ) >= 0 )
				m_TrailList.removeAt( indexOf( o ) );
			return true;
		}
	}
	return false;
}

void SolarSystemListComponent::drawTrails( KStars *ks, QPainter& psky, double scale ) {
	if ( ! visible() || !ksp->hasTrail() ) return;

	foreach ( SkyObject *obj, m_TrailList ) {
		KStarsPlanetBase *ksp = (KStarsPlanetBase*)obj;
		SkyMap *map = ks->map();
		KStarsData *data = ks->data();
	
		int Width = int( scale * map->width() );
		int Height = int( scale * map->height() );
	
		QColor tcolor1 = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
		QColor tcolor2 = QColor( data->colorScheme()->colorNamed( "SkyColor" ) );
	
		SkyPoint *p = ksp->trail()->first();
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
		QPoint cur( o );
	
		bool doDrawLine(false);
		int i = 0;
		int n = ksp->trail()->count();
	
		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			psky.moveTo(o.x(), o.y());
			doDrawLine = true;
		}
	
		psky.setPen( QPen( tcolor1, 1 ) );
		for ( p = ksp->trail()->next(); p; p = ksp->trail()->next() ) {
			if ( Options::fadePlanetTrails() ) {
				//Define interpolated color
				QColor tcolor = QColor(
							(i*tcolor1.red()   + (n-i)*tcolor2.red())/n,
							(i*tcolor1.green() + (n-i)*tcolor2.green())/n,
							(i*tcolor1.blue()  + (n-i)*tcolor2.blue())/n );
				++i;
				psky.setPen( QPen( tcolor, 1 ) );
			}
	
			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
	
				//Want to disable line-drawing if this point and the last are both outside bounds of display.
				if ( ! rect().contains( o ) && ! rect().contains( cur ) ) doDrawLine = false;
				cur = o;
	
				if ( doDrawLine ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
					doDrawLine = true;
				}
			}
		}
	}
}
