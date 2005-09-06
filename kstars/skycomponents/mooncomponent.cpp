/***************************************************************************
                          mooncomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/06
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mooncomponent.h"

#include "ksmoon.h"

MoonComponent::MoonComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize)
: AbstractPlanetComponent(parent, visibleMethod, msize)
{
	Moon = 0;
}

MoonComponent::~MoonComponent()
{
	delete Moon;
}

void MoonComponent::init(KStarsData *data)
{
	Moon = new KSMoon(data);
//	ObjNames.append( Moon );
	Moon->loadData();
}

void MoonComponent::drawTrail(SkyMap *map, QPainter& psky, double scale)
{
	if ( visible() && Moon->hasTrail() )
		drawPlanetTrail( psky, Moon, scale );
}

void MoonComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	if ( visible() )
	{
		drawPlanet(psky, Moon, QColor( "White" ), MINZOOM, 1, scale );
	}
}

void MoonComponent::updateMoons(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( visible() )
	{
		Moon->findPosition( num, data->geo->lat(), LST );
		Moon->findPhase( sun() ); // TODO sun is unknown
	}
}

void MoonComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( visible() ) {
		Moon->EquatorialToHorizontal( data->LST, data->geo->lat() );
		if ( Moon->hasTrail() ) Moon->updateTrail( data->LST, geo->lat() );
	}
}
