/***************************************************************************
                          jupitermoonscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/13/08
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

#include "jupitermoonscomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "jupitermoons.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

JupiterMoonsComponent::JupiterMoonsComponent( SkyComponent *p, bool (*visibleMethod)() ) : SkyComponent( p, visibleMethod )
{
	jmoons = 0;
}

JupiterMoonsComponent::~JupiterMoonsComponent()
{
	delete jmoons;
}

void JupiterMoonsComponent::init(KStarsData *)
{
	jmoons = new JupiterMoons();
}

void JupiterMoonsComponent::update( KStarsData *data, KSNumbers * )
{
	if ( visible() ) 
		jmoons->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void JupiterMoonsComponent::updateMoons( KStarsData *, KSNumbers *num )
{
	//TODO findPosition should named updatePosition
	if ( visible() ) 
		jmoons->findPosition( num, (KSPlanet*)(parent()->findByName("Jupiter")), (KSSun*)(parent()->findByName( "Sun" )) );
}

void JupiterMoonsComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !Options::showJupiter() ) return;

	SkyMap *map = ks->map();
	float Width = scale * map->width();
	float Height = scale * map->height();
	
	//Re-draw Jovian moons which are in front of Jupiter, also draw all 4 moon labels.
	psky.setPen( QPen( QColor( "white" ) ) );
	if ( Options::zoomFactor() > 10.*MINZOOM )
	{
		QFont pfont = psky.font();
		QFont moonFont = psky.font();
		moonFont.setPointSize( pfont.pointSize() - 2 );
		psky.setFont( moonFont );

		for ( unsigned int i=0; i<4; ++i )
		{
			QPointF o = map->toScreen( jmoons->pos(i), scale );

			if ( ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) )
			{
				if ( jmoons->z(i) < 0.0 ) //Moon is nearer than Jupiter
					if ( Options::useAntialias() )
						psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
					else
						psky.drawEllipse( QRect( int(o.x())-1, int(o.y())-1, 2, 2 ) );

				//Draw Moon name labels if at high zoom
				if (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM)
				{
					float offset = 3.0*scale;
					if ( Options::useAntialias() )
						psky.drawText( QPointF( o.x() + offset, o.y() + offset ), jmoons->name(i));
					else
						psky.drawText( int(o.x() + offset), int(o.y() + offset), jmoons->name(i));
				}
			}
		}
		//reset font
		psky.setFont( pfont );
	}
}
