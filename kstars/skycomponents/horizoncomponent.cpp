/***************************************************************************
                          horizoncomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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
#include <QPointF>
#include <QPainter>

#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

HorizonComponent::HorizonComponent(SkyComponent *parent, bool (*visibleMethod)()) 
: PointListComponent(parent, visibleMethod)
{
}

HorizonComponent::~HorizonComponent()
{
}

void HorizonComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Creating horizon" ) );

	//Define Horizon
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint *o = new SkyPoint();
		o->setAz( i*360./NCIRCLE );
		o->setAlt( 0.0 );

		o->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
		pointList().append( o );
	}
}

void HorizonComponent::update( KStarsData *data, KSNumbers * ) {
	if ( visible() ) {
		foreach ( SkyPoint *p, pointList() ) {
			p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
		}
	}
}

//Only half of the Horizon circle is ever valid, the invalid half is "behind" the observer.
//To select the valid half, we start with the azimuth of the central focus point.
//The valid horizon points have azimuth between this az +- 90
//This is true for Equatorial or Horizontal coordinates
void HorizonComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( ! visible() ) return;

	SkyMap *map = ks->map();
	float Width = scale * map->width();
	float Height = scale * map->height();
	QPolygonF groundPoly;
	SkyPoint *pAnchor(0), *pAnchor2(0);

	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "HorzColor" ) ), 1, Qt::SolidLine ) );

	if ( Options::showGround() )
		psky.setBrush( QColor ( ks->data()->colorScheme()->colorNamed( "HorzColor" ) ) );
	else
		psky.setBrush( Qt::NoBrush );

	double daz = 90.;
	if ( Options::useAltAz() )
		daz = 0.5*Width*57.3/Options::zoomFactor(); //center to edge, in degrees
	if ( daz > 90.0 ) daz = 90.0;

	double az1 = map->focus()->az()->Degrees() - daz;
	double az2 = map->focus()->az()->Degrees() + daz;

	QPointF o;
	bool allGround(true);
	bool allSky(true);

	//Add points on the left that may be slightly West of North
	if ( az1 < 0. ) {
		az1 += 360.;
		foreach ( SkyPoint *p, pointList() ) {
			if ( p->az()->Degrees() > az1 ) {
				groundPoly << map->getXY( p, Options::useAltAz(), false, scale );

				//Set the anchor point if this point is onscreen
				if ( o.x() < Width && o.y() > 0. && o.y() < Height ) 
					pAnchor = p;

				if ( o.y() > 0. ) allGround = false;
				if ( o.y() < Height && o.y() > 0. ) allSky = false;
			}
		}
		az1 = 0.0;
	}

	//Add points in normal range, 0 to 360
	foreach ( SkyPoint *p, pointList() ) {
		if ( p->az()->Degrees() > az1 && p->az()->Degrees() < az2 ) {
			o = map->getXY( p, Options::useAltAz(), false, scale );
			groundPoly << o;

			//Set the anchor point if this point is onscreen
			if ( o.x() < Width && o.y() > 0. && o.y() < Height ) 
				pAnchor = p;

			if ( o.y() > 0. ) allGround = false;
			if ( o.y() < Height && o.y() > 0. ) allSky = false;
		} else if ( p->az()->Degrees() > az2 )
			break;
	}

	//Add points on the right that may be slightly East of North
	if ( az2 > 360. ) {
		az2 -= 360.;
		foreach ( SkyPoint *p, pointList() ) {
			if ( p->az()->Degrees() < az2 ) {
				o = map->getXY( p, Options::useAltAz(), false, scale );
				groundPoly << o;

				//Set the anchor point if this point is onscreen
				if ( o.x() < Width && o.y() > 0. && o.y() < Height ) 
					pAnchor = p;

				if ( o.y() > 0. ) allGround = false;
				if ( o.y() < Height && o.y() > 0. ) allSky = false;
			} else
				break;
		}
	}

	if ( allSky ) return; //no horizon onscreen

	if ( allGround ) { 
		//Ground fills the screen.  Reset groundPoly to surround screen perimeter
		//Just draw the poly (if ground is filled)
		//No need for compass labels or "Horizon" label
		if ( Options::showGround() ) {
			groundPoly.clear();
			groundPoly << QPointF( -10., -10. ) 
						<< QPointF( Width + 10., -10. )
						<< QPointF( Width + 10., Height + 10. )
						<< QPointF( -10., Height + 10. );
	
			psky.drawPolygon( groundPoly );
		}
		return;
	}

	//groundPoly now contains QPointF's of the screen coordinates of points 
	//along the "front half" of the Horizon, in order from left to right.
	//If we are using Equatorial coords, then all we need to do is connect 
	//these points.
	if ( ! Options::useAltAz() ) {
		for ( int i=1; i < groundPoly.size(); ++i ) 
			psky.drawLine( groundPoly.at(i-1), groundPoly.at(i) );

	//If we are using Horizontal coordinates, there is more work to do. 
	//We need to complete the ground polygon by going right to left.
	//If the zoomLevel is high (as indicated by (daz<75.0)), then we 
	//complete the polygon by simply adding points along thebottom edge 
	//of the screen.  If the zoomLevel is high (daz>75.0), then we add points 
	//along the bottom edge of the sky circle.  The sky circle has 
	//a radius of 2*sin(pi/4)*ZoomFactor, and the endpoints of the current 
	//groundPoly points lie 180 degrees apart along its circumference.  
	//We determine the polar angles t1, t2 corresponding to these end points, 
	//and then step along the circumference, adding points between them.
	//(In Horizontal coordinates, t1 and t2 are always 360 and 180, respectively).
	} else { //Horizontal coords
		if ( daz < 75.0 ) { //can complete polygon by simply adding offscreen points
			groundPoly << QPointF( Width + 10., groundPoly.last().y() )
						<< QPointF( Width + 10., Height + 10. )
						<< QPointF( -10., Height + 10. )
						<< QPointF( -10., groundPoly.first().y() );
	
		} else { //complete polygon along bottom of sky circle
			double r0 = 2.0*sin(0.25*dms::PI);
			double t1 = 360.;
			double t2 = 180.;

//NOTE: Uncomment if we ever want opaque ground while using Equatorial coords
// 		if ( ! Options::useAltAz() ) { //compute t1,t2
// 			//groundPoly.last() is the point on the Horizon that intersects 
// 			//the visible sky circle on the right
// 			t1 = -1.0*acos( (groundPoly.last().x() - 0.5*Width)/r0/Options::zoomFactor() )/dms::DegToRad; //angle in degrees
// 			//Resolve quadrant ambiguity
// 			if ( groundPoly.last().y() < 0. ) t1 = 360. - t1;
// 	
// 			t2 = t1 - 180.;
// 		}

			for ( double t=t1; t >= t2; t-=2. ) {  //step along circumference
				dms a( t );
				double sa(0.), ca(0.);
				a.SinCos( sa, ca );
				float xx = 0.5*Width  + r0*Options::zoomFactor()*ca;
				float yy = 0.5*Height - r0*Options::zoomFactor()*sa;
		
				groundPoly << QPointF( xx, yy );
			}
		}

		//Finally, draw the ground Polygon.
		psky.drawPolygon( groundPoly );
	}

	//Set color for compass labels and "Horizon" label.
	if ( Options::showGround() && Options::useAltAz() )
		psky.setPen( QColor ( ks->data()->colorScheme()->colorNamed( "CompassColor" ) ) );
	else
		psky.setPen( QColor ( ks->data()->colorScheme()->colorNamed( "HorzColor" ) ) );

	drawCompassLabels( ks, psky, scale );

	//Draw Horizon name label
	//pAnchor contains the last point of the Horizon before it went offcreen 
	//on the right/top/bottom edge.  oAnchor2 is the next point after oAnchor.
	if ( pAnchor ) {
		int iAnchor = pointList().indexOf( pAnchor );
		if ( iAnchor == pointList().size()-1 ) iAnchor = 0;
		else iAnchor++;
		pAnchor2 = pointList().at( iAnchor );
	
		QPointF o1 = map->getXY( pAnchor, Options::useAltAz(), false, scale );
		QPointF o2 = map->getXY( pAnchor2, Options::useAltAz(), false, scale );
		
		float x1, x2;
		//there are 3 possibilities:  (o2.x() > width()); (o2.y() < 0); (o2.y() > height())
		if ( o2.x() > Width ) {
			x1 = (Width - o1.x())/(o2.x() - o1.x());
			x2 = (o2.x() - Width)/(o2.x() - o1.x());
		} else if ( o2.y() < 0 ) {
			x1 = o1.y()/(o1.y() - o2.y());
			x2 = -1.0*o2.y()/(o1.y() - o2.y());
		} else if ( o2.y() > Height ) {
			x1 = (Height - o1.y())/(o2.y() - o1.y());
			x2 = (o2.y() - Height)/(o2.y() - o1.y());
		} else {  //should never get here
			x1 = 0.0;
			x2 = 1.0;
		}
	
		//ra0 is the exact RA at which the Horizon intersects a screen edge
		double ra0 = x1*pAnchor2->ra()->Hours() + x2*pAnchor->ra()->Hours();
		//dec0 is the exact Dec at which the Horizon intersects a screen edge
		double dec0 = x1*pAnchor2->dec()->Degrees() + x2*pAnchor->dec()->Degrees();
	
		//LabelPoint is offset from the anchor point by -2.0 degrees in azimuth
		//and -0.4 degree altitude, scaled by 2000./zoomFactor so that they are
		//independent of zoom.
		SkyPoint LabelPoint(ra0, dec0);
		LabelPoint.EquatorialToHorizontal( ks->data()->lst(), ks->data()->geo()->lat() );
		LabelPoint.setAlt( LabelPoint.alt()->Degrees() - 800./Options::zoomFactor() );
		LabelPoint.setAz( LabelPoint.az()->Degrees() - 4000./Options::zoomFactor() );
		LabelPoint.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	
		o = map->getXY( &LabelPoint, Options::useAltAz(), false, scale );

		if ( o.x() > Width || o.x() < 0 ) {
			//the LabelPoint is offscreen.  Either we are in the Southern hemisphere,
			//or the sky is rotated upside-down.  Use an azimuth offset of +2.0 degrees
		LabelPoint.setAlt( LabelPoint.alt()->Degrees() + 1600./Options::zoomFactor() );
			LabelPoint.setAz( LabelPoint.az()->Degrees() + 8000./Options::zoomFactor() );
			LabelPoint.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
		}
	
		//p2 is a skypoint offset from LabelPoint by +/-1 degree azimuth (scaled by
		//2000./zoomFactor).  We use p2 to determine the rotation angle for the
		//Horizon label, which we want to be parallel to the line between LabelPoint and p2.
		SkyPoint p2( LabelPoint.ra(), LabelPoint.dec() );
		p2.EquatorialToHorizontal( ks->data()->lst(), ks->data()->geo()->lat() );
		p2.setAz( p2.az()->Degrees() + 2000./Options::zoomFactor() );
		p2.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	
		o2 = map->getXY( &p2, Options::useAltAz(), false, scale );
	
		float sx = o.x() - o2.x();
		float sy = o.y() - o2.y();
		float angle;
		if ( sx ) {
			angle = atan( sy/sx )*180.0/dms::PI;
		} else {
			angle = 90.0;
			if ( sy < 0 ) angle = -90.0;
		}
	
		//Finally, draw the "Horizon" label at the determined location and angle
		psky.save();
		psky.translate( o.x(), o.y() );
		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( 0, 0, i18n( "Horizon" ) );
		psky.restore(); //reset coordinate system
	}
}


void HorizonComponent::drawCompassLabels( KStars *ks, QPainter& psky, double scale ) {
	SkyPoint c;
	QPointF cpoint;
	SkyMap *map = ks->map();
	float Width = scale * map->width();
	float Height = scale * map->height();

	//North
	c.setAz( 359.99 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "North", "N" ) );
	}

	//NorthEast
	c.setAz( 45.0 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "Northeast", "NE" ) );
	}

	//East
	c.setAz( 90.0 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "East", "E" ) );
	}

	//SouthEast
	c.setAz( 135.0 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "Southeast", "SE" ) );
	}

	//South
	c.setAz( 179.99 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "South", "S" ) );
	}

	//SouthWest
	c.setAz( 225.0 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "Southwest", "SW" ) );
	}

	//West
	c.setAz( 270.0 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "West", "W" ) );
	}

	//NorthWest
	c.setAz( 315.0 );
	c.setAlt( 0.0 );
	if ( !Options::useAltAz() ) c.HorizontalToEquatorial( ks->data()->lst(), ks->data()->geo()->lat() );
	cpoint = map->getXY( &c, Options::useAltAz(), false, scale );
	cpoint.setY( cpoint.y() + scale*20. );
	if (cpoint.x() > 0. && cpoint.x() < Width && cpoint.y() > 0. && cpoint.y() < Height ) {
		psky.drawText( cpoint, i18n( "Northwest", "NW" ) );
	}
}
