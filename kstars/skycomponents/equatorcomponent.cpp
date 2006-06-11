/***************************************************************************
                          equatorcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10 Sept. 2005
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

#include "equatorcomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "ksnumbers.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h"
#include "dms.h"
#include "Options.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

EquatorComponent::EquatorComponent(SkyComponent *parent, bool (*visibleMethod)()) : PointListComponent(parent, visibleMethod)
{
}

EquatorComponent::~EquatorComponent()
{
}

void EquatorComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Creating equator" ) );
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint *o = new SkyPoint( i*24./NCIRCLE, 0.0 );
		o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
		pointList().append( o );
	}
}

void EquatorComponent::draw( KStars *ks, QPainter &psky, double scale ) {
	if ( ! visible() ) return;

	SkyMap *map = ks->map();
	KStarsData *data = ks->data();
	float Width = scale * map->width();
	float Height = scale * map->height();
	float dmax = scale*Options::zoomFactor()/10.0;

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "EqColor" ) ), 1, Qt::SolidLine ) );

	//index of point near the right or top/bottom edge
	int iSmall(0), iSmall2(0);
	float xSmall(100000.); //ridiculous initial value

	bool FirstPoint = true;
	QPointF o, oFirst, oPrev, o2;

	foreach ( SkyPoint *p, pointList() ) {
		o = map->toScreen( p, scale );

		if ( FirstPoint ) {
			FirstPoint = false;
			o2 = o;
			oFirst = o;
			oPrev = o;
			continue; //that's all we need for the first point
		}

		if ( map->checkVisibility( p ) ) {
			//If the distance to the next point is too large, we won't draw
			//the segment (this protects against artifacts)
			float dx = o.x() - oPrev.x();
			float dy = o.y() - oPrev.y();
			if ( fabs(dx) < dmax && fabs(dy) < dmax ) {
				if ( Options::useAntialias() )
					psky.drawLine( oPrev, o );
				else
					psky.drawLine( QPoint(int(oPrev.x()),int(oPrev.y())), 
								QPoint(int(o.x()),int(o.y())) );
			}

			//We will draw the Equator label near the left edge of the screen, so 
			//record the index of the onscreen point with the smallest x-coordinate
			//Store the index in iSmall, and its coordinate in xSmall
			if ( o.x() > 0 && o.x() < xSmall && o.y() > 0 && o.y() < Height ) {
				xSmall = o.x();
				iSmall = pointList().indexOf( p );
			}
		}
		oPrev = o;
	}

	//connect the final segment
	//note that oPrev is now the final position, because it is set at the 
	//end of the foreach-loop
	float dx = oPrev.x() - oFirst.x();
	float dy = oPrev.y() - oFirst.y();
	if ( fabs(dx) < dmax && fabs(dy) < dmax ) {
		if ( Options::useAntialias() )
			psky.drawLine( oPrev, oFirst );
		else
			psky.drawLine( QPoint(int(oPrev.x()),int(oPrev.y())), 
						QPoint(int(oFirst.x()),int(oFirst.y())) );
	}

	if ( ! map->isSlewing() && xSmall < float(Width) ) {
		//Draw the "Equator" label.  We have flagged the leftmost onscreen Equator point.
		//If the zoom level is below 1000, simply adopt this point as the anchor for the
		//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
		//point at which the Equator goes offscreen, and anchor from that point.
		SkyPoint *p = pointList().at(iSmall);
		double ra0(0.0);  //the ra of our anchor point

		if ( Options::zoomFactor() < 1000. ) {
			ra0 = p->ra()->Hours();

		} else {
			//Somewhere between Equator point p and its immediate neighbor, the Equator goes
			//offscreen.  Determine the exact point at which this happens.
			iSmall2 = iSmall + 1;
			if ( iSmall2 >= pointList().size() ) iSmall2 -= pointList().size();

			SkyPoint *p2 = pointList().at(iSmall2);

			o = map->toScreen( p, scale );
			o2 = map->toScreen( p2, scale );

			float x1, x2;
			//there are 3 possibilities:  (o2.x() < 0); (o2.y() < 0); (o2.y() > height())
			if ( o2.x() < 0 ) {
				x1 = ( Width - o.x() )/( o2.x() - o.x() );
				x2 = ( o2.x() - Width )/( o2.x() - o.x() );
			} else if ( o2.y() < 0 ) {
				x1 = ( o.y() )/( o.y() - o2.y() );
				x2 = -1.0*o2.y()/( o.y() - o2.y() );
			} else if ( o2.y() > Height ) {
				x1 = ( Height - o.y() )/( o2.y() - o.y() );
				x2 = ( o2.y() - Height )/( o2.y() - o.y() );
			} else {  //should never get here
				x1 = 0.0;
				x2 = 1.0;
			}

			//ra0 is the exact RA at which the Equator intersects a screen edge
			ra0 = x1*p2->ra()->Hours() + x2*p->ra()->Hours();
		}

		KSNumbers num( data->ut().djd() );
		dms ecLong, ecLat;

		//LabelPoint is the top left corner of the text label.  It is
		//offset from the anchor point by -1.5 degree (0.1 hour) in RA
		//and -0.4 degree in Dec, scaled by 2000./zoomFactor so that they are
		//independent of zoom.
		SkyPoint LabelPoint(ra0 - 200./Options::zoomFactor(), -800./Options::zoomFactor() );
		if ( Options::useAltAz() )
			LabelPoint.EquatorialToHorizontal( data->lst(), data->geo()->lat() );

		//p2 is a SkyPoint offset from LabelPoint in RA by -0.1 hour/zoomFactor.
		//We use this point to determine the rotation angle for the text (which
		//we want to be parallel to the line joining LabelPoint and p2)
		SkyPoint p2 = LabelPoint;
		p2.setRA( p2.ra()->Hours() - 200./Options::zoomFactor() );
		if ( Options::useAltAz() )
			p2.EquatorialToHorizontal( data->lst(), data->geo()->lat() );

		//o and o2 are the screen positions of LabelPoint and p2.
		o = map->toScreen( &LabelPoint, scale );
		o2 = map->toScreen( &p2, scale );

		double sx = double( o.x() - o2.x() );
		double sy = double( o.y() - o2.y() );
		double angle;
		if ( sx ) {
			angle = atan( sy/sx )*180.0/dms::PI;
		} else {
			angle = 90.0;
			if ( sy < 0 ) angle = -90.0;
		}

		//Finally, draw the "Equator" label at the determined location and angle
		psky.save();
		if ( Options::useAntialias() )
			psky.translate( o.x(), o.y() );
		else
			psky.translate( int(o.x()), int(o.y()) );

		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( 0, 0, i18n( "Equator" ) );
		psky.restore(); //reset coordinate system
	}
}
