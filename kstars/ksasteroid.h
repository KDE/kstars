/***************************************************************************
                          ksasteroid.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
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

#ifndef KSASTEROID_H
#define KSASTEROID_H

#include <klocale.h>

#include <qstring.h>
#include <qimage.h>

#include "ksplanetbase.h"

/** KSAsteroid is a subclass of KSPlanetBase that implements asteroids.
 *  The orbital elements are stored as private member variables, and it
 *  provides methods to compute the ecliptic coordinates for any time
 *  from the orbital elements.
 *
 *  The orbital elements are:
 *
 *  JD    Epoch of element values
 *  a     semi-major axis length (AU)
 *  e     eccentricity of orbit
 *  i     inclination angle (with respect to J2000.0 ecliptic plane)
 *  w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
 *  N     longitude of ascending node (J2000.0 ecliptic)
 *  M     mean anomaly at epoch JD
 *  H     absolute magnitude
 *
 *@short Encapsulates an asteroid.
 *@author Jason Harris
 *@version 0.9
 */

class KStarsData;
class KSNumbers;
class dms;

class KSAsteroid : public KSPlanetBase
{
	public:
		KSAsteroid( KStarsData *kd, QString s, QString image_file,
			long double JD, double a, double e, dms i, dms w, dms N, dms M, double H );

		virtual ~KSAsteroid() {}

		virtual bool loadData();

	protected:
/**
	*Calculate the geocentric RA, Dec coordinates of the Asteroid.
	*@param num time-dependent values for the desired date
	*@param Earth planet Earth (needed to calculate geocentric coords)
	*@returns true if position was successfully calculated.
	*/
		virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

	private:
		KStarsData *kd;
		long double JD;
		double a, e, H, P;
		dms i, w, M, N;

};

#endif
