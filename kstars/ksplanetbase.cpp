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
#include "ksutils.h"
#include "ksplanetbase.h"


KSPlanetBase::KSPlanetBase( QString s, QString image_file )
 : SkyObject( 2, 0.0, 0.0, 0.0, s, "" ), Image(0) {

	 if (! image_file.isEmpty()) {
		QFile imFile;

		if ( KSUtils::openDataFile( imFile, image_file ) ) {
			imFile.close();
			Image.load( imFile.name() );
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
