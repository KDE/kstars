/***************************************************************************
                          ksplanet.cpp  -  K Desktop Planetarium
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

#include <qfile.h>
#include <qtextstream.h>
#include <math.h>
#include "kstarsdata.h"
#include "ksplanet.h"

KSPlanet::KSPlanet(){
	KSPlanet( I18N_NOOP( "Unnamed" ) );
}

KSPlanet::KSPlanet( QString s, QImage im )
 : SkyObject( 2, 0.0, 0.0, 0.0, s, "" ) {
	EcLong.setD( 0.0 );
	EcLat.setD( 0.0 );
	Image = im;

/*************
	//yuck.  I need the untranslated name to generate data filenames
	if ( s == i18n( "Mercury" ) ) { EnglishName = "Mercury"; }
	else if ( s == i18n( "Venus" ) ) { EnglishName = "Venus"; }
	else if ( s == i18n( "Earth" ) ) { EnglishName = "Earth"; }
	else if ( s == i18n( "Mars" ) ) { EnglishName = "Mars"; }
	else if ( s == i18n( "Jupiter" ) ) { EnglishName = "Jupiter"; }
	else if ( s == i18n( "Saturn" ) ) { EnglishName = "Saturn"; }
	else if ( s == i18n( "Uranus" ) ) { EnglishName = "Uranus"; }
	else if ( s == i18n( "Neptune" ) ) { EnglishName = "Neptune"; }
	else if ( s == i18n( "Pluto" ) ) { EnglishName = "Pluto"; }
**************/
}

bool KSPlanet::findPosition( long double jd, KSPlanet *Earth ) {
	QString fname, snum, line;
	QFile f;
	double sum[6], A, B, C, T;
	double x, y, z;
	double sinL, sinL0, sinB, sinB0;
	double cosL, cosL0, cosB, cosB0;
	int nCount = 0;

	T = (jd - J2000)/365250.0; //Julian millenia since J2000

	//First, find heliocentric coordinates
	//Ecliptic Longitude
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		snum.setNum( i );
//		fname = EnglishName.lower() + ".L" + snum + ".vsop";
		fname = name().lower() + ".L" + snum + ".vsop";

		if ( KStarsData::openDataFile( f, fname ) ) {
			++nCount;
		  QTextStream stream( &f );
  		while ( !stream.eof() ) {
				line = stream.readLine();
				QTextIStream instream( &line );
  			instream >> A >> B >> C;
				sum[i] += A * cos( B + C*T );
	  	}
  		f.close();
  	}
  }

	if ( nCount==0 ) return false; //No longitude data found!

  EcLong.setRadians( sum[0] + sum[1]*T + sum[2]*T*T + sum[3]*T*T*T + sum[4]*T*T*T*T + sum[5]*T*T*T*T*T );
	EcLong.setD( EcLong.reduce().Degrees() );
	  	
	nCount = 0;
	//Compute Ecliptic Latitude
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		snum.setNum( i );
//		fname = EnglishName.lower() + ".B" + snum + ".vsop";
		fname = name().lower() + ".B" + snum + ".vsop";
		if ( KStarsData::openDataFile( f, fname ) ) {
			++nCount;
		  QTextStream stream( &f );
  		while ( !stream.eof() ) {
				line = stream.readLine();
				QTextIStream instream( &line );
  			instream >> A >> B >> C;
				sum[i] += A * cos( B + C*T );
	  	}
  		f.close();
  	}
  }

	if (nCount==0) return false; //no latitude data found!

  EcLat.setRadians( sum[0] + sum[1]*T + sum[2]*T*T + sum[3]*T*T*T + sum[4]*T*T*T*T + sum[5]*T*T*T*T*T );
  	
	//Compute Heliocentric Distance
	nCount = 0;
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		snum.setNum( i );
//		fname = EnglishName.lower() + ".R" + snum + ".vsop";
		fname = name().lower() + ".R" + snum + ".vsop";
		if ( KStarsData::openDataFile( f, fname ) ) {
			++nCount;
		  QTextStream stream( &f );
  		while ( !stream.eof() ) {
				line = stream.readLine();
				QTextIStream instream( &line );
  			instream >> A >> B >> C;
				sum[i] += A * cos( B + C*T );
	  	}
  		f.close();
  	}
  }

	if (nCount==0) return false; //no distance data found!

  Rsun = sum[0] + sum[1]*T + sum[2]*T*T + sum[3]*T*T*T + sum[4]*T*T*T*T + sum[5]*T*T*T*T*T;

	//find geometric geocentric coordinates
	if ( Earth != NULL ) {
		EcLong.SinCos( sinL, cosL );
		EcLat.SinCos( sinB, cosB );
		Earth->EcLong.SinCos( sinL0, cosL0 );
		Earth->EcLat.SinCos( sinB0, cosB0 );
		x = Rsun*cosB*cosL - Earth->Rsun*cosB0*cosL0;
		y = Rsun*cosB*sinL - Earth->Rsun*cosB0*sinL0;
		z = Rsun*sinB - Earth->Rsun*sinB0;
	
		EcLong.setRadians( atan( y/x ) );
		if (x<0) EcLong.setD( EcLong.Degrees() + 180.0 ); //resolve atan ambiguity
		EcLat.setRadians( atan( z/( sqrt( x*x + y*y ) ) ) );
	  EclipticToEquatorial( jd );
	}

	return true;
}

