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

#include <QPoint>
#include <QPainter>

#include "skycomposite.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "Options.h"

CoordinateGridComponent::CoordinateGridComponent(SkyComponent *parent, bool (*visibleMethod)(), bool isParallel, double coord ) 
: PointListComponent(parent, visibleMethod), Parallel( isParallel ), Coordinate( coord )
{
}

CoordinateGridComponent::~CoordinateGridComponent() {
}

void CoordinateGridComponent::init( KStarsData *data ) {
	if ( Parallel ) { //line of constant Declination
		double dra = 1./15.; //360 points around full circle
		for ( double ra=0.0; ra < 24.0; ra += dra ) {
			SkyPoint *sp = new SkyPoint( ra, Coordinate );
			sp->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			gridList.append( sp );
		}
	} else { //line of constant Right Ascension
		double RA = Coordinate;
		double OppositeRA = Coordinate + 12.0;
		if ( OppositeRA > 24.0 ) OppositeRA -= 24.0;
		for ( double dec=-90.; dec < 270.; dec += 1.0 ) {
			double Dec = dec;
			
			if ( dec > 90. ) {
				Dec = 180. - dec;
				RA = OppositeRA;
			}
			SkyPoint *sp = new SkyPoint( RA, Dec );
			sp->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			gridList.append( sp );
		}
	}
}

void CoordinateGridComponent::draw(KStars *ks, QPainter& psky, double scale)
{
// TODO add accessor methods to map for guideMax etc.

	if (!Options::showGrid()) return;
	
	SkyMap *map = ks->map();

	QPoint cur;
	bool newlyVisible = false;

	//Draw coordinate grid
	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "GridColor" ) ), 1, DotLine ) ); //change to GridColor

	SkyPoint *sp = gridList[0];
	QPoint o = getXY( sp, Options::useAltAz(), Options::useRefraction(), scale );
	QPoint o1 = o;
	cur = o;
	psky.moveTo( o.x(), o.y() );

	foreach ( sp, gridList ) {
		if ( map->checkVisibility( sp ) ) {
			o = getXY( sp, Options::useAltAz(), Options::useRefraction(), scale );

			//When drawing on the printer, the psky.pos() point does NOT get updated
			//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
			int dx = cur.x() - o.x();
			int dy = cur.y() - o.y();
			cur = o;

			if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
				if ( newlyVisible ) {
					newlyVisible = false;
					psky.moveTo( o.x(), o.y() );
				} else {
					psky.lineTo( o.x(), o.y() );
				}
			} else {
				psky.moveTo( o.x(), o.y() );
			}
		}
	}

	//connect the final segment
	int dx = psky.pos().x() - o1.x();
	int dy = psky.pos().y() - o1.y();
	if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
		psky.lineTo( o1.x(), o1.y() );
	} else {
		psky.moveTo( o1.x(), o1.y() );
	}
}
