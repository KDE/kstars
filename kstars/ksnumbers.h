/***************************************************************************
                          ksnumbers.h  -  description
                             -------------------
    begin                : Sun Jan 13 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef KSNUMBERS_H
#define KSNUMBERS_H

#include "dms.h"

/**There are several time-dependent values used in position calculations,
	*that are not specific to an object.  This class provides
	*storage for these values, and methods for calculating them for a given date.
	*The numbers include solar data like the true/mean solar anomalies
	*and longitudes, the longitude of the Earth's perihelion, the
	*eccentricity of Earth's orbit, the
	*constant of aberration, the obliquity of the Ecliptic, the effects of
	*Nutation (delta Obliquity and delta Ecliptic longitude),
	*the Julian Day/Century/Millenium, and arrays for computing the precession.
	*@short Numbers used in a variety of position calculations.
	*@author Jason Harris
	*@version 0.9
	*/

class KSNumbers {
public: 
	/**Constructor. */
	KSNumbers( long double jd );
	/**Destructor (empty). */
	~KSNumbers();

	/**@returns the current Obliquity (the angle of inclination between
		*the celestial equator and the ecliptic)
		*/
	const dms* obliquity() const { return &Obliquity; }

	/**@returns the constant of aberration (20.49 arcsec). */
	dms constAberr() const { return K; }

	/**@returns the mean solar anomaly. */
	dms sunMeanAnomaly() const { return M; }

	/**@returns the mean solar longitude. */
	dms sunMeanLongitude() const { return L; }

	/**@returns the true solar anomaly. */
	dms sunTrueAnomaly() const { return M0; }

	/**@returns the true solar longitude. */
	dms sunTrueLongitude() const { return L0; }

	/**@returns the longitude of the Earth's perihelion point. */
	dms earthPerihelionLongitude() const { return P; }

	/**@returns eccentricity of Earth's orbit.*/
	double earthEccentricity() const { return e; }

	/**@returns the change in obliquity due to the nutation of Earth's orbit. */
	double dObliq() const { return deltaObliquity; }

	/**@returns the change in Ecliptic Longitude due to nutation. */
	double dEcLong() const { return deltaEcLong; }

	/**@returns Julian centuries since J2000*/
	double julianCenturies() const { return T; }

	/**@returns Julian Day*/
	long double julianDay() const { return days; }

	/**@returns Julian Millenia since J2000*/
	double julianMillenia() const { return jm; }

	/**@returns element of P1 precession array at position [i1][i2] */
	double p1( int i1, int i2 ) const { return P1[i1][i2]; }

	/**@returns element of P2 precession array at position [i1][i2] */
	double p2( int i1, int i2 ) const { return P2[i1][i2]; }

	/**@short update all values for the date given as an argument. */
	void updateValues( long double jd );

private:
	dms Obliquity, K, L, L0, LM, M, M0, O, P;
	dms XP, YP, ZP;
	double CX, SX, CY, SY, CZ, SZ;
	double P1[3][3], P2[3][3];
	double deltaObliquity, deltaEcLong;
	double e, T;
	long double days;
	double jm;

};

#endif
