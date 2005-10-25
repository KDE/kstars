/***************************************************************************
                          suncomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/24/09
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

#include "suncomponent.h"

SunComponent::SunComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), double diameter, int msize) 
: SolarSystemSingleComponent(parent, visibleMethod, msize) 
{
  //TODO: KSSun ctor must construct image name from name string
  sun = new KSSun( data, diameter ); 
}

SunComponent::~SunComponent() {
  if ( sun ) delete sun;
}

void SunComponent::init(KStarsData *data)
{
  //TODO: probably want to move the init code into this class
  sun->loadData();
  data->appendNamedObject( sun );
}

void SunComponent::draw(KStars *ks, QPainter& psky, double scale) {
	if ( !visible() ) return;

	SkyMap *map = ks->map();

	//TODO: default values for 2nd & 3rd arg. of SkyMap::checkVisibility()
	if ( map->checkVisibility( sun ) ) {
		int Width = int( scale * map->width() );
		int Height = int( scale * map->height() );

		int sizemin = 4;
		if ( sun->name() == "Sun" || sun->name() == "Moon" ) sizemin = 8;
		sizemin = int( sizemin * scale );

		//TODO: KSPlanet needs a color property, and someone has to set the planet's color
		psky.setPen( sun->color() );
		psky.setBrush( sun->color() );
		QPoint o = map->getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

		//Is planet onscreen?
		if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
			int size = int( sun->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
			if ( size < sizemin ) size = sizemin;

			//Draw planet image if:
			if ( Options::showPlanetImages() &&  //user wants them,
					int(Options::zoomFactor()) >= int(zoommin) &&  //zoomed in enough,
					!sun->image()->isNull() &&  //image loaded ok,
					size < Width ) {  //and size isn't too big.

				//Image size must be modified to account for possibility that rotated image's
				//size is bigger than original image size.  The rotated image is a square
				//superscribed on the original image.  The superscribed square is larger than
				//the original square by a factor of (cos(t) + sin(t)) where t is the angle by
				//which the two squares are rotated (in our case, equal to the position angle +
				//the north angle, reduced between 0 and 90 degrees).
				//The proof is left as an exercise to the student :)
				dms pa( map->findPA( p, o.x(), o.y(), scale ) );
				double spa, cpa;
				pa.SinCos( spa, cpa );
				cpa = fabs(cpa);
				spa = fabs(spa);
				size = int( size * (cpa + spa) );

				//Because Saturn has rings, we inflate its image size by a factor 2.5
				if ( sun->name() == "Saturn" ) size = int(2.5*size);

				if (resize_mult != 1) {
					size *= resize_mult;
				}

				sun->scaleRotateImage( size, pa.Degrees() );
				int x1 = o.x() - sun->image()->width()/2;
				int y1 = o.y() - sun->image()->height()/2;
				psky.drawImage( x1, y1, *(sun->image()));

			} else {                                   //Otherwise, draw a simple circle.

				psky.drawEllipse( o.x()-size/2, o.y()-size/2, size, size );
			}

			//draw Name
			if ( Options::showPlanetNames() ) {
				psky.setPen( QColor( data->colorScheme()->colorNamed( "PNameColor" ) ) );
				drawNameLabel( psky, p, o.x(), o.y(), scale );
			}
		}
	}
}

#include "suncomponent.moc"
