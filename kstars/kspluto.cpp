/***************************************************************************
                          kspluto.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Sep 24 2001
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

#include <qfile.h>
#include <qtextstream.h>
#include <math.h>
#include "kstarsdata.h"
#include "kspluto.h"

KSPluto::KSPluto() : KSPlanet( I18N_NOOP( "Pluto" ) ) {
}

bool KSPluto::findPosition( long double jd, KSPlanet *Earth ) {
	QString fname, snum, line;
	QFile f;
	double freq[106], T, U;
	double ac, as, X, Y, Z, X0, Y0, Z0, RARad;
	dms L0, B0; //geocentric ecliptic coords of Sun
	dms EarthLong, EarthLat; //heliocentric ecliptic coords of Earth
	double sinL0, cosL0, sinB0, cosB0;

	T = 2.0*(jd -  2341972.5)/146120.0 - 1.0;
	U =  T*73060.0;

	//compute secular terms
  X = 98083308510. - 1465718392.*T + 11528487809.*T*T + 55397965917.*T*T*T;
	Y = 101846243715. + 57789.*T - 5487929294.*T*T + 8520205290.*T*T*T;
	Z = 2183700004. + 433209785.*T - 4911803413.*T*T - 14029741184.*T*T*T;

  //read in the periodic frequencies
	int n = 0;
	fname = "pluto.freq";
	if ( KStarsData::openDataFile( f, fname ) ) {
		QTextStream stream( &f );
  	while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
  		instream >> freq[n++];
	  }
  	f.close();
	}

	if (n==0) return false;

// compute periodic X terms
	n = 0;
	fname = "pluto.x";
	if ( KStarsData::openDataFile( f, fname ) ) {
		QTextStream stream( &f );
  	while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
  		instream >> ac >> as;
			if ( n > 100 ) {
				X += T*T* ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
			} else if ( n > 81 ) {
				X += T* ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
			} else {
				X += ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
      }
			++n;
	  }
  	f.close();
	}

	if (n==0) return false;

// compute periodic Y terms
	n = 0;
	fname = "pluto.y";
	if ( KStarsData::openDataFile( f, fname ) ) {
		QTextStream stream( &f );
  	while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
  		instream >> ac >> as;
			if ( n > 100 ) {
				Y += T*T* ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
			} else if ( n > 81 ) {
				Y += T* ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
			} else {
				Y += ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
      }
			++n;
	  }
  	f.close();
	}

	if (n==0) return false;

// compute periodic Z terms
	n = 0;
	fname = "pluto.z";
	if ( KStarsData::openDataFile( f, fname ) ) {
		QTextStream stream( &f );
  	while ( !stream.eof() ) {
			line = stream.readLine();
			QTextIStream instream( &line );
  		instream >> ac >> as;
			if ( n > 100 ) {
				Z += T*T* ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
			} else if ( n > 81 ) {
				Z += T* ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
			} else {
				Z += ( ac*cos( freq[n]*U ) + as*sin( freq[n]*U ) );
      }
			++n;
	  }
  	f.close();
	}

	if (n==0) return false;

	//convert X, Y, Z to AU (given in 1.0E10 AU)
	X /= 10000000000.0; Y /= 10000000000.0; Z /= 10000000000.0;

	//L0, B0 are Sun's Ecliptic coords (L0 = Learth + 180; B0 = -Bearth)
	L0.setD( Earth->ecLong().Degrees() + 180.0 );
	L0.setD( L0.reduce().Degrees() );
	B0.setD( -1.0*Earth->ecLat().Degrees() );

	L0.SinCos( sinL0, cosL0 );
	B0.SinCos( sinB0, cosB0 );

//find Mean Obliquity
	T = (jd - J2000)/36525.0; //julian centuries since J2000.0
	dms Obliquity;
	double cosOb, sinOb;
	double DeltaObliq = 46.815*T + 0.00059*T*T - 0.001813*T*T*T; //change in Obliquity, in arcseconds
	Obliquity.setD( 23.439292 - DeltaObliq/3600.0 );
	Obliquity.SinCos( sinOb, cosOb );

	X0 = Earth->rsun()*cosB0*cosL0;
	Y0 = Earth->rsun()*( cosB0*sinL0*cosOb - sinB0*sinOb );
	Z0 = Earth->rsun()*( cosB0*sinL0*sinOb + sinB0*cosOb );

	//transform to geocentric rectangular coordinates by adding Sun's values
	X = X + X0; Y = Y + Y0; Z = Z + Z0;

  //Use Meeus's Eq. 32.10 to find Rsun, RA and Dec:
	setRsun( sqrt( X*X + Y*Y + Z*Z ) );
	RARad = atan( Y / X );
	if ( X<0 ) RARad += dms::PI;
	if ( X>0 && Y<0 ) RARad += 2.0*dms::PI;
	dms newRA; newRA.setRadians( RARad );
	dms newDec; newDec.setRadians( asin( Z/rsun() ) );
	pos()->setRA( newRA );
	pos()->setDec( newDec ); 	

	//compute Ecliptic coordinates
	EquatorialToEcliptic( jd );

	return true;
}
