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
#include <klocale.h>

#include "dms.h"
#include "skypoint.h"

void SkyPoint::set( dms r, dms d ) { 
	RA0.set( r ); 
	Dec0.set( d ); 
	RA.set( r ); 
	Dec.set( d ); 
}

void SkyPoint::set( double r, double d ) { 
	RA0.setH( r ); 
	Dec0.setD( d ); 
	RA.setH( r ); 
	Dec.setD( d ); 
}

SkyPoint::~SkyPoint(){
}

void SkyPoint::EquatorialToHorizontal( const dms *LSTh, const dms *lat ) {
	double AltRad, AzRad;
	double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
  double sinAlt, cosAlt;

	dms HourAngle = LSTh->Degrees() - ra()->Degrees();

	lat->SinCos( sinlat, coslat );
	dec()->SinCos( sindec, cosdec );
	HourAngle.SinCos( sinHA, cosHA );

	sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
	AltRad = asin( sinAlt );
	cosAlt = cos( AltRad );

	AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
	if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity

	Az.setRadians( AzRad );
	Alt.setRadians( AltRad );
}

void SkyPoint::HorizontalToEquatorial( const dms *LSTh, const dms *lat ) {
	double HARad, DecRad;
	double sinlat, coslat, sinAlt, cosAlt, sinAz, cosAz;
  double sinDec, cosDec;

	lat->SinCos( sinlat, coslat );
	alt()->SinCos( sinAlt, cosAlt );
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

	RA.setRadians( LSTh->radians() - HARad );
	RA.setD( RA.reduce().Degrees() );  // 0 <= RA < 24
}

void SkyPoint::findEcliptic( const dms *Obliquity, dms &EcLong, dms &EcLat ) {
	double sinRA, cosRA, sinOb, cosOb, sinDec, cosDec, tanDec;
	ra()->SinCos( sinRA, cosRA );
	dec()->SinCos( sinDec, cosDec );
	Obliquity->SinCos( sinOb, cosOb );

	tanDec = sinDec/cosDec;
	double y = sinRA*cosOb + tanDec*sinOb;
	double ELongRad = atan( y/cosRA );
	//resolve atan ambiguity
	if ( cosRA < 0 ) ELongRad += dms::PI;
	if ( cosRA > 0 && y < 0 ) ELongRad += 2.0*dms::PI;

	EcLong.setRadians( ELongRad );
	EcLat.setRadians( asin( sinDec*cosOb - cosDec*sinOb*sinRA ) );
}

void SkyPoint::setFromEcliptic( const dms *Obliquity, const dms *EcLong, const dms *EcLat ) {
	double sinLong, cosLong, sinLat, cosLat, sinObliq, cosObliq;
	EcLong->SinCos( sinLong, cosLong );
	EcLat->SinCos( sinLat, cosLat );
	Obliquity->SinCos( sinObliq, cosObliq );
	
	double sinDec = sinLat*cosObliq + cosLat*sinObliq*sinLong;
	
	double y = sinLong*cosObliq - (sinLat/cosLat)*sinObliq;
	double RARad =  atan( y / cosLong );
	
	//resolve ambiguity of atan:
	if ( cosLong < 0 ) RARad += dms::PI;
	if ( cosLong > 0 && y < 0 ) RARad += 2.0*dms::PI;
	
	//DMS_SPEED
	//dms newRA, newDec;
	//newRA.setRadians( RARad );
	//newDec.setRadians( asin( sinDec ) );
	RA.setRadians( RARad );
	Dec.setRadians( asin(sinDec) );
}

