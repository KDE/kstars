/***************************************************************************
                          ksnumbers.h  -  description
                             -------------------
    begin                : Sun Jan 13 2002
    copyright            : (C) 2002-2005 by Jason Harris
    email                : kstars@30doradus.org
    copyright            : (C) 2004-2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
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

#define NUTTERMS 63

#include "dms.h"

/**@class KSNumbers
	*
	*There are several time-dependent values used in position calculations,
	*that are not specific to an object.  This class provides
	*storage for these values, and methods for calculating them for a given date.
	*The numbers include solar data like the true/mean solar anomalies
	*and longitudes, the longitude of the Earth's perihelion, the
	*eccentricity of Earth's orbit, the
	*constant of aberration, the obliquity of the Ecliptic, the effects of
	*Nutation (delta Obliquity and delta Ecliptic longitude),
	*the Julian Day/Century/Millenium, and arrays for computing the precession.
	*@short Store several time-dependent astronomical quantities.
	*@author Jason Harris
	*@version 1.0
	*/

class KSNumbers {
public: 
	/**Constructor. */
	KSNumbers( long double jd );
	/**Destructor (empty). */
	~KSNumbers();

	/**@return the current Obliquity (the angle of inclination between
		*the celestial equator and the ecliptic)
		*/
	const dms* obliquity() const { return &Obliquity; }

	/**@return the constant of aberration (20.49 arcsec). */
	dms constAberr() const { return K; }

	/**@return the mean solar anomaly. */
	dms sunMeanAnomaly() const { return M; }

	/**@return the mean solar longitude. */
	dms sunMeanLongitude() const { return L; }

	/**@return the true solar anomaly. */
	dms sunTrueAnomaly() const { return M0; }

	/**@return the true solar longitude. */
	dms sunTrueLongitude() const { return L0; }

	/**@return the longitude of the Earth's perihelion point. */
	dms earthPerihelionLongitude() const { return P; }

	/**@return eccentricity of Earth's orbit.*/
	double earthEccentricity() const { return e; }

	/**@return the change in obliquity due to the nutation of 
	 * Earth's orbit. Value is in degrees */
	double dObliq() const { return deltaObliquity; }

	/**@return the change in Ecliptic Longitude due to nutation.
	 * Value is in degrees. */
	double dEcLong() const { return deltaEcLong; }

	/**@return Julian centuries since J2000*/
	double julianCenturies() const { return T; }

	/**@return Julian Day*/
	long double julianDay() const { return days; }

	/**@return Julian Millenia since J2000*/
	double julianMillenia() const { return jm; }

	/**@return element of P1 precession array at position [i1][i2] */
	double p1( int i1, int i2 ) const { return P1[i1][i2]; }

	/**@return element of P2 precession array at position [i1][i2] */
	double p2( int i1, int i2 ) const { return P2[i1][i2]; }

	/**@return element of P1B precession array at position [i1][i2] */
	double p1b( int i1, int i2 ) const { return P1B[i1][i2]; }

	/**@return element of P2B precession array at position [i1][i2] */
	double p2b( int i1, int i2 ) const { return P2B[i1][i2]; }

	/**@short update all values for the date given as an argument. 
		*@param jd the Julian date for which to compute values
		*/
	void updateValues( long double jd );

	double vEarth(int i) const {return vearth[i];}

private:
	dms Obliquity, K, L, L0, LM, M, M0, O, P, D, MM, F;
	dms XP, YP, ZP, XB, YB, ZB;
	double CX, SX, CY, SY, CZ, SZ;
	double CXB, SXB, CYB, SYB, CZB, SZB;
	double P1[3][3], P2[3][3], P1B[3][3], P2B[3][3];
	double deltaObliquity, deltaEcLong;
	double e, T, TB;
	long double days;
	double jm;
	static const int arguments[NUTTERMS][5];
	static const int amp[NUTTERMS][4];
	double vearth[3];
};

#endif
