/***************************************************************************
                          kspluto.h  -  K Desktop Planetarium
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

#ifndef KSPLUTO_H
#define KSPLUTO_H

#include "ksasteroid.h"

/**@class KSPluto
	*A subclass of KSAsteroid that represents the planet Pluto.  Now, we 
	*are certainly NOT claiming that Pluto is an asteroid.  However, the 
	*findPosition() routines of KSPlanet do not work properly for Pluto.
	*We had been using a unique polynomial expansion for Pluto, but even
	*this fails spectacularly for dates much more remote than a few 
	*hundred years.  We consider it better to instead treat Pluto's 
	*orbit much more simply, using elliptical orbital elements as we do 
	*for asteroids.  In order to improve the long-term accuracy of Pluto's
	*position, we are also including linear approximations of the evolution
	*of each orbital element with time.
	*
	*The orbital element data (including the time-derivatives) come from 
	*the NASA/JPL website:  http://ssd.jpl.nasa.gov/elem_planets.html
	*
	*@short Provides necessary information about Pluto.
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsData;
class KSPluto : public KSAsteroid  {
public:
/**Constructor.  Calls KSAsteroid constructor with name="Pluto", and fills
	*in orbital element data (which is hard-coded for now).
	*@p kd pointer to the KStarsData object
	*@p fn filename of Pluto's image
	*@p pSize physical diameter of Pluto, in km
	*/
	KSPluto(KStarsData *kd, QString fn="", double pSize=0);

/**Destructor (empty) */
	virtual ~KSPluto();

protected:
/**A custom findPosition() function for Pluto.  Computes the values of the 
	*orbital elements on the requested date, and calls KSAsteroid::findGeocentricPosition()
	*using those elements.
	*@param num time-dependent values for the desired date
	*@param Earth planet Earth (needed to calculate geocentric coords)
	*@return true if position was successfully calculated.
	*/
	virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

private:
	//The base orbital elements for J2000 (these don't change with time)
	double a0, e0;
	dms i0, w0, M0, N0;

	//Rates-of-change for each orbital element
	double a1, e1, i1, w1, M1, N1;
};

#endif
