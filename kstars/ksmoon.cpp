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
#include "ksutils.h"
#include "kssun.h"
#include "ksmoon.h"

KSMoon::KSMoon()
 : KSPlanetBase( I18N_NOOP( "Moon" ) ) {
}

bool KSMoon::data_loaded = false;
QList<KSMoon::MoonLRData> KSMoon::LRData;
QList<KSMoon::MoonBData> KSMoon::BData;

bool KSMoon::loadData() {
	if (data_loaded) return true;

	QString line;
	QFile f;
	int nd, nm, nm1, nf;
	double Li, Ri, Bi; //coefficients of the sums

	if ( KSUtils::openDataFile( f, "moonLR.dat" ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
			instream >> nd >> nm >> nm1 >> nf >> Li >> Ri;
			LRData.append(new MoonLRData(nd, nm, nm1, nf, Li, Ri));
		}
		f.close();
	} else
		return false;


	if ( KSUtils::openDataFile( f, "moonB.dat" ) ) {
		QTextStream stream( &f );
		while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
			instream >> nd >> nm >> nm1 >> nf >> Bi;
			BData.append(new MoonBData(nd, nm, nm1, nf, Bi));
		}
		f.close();
	}

	data_loaded = true;
	return true;
}

bool KSMoon::findPosition( const KSNumbers *num, const KSPlanetBase *Earth) {

	//Algorithms in this subroutine are taken from Chapter 45 of "Astronomical Algorithms"
  //by Jean Meeus (1991, Willmann-Bell, Inc. ISBN 0-943396-35-2.  http://www.willbell.com/math/mc1.htm)
	QString fname, snum, line;
	QFile f;
	double DegtoRad;
	double T, L, D, M, M1, F, E, A1, A2, A3;
	double sumL, sumR, sumB;

	DegtoRad = acos( -1.0 )/180.0;

	//Julian centuries since J2000
	T = num->julianCenturies();

	double Et = 1.0 - 0.002516*T - 0.0000074*T*T;

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

	if (!loadData()) return false;

	for (MoonLRData *mlrd = LRData.first(); mlrd != 0; mlrd = LRData.next()) {

		E = 1.0;
		if ( mlrd->nm ) { //if M != 0, include changing eccentricity of Earth's orbit
			E = Et;
			if ( abs( mlrd->nm )==2 ) E = E*E; //use E^2
		}
		sumL += E*mlrd->Li*sin( mlrd->nd*D + mlrd->nm*M + mlrd->nm1*M1 + mlrd->nf*F );
		sumR += E*mlrd->Ri*cos( mlrd->nd*D + mlrd->nm*M + mlrd->nm1*M1 + mlrd->nf*F );
	}

	sumB = 0.0;
	for (MoonBData *mbd = BData.first(); mbd != 0; mbd = BData.next()) {

		E = 1.0;
		if ( mbd->nm ) { //if M != 0, include changing eccentricity of Earth's orbit
			E = Et;
			if ( abs( mbd->nm )==2 ) E = E*E; //use E^2
		}
		sumB += E*mbd->Bi*sin( mbd->nd*D + mbd->nm*M + mbd->nm1*M1 + mbd->nf*F );
	}

	//Additive terms for sumL and sumB
	sumL += ( 3958.0*sin( A1 ) + 1962.0*sin( L-F ) + 318.0*sin( A2 ) );
	sumB += ( -2235.0*sin( L ) + 382.0*sin( A3 ) + 175.0*sin( A1-F ) + 175.0*sin( A1+F ) + 127.0*sin( L-M1 ) - 115.0*sin( L+M1 ) );

	//Geocentric coordinates
	setEcLong( ( L + DegtoRad*sumL/1000000.0 ) * 180./PI() );  //convert radians to degrees
	setEcLat( ( DegtoRad*sumB/1000000.0 ) * 180./PI() );
	Rearth = 385000.56 + sumR/1000.0;


	EclipticToEquatorial( num->obliquity() );

	return true;
}

void KSMoon::findPosition( KSNumbers *num, dms lat, dms LST ) {
	findPosition(num);
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
	HA.setD( LST.Degrees() - ra().Degrees() );
	HA.SinCos( sinHA, cosHA );
	dec().SinCos( sinDec, cosDec );
	D = atan( ( rcosp*sinHA )/( Rearth*cosDec/6378.14 - rcosp*cosHA ) );
	ra().setRadians( ra().radians() - D );

	HA2.setD( LST.Degrees() - ra().Degrees() );
	cosHA2 = cos( HA2.radians() );
	dec().setRadians( atan( cosHA2*( Rearth*sinDec/6378.14 - rsinp )/( Rearth*cosDec*cosHA/6378.14 - rcosp ) ) );
}

void KSMoon::findPhase( const KSSun *Sun ) {
	Phase.setD( ecLong().Degrees() - Sun->ecLong().Degrees() );
	Phase.setD( Phase.reduce().Degrees() );
//	int iPhase = 24 - int( Phase.Hours()+0.5 );
	int iPhase = int( Phase.Hours()+0.5 );
	if (iPhase==24) iPhase = 0;
	QString sPhase;
	sPhase = sPhase.sprintf( "%d", iPhase );
	QString imName = "moon" + sPhase + ".png";

	QFile imFile;
	if ( KSUtils::openDataFile( imFile, imName ) ) {
		imFile.close();
		image0()->load( imFile.name() );
		image()->load( imFile.name() );
		double p = pa();
		setPA( 0.0 );
		updatePA( p );
	}
}
