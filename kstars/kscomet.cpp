/***************************************************************************
                          kscomet.cpp  -  K Desktop Planetarium
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

#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "dms.h"
#include "kscomet.h"


KSComet::KSComet( KStarsData *_kd, QString _s, QString imfile,
		long double _JD, double _q, double _e, dms _i, dms _w, dms _Node, double Tp )
 : KSPlanetBase(_kd, _s, imfile), kd(_kd), JD(_JD), q(_q), e(_e), i(_i), w(_w), N(_Node) {

	setType( 9 ); //Comet

	//Find the Julian Day of Perihelion from Tp
	//Tp is a double which encodes a date like: YYYYMMDD.DDDDD (e.g., 19730521.33333
	int year = int( Tp/10000.0 );
	int month = int( (int(Tp) % 10000)/100.0 );
	int day = int( int(Tp) % 100 );
	double Hour = 24.0 * ( Tp - int(Tp) );
	int h = int( Hour );
	int m = int( 60.0 * ( Hour - h ) );
	int s = int( 60.0 * ( 60.0 * ( Hour - h) - m ) );

	JDp = KStarsDateTime( ExtDate( year, month, day ), QTime( h, m, s ) ).djd();

	//compute the semi-major axis, a:
	a = q/(1.0-e);

	//Compute the orbital Period from Kepler's 3rd law:
	P = 365.2568984 * pow(a, 1.5); //period in days

	//If the name contains a "/", make this name2 and make name a truncated version without the leading "P/" or "C/"
	if ( name().contains( "/" ) ) {
		setLongName( name() );
		setName( name().mid( name().find("/") + 1 ) );
	}
}

bool KSComet::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {
	double v(0.0), r(0.0);

	//Precess the longitude of the Ascending Node to the desired epoch:
	dms n = dms( double(N.Degrees() - 3.82394E-5 * ( num->julianDay() - J2000 )) ).reduce();

	if ( e > 0.98 ) {
		//Use near-parabolic approximation
		double k = 0.01720209895; //Gauss gravitational constant
		double a = 0.75 * ( num->julianDay() - JDp ) * k * sqrt( (1+e)/(q*q*q) );
		double b = sqrt( 1.0 + a*a );
		double W = pow((b+a),1.0/3.0) - pow((b-a),1.0/3.0);
		double c = 1.0 + 1.0/(W*W);
		double f = (1.0-e)/(1.0+e);
		double g = f/(c*c);

		double a1 = (2.0/3.0) + (2.0*W*W/5.0);
		double a2 = (7.0/5.0) + (33.0*W*W/35.0) + (37.0*W*W*W*W/175.0);
		double a3 = W*W*( (432.0/175.0) + (956.0*W*W/1125.0) + (84.0*W*W*W*W/1575.0) );
		double w = W*(1.0 + g*c*( a1 + a2*g + a3*g*g ));

		v = 2.0*atan(w) / dms::DegToRad;
		r = q*( 1.0 + w*w )/( 1.0 + w*w*f );
	} else {
		//Use normal ellipse method
		//Determine Mean anomaly for desired date:
		dms m = dms( double(360.0*( num->julianDay() - JDp )/P) ).reduce();
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

		v = atan( yv/xv ) / dms::DegToRad;
		//resolve atan ambiguity
		if ( xv < 0.0 ) v += 180.0;

		r = sqrt( xv*xv + yv*yv );
	}

	//vw is the sum of the true anomaly and the argument of perihelion
	dms vw( v + w.Degrees() );
	double sinN, cosN, sinvw, cosvw, sini, cosi;

	n.SinCos( sinN, cosN );
	vw.SinCos( sinvw, cosvw );
	i.SinCos( sini, cosi );

	//xh, yh, zh are the heliocentric cartesian coords with the ecliptic plane congruent with zh=0.
	double xh = r * ( cosN * cosvw - sinN * sinvw * cosi );
	double yh = r * ( sinN * cosvw + cosN * sinvw * cosi );
	double zh = r * ( sinvw * sini );

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

	//Finally, the spherical ecliptic coordinates:
	double ELongRad = atan( yh/xh );
	//resolve atan ambiguity
	if ( xh < 0.0 ) ELongRad += dms::PI;

	double rr = sqrt( xh*xh + yh*yh );
	double ELatRad = atan( zh/rr );  //(rr can't possibly be negative, so no atan ambiguity)

	ep.longitude.setRadians( ELongRad );
	ep.latitude.setRadians( ELatRad );
	setRsun( r );
	setRearth( Earth );
	
	EclipticToEquatorial( num->obliquity() );
	nutate( num );
	aberrate( num );

	return true;
}

//Unused virtual function from KSPlanetBase
bool KSComet::loadData() { return false; }
