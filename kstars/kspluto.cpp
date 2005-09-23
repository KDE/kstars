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

#include <math.h>

#include <qfile.h>
#include <kdebug.h>

#include "kspluto.h"
#include "ksutils.h"
#include "ksnumbers.h"

#ifdef B0
// There are systems that #define B0 as a special termios flag (for baud rate)
#undef B0
#endif

KSPluto::KSPluto(KStarsData *kd, QString fn, double pSize ) 
		: KSAsteroid( kd, I18N_NOOP( "Pluto" ), fn, J2000, 
			39.48168677, 0.24880766, dms(17.14175), dms(113.76329),
			dms(110.30347), dms(14.86205), 1.0 ) {
	//Initialize the base orbital element values for J2000:
	a0 = 39.48168677; //semi-major axis (AU)
	e0 = 0.24880766;  //eccentricity
	i0.setD( 17.14175 );    //inclination (degrees)
	w0.setD( 113.76329 );   //argument of perihelion (degrees)
	N0.setD( 110.30347 );   //long. of ascending node (degrees)
	M0.setD(  14.86205 );   //mean anomaly (degrees)

	//rate-of-change values for the orbital elements
	a1 = -0.00076912;     // da/dt (AU/cnetury)
	e1 = 0.00006465;      // de/dt (1/century)
	i1 = 11.07/3600.;     // di/dt (degrees/century)
	w1 = -94.92/3600.;   // dw/dt (degrees/century)
	N1 = -37.33/3600.;    // dN/dt (degrees/century)
	M1 = 522880.15/3600.; // dM/dt (degrees/century)

	setPhysicalSize( pSize );
	setType( 2 ); //It's really a planet, not an asteroid
}

KSPluto::~KSPluto() {
}

//Determine values for the orbital elements for the requested JD, then 
//call KSAsteroid::findGeocentricPosition()
bool KSPluto::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {
	//First, set the orbital element values according to the current epoch
	double t = num->julianCenturies();
	set_a( a0 + a1*t );
	set_e( e0 + e1*t );
	set_i( i0.Degrees() + i1*t );
	set_N( N0.Degrees() + N1*t );
	set_M( M0.Degrees() + M1*t );
	set_P( 365.2568984 * pow((a0+a1*t), 1.5) ); //set period from Kepler's 3rd law
	setJD( num->julianDay() );

	return KSAsteroid::findGeocentricPosition( num, Earth );
}
