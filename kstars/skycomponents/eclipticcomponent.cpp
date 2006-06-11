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

#include <QPainter>

#include "ksnumbers.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

EclipticComponent::EclipticComponent(SkyComponent *parent, bool (*visibleMethod)()) : PointListComponent(parent, visibleMethod)
{
}

EclipticComponent::~EclipticComponent()
{
}

void EclipticComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Creating ecliptic" ) );
	// Define the Ecliptic
	KSNumbers num( data->ut().djd() );
	dms elat(0.0), elng(0.0);
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		elng.setD( double( i ) );
		SkyPoint *o = new SkyPoint( 0.0, 0.0 );
		o->setFromEcliptic( num.obliquity(), &elng, &elat );
		o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
		pointList().append( o );
	}
}

void EclipticComponent::draw( KStars *ks, QPainter &psky, double scale ) {
	if ( !Options::showEcliptic() ) return;

	SkyMap *map = ks->map();
	KStarsData *data = ks->data();
	float Width = scale * map->width();
	float Height = scale * map->height();

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "EclColor" ) ), 1, Qt::SolidLine ) );

	//index of point near the right or top/bottom edge
	int iBig(0), iBig2(0);
	float xBig(-100.); //ridiculous initial value

	bool FirstPoint = true;
	QPointF o, oFirst, oPrev;

	foreach ( SkyPoint *p, pointList() ) {
		o = map->toScreen( p, scale );

		if ( FirstPoint ) {
			FirstPoint = false;
			oFirst = o;
			oPrev = o;
			continue; //that's all we need for the first point
		}

		if ( map->checkVisibility( p ) ) {
			//If the distance to the next point is too large, we won't draw
			//the segment (this protects against artifacts)
			float dx = o.x() - oPrev.x();
			float dy = o.y() - oPrev.y();
			if ( fabs(dx) < map->guideMaxLength()*scale && fabs(dy) < map->guideMaxLength()*scale ) {
				if ( Options::useAntialias() )
					psky.drawLine( oPrev, o );
				else
					psky.drawLine( QPoint(int(oPrev.x()),int(oPrev.y())), 
								QPoint(int(o.x()),int(o.y())) );
			}

			//We will draw the Ecliptic label near the right edge of the screen, so 
			//record the index of the onscreen point with the largest x-coordinate
			//Store the index in iBig, and its coordinate in xBig
			if ( o.x() > xBig && o.x() < Width && o.y() > 0. && o.y() < Height ) {
				xBig = o.x();
				iBig = pointList().indexOf( p );
			}
		}
		oPrev = o;
	}

	//connect the final segment
	//note that oPrev is now the final position, because it is set at the 
	//end of the foreach-loop
	float dx = oPrev.x() - oFirst.x();
	float dy = oPrev.y() - oFirst.y();
	if ( fabs(dx) < map->guideMaxLength()*scale && fabs(dy) < map->guideMaxLength()*scale ) {
				if ( Options::useAntialias() )
					psky.drawLine( oFirst, oPrev );
				else
					psky.drawLine( QPoint(int(oFirst.x()),int(oFirst.y())), 
								QPoint(int(oPrev.x()),int(oPrev.y())) );
	}

	if ( ! map->isSlewing() && xBig > 0 ) {
		//Draw the "Ecliptic" label.  We have flagged the rightmost onscreen Ecliptic point.
		//If the zoom level is below 1000, simply adopt this point as the anchor for the
		//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
		//point at which the Ecliptic goes offscreen, and anchor from that point.
		double ra0(0.0);  //the ra of our anchor point
		double dec0(0.0); //the dec of our anchor point

		SkyPoint *p = pointList().at(iBig);
		if ( Options::zoomFactor() < 1000. ) {
			ra0 = p->ra()->Hours();
			dec0 = p->dec()->Degrees();
		} else {
			//Somewhere between Ecliptic point p and its immediate neighbor, the Ecliptic goes
			//offscreen.  Determine the exact point at which this happens.
			if ( iBig == 0 ) iBig2 = pointList().size() - 1;
			else iBig2 = iBig - 1;
			SkyPoint *p2 = pointList().at(iBig2);

			o = map->toScreen( p, scale );
			QPointF o2 = map->toScreen( p2, scale );

			float x1, x2;
			//there are 3 possibilities:  (o2.x() > width()); (o2.y() < 0); (o2.y() > height())
			if ( o2.x() > Width ) {
				x1 = (Width - o.x())/(o2.x() - o.x());
				x2 = (o2.x() - Width)/(o2.x() - o.x());
			} else if ( o2.y() < 0. ) {
				x1 = o.y()/(o.y() - o2.y());
				x2 = -1.0*o2.y()/(o.y() - o2.y());
			} else if ( o2.y() > Height ) {
				x1 = (Height - o.y())/(o2.y() - o.y());
				x2 = (o2.y() - Height)/(o2.y() - o.y());
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
			LabelPoint.EquatorialToHorizontal( data->lst(), data->geo()->lat() );

		//p2 is a SkyPoint offset from LabelPoint by -1.0 degrees of ecliptic longitude.
		//we use p2 to determine the onscreen rotation angle for the ecliptic label,
		//which we want to be parallel to the line between LabelPoint and p2.
		SkyPoint p2(ra0, dec0);
		p2.findEcliptic( num.obliquity(), ecLong, ecLat );
		ecLong.setD( ecLong.Degrees() + 2000./Options::zoomFactor() );
		ecLat.setD( ecLat.Degrees() - 800./Options::zoomFactor() );
		p2.setFromEcliptic( num.obliquity(), &ecLong, &ecLat );
		if ( Options::useAltAz() )
			p2.EquatorialToHorizontal( data->lst(), data->geo()->lat() );

		//o and o2 are the screen positions of LabelPoint and p2.
		o = map->toScreen( &LabelPoint, scale );
		QPointF o2 = map->toScreen( &p2, scale );

		float sx = o.x() - o2.x();
		float sy = o.y() - o2.y();
		double angle;
		if ( sx ) {
			angle = atan( sy/sx )*180.0/dms::PI;
		} else {
			angle = 90.0;
			if ( sy < 0 ) angle = -90.0;
		}

		//Finally, draw the "Ecliptic" label at the determined location and angle
		psky.save();
		if ( Options::useAntialias() ) 
			psky.translate( o.x(), o.y() );
		else
			psky.translate( int(o.x()), int(o.y()) );

		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( QPointF(0, 0), i18n( "Ecliptic" ) );
		psky.restore(); //reset coordinate system
	}
}
