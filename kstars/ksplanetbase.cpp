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

#include <qfile.h>
#include <qpoint.h>
#include <qwmatrix.h>

#include "ksplanetbase.h"
#include "ksplanet.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "kspopupmenu.h"


KSPlanetBase::KSPlanetBase( KStarsData *kd, QString s, QString image_file, double pSize )
 : SkyObject( 2, 0.0, 0.0, 0.0, s, "" ), Rearth(0.0), Image(0), data(kd) {

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
	PhysicalSize = pSize;
	Trail.setAutoDelete( TRUE );
}

void KSPlanetBase::EquatorialToEcliptic( const dms *Obliquity ) {
	findEcliptic( Obliquity, ep.longitude, ep.latitude );
}

void KSPlanetBase::EclipticToEquatorial( const dms *Obliquity ) {
	setFromEcliptic( Obliquity, &ep.longitude, &ep.latitude );
}

void KSPlanetBase::updateCoords( KSNumbers *num, bool includePlanets, const dms *lat, const dms *LST ){
	if ( includePlanets ) {
		data->earth()->findPosition( num ); //since we don't pass lat & LST, localizeCoords will be skipped

		if ( lat && LST ) {
			findPosition( num, lat, LST, data->earth() );
			if ( hasTrail() ) Trail.removeLast();
		} else {
			findGeocentricPosition( num, data->earth() );
		}
	}
}

void KSPlanetBase::findPosition( const KSNumbers *num, const dms *lat, const dms *LST, const KSPlanetBase *Earth ) {
	findGeocentricPosition( num, Earth );  //private function, reimplemented in each subclass

	if ( lat && LST )
		localizeCoords( num, lat, LST ); //correct for figure-of-the-Earth

	if ( Earth ) setRearth( Earth );

	if ( hasTrail() ) {
		Trail.append( new SkyPoint( ra(), dec() ) );
		if ( Trail.count() > MAXTRAIL ) Trail.removeFirst();
	}
}

void KSPlanetBase::localizeCoords( const KSNumbers *num, const dms *lat, const dms *LST ) {
	//convert geocentric coordinates to local apparent coordinates (topocentric coordinates)
	dms HA, HA2; //Hour Angle, before and after correction
	double rsinp, rcosp, u, sinHA, cosHA, sinDec, cosDec, D;
	double cosHA2;
	double r = Rearth * AU_KM; //distance from Earth, in km
	u = atan( 0.996647*tan( lat->radians() ) );
	rsinp = 0.996647*sin( u );
	rcosp = cos( u );
	HA.setD( LST->Degrees() - ra()->Degrees() );
	HA.SinCos( sinHA, cosHA );
	dec()->SinCos( sinDec, cosDec );

	D = atan( ( rcosp*sinHA )/( r*cosDec/6378.14 - rcosp*cosHA ) );
	dms temp;
	temp.setRadians( ra()->radians() - D );
	setRA( temp );

	HA2.setD( LST->Degrees() - ra()->Degrees() );
	cosHA2 = cos( HA2.radians() );
	temp.setRadians( atan( cosHA2*( r*sinDec/6378.14 - rsinp )/( r*cosDec*cosHA/6378.14 - rcosp ) ) );
	setDec( temp );

	EquatorialToEcliptic( num->obliquity() );
}

void KSPlanetBase::setRearth( const KSPlanetBase *Earth ) {
	double sinL, sinB, sinL0, sinB0;
	double cosL, cosB, cosL0, cosB0;
	double x,y,z;

	//The Moon's Rearth is set in its findGeocentricPosition()...
	if ( name() == "Moon" ) {
		return;
	}

	if ( name() == "Earth" ) {
		Rearth = 0.0;
		return;
	}

	if ( ! Earth && name() != "Moon" ) {
		kdDebug() << i18n( "KSPlanetBase::setRearth():  Error: Need an Earth pointer.  (" ) << name() << ")" << endl;
		Rearth = 1.0;
		return;
	}

	Earth->ecLong()->SinCos( sinL0, cosL0 );
	Earth->ecLat()->SinCos( sinB0, cosB0 );
	double eX = Earth->rsun()*cosB0*cosL0;
	double eY = Earth->rsun()*cosB0*sinL0;
	double eZ = Earth->rsun()*sinB0;

	ecLong()->SinCos( sinL, cosL );
	ecLat()->SinCos( sinB, cosB );
	x = rsun()*cosB*cosL - eX;
	y = rsun()*cosB*sinL - eY;
	z = rsun()*sinB - eZ;

	Rearth = sqrt(x*x + y*y + z*z);

	//Set angular size, in arcmin
	AngularSize = asin(PhysicalSize/Rearth/AU_KM)*60.*180./dms::PI;
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

void KSPlanetBase::scaleRotateImage( int scale, double imAngle ) {
	ImageAngle = imAngle;
	QWMatrix m;
	m.rotate( ImageAngle );
	Image = Image0.xForm( m ).smoothScale( scale, scale );
}
