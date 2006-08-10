/***************************************************************************
                          satellitetrack.cpp  -  description
                             -------------------
    begin                : Fri 14 July 2006
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

#include "satellitetrack.h"

SatelliteTrack::SatelliteTrack() {
}

SatelliteTrack::SatelliteTrack( SPositionSat *pSat[], long nPos, const dms *LST, const dms *lat ) {
	SkyPoint p1, p2;
	p2.setAlt( pSat[0]->sat_ele );
	p2.setAz( pSat[0]->sat_azi );
	p2.HorizontalToEquatorial( LST, lat );

	for ( int i=1; i<nPos; i++ ) {
		p1 = p2;
		p2.setAlt( pSat[i]->sat_ele );
		p2.setAz( pSat[i]->sat_azi );
		p2.HorizontalToEquatorial( LST, lat );

		Path.append( new SkyLine( p1, p2 ) );
	}
}

SatelliteTrack::~SatelliteTrack() {
	while ( ! Path.isEmpty() ) delete Path.takeFirst();
}

const QList<SkyLine*>& SatelliteTrack::path() const {
	return Path;
}
