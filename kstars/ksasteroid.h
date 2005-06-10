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

#include "ksplanetbase.h"

/**@class KSAsteroid 
	*@short A subclass of KSPlanetBase that implements asteroids.
	*
	*The orbital elements are stored as private member variables, and it
	*provides methods to compute the ecliptic coordinates for any time
	*from the orbital elements.
	*
	*The orbital elements are:
	*@li JD    Epoch of element values
	*@li a     semi-major axis length (AU)
	*@li e     eccentricity of orbit
	*@li i     inclination angle (with respect to J2000.0 ecliptic plane)
	*@li w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
	*@li N     longitude of ascending node (J2000.0 ecliptic)
	*@li M     mean anomaly at epoch JD
	*@li H     absolute magnitude
	*
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsData;
class KSNumbers;
class dms;

class KSAsteroid : public KSPlanetBase
{
	public:
		/**Constructor.
			*@p kd pointer to the KStarsData object
			*@p s the name of the asteroid
			*@p image_file the filename for an image of the asteroid
			*@p JD the Julian Day for the orbital elements
			*@p a the semi-major axis of the asteroid's orbit (AU)
			*@p e the eccentricity of the asteroid's orbit
			*@p i the inclination angle of the asteroid's orbit
			*@p w the argument of the orbit's perihelion
			*@p N the longitude of the orbit's ascending node
			*@p M the mean anomaly for the Julian Day
			*@p H absolute magnitude
			*/
		KSAsteroid( KStarsData *kd, QString s, QString image_file,
			long double JD, double a, double e, dms i, dms w, dms N, dms M, double H );

		/**Destructor (empty)*/
		virtual ~KSAsteroid() {}

		/**This is inherited from KSPlanetBase.  We don't use it in this class,
			*so it is empty.
			*/
		virtual bool loadData();

	protected:
/**Calculate the geocentric RA, Dec coordinates of the Asteroid.
	*@note reimplemented from KSPlanetBase
	*@param num time-dependent values for the desired date
	*@param Earth planet Earth (needed to calculate geocentric coords)
	*@return true if position was successfully calculated.
	*/
		virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

		//these set functions are needed for the new KSPluto subclass
		void set_a( double newa ) { a = newa; }
		void set_e( double newe ) { e = newe; }
		void set_P( double newP ) { P = newP; }
		void set_i( double newi ) { i.setD( newi ); }
		void set_w( double neww ) { w.setD( neww ); }
		void set_M( double newM ) { M.setD( newM ); }
		void set_N( double newN ) { N.setD( newN ); }
		void setJD( long double jd ) { JD = jd; }

	private:
		KStarsData *kd;
		long double JD;
		double a, e, H, P;
		dms i, w, M, N;
};

#endif
