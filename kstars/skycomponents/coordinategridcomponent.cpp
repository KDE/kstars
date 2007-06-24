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
		for ( double ra=0.0; ra < 24.0; ra += dra ) {
			SkyPoint p( ra, Coordinate );
			p.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
			appendPoint( p );
		}
	} else { //line of constant Right Ascension
		double RA = Coordinate;
		double OppositeRA = Coordinate + 12.0;
		if ( OppositeRA > 24.0 ) OppositeRA -= 24.0;

		for ( double dec=-90.; dec <= 270.; dec += 4.0 ) {
			double Dec = dec;
			if ( dec > 90. ) {
				Dec = 180. - dec;
				RA = OppositeRA;
			}
			SkyPoint p( RA, Dec );
			p.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
			appendPoint( p );
		}
	}
}