void SkyPoint::precess( const KSNumbers *num) {
	double cosRA0, sinRA0, cosDec0, sinDec0;
	double v[3], s[3];

	RA0.SinCos( sinRA0, cosRA0 );
	Dec0.SinCos( sinDec0, cosDec0 );

	s[0] = cosRA0*cosDec0;
	s[1] = sinRA0*cosDec0;
	s[2] = sinDec0;	
	//Multiply P2 and s to get v, the vector representing the new coords.
	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = 0.0;
		for (uint j=0; j< 3; ++j) {
			v[i] += num->p2( j, i )*s[j];
		}
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

void SkyPoint::nutate(const KSNumbers *num) {
	double cosRA, sinRA, cosDec, sinDec, tanDec;
	dms dRA1, dRA2, dDec1, dDec2;
	double cosOb, sinOb;

	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	num->obliquity()->SinCos( sinOb, cosOb );

	//Step 2: Nutation
	if ( fabs( Dec.Degrees() ) < 80.0 ) { //approximate method
		tanDec = sinDec/cosDec;
		
		dRA1.setD( num->dEcLong()*( cosOb + sinOb*sinRA*tanDec ) - num->dObliq()*cosRA*tanDec );
		dDec1.setD( num->dEcLong()*( sinOb*cosRA ) + num->dObliq()*sinRA );
		
		RA.setD( RA.Degrees() + dRA1.Degrees() );
		Dec.setD( Dec.Degrees() + dDec1.Degrees() );
	} else { //exact method
		dms EcLong, EcLat;
		findEcliptic( num->obliquity(), EcLong, EcLat );
		
		//Add dEcLong to the Ecliptic Longitude
		dms newLong( EcLong.Degrees() + num->dEcLong() );
		setFromEcliptic( num->obliquity(), &newLong, &EcLat );
	}
}

void SkyPoint::aberrate(const KSNumbers *num) {
	double cosRA, sinRA, cosDec, sinDec;
	dms dRA2, dDec2;
	double cosOb, sinOb, cosL, sinL, cosP, sinP;

	double K = num->constAberr().Degrees();  //constant of aberration
	double e = num->earthEccentricity();

	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	num->obliquity()->SinCos( sinOb, cosOb );
	double tanOb = sinOb/cosOb;

	num->sunTrueLongitude().SinCos( sinL, cosL );
	num->earthPerihelionLongitude().SinCos( sinP, cosP );


	//Step 3: Aberration
	dRA2.setD( -1.0 * K * ( cosRA * cosL * cosOb + sinRA * sinL )/cosDec
		+ e * K * ( cosRA * cosP * cosOb + sinRA * sinP )/cosDec );

	dDec2.setD( -1.0 * K * ( cosL * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinL )
		+ e * K * ( cosP * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinP ) );

	RA.setD( RA.Degrees() + dRA2.Degrees() );
	Dec.setD( Dec.Degrees() + dDec2.Degrees() );
}

void SkyPoint::updateCoords( KSNumbers *num, bool includePlanets ) {
	dms pRA, pDec;

	//Correct the catalog coordinates for the time-dependent effects
	//of precession, nutation and aberration
	
	precess(num);

	nutate(num);

	aberrate(num);

	
}

void SkyPoint::precessFromAnyEpoch(long double jd0, long double jdf){

 	double cosRA0, sinRA0, cosDec0, sinDec0;
	double v[3], s[3];

	RA0.SinCos( sinRA0, cosRA0 );
	Dec0.SinCos( sinDec0, cosDec0 );

	//Need first to precess to J2000.0 coords

	if ( jd0 != J2000 ) {

	//v is a column vector representing input coordinates.

		KSNumbers *num = new KSNumbers (jd0);

		v[0] = cosRA0*cosDec0;
		v[1] = sinRA0*cosDec0;
		v[2] = sinDec0;

	//s is the product of P1 and v; s represents the
	//coordinates precessed to J2000
		for ( unsigned int i=0; i<3; ++i ) {
			s[i] = num->p1( 0, i )*v[0] + num->p1( 1, i )*v[1] +
				num->p1( 2, i )*v[2];
		}
		delete num;

	} else {

	//Input coords already in J2000, set s accordingly.



		s[0] = cosRA0*cosDec0;
		s[1] = sinRA0*cosDec0;
		s[2] = sinDec0;
       }

	KSNumbers *num = new KSNumbers (jdf);

	//Multiply P2 and s to get v, the vector representing the new coords.

	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = num->p2( 0, i )*s[0] + num->p2( 1, i )*s[1] +
		num->p2( 2, i )*s[2];
	}

	delete num;

	//Extract RA, Dec from the vector:

	//RA.setRadians( atan( v[1]/v[0] ) );
	RA.setRadians( atan2( v[1],v[0] ) );
	Dec.setRadians( asin( v[2] ) );

	//resolve ambiguity of atan()

//	if ( v[0] < 0.0 ) {
//		RA.setD( RA.Degrees() + 180.0 );
//	} else if( v[1] < 0.0 ) {
//		RA.setD( RA.Degrees() + 360.0 );
//	}

	if (RA.Degrees() < 0.0 )
		RA.setD( RA.Degrees() + 360.0 );

}
/** No descriptions */
void SkyPoint::apparentCoord(long double jd0, long double jdf){

	precessFromAnyEpoch(jd0,jdf);

	KSNumbers *num = new KSNumbers (jdf);

	nutate(num);
	aberrate(num);

	delete num;
}
