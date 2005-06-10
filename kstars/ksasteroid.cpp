/***************************************************************************
                          ksasteroid.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
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

#include <kdebug.h>

#include "ksasteroid.h"
#include "dms.h"
#include "ksnumbers.h"
#include "ksutils.h"
#include "kstarsdata.h"

KSAsteroid::KSAsteroid( KStarsData *_kd, QString s, QString imfile,
		long double _JD, double _a, double _e, dms _i, dms _w, dms _Node, dms _M, double _H )
 : KSPlanetBase(_kd, s, imfile), kd(_kd), JD(_JD), a(_a), e(_e), H(_H), i(_i), w(_w), M(_M), N(_Node) {

	setType( 10 ); //Asteroid
	setMag( H );
	//Compute the orbital Period from Kepler's 3rd law:
	P = 365.2568984 * pow(a, 1.5); //period in days
}

bool KSAsteroid::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {
	//Precess the longitude of the Ascending Node to the desired epoch:
	dms n = dms( double( N.Degrees() - 3.82394E-5 * ( num->julianDay() - J2000 )) ).reduce();

	//determine the mean anomaly for the desired date.  This is the mean anomaly for the
	//ephemeis epoch, plus the number of days between the desired date and ephemeris epoch,
	//times the asteroid's mean daily motion (360/P):
	dms m = dms( double( M.Degrees() + ( num->julianDay() - JD ) * 360.0/P ) ).reduce();
	double sinm, cosm;
	m.SinCos( sinm, cosm );

	//compute eccentric anomaly:
	double E = m.Degrees() + e*180.0/dms::PI * sinm * ( 1.0 + e*cosm );

	if ( e > 0.05 ) { //need more accurate approximation, iterate...
		double E0;
		int iter(0);
		do {
			E0 = E;
			iter++;
			E = E0 - ( E0 - e*180.0/dms::PI *sin( E0*dms::DegToRad ) - m.Degrees() )/(1 - e*cos( E0*dms::DegToRad ) );
		} while ( fabs( E - E0 ) > 0.001 && iter < 1000 );
	}

	double sinE, cosE;
	dms E1( E );
	E1.SinCos( sinE, cosE );

	double xv = a * ( cosE - e );
	double yv = a * sqrt( 1.0 - e*e ) * sinE;

	//v is the true anomaly; r is the distance from the Sun

	double v = atan( yv/xv ) / dms::DegToRad;
	//resolve atan ambiguity
	if ( xv < 0.0 ) v += 180.0;

	double r = sqrt( xv*xv + yv*yv );

	//vw is the sum of the true anomaly and the argument of perihelion
	dms vw( v + w.Degrees() );
	double sinN, cosN, sinvw, cosvw, sini, cosi;

	N.SinCos( sinN, cosN );
	vw.SinCos( sinvw, cosvw );
	i.SinCos( sini, cosi );

	//xh, yh, zh are the heliocentric cartesian coords with the ecliptic plane congruent with zh=0.
	double xh = r * ( cosN * cosvw - sinN * sinvw * cosi );
	double yh = r * ( sinN * cosvw + cosN * sinvw * cosi );
	double zh = r * ( sinvw * sini );

	//the spherical ecliptic coordinates:
	double ELongRad = atan( yh/xh );
	//resolve atan ambiguity
	if ( xh < 0.0 ) ELongRad += dms::PI;
	double ELatRad = atan( zh/r );  //(r can't possibly be negative, so no atan ambiguity)

	helEcPos.longitude.setRadians( ELongRad );
	helEcPos.latitude.setRadians( ELatRad );
	setRsun( r );

	if ( Earth ) {
		//xe, ye, ze are the Earth's heliocentric cartesian coords
		double cosBe, sinBe, cosLe, sinLe;
		Earth->ecLong()->SinCos( sinLe, cosLe );
		Earth->ecLat()->SinCos( sinBe, cosBe );
	
		double xe = Earth->rsun() * cosBe * cosLe;
		double ye = Earth->rsun() * cosBe * sinLe;
		double ze = Earth->rsun() * sinBe;
	
		//convert to geocentric ecliptic coordinates by subtracting Earth's coords:
		xh -= xe;
		yh -= ye;
		zh -= ze;
	}

	//the spherical geocentricecliptic coordinates:
	ELongRad = atan( yh/xh );
	//resolve atan ambiguity
	if ( xh < 0.0 ) ELongRad += dms::PI;

	double rr = sqrt( xh*xh + yh*yh + zh*zh );
	ELatRad = atan( zh/rr );  //(rr can't possibly be negative, so no atan ambiguity)

	ep.longitude.setRadians( ELongRad );
	ep.latitude.setRadians( ELatRad );
	if ( Earth ) setRearth( Earth );
	
	EclipticToEquatorial( num->obliquity() );
	nutate( num );
	aberrate( num );

	return true;
}

//Unused virtual function from KSPlanetBase
bool KSAsteroid::loadData() { return false; }

