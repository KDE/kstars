/***************************************************************************
                          kssun.cpp  -  K Desktop Planetarium
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

#include <klocale.h>
#include <kstars.h>
#include <qfile.h>
#include <qtextstream.h>
#include <math.h>
#include "kssun.h"

KSSun::KSSun() {
	KSSun( J2000 );
}

KSSun::KSSun( long double Epoch ) : KSPlanet( i18n( "Sun" ) ) {
	JD0 = 2447892.5; //Jan 1, 1990
	eclong0 = 279.403303; //mean ecliptic longitude at JD0
	plong0 = 282.768422; //longitude of sun at perigee for JD0
	e0 = 0.016713; //eccentricity of Earth's orbit at JD0
	findPosition( Epoch );
}

KSSun::~KSSun(){
}

bool KSSun::findPosition( long double jd ) {
	QString fname, snum, line;
	QFile f;
	long double sum[6], T, RE;
	double A, B, C;
	int nCount = 0;

	dms EarthLong, EarthLat; //heliocentric coords of Earth
	
	T = (jd - J2000)/365250.0; //Julian millenia since J2000

  //Find Earth's heliocentric coords
	//Ecliptic Longitude
	nCount = 0;
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		snum.setNum( i );
		fname = "earth.L" + snum + ".vsop";
		if ( KStarsData::openDataFile( f, fname, false ) ) {
			++nCount;
		  QTextStream stream( &f );
  		while ( !stream.eof() ) {
				line = stream.readLine();
				QTextIStream instream( &line );
  			instream >> A >> B >> C;
				line = line.sprintf( "%f", C );
				sum[i] += A * cos( B + C*T );
	  	}
  		f.close();
  	}
  }

	if (nCount==0) return false; //no longitude data found!

  EarthLong.setRadians( sum[0] + sum[1]*T + sum[2]*T*T + sum[3]*T*T*T + sum[4]*T*T*T*T + sum[5]*T*T*T*T*T );
	EarthLong.setD( EarthLong.reduce().getD() );
	  	
	//Ecliptic Latitude
	nCount = 0;
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		snum.setNum( i );
		fname = "earth.B" + snum + ".vsop";
		if ( KStarsData::openDataFile( f, fname, false ) ) {
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

  EarthLat.setRadians( sum[0] + sum[1]*T + sum[2]*T*T + sum[3]*T*T*T + sum[4]*T*T*T*T + sum[5]*T*T*T*T*T );

	EcLong.setD( EarthLong.getD() + 180.0 );
	EcLong.setD( EcLong.reduce().getD() );
	EcLat.setD( -1.0*EarthLat.getD() );

	//Compute Heliocentric Distance
	nCount = 0;
	for (int i=0; i<6; ++i) {
		sum[i] = 0.0;
		snum.setNum( i );
		fname = "earth.R" + snum + ".vsop";
		if ( KStarsData::openDataFile( f, fname, false ) ) {
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

	//Well, for the Sun, Rsun is the geocentric distance, not the heliocentric! :)
  Rsun = sum[0] + sum[1]*T + sum[2]*T*T + sum[3]*T*T*T + sum[4]*T*T*T*T + sum[5]*T*T*T*T*T;

	//Finally, convert Ecliptic coords to Ra, Dec.  Ecliptic latitude is zero, by definition
	EclipticToEquatorial( jd );

	return true;	
}
