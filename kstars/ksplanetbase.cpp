/***************************************************************************
                          ksplanetbase.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <math.h>
#include <qwmatrix.h>
#include "ksutils.h"
#include "ksplanetbase.h"


KSPlanetBase::KSPlanetBase( QString s, QString image_file )
 : SkyObject( 2, 0.0, 0.0, 0.0, s, "" ), Image(0) {

	 if (! image_file.isEmpty()) {
		QFile imFile;

		if ( KSUtils::openDataFile( imFile, image_file ) ) {
			imFile.close();
			Image0.load( imFile.name() );
			Image = Image0;
			PositionAngle = 0.0;
		}
	}
}

void KSPlanetBase::EquatorialToEcliptic( dms Obliquity ) {
	findEcliptic( Obliquity, ep.longitude, ep.latitude );
}
	
void KSPlanetBase::EclipticToEquatorial( dms Obliquity ) {
	setFromEcliptic( Obliquity, ep.longitude, ep.latitude );
}

void KSPlanetBase::updateCoords( KSNumbers *num ) {
}

void KSPlanetBase::updatePA( double p ) {
//Update PositionAngle and rotate Image if the new position angle (p) is
//more than 5 degrees from the stored PositionAngle.
	if ( fabs( p - PositionAngle ) > 5.0 ) {
		PositionAngle = p;

		QWMatrix m;
		m.rotate( PositionAngle );
		Image = Image0.xForm( m );
	}
}
