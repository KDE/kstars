/***************************************************************************
                          abstractplanetcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

//JH: This file does not exist...
//#include "planethelper.h" 

#include "abstractplanetcomponent.h"

#include "ksplanet.h"
#include "skymap.h"
#include "kstarsdata.h"

AbstractPlanetComponent::AbstractPlanetComponent(SolarSystemComposite *parent, KSPlanet *earth, bool (*visibleMethod)(), int msize)
: SkyComponent(parent)
{
	visible = visibleMethod;
	minsize = msize;
	sizeScale = 1.0;
	earth = parent->earth(); // TODO right syntax?
}

void AbstractPlanetComponent::drawTrail(SkyMap *map, QPainter& psky, KSPlanetBase *ksp, double scale )
{
  if ( ! visible() || !ksp->hasTrail() ) return;
	
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	QColor tcolor1 = QColor( map->data()->colorScheme()->colorNamed( "PlanetTrailColor" ) );
	QColor tcolor2 = QColor( map->data()->colorScheme()->colorNamed( "SkyColor" ) );

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

// see SkyComponent::drawNameLabel()
int AbstractPlanetComponent::labelSize(SkyObject *obj)
{
	// it's save to cast here
	KSPlanetBase *p = (KSPlanetBase*)obj;

	size = int(p->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0);
// TODO init following components with these minsize values
// 	COMET minsize = 2;
// 	Sun = 8;
	if (size < minsize)
	{
		size = minsize;
	}
	// saturn is scaled by factor 2.5
	size = int(sizeScale * size);
	return size;
}


void AbstractPlanetComponent::setSizeScale(float scale)
{
	sizeScale = scale;
}

/*JH: Moved to PlanetComponent::draw()
void AbstractPlanetComponent::drawPlanet(SkyMap *map, QPainter &psky, KSPlanetBase *p, QColor c, double zoommin, int resize_mult, double scale )
{

	if ( map->checkVisibility( p, fov(), XRange ) ) {
		int Width = int( scale * map->width() );
		int Height = int( scale * map->height() );

// TODO not needed anymore
//		int sizemin = 4;
//		if ( p->name() == "Sun" || p->name() == "Moon" ) sizemin = 8;
		sizemin = int( sizemin * scale );

		psky.setPen( c );
		psky.setBrush( c );
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

		//Is planet onscreen?
		if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
			int size = int( p->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
			if ( size < sizemin ) size = sizemin;

			//Draw planet image if:
			if ( Options::showPlanetImages() &&  //user wants them,
					int(Options::zoomFactor()) >= int(zoommin) &&  //zoomed in enough,
					!p->image()->isNull() &&  //image loaded ok,
					size < Width )
			{  //and size isn't too big.

				//Image size must be modified to account for possibility that rotated image's
				//size is bigger than original image size.  The rotated image is a square
				//superscribed on the original image.  The superscribed square is larger than
				//the original square by a factor of (cos(t) + sin(t)) where t is the angle by
				//which the two squares are rotated (in our case, equal to the position angle +
				//the north angle, reduced between 0 and 90 degrees).
				//The proof is left as an exercise to the student :)
				dms pa( findPA( p, o.x(), o.y(), scale ) );
				double spa, cpa;
				pa.SinCos( spa, cpa );
				cpa = fabs(cpa);
				spa = fabs(spa);
				size = int( size * (cpa + spa) );

				//Because Saturn has rings, we inflate its image size by a factor 2.5
				if ( p->name() == "Saturn" ) size = int(2.5*size);

				if (resize_mult != 1) {
					size *= resize_mult;
				}

				p->scaleRotateImage( size, pa.Degrees() );
				int x1 = o.x() - p->image()->width()/2;
				int y1 = o.y() - p->image()->height()/2;
				psky.drawImage( x1, y1, *(p->image()));

			} else {
				//Otherwise, draw a simple circle.
				psky.drawEllipse(o.x()-size/2, o.y()-size/2, size, size);
			}

			//draw Name
			if ( Options::showPlanetNames() ) {
				psky.setPen(QColor(map->data()->colorScheme()->colorNamed("PNameColor")));
				drawNameLabel( psky, p, o.x(), o.y(), scale );
			}
		}
	}
}
*/
