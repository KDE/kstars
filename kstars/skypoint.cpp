/***************************************************************************
                          skypoint.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
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

#include "dms.h"
#include "kstars.h"
#include "skypoint.h"

SkyPoint::~SkyPoint(){
}

void SkyPoint::EquatorialToHorizontal( dms LSTh, dms lat ) {
	double AltRad, AzRad;
	double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
  double sinAlt, cosAlt;

	dms HourAngle = LSTh.Degrees() - RA.Degrees();

	lat.SinCos( sinlat, coslat );
	dec().SinCos( sindec, cosdec );
	HourAngle.SinCos( sinHA, cosHA );

	sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
	AltRad = asin( sinAlt );
	cosAlt = cos( AltRad );

	AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
	if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity

	Az.setRadians( AzRad );
	Alt.setRadians( AltRad );
}

void SkyPoint::HorizontalToEquatorial( dms LSTh, dms lat ) {
	double HARad, DecRad;
	double sinlat, coslat, sinAlt, cosAlt, sinAz, cosAz;
  double sinDec, cosDec;

	lat.SinCos( sinlat, coslat );
	Alt.SinCos( sinAlt, cosAlt );
	Az.SinCos( sinAz,  cosAz );

	sinDec = sinAlt*sinlat + cosAlt*coslat*cosAz;
	DecRad = asin( sinDec );
	cosDec = cos( DecRad );
	Dec.setRadians( DecRad );
	
	double x = ( sinAlt - sinlat*sinDec )/( coslat*cosDec );

//Under certain circumstances, x can be very slightly less than -1.0000, or slightly
//greater than 1.0000, leading to a crash on acos(x).  However, the value isn't
//*really* out of range; it's a kind of roundoff error.
	if ( x < -1.0 && x > -1.000001 ) HARad = dms::PI;
	else if ( x > 1.0 && x < 1.000001 ) HARad = 0.0;
	else if ( x < -1.0 ) {
		kdWarning() << i18n( "Coordinate out of range!" );
		HARad = dms::PI;
	} else if ( x > 1.0 ) {
		kdWarning() << i18n( "Coordinate out of range!" );
		HARad = 0.0;
	} else HARad = acos( x );

	if ( sinAz > 0.0 ) HARad = 2.0*dms::PI - HARad; // resolve acos() ambiguity	

	RA.setRadians( LSTh.radians() - HARad );
	RA.setD( RA.reduce().Degrees() );  // 0 <= RA < 24
}

void SkyPoint::setEcliptic( double ELong, double ELat, long double jd ) {
	//compute obliquity of ecliptic for date jd
	double T = (jd - 2451545.0)/36525.0; //number of centuries since Jan 1, 2000
	double DeltaObliq = 46.815*T + 0.0006*T*T - 0.00181*T*T*T; //change in Obliquity, in arcseconds
	dms Obliquity;
	Obliquity.setD( 23.439292 - DeltaObliq/3600.0 );
	
	dms EcLong( ELong ), EcLat( ELat );
	double sinLong, cosLong, sinLat, cosLat, sinObliq, cosObliq;
	EcLong.SinCos( sinLong, cosLong );
	EcLat.SinCos( sinLat, cosLat );
	Obliquity.SinCos( sinObliq, cosObliq );
	
	double sinDec = sinLat*cosObliq + cosLat*sinObliq*sinLong;
	
	double y = sinLong*cosObliq - (sinLat/cosLat)*sinObliq;
	double RARad =  atan( y / cosLong );
	
	//resolve ambiguity of atan:
	if ( cosLong < 0 ) RARad += dms::PI;
	if ( cosLong > 0 && y < 0 ) RARad += 2.0*dms::PI;
	
	dms newRA, newDec;
	newRA.setRadians( RARad );
	newDec.setRadians( asin( sinDec ) );
	setRA( newRA );
	setDec( newDec );
}

void SkyPoint::updateCoords( long double NewEpoch, dms Obliquity, double dObliq, double dEcLong ) {
	//Correct the catalog coordinates for the time-dependent effects
	//of precession, nutation and aberration
	dms dRA1, dRA2, dDec1, dDec2;
	double cosOb, sinOb, cosRA, sinRA, cosDec, sinDec, tanDec;
	
	//precession
	precess( NewEpoch, J2000 );

	//nutation
	if ( fabs( Dec.Degrees() ) < 80.0 ) { //approximate method
		Obliquity.SinCos( sinOb, cosOb );
		RA.SinCos( sinRA, cosRA );
		Dec.SinCos( sinDec, cosDec );
		tanDec = sinDec/cosDec;
		
		dRA1.setD( dEcLong*( cosOb + sinOb*sinRA*tanDec ) - dObliq*cosRA*tanDec );
		dDec1.setD( dEcLong*( sinOb*cosRA ) + dObliq*sinRA );
	
		RA = RA.Degrees() + dRA1.Degrees();
		Dec = Dec.Degrees() + dDec1.Degrees();
	} else { //exact method, not yet implemented
	}
	
	//aberration: coming soon
}

void SkyPoint::precess( long double JD, long double JD0 ) {
	//Given coordinates (RA0, Dec0), which are appropriate for epoch JD0,
	//determine coordinates (RA, Dec) for epoch JD.
	//calculations taken from "Practical Astronomy with Your Calculator"
	//by Peter Duffett-Smith
	//
	//Due primarily to the gravity of the Moon, the direction of the
	//Earth's spin axis changes over time.  It traces a circle across
	//the sky, completing one circuit in about 26000 years.
	//See the Astronomy help pages for more information. 	
	dms X, Z, Y;
	double T, CX, SX, CY, SY, CZ, SZ;
	double cosRA, sinRA, cosDec, sinDec;
	double P[3][3];
	double v[3], s[3];

	RA0.SinCos( sinRA, cosRA );
	Dec0.SinCos( sinDec, cosDec );

	if ( JD0 != J2000 ) { //Need to first precess to J2000.0 coords
		T = (JD0 - J2000)/36525.0; // T is number of centuries from J2000.0

		X.setD( 0.6406161*T + 0.0000839*T*T + 0.0000050*T*T*T );
		Y.setD( 0.5567530*T - 0.0001185*T*T - 0.0000116*T*T*T );
		Z.setD( 0.6406161*T + 0.0003041*T*T + 0.0000051*T*T*T );

		X.SinCos( SX, CX );
		Y.SinCos( SY, CY );
		Z.SinCos( SZ, CZ );

		P[0][0] = CX*CY*CZ - SX*SZ;      //P is an array that handles the precession
		P[1][0] = CX*CY*SZ + SX*CZ;
		P[2][0] = CX*SY;
		P[0][1] = -1.0*SX*CY*CZ - CX*SZ;
		P[1][1] = -1.0*SX*CY*SZ + CX*CZ;
		P[2][1] = -1.0*SX*SY;
		P[0][2] = -1.0*SY*CZ;
		P[1][2] = -1.0*SY*SZ;
		P[2][2] = CY;

		v[0] = cosRA*cosDec; //v is a column vector representing input coordinates.
		v[1] = sinRA*cosDec;
		v[2] = sinDec;	

		//s is the product of P and v; s represents the coordinates precessed to J2000
		for ( unsigned int i=0; i<3; ++i ) {
			s[i] = P[0][i]*v[0] + P[1][i]*v[1] + P[2][i]*v[2];
		}

	} else { //Input coords already in J2000, set s accordingly.
		s[0] = cosRA*cosDec;
		s[1] = sinRA*cosDec;
		s[2] = sinDec;	
  }

	//Now, precess from J2000 to desired epoch (JD).
	T = (JD - J2000)/36525.0; // T is number of centuries from J2000.0

	X.setD( 0.6406161*T + 0.0000839*T*T + 0.0000050*T*T*T);
	Y.setD( 0.5567530*T - 0.0001185*T*T - 0.0000116*T*T*T);
	Z.setD( 0.6406161*T + 0.0003041*T*T + 0.0000051*T*T*T);

	X.SinCos( SX, CX );
	Y.SinCos( SY, CY );
	Z.SinCos( SZ, CZ );

  //P is now the *transpose* of the above matrix, because we are precessing from
  //(rather than to) J2000.
	P[0][0] = CX*CY*CZ - SX*SZ;
	P[1][0] = -1.0*SX*CY*CZ - CX*SZ;
	P[2][0] = -1.0*SY*CZ;
	P[0][1] = CX*CY*SZ + SX*CZ;
	P[1][1] = -1.0*SX*CY*SZ + CX*CZ;
	P[2][1] = -1.0*SY*SZ;
	P[0][2] = CX*SY;
	P[1][2] = -1.0*SX*SY;
	P[2][2] = CY;

	//Multiply P and s to get v, the vector representing the new coords.
	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = P[0][i]*s[0] + P[1][i]*s[1] + P[2][i]*s[2];
	}

	//Extract RA, Dec from the vector:
	RA.setRadians( atan( v[1]/v[0] ) );
	Dec.setRadians( asin( v[2] ) );
	
	//resolve ambiguity of atan()
	if ( v[0] < 0.0 ) {
		RA.setD( RA.Degrees() + 180.0 );
	} else if( v[1] < 0.0 ) {
		RA.setD( RA.Degrees() + 360.0 );
	}
}

