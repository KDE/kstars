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

#include "skypoint.h"
#include "dms.h"
#include "ksnumbers.h"

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

void SkyPoint::EquatorialToHorizontal( const dms *LST, const dms *lat ) {
	double AltRad, AzRad;
	double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
  double sinAlt, cosAlt;

	dms HourAngle = LST->Degrees() - ra()->Degrees();

	lat->SinCos( sinlat, coslat );
	dec()->SinCos( sindec, cosdec );
	HourAngle.SinCos( sinHA, cosHA );

	sinAlt = sindec*sinlat + cosdec*coslat*cosHA;
	AltRad = asin( sinAlt );
	cosAlt = cos( AltRad );

	AzRad = acos( ( sindec - sinlat*sinAlt )/( coslat*cosAlt ) );
	if ( sinHA > 0.0 ) AzRad = 2.0*dms::PI - AzRad; // resolve acos() ambiguity

	//Do not reset Az if Alt > 89.5 or <-89.5
	Alt.setRadians( AltRad );
	if ( Alt.Degrees() < 89.5 && Alt.Degrees() > -89.5 )
		Az.setRadians( AzRad );
}

void SkyPoint::HorizontalToEquatorial( const dms *LST, const dms *lat ) {
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
		kdWarning() << i18n( "Coordinate out of range." );
		HARad = dms::PI;
	} else if ( x > 1.0 ) {
		kdWarning() << i18n( "Coordinate out of range." );
		HARad = 0.0;
	} else HARad = acos( x );

	if ( sinAz > 0.0 ) HARad = 2.0*dms::PI - HARad; // resolve acos() ambiguity

	RA.setRadians( LST->radians() - HARad );
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

void SkyPoint::updateCoords( KSNumbers *num, bool includePlanets, const dms *lat, const dms *LST ) {
	dms pRA, pDec;
	//Correct the catalog coordinates for the time-dependent effects
	//of precession, nutation and aberration

	precess(num);
	nutate(num);
	aberrate(num);

	if ( lat || LST )
		kdWarning() << i18n( "lat and LST parameters should only be used in KSPlanetBase objects." ) << endl;
}

void SkyPoint::precessFromAnyEpoch(long double jd0, long double jdf){

	double cosRA, sinRA, cosDec, sinDec;
        double v[3], s[3];

	RA.setD( RA0.Degrees() );
	Dec.setD( Dec0.Degrees() );

	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	if (jd0 == jdf)
		return;

	if ( jd0 == B1950) {
		B1950ToJ2000();
		jd0 = J2000;
		RA.SinCos( sinRA, cosRA );
		Dec.SinCos( sinDec, cosDec );

	}

	if (jd0 !=jdf) {
		// The original coordinate is referred to the FK5 system and
		// is NOT J2000.
		if ( jd0 != J2000 ) {

		//v is a column vector representing input coordinates.

			KSNumbers *num = new KSNumbers (jd0);

			v[0] = cosRA*cosDec;
			v[1] = sinRA*cosDec;
			v[2] = sinDec;

			//Need to first precess to J2000.0 coords
			//s is the product of P1 and v; s represents the
			//coordinates precessed to J2000
			for ( unsigned int i=0; i<3; ++i ) {
				s[i] = num->p1( 0, i )*v[0] +
					num->p1( 1, i )*v[1] +
					num->p1( 2, i )*v[2];
			}
			delete num;

		//Input coords already in J2000, set s accordingly.
		} else {

			s[0] = cosRA*cosDec;
			s[1] = sinRA*cosDec;
			s[2] = sinDec;
		}

		if ( jdf == B1950) {

			RA.setRadians( atan2( s[1],s[0] ) );
			Dec.setRadians( asin( s[2] ) );
			J2000ToB1950();

			return;
		}

		KSNumbers *num = new KSNumbers (jdf);

		for ( unsigned int i=0; i<3; ++i ) {
			v[i] = num->p2( 0, i )*s[0] +
			num->p2( 1, i )*s[1] +
			num->p2( 2, i )*s[2];
		}

		delete num;

		RA.setRadians( atan2( v[1],v[0] ) );
		Dec.setRadians( asin( v[2] ) );

		if (RA.Degrees() < 0.0 )
			RA.setD( RA.Degrees() + 360.0 );

		return;
	} 

}

/** No descriptions */
void SkyPoint::apparentCoord(long double jd0, long double jdf){

	precessFromAnyEpoch(jd0,jdf);

	KSNumbers *num = new KSNumbers (jdf);

	nutate(num);
	aberrate(num);

	delete num;
}

void SkyPoint::Equatorial1950ToGalactic(dms &galLong, dms &galLat) {
	
	double a = 192.25;
	dms b, c;
	double sinb, cosb, sina_RA, cosa_RA, sinDEC, cosDEC, tanDEC;

	c.setD(303);
	b.setD(27.4);
	tanDEC = tan( Dec.radians() );

	b.SinCos(sinb,cosb);
	dms( a - RA.Degrees() ).SinCos(sina_RA,cosa_RA);
	Dec.SinCos(sinDEC,cosDEC);

	galLong.setRadians( c.radians() - atan2( sina_RA, cosa_RA*sinb-tanDEC*cosb) );
	galLong = galLong.reduce();

	galLat.setRadians( asin(sinDEC*sinb+cosDEC*cosb*cosa_RA) );
}

