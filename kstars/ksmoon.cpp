/***************************************************************************
                          ksmoon.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 26 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdlib.h>
#include <math.h>
#include <klocale.h>
#include <qtextstream.h>
#include "kstarsdata.h"
#include "kssun.h"
#include "ksmoon.h"

KSMoon::KSMoon( long double Epoch )
 : KSPlanet( I18N_NOOP( "Moon" ) ) {
	findPosition( Epoch );
}

void KSMoon::findPosition( long double Epoch ) {
	//Algorithms in this subroutine are taken from Chapter 45 of "Astronomical Algorithms"
  //by Jean Meeus (1991, Willmann-Bell, Inc. ISBN 0-943396-35-2.  http://www.willbell.com/math/mc1.htm)
	QString fname, snum, line;
	QFile f;
	int nd, nm, nm1, nf;
	double DegtoRad;
	double T, L, D, M, M1, F, E, A1, A2, A3;
	double Li, Ri, Bi; //coefficients of the sums
  double sumL, sumR, sumB;

	DegtoRad = acos( -1.0 )/180.0;

	//Julian centuries since J2000
	T = (Epoch - J2000)/36525.0;

	//Moon's mean longitude
	L = 218.3164591 + 481267.88134236*T - 0.0013268*T*T + T*T*T/538841.0 - T*T*T*T/65194000.0;
	while ( L > 360.0 ) L -= 360.0;
	while ( L < 0.0 ) L += 360.0;
	L *= DegtoRad;
	//Moon's mean elongation
	D = 297.8502042 + 445267.1115168*T - 0.0016300*T*T + T*T*T/545868.0 - T*T*T*T/113065000.0;
	while ( D > 360.0 ) D -= 360.0;
	while ( D < 0.0 ) D += 360.0;
	D *= DegtoRad;
	//Sun's mean anomaly
  M = 357.5291092 + 35999.0502909*T - 0.0001536*T*T + T*T*T/24490000.0;
	while ( M > 360.0 ) M -= 360.0;
	while ( M < 0.0 ) M += 360.0;
	M *= DegtoRad;
	//Moon's mean anomaly
	M1= 134.9634114 + 477198.8676313*T + 0.0089970*T*T + T*T*T/69699.0 - T*T*T*T/14712000.0;
	while ( M1 > 360.0 ) M1 -= 360.0;
	while ( M1 < 0.0 ) M1 += 360.0;
	M1 *= DegtoRad;
  //Moon's argument of latitude (angle from ascending node)
	F = 93.2720993 + 483202.0175273*T - 0.0034029*T*T - T*T*T/3526000.0 + T*T*T*T/863310000.0;
	while ( F > 360.0 ) F -= 360.0;
	while ( F < 0.0 ) F += 360.0;
	F *= DegtoRad;

	A1 = 119.75 +    131.849*T;
	A2 =  53.09 + 479264.290*T;
	A3 = 313.45 + 481226.484*T;
	while ( A1 > 360.0 ) A1 -= 360.0;
	while ( A1 < 0.0 ) A1 += 360.0;
	while ( A2 > 360.0 ) A2 -= 360.0;
	while ( A2 < 0.0 ) A2 += 360.0;
	while ( A3 > 360.0 ) A3 -= 360.0;
	while ( A3 < 0.0 ) A3 += 360.0;
	A1 *= DegtoRad;
	A2 *= DegtoRad;
	A3 *= DegtoRad;

	//Calculate the series expansions stored in moonLR.txt and moonB.txt.
	//
	sumL = 0.0;
	sumR = 0.0;

	if ( KStarsData::openDataFile( f, "moonLR.dat" ) ) {
		QTextStream stream( &f );
  	while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
  		instream >> nd >> nm >> nm1 >> nf >> Li >> Ri;

			E = 1.0;
			if ( nm ) { //if M != 0, include changing eccentricity of Earth's orbit
				E = 1.0 - 0.002516*T - 0.0000074*T*T;
				if ( abs( nm )==2 ) E = E*E; //use E^2
			}
			sumL += E*Li*sin( nd*D + nm*M + nm1*M1 + nf*F );
			sumR += E*Ri*cos( nd*D + nm*M + nm1*M1 + nf*F );
	  }
  	f.close();
  }

	sumB = 0.0;
	if ( KStarsData::openDataFile( f, "moonB.dat" ) ) {
		QTextStream stream( &f );
  	while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
  		instream >> nd >> nm >> nm1 >> nf >> Bi;

			E = 1.0;
			if ( nm ) { //if M != 0, include changing eccentricity of Earth's orbit
				E = 1.0 - 0.002516*T - 0.0000074*T*T;
				if ( abs( nm )==2 ) E = E*E; //use E^2
			}
			sumB += E*Bi*sin( nd*D + nm*M + nm1*M1 + nf*F );
	  }
  	f.close();
  }

	//Additive terms for sumL and sumB
	sumL += ( 3958.0*sin( A1 ) + 1962.0*sin( L-F ) + 318.0*sin( A2 ) );
	sumB += ( -2235.0*sin( L ) + 382.0*sin( A3 ) + 175.0*sin( A1-F ) + 175.0*sin( A1+F ) + 127.0*sin( L-M1 ) - 115.0*sin( L+M1 ) );

	//Geocentric coordinates
	setEcLong( ( L + DegtoRad*sumL/1000000.0 ) * 180./dms::PI );  //convert radians to degrees
	setEcLat( ( DegtoRad*sumB/1000000.0 ) * 180./dms::PI );
	Rearth = 385000.56 + sumR/1000.0;

  EclipticToEquatorial( Epoch );
}

void KSMoon::findPosition( long double Epoch, dms lat, dms LST ) {
	findPosition( Epoch );
	localizeCoords( lat, LST );
}

void KSMoon::localizeCoords( dms lat, dms LST ) {
	//convert geocentric coordinates to local apparent coordinates (topocentric coordinates)
	dms HA, HA2; //Hour Angle, before and after correction
	double rsinp, rcosp, u, sinHA, cosHA, sinDec, cosDec, D;
 double cosHA2;
	u = atan( 0.996647*tan( lat.radians() ) );
	rsinp = 0.996647*sin( u );
	rcosp = cos( u );
	HA.setD( LST.Degrees() - pos()->ra().Degrees() );
	HA.SinCos( sinHA, cosHA );
	pos()->dec().SinCos( sinDec, cosDec );
	D = atan( ( rcosp*sinHA )/( Rearth*cosDec/6378.14 - rcosp*cosHA ) );
	pos()->ra().setRadians( pos()->ra().radians() - D );

	HA2.setD( LST.Degrees() - pos()->ra().Degrees() );
	cosHA2 = cos( HA2.radians() );
	pos()->dec().setRadians( atan( cosHA2*( Rearth*sinDec/6378.14 - rsinp )/( Rearth*cosDec*cosHA/6378.14 - rcosp ) ) );
}

void KSMoon::findPhase( KSSun *Sun ) {
	Phase.setD( ecLong().Degrees() - Sun->ecLong().Degrees() );
	Phase.setD( Phase.reduce().Degrees() );
//	int iPhase = 24 - int( Phase.Hours()+0.5 );
	int iPhase = int( Phase.Hours()+0.5 );
	if (iPhase==24) iPhase = 0;
	QString sPhase;
	sPhase = sPhase.sprintf( "%d", iPhase );
	QString imName = "moon" + sPhase + ".png";

	QFile imFile;
	if ( KStarsData::openDataFile( imFile, imName ) ) {
		imFile.close();
		image()->load( imFile.name() );
	}
}
