/***************************************************************************
                          satellitecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 14 July 2006
    copyright            : (C) 2006 by Jason Harris
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

#include "satellitecomponent.h"

#include <QBrush>
#include <QPainter>

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyline.h"
#include "Options.h"

SatelliteComponent::SatelliteComponent(SkyComponent *parent, bool (*visibleMethod)())
: LineListComponent(parent, visibleMethod)
{
}

SatelliteComponent::~SatelliteComponent() {
}

void SatelliteComponent::init( const QString &name, KStarsData *data, SPositionSat *pSat[], int npos ) {
	SkyPoint p1, p2;

	setLabel( name );
	setLabelPosition( LineListComponent::RightEdgeLabel );

	setPen( QPen( QBrush( data->colorScheme()->colorNamed( "SatColor" ) ), 
										 2.5, Qt::SolidLine ) );

	p2.setAlt( pSat[0]->sat_ele );
	p2.setAz( pSat[0]->sat_azi );
	p2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );

	for ( uint i=1; i<npos; i++ ) {
		p1 = p2;
		p2.setAlt( pSat[i]->sat_ele );
		p2.setAz( pSat[i]->sat_azi );
		p2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );

		lineList().append( new SkyLine( p1, p2 ) );
		jdList().append( pSat[i]->jd );

	}
}

void SatelliteComponent::draw( KStars *ks, QPainter &psky, double scale ) {
	LineListComponent::draw( ks, psky, scale );

	if ( jdList().size() == 0 ) return;

	//Add tickmarks and timestamps along the portion of the 
	//satellite track that is above the horizon
	//The time at each position is stored in the pSat array 
	//as the julian day.  Parse these days, and interpolate to find 
	//times with :00 seconds (e.g., 12:34:00)
	KStarsDateTime dtLast( jdList()[0] );
	for ( uint i=1; i<jdList().size(); ++i ) {
		KStarsDateTime dt( jdList()[i] );
		SkyLine *sl = lineList()[i];
		SkyLine *sl2;
		if ( i<lineList().size()-1 )
			sl2 = lineList()[i+1];
		else
			sl2 = lineList()[i-1];

		if ( sl->startPoint()->alt()->Degrees() > 0.0 
				 && dt.time().minute() != dtLast.time().minute() ) {
			double t1 = double(dtLast.time().second());
			double t2 = double(dt.time().second()) + 60.0;
			double f = ( 60.0 - t1 )/( t2 - t1 );

			//Create a SkyLine which is a tickmark at the position along 
			//the track corresponding to the even-second time.
			//f is the fractional distance between the endpoints.
			SkyLine *sl = lineList()[i];
			double ra = f*sl->endPoint()->ra()->Hours() 
				+ (1.0-f)*sl->startPoint()->ra()->Hours();
			double dc = f*sl->endPoint()->dec()->Degrees() 
				+ (1.0-f)*sl->startPoint()->dec()->Degrees();
			SkyPoint sp(ra, dc);
			SkyLine satTick( &sp, sl2->endPoint() );

			//satTick is a skyline that starts at the tick position,
			//and is parallel to the satellite track.  We want to change 
			//the endpoint so it's perpendicular to the track, and 
			//change its length to be a few pixels
//			satTick.rotate( 90.0 );
			satTick.setAngularSize( 300/Options::zoomFactor() );
			satTick.startPoint()->EquatorialToHorizontal( ks->data()->lst(), ks->data()->geo()->lat() );
			satTick.endPoint()->EquatorialToHorizontal( ks->data()->lst(), ks->data()->geo()->lat() );

			QLineF tick = ks->map()->toScreen( &satTick, scale );
			double labelpa = atan2( tick.dy(), tick.dx() )/dms::DegToRad;
			QLineF tick2 = tick.normalVector();
			if ( tick2.y2() < tick2.y1() ) {
				//Rotate tick by 180 degrees
				double x1 = tick2.x1();
				double y1 = tick2.y1();
				tick2 = QLineF( x1, y1, x1 - tick2.dx(), y1 - tick2.dy() );
			}
			psky.drawLine( tick2 );

			//Now, add a label to the tickmark showing the time
			if ( labelpa > 270.0 ) labelpa -= 360.0;
			else if ( labelpa > 90.0 ) labelpa -= 180.0;
			else if ( labelpa < -270.0 ) labelpa += 360.0;
			else if ( labelpa < -90.0  ) labelpa += 180.0;

			QTime tlabel = dt.time().addSecs( 3600.0*ks->data()->geo()->TZ() );
			psky.save();
			QFont stdFont( psky.font() );
			QFont smallFont( stdFont );
			smallFont.setPointSize( stdFont.pointSize() - 2 );
			psky.setFont( smallFont );
			psky.translate( tick.p2() );
			psky.rotate( labelpa );
			psky.drawText( QRectF(-25.0, 6.0, 40.0, 10.0), Qt::AlignCenter, 
										 tlabel.toString( "hh:mm" ) );
			psky.restore();
			psky.setFont( stdFont );
		}

		dtLast = dt;
	}

}
	
