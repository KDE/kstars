/***************************************************************************
                          eclipticcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 16 Sept. 2005
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

#include "eclipticcomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

EclipticComponent::EclipticComponent(SkyComponent *parent, bool (*visibleMethod)()) : PointListComponent(parent, visibleMethod)
{
}

EclipticComponent::~EclipticComponent()
{
}

// was KStarsData::initGuides(KSNumbers *num)
// needs dms *LST, *HourAngle from KStarsData
// TODO: ecliptic + equator needs partial code of algorithm
// -> solution:
//	-all 3 objects in 1 component (this is messy)
//	-3 components which share a algorithm class
void EclipticComponent::init(KStarsData *data)
{
	// Define the Ecliptic
  KSNumbers num( data->ut().djd() );
  dms elat(0.0);
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
	  dms elng( double( i ) );
		SkyPoint *o = new SkyPoint( 0.0, 0.0 );
		o->setFromEcliptic( num.obliquity(), &elng, &elat );
		o->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		Ecliptic.append( o );
	}
}

void EclipticComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( Options::showEcliptic() ) {
	  foreach ( SkyPoint *p, Ecliptic ) {
			p->EquatorialToHorizontal( LST, data->geo()->lat() );
		}
	}
}

void EclipticComponent::draw( KStars *ks, QPainter &p, double scale ) {
	if ( !Options::showEcliptic() ) return;

	SkyMap *map = ks->map();
	KStarsData *data = ks->data();
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "EclColor" ) ), 1, SolidLine ) );

	//index of point near the right or top/bottom edge
	uint iBig(0), iBig2(0);
	int xBig(-100); //ridiculous initial value

	bool Visible = false;
	bool FirstPoint = true;
	QPoint o, oFirst, oPrev;

	foreach ( SkyPoint *p, Ecliptic ) {
		o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

		if ( FirstPoint ) {
			FirstPoint = false;
			o2 = o;
			oFirst = o;
			oPrev = o;
			cur = o;
			psky.moveTo( o.x(), o.y() );
			continue; //that's all we need for the first point
		}

		if ( checkVisibility( p, guideFOV, guideXRange ) ) {
			//If this is the first visible point, move the cursor 
			//to the previous point before we draw the first line segment
			if ( !Visible ) {
				psky.moveTo( oPrev.x(), oPrev.y() );
				Visible = true;
			}

			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

			//If the distance to the next point is too large, we won't draw
			//the segment (this protects against artifacts)
			int dx = o.x() - oPrev.x();
			int dy = o.y() - oPrev.y();
			if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
				psky.lineTo( o.x(), o.y() );
			} else {
				psky.moveTo( o.x(), o.y() );
			}

			//We will draw the Ecliptic label near the right edge of the screen, so 
			//record the index of the onscreen point with the largest x-coordinate
			//Store the index in iBig, and its coordinate in xBig
			if ( o.x() > xBig && o.x() < width() && o.y() > 0 && o.y() < height() ) {
				xBig = o.x();
				iBig = data->Ecliptic.at();
			}

		} else {
			Visible = false;
		}
		oPrev = o;
	}

	//connect the final segment
	//note that oPrev is now the final position, because it is set at the 
	//end of the foreach-loop
	int dx = oPrev.x() - oFirst.x();
	int dy = oPrev.y() - oFirst.y();
	if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
		psky.lineTo( oFirst.x(), oFirst.y() );
	} else {
		//FIXME: no need for this?
		psky.moveTo( oFirst.x(), oFirst.y() );
	}

	if ( ! slewing && xBig > 0 ) {
		//Draw the "Ecliptic" label.  We have flagged the rightmost onscreen Ecliptic point.
		//If the zoom level is below 1000, simply adopt this point as the anchor for the
		//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
		//point at which the Ecliptic goes offscreen, and anchor from that point.
		p = Ecliptic.at(iBig);
		double ra0(0.0);  //the ra of our anchor point
		double dec0(0.0); //the dec of our anchor point

		if ( Options::zoomFactor() < 1000. ) {
			ra0 = p->ra()->Hours();
			dec0 = p->dec()->Degrees();
		} else {
			//Somewhere between Ecliptic point p and its immediate neighbor, the Ecliptic goes
			//offscreen.  Determine the exact point at which this happens.
			if ( iBig == 0 ) iBig2 = 0;
			else iBig2 = iBig1 - 1;
			SkyPoint *p2 = data->Ecliptic.at(iBig2);

			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			o2 = getXY( p2, Options::useAltAz(), Options::useRefraction(), scale );

			double x1, x2;
			//there are 3 possibilities:  (o2.x() > width()); (o2.y() < 0); (o2.y() > height())
			if ( o2.x() > width() ) {
				x1 = double(width()-o.x())/double(o2.x()-o.x());
				x2 = double(o2.x()-width())/double(o2.x()-o.x());
			} else if ( o2.y() < 0 ) {
				x1 = double(o.y())/double(o.y()-o2.y());
				x2 = -1.0*double(o2.y())/double(o.y()-o2.y());
			} else if ( o2.y() > height() ) {
				x1 = double(height() - o.y())/double(o2.y()-o.y());
				x2 = double(o2.y() - height())/double(o2.y()-o.y());
			} else {  //should never get here
				x1 = 0.0;
				x2 = 1.0;
			}

			//ra0 is the exact RA at which the Ecliptic intersects a screen edge
			ra0 = x1*p2->ra()->Hours() + x2*p->ra()->Hours();
			//dec0 is the exact Dec at which the Ecliptic intersects a screen edge
			dec0 = x1*p2->dec()->Degrees() + x2*p->dec()->Degrees();
		}

		KSNumbers num( data->ut().djd() );
		dms ecLong, ecLat;

		//LabelPoint is offset from the anchor point by +2.0 degree ecl. Long
		//and -0.4 degree in ecl. Lat, scaled by 2000./zoomFactor so that they are
		//independent of zoom.
		SkyPoint LabelPoint(ra0, dec0);
		LabelPoint.findEcliptic( num.obliquity(), ecLong, ecLat );
		ecLong.setD( ecLong.Degrees() + 4000./Options::zoomFactor() );
		ecLat.setD( ecLat.Degrees() - 800./Options::zoomFactor() );
		LabelPoint.setFromEcliptic( num.obliquity(), &ecLong, &ecLat );
		if ( Options::useAltAz() )
			LabelPoint.EquatorialToHorizontal( data->LST, data->geo()->lat() );

		//p2 is a SkyPoint offset from LabelPoint by -1.0 degrees of ecliptic longitude.
		//we use p2 to determine the onscreen rotation angle for the ecliptic label,
		//which we want to be parallel to the line between LabelPoint and p2.
		SkyPoint p2(ra0, dec0);
		p2.findEcliptic( num.obliquity(), ecLong, ecLat );
		ecLong.setD( ecLong.Degrees() + 2000./Options::zoomFactor() );
		ecLat.setD( ecLat.Degrees() - 800./Options::zoomFactor() );
		p2.setFromEcliptic( num.obliquity(), &ecLong, &ecLat );
		if ( Options::useAltAz() )
			p2.EquatorialToHorizontal( data->LST, data->geo()->lat() );

		//o and o2 are the screen positions of LabelPoint and p2.
		o = getXY( &LabelPoint, Options::useAltAz(), Options::useRefraction(), scale );
		o2 = getXY( &p2, Options::useAltAz(), Options::useRefraction() );

		double sx = double( o.x() - o2.x() );
		double sy = double( o.y() - o2.y() );
		double angle;
		if ( sx ) {
			angle = atan( sy/sx )*180.0/dms::PI;
		} else {
			angle = 90.0;
			if ( sy < 0 ) angle = -90.0;
		}

		//Finally, draw the "Ecliptic" label at the determined location and angle
		psky.save();
		psky.translate( o.x(), o.y() );
		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( 0, 0, i18n( "Ecliptic" ) );
		psky.restore(); //reset coordinate system
	}
}
