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

#include "kstarsdata.h"
#include "skyline.h"
#include "satellitecomponent.h"

SatelliteComponent::SatelliteComponent(SkyComponent *parent, bool (*visibleMethod)())
: LineListComponent(parent, visibleMethod)
{
}

SatelliteComponent::~SatelliteComponent() {
}

void SatelliteComponent::init( KStarsData *data, SPositionSat *pSat[], int npos ) {
	SkyPoint p1, p2;
	p2.setAlt( pSat[0]->sat_ele );
	p2.setAz( pSat[0]->sat_azi );
	p2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );

	for ( int i=1; i<npos; i++ ) {
		p1 = p2;
		p2.setAlt( pSat[i]->sat_ele );
		p2.setAz( pSat[i]->sat_azi );
		p2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );

		lineList().append( new SkyLine( p1, p2 ) );
	}
}
