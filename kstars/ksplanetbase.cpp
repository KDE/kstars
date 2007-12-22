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

	if ( Earth ) setRearth( Earth );

	if ( lat && LST )
		localizeCoords( num, lat, LST ); //correct for figure-of-the-Earth

	if ( hasTrail() ) {
		Trail.append( new SkyPoint( ra(), dec() ) );
		if ( Trail.count() > MAXTRAIL ) Trail.removeFirst();
	}

	if ( isMajorPlanet() )
		findMagnitude(num);
}

bool KSPlanetBase::isMajorPlanet() const {
	if ( name() == "Mercury" || name() == "Venus" || name() == "Mars" ||
				name() == "Jupiter" || name() == "Saturn" || name() == "Uranus" ||
				name() == "Neptune" || name() == "Pluto" )
		return true;
	return false;
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

	helEcLong()->SinCos( sinL, cosL );
	helEcLat()->SinCos( sinB, cosB );
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

void KSPlanetBase::findMagnitude(const KSNumbers *num) {
	
	double cosDec, sinDec;
	dec()->SinCos(cosDec, sinDec);
	
	/* Phase of the planet in degrees */
	double earthSun = 1.;
	double cosPhase = (rsun()*rsun() + rearth()*rearth() - earthSun*earthSun) 
	  / (2 * rsun() * rearth() );   
	double phase = acos ( cosPhase ) * 180.0 / dms::PI;

	/* Computation of the visual magnitude (V band) of the planet.
	* Algorithm provided by Pere Planesas (Observatorio Astronomico Nacional)
	* It has some simmilarity to J. Meeus algorithm in Astronomical Algorithms, Chapter 40.
	* */

	// Initialized to the faintest magnitude observable with the HST
	float magnitude = 30;

	double param = 5 * log10(rsun() * rearth() );
	double f1 = phase/100.;

	if ( name() == "Mercury" ) {
		if ( phase > 150. ) f1 = 1.5; 
		magnitude = -0.36 + param + 3.8*f1 - 2.73*f1*f1 + 2*f1*f1*f1;
	}
	if ( name() =="Venus")
		magnitude = -4.29 + param + 0.09*f1 + 2.39*f1*f1 - 0.65*f1*f1*f1;
	if( name() == "Mars")
		magnitude = -1.52 + param + 0.016*phase; 
	if( name() == "Jupiter") 
		magnitude = -9.25 + param + 0.005*phase;  

	if( name() == "Saturn") {
		double T = num->julianCenturies();
		double a0 = (40.66-4.695*T)* dms::PI / 180.;
		double d0 = (83.52+0.403*T)* dms::PI / 180.;
		double sinx = -cos(d0)*cosDec*cos(a0 - ra()->radians());
		sinx = fabs(sinx-sin(d0)*sinDec);
		double rings = -2.6*sinx + 1.25*sinx*sinx;
		magnitude = -8.88 + param + 0.044*phase + rings;  
	}

	if( name() == "Uranus")
		magnitude = -7.19 + param + 0.0028*phase;  
	if( name() == "Neptune")
		magnitude = -6.87 + param;
	if( name() == "Pluto" )
		magnitude = -1.01 + param + 0.041*phase;

	setMag(magnitude);
}
