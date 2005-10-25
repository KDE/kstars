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

#include "horizoncomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

JupiterMoonsComponent::JupiterMoonsComponent(SkyComposite *parent) : SkyComponent(parent)
{
	jmoons = 0;
}

JupiterMoonsComponent::~JupiterMoonsComponent()
{
	delete jmoons;
}

void JupiterMoonsComponent::init(KStarsData *data)
{
	jmoons = new JupiterMoons();
}

void JupiterMoonsComponent::updateMoons(KStarsData*, KSNumbers*, bool needNewCoords)
{
	//TODO parent is jupiter -> remove pcatalog
	//TODO findPosition should named updatePosition
	//for now, update positions of Jupiter's moons here also
	if ( Options::showPlanets() && Options::showJupiter() )
		jmoons->findPosition( &num, (const KSPlanet*)PCat->findByName("Jupiter"), PCat->planetSun() );

}

void JupiterMoonsComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !Options::showJupiter() ) return;

	SkyMap *map = ks->map();
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );
	
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
			QPoint o = map->getXY( jmoons->pos(i), Options::useAltAz(), Options::useRefraction(), scale );
			if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) )
			{
				if ( jmoons->z(i) < 0.0 ) //Moon is nearer than Jupiter
					psky.drawEllipse( o.x()-1, o.y()-1, 2, 2 );

				//Draw Moon name labels if at high zoom
				if (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM)
				{
					int offset = int(3*scale);
					psky.drawText(o.x() + offset, o.y() + offset, jmoons->name(i));
				}
			}
		}
		//reset font
		psky.setFont( pfont );
	}
}
