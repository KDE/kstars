/***************************************************************************
                          milkywaycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/08
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

#include "coordinategridcomponent.h"

#include <QPainter>

#include "skycomposite.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyline.h" 
#include "skypoint.h" 
#include "Options.h"

CoordinateGridComponent::CoordinateGridComponent(SkyComponent *parent, bool (*visibleMethod)(), bool isParallel, double coord ) 
: LineListComponent(parent, visibleMethod), Parallel( isParallel ), Coordinate( coord )
{
}

CoordinateGridComponent::~CoordinateGridComponent() {
}

void CoordinateGridComponent::init( KStarsData *data ) {
	emitProgressText( i18n("Loading coordinate grid" ) );

	setPen( QPen( QBrush( data->colorScheme()->colorNamed( "GridColor" ) ), 
								1, Qt::DotLine ) );

	if ( Parallel ) { //line of constant Declination
		double dra = 1./5.; //120 points around full circle
		SkyPoint o1( 0.0, Coordinate );
		for ( double ra=0.0; ra < 24.0; ra += dra ) {
			SkyPoint o2( ra, Coordinate );
			SkyLine *sl = new SkyLine( o1, o2 );
			sl->update( data );
			lineList().append( sl );
			o1 = o2;
		}
	} else { //line of constant Right Ascension
		double RA = Coordinate;
		double OppositeRA = Coordinate + 12.0;
		if ( OppositeRA > 24.0 ) OppositeRA -= 24.0;

		SkyPoint o1( RA, -90.0 );
		for ( double dec=-90. + 4.0; dec < 270.; dec += 4.0 ) {
			double Dec = dec;
			if ( dec > 90. ) {
				Dec = 180. - dec;
				RA = OppositeRA;
			}
			SkyPoint o2( RA, Dec );
			SkyLine *sl = new SkyLine( o1, o2 );
			sl->update( data );
			lineList().append( sl );
			o1 = o2;
		}
	}
}

/*
void CoordinateGridComponent::draw(KStars *ks, QPainter& psky, double scale)
{
// TODO add accessor methods to map for guideMax etc.

	if ( ! visible() ) return;
	
	SkyMap *map = ks->map();

	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "GridColor" ) ), 1, Qt::DotLine ) ); //change to GridColor

	SkyPoint *sp = pointList()[0];
	QPointF o1, oLast;

	foreach ( sp, pointList() ) {
		if ( map->checkVisibility( sp ) ) {
			QPointF o = map->toScreen( sp, scale );
			if ( o1.isNull() ) { o1 = o; oLast = o; continue; } //First item in list

			float dx = o.x() - oLast.x();
			float dy = o.y() - oLast.y();

			if ( fabs(dx) < map->guideMaxLength()*scale && fabs(dy) < map->guideMaxLength()*scale ) {
				if ( Options::useAntialias() )
					psky.drawLine( oLast, o );
				else
					psky.drawLine( QPoint(int(oLast.x()),int(oLast.y())), 
							QPoint(int(o.x()),int(o.y())) );
			}
			oLast = o;
		}
	}

	//connect the final segment
	float dx = oLast.x() - o1.x();
	float dy = oLast.y() - o1.y();
	if ( fabs(dx) < map->guideMaxLength()*scale && fabs(dy) < map->guideMaxLength()*scale ) {
		if ( Options::useAntialias() )
			psky.drawLine( oLast, o1 );
		else
			psky.drawLine( QPoint(int(oLast.x()),int(oLast.y())), 
					QPoint(int(o1.x()),int(o1.y())) );
	}
}
*/