void SkyPoint::GalacticToEquatorial1950(const dms* galLong, const dms* galLat) {

	double a = 123.0;
	dms b, c, galLong_a;
	double sinb, cosb, singLat, cosgLat, tangLat, singLong_a, cosgLong_a;

	c.setD(12.25);
	b.setD(27.4);
	tangLat = tan( galLat->radians() );


	galLat->SinCos(singLat,cosgLat);

	dms( galLong->Degrees()-a ).SinCos(singLong_a,cosgLong_a);
	b.SinCos(sinb,cosb);

	RA.setRadians(c.radians() + atan2(singLong_a,cosgLong_a*sinb-tangLat*cosb) );
	RA = RA.reduce();
//	raHourCoord = dms( raCoord.Hours() );

	Dec.setRadians( asin(singLat*sinb+cosgLat*cosb*cosgLong_a) );
}

void SkyPoint::B1950ToJ2000(void) {
	double cosRA, sinRA, cosDec, sinDec;
//	double cosRA0, sinRA0, cosDec0, sinDec0;
        double v[3], s[3];

	// 1984 January 1 0h
	KSNumbers *num = new KSNumbers (2445700.5);

	// Eterms due to aberration
	addEterms();
	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	// Precession from B1950 to J1984
	s[0] = cosRA*cosDec;
	s[1] = sinRA*cosDec;
	s[2] = sinDec;
	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = num->p2b( 0, i )*s[0] + num->p2b( 1, i )*s[1] +
		num->p2b( 2, i )*s[2];
	}

	// RA zero-point correction at 1984 day 1, 0h.
	RA.setRadians( atan2( v[1],v[0] ) );
	Dec.setRadians( asin( v[2] ) );

	RA.setH( RA.Hours() + 0.06390/3600. );
	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	s[0] = cosRA*cosDec;
	s[1] = sinRA*cosDec;
	s[2] = sinDec;

	// Precession from 1984 to J2000.

	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = num->p1( 0, i )*s[0] +
		num->p1( 1, i )*s[1] +
		num->p1( 2, i )*s[2];
	}

	RA.setRadians( atan2( v[1],v[0] ) );
	Dec.setRadians( asin( v[2] ) );

	delete num;
}

void SkyPoint::J2000ToB1950(void) {
	double cosRA, sinRA, cosDec, sinDec;
//	double cosRA0, sinRA0, cosDec0, sinDec0;
        double v[3], s[3];

	// 1984 January 1 0h
	KSNumbers *num = new KSNumbers (2445700.5);

	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	s[0] = cosRA*cosDec;
	s[1] = sinRA*cosDec;
	s[2] = sinDec;

	// Precession from J2000 to 1984 day, 0h.

	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = num->p2( 0, i )*s[0] +
		num->p2( 1, i )*s[1] +
		num->p2( 2, i )*s[2];
	}

	RA.setRadians( atan2( v[1],v[0] ) );
	Dec.setRadians( asin( v[2] ) );

	// RA zero-point correction at 1984 day 1, 0h.

	RA.setH( RA.Hours() - 0.06390/3600. );
	RA.SinCos( sinRA, cosRA );
	Dec.SinCos( sinDec, cosDec );

	// Precession from B1950 to J1984

	s[0] = cosRA*cosDec;
	s[1] = sinRA*cosDec;
	s[2] = sinDec;
	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = num->p1b( 0, i )*s[0] + num->p1b( 1, i )*s[1] +
		num->p1b( 2, i )*s[2];
	}

	RA.setRadians( atan2( v[1],v[0] ) );
	Dec.setRadians( asin( v[2] ) );

	// Eterms due to aberration
	subtractEterms();

	delete num;
}

SkyPoint SkyPoint::Eterms(void) {

	double sd, cd, sinEterm, cosEterm;
	dms raTemp, raDelta, decDelta;

	Dec.SinCos(sd,cd);
	raTemp.setH( RA.Hours() + 11.25);
	raTemp.SinCos(sinEterm,cosEterm);

	raDelta.setH( 0.0227*sinEterm/(3600.*cd) );
	decDelta.setD( 0.341*cosEterm*sd/3600. + 0.029*cd/3600. );

	SkyPoint spDelta = SkyPoint (raDelta, decDelta);

	return spDelta;
}

void SkyPoint::addEterms(void) {

	SkyPoint spd = Eterms();

	RA.setD( RA.Degrees() + spd.ra()->Degrees() );
	Dec.setD( Dec.Degrees() + spd.dec()->Degrees() );

}

void SkyPoint::subtractEterms(void) {

	SkyPoint spd = Eterms();

	RA.setD( RA.Degrees() - spd.ra()->Degrees() );
	Dec.setD( Dec.Degrees() - spd.dec()->Degrees() );
}

dms SkyPoint::angularDistanceTo(SkyPoint *sp) {

	double dalpha = ra()->radians() - sp->ra()->radians() ;
	double ddelta = dec()->radians() - sp->dec()->radians() ;

	double sa = sin(dalpha/2.);
	double sd = sin(ddelta/2.);
	
	double hava = sa*sa;
	double havd = sd*sd;

	double aux = havd + cos (dec()->radians()) * cos(sp->dec()->radians()) 
		* hava;
	
	dms angDist;
	angDist.setRadians( 2 * fabs(asin( sqrt(aux) )) );
	
	return angDist;
}
