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
#include "kstars.h"
#include "ksutils.h"
#include "ksplanetbase.h"


KSPlanetBase::KSPlanetBase( KStars *ks, QString s, QString image_file )
 : SkyObject( 2, 0.0, 0.0, 0.0, s, "" ), Image(0), kstars(ks) {

	 if (! image_file.isEmpty()) {
		QFile imFile;

		if ( KSUtils::openDataFile( imFile, image_file ) ) {
			imFile.close();
			Image0.load( imFile.name() );
			Image = Image0.convertDepth( 32 );
			Image0 = Image;
		}
	}
	PositionAngle = 0.0;
	ImageAngle = 0.0;
	Trail.setAutoDelete( TRUE );
}

void KSPlanetBase::EquatorialToEcliptic( const dms *Obliquity ) {
	findEcliptic( Obliquity, ep.longitude, ep.latitude );
}
	
void KSPlanetBase::EclipticToEquatorial( const dms *Obliquity ) {
	setFromEcliptic( Obliquity, &ep.longitude, &ep.latitude );
}

void KSPlanetBase::updateCoords( KSNumbers *num, bool includePlanets ){
	if ( includePlanets ) {
		kstars->data()->earth()->findPosition( num );
		findPosition( num, kstars->data()->earth() );
	}
}

void KSPlanetBase::updateTrail( dms *LST, const dms *lat ) {
	for ( SkyPoint *sp = Trail.first(); sp; sp = Trail.next() ) 
		sp->EquatorialToHorizontal( LST, lat );
}

void KSPlanetBase::findPA( const KSNumbers *num ) {
	//Determine position angle of planet (assuming that it is aligned with
	//the Ecliptic, which is only roughly correct).
	//Displace a point along +Ecliptic Latitude by 1 degree
	SkyPoint test;
	dms newELat( ecLat()->Degrees() + 1.0 );
	test.setFromEcliptic( num->obliquity(), ecLong(), &newELat );
	double dx = test.ra()->Degrees() - ra()->Degrees();
	double dy = dec()->Degrees() - test.dec()->Degrees();
	double pa;
	if ( dy ) {
		pa = atan( dx/dy )*180.0/dms::PI;
	} else {
		pa = 90.0;
		if ( dx > 0 ) pa = -90.0;
	}
	setPA( pa );
}

void KSPlanetBase::rotateImage( double imAngle ) {
	ImageAngle = imAngle;
	QWMatrix m;
	m.rotate( ImageAngle );
	Image = Image0.xForm( m );
}