void KSPlanet::EquatorialToEcliptic( long double jd ) {
	//compute obliquity of ecliptic for date jd
	double T = (jd - 2451545.0)/36525.0; //number of centuries since Jan 1, 2000
	double DeltaObliq = 46.815*T + 0.0006*T*T - 0.00181*T*T*T; //change in Obliquity, in arcseconds
	dms Obliquity;
	Obliquity.setD( 23.439292 - DeltaObliq/3600.0 );

	double sinRA, cosRA, sinOb, cosOb, sinDec, cosDec, tanDec;
  pos()->ra().SinCos( sinRA, cosRA );
	pos()->dec().SinCos( sinDec, cosDec );
	Obliquity.SinCos( sinOb, cosOb );

	tanDec = sinDec/cosDec;
	double y = sinRA*cosOb + tanDec*sinOb;
	double ELongRad = atan( y/cosRA );
	//resolve atan ambiguity
	if ( cosRA < 0 ) ELongRad += PI();
	if ( cosRA > 0 && y < 0 ) ELongRad += 2.0*PI();

	EcLong.setRadians( ELongRad );
	EcLat.setRadians( asin( sinDec*cosOb - cosDec*sinOb*sinRA ) );
}
	
void KSPlanet::EclipticToEquatorial( long double jd ) {
	//compute obliquity of ecliptic for date jd
	double T = (jd - 2451545.0)/36525.0; //number of centuries since Jan 1, 2000
	double DeltaObliq = 46.815*T + 0.00059*T*T - 0.001813*T*T*T; //change in Obliquity, in arcseconds
	dms Obliquity;
	double dEclong, dObliq;
	nutate( jd, dEclong, dObliq );
	Obliquity.setD( 23.439292 - DeltaObliq/3600.0 + dObliq/3600.0 );
	EcLong.setD( EcLong.Degrees() + dEclong );

	double sinLong, cosLong, sinLat, cosLat, sinObliq, cosObliq;
	EcLong.SinCos( sinLong, cosLong );
	EcLat.SinCos( sinLat, cosLat );
	Obliquity.SinCos( sinObliq, cosObliq );
	
	double sinDec = sinLat*cosObliq + cosLat*sinObliq*sinLong;
	
	double y = sinLong*cosObliq - (sinLat/cosLat)*sinObliq;
	double RARad =  atan( y / cosLong );
	
	//resolve ambiguity of atan:
	if ( cosLong < 0 ) RARad += PI();
	if ( cosLong > 0 && y < 0 ) RARad += 2.0*PI();
	
	dms newRA, newDec;
	newRA.setRadians( RARad );
	newDec.setRadians( asin( sinDec ) );
	pos()->setRA( newRA );
	pos()->setDec( newDec );
}

void KSPlanet::nutate( long double JD, double &dEcLong, double &dObliq ) {
	//This uses the pre-1984 theory of nutation.  The results
	//are accurate to 0.5 arcsec, should be fine for KStars.
	//calculations from "Practical Astronomy with Your Calculator"
	//by Peter Duffett-Smith
	//
	//Nutation is a small, quasi-periodic oscillation of the Earth's
	//spin axis (in addition to the more regular precession).  It
	//is manifested as a change in the zeropoint of the Ecliptic
	//Longitude (dEcLong) and a change in the Obliquity angle (dObliq)
	//see the Astronomy Help pages for more info.
	
	dms  L2, M2, O, O2;
	double T;
	double sin2L, cos2L, sin2M, cos2M;
	double sinO, cosO, sin2O, cos2O;
	
	T = ( JD - J2000 )/36525.0; //centuries since J2000.0
  O.setD( 125.04452 - 1934.136261*T + 0.0020708*T*T + T*T*T/450000.0 ); //ecl. long. of Moon's ascending node
	O2.setD( 2.0*O.Degrees() );
	L2.setD( 2.0*( 280.4665 + 36000.7698*T ) ); //twice mean ecl. long. of Sun
	M2.setD( 2.0*( 218.3165 + 481267.8813*T ) );//twice mean ecl. long. of Moon
		
	O.SinCos( sinO, cosO );
	O2.SinCos( sin2O, cos2O );
	L2.SinCos( sin2L, cos2L );
	M2.SinCos( sin2M, cos2M );

	dEcLong = ( -17.2*sinO - 1.32*sin2L - 0.23*sin2M + 0.21*sin2O)/3600.0; //Ecl. long. correction
	dObliq = (   9.2*cosO + 0.57*cos2L + 0.10*cos2M - 0.09*cos2O)/3600.0; //Obliq. correction
}
