/***************************************************************************
                          kscomet.h  -  K Desktop Planetarium
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

#ifndef KSCOMET_H
#define KSCOMET_H

#include "ksplanetbase.h"

/**@class KSComet 
	*@short A subclass of KSPlanetBase that implements comets.
	*
	*The orbital elements are stored as private member variables, and
	*it provides methods to compute the ecliptic coordinates for any
	*time from the orbital elements.
	*
	*The orbital elements are:
	*@li JD    Epoch of element values
	*@li q     perihelion distance (AU)
	*@li e     eccentricity of orbit
	*@li i     inclination angle (with respect to J2000.0 ecliptic plane)
	*@li w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
	*@li N     longitude of ascending node (J2000.0 ecliptic)
	*@li Tp    time of perihelion passage (YYYYMMDD.DDD)
	*
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsData;
class KSNumbers;
class dms;

class KSComet : public KSPlanetBase
{
	public:
		/**Constructor.
			*@p kd pointer to the KStarsData object
			*@p s the name of the comet
			*@p image_file the filename for an image of the comet
			*@p JD the Julian Day for the orbital elements
			*@p q the perihelion distance of the comet's orbit (AU)
			*@p e the eccentricity of the comet's orbit
			*@p i the inclination angle of the comet's orbit
			*@p w the argument of the orbit's perihelion
			*@p N the longitude of the orbit's ascending node
			*@p M the mean anomaly for the Julian Day
			*@p Tp The date of the most proximate perihelion passage (YYYYMMDD.DDD)
			*/
		KSComet( KStarsData *kd, QString s, QString image_file,
			long double JD, double q, double e, dms i, dms w, dms N, double Tp );

		/**Destructor (empty)*/
		virtual ~KSComet() {}

		/**Unused virtual function inherited from KSPlanetBase, 
			*so it's simply empty here.
			*/
		virtual bool loadData();


	protected:
	/**Calculate the geocentric RA, Dec coordinates of the Comet.
		*@note reimplemented from KSPlanetBase
		*@param num time-dependent values for the desired date
		*@param Earth planet Earth (needed to calculate geocentric coords)
		*@return true if position was successfully calculated.
		*/
		virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

	private:
		KStarsData *kd;
		long double JD, JDp;
		double q, e, a, P;
		dms i, w, N;

};

#endif
