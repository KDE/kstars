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

#include <ksplanet.h>

/**
	*A subclass of KSPlanet that provides a custom findPosition() function
	*needed for the unique orbit of Pluto.  The Pluto ephemeris gives its
	*Heliocentric coordinates in rectangular (X,Y,Z), which must be converted
	*to (R, Longitude, Latitude)
	*@short Provides necessary information about Pluto.
  *@author Jason Harris
  *@version 0.7
  */

class KSPluto : public KSPlanet  {
public:
/**
	*Default constructor.  Calls KSPlanet constructor with name="Pluto" and
	*a null image.
	*/
	KSPluto();
	~KSPluto();
/**
	*A custom findPosition() function needed for the unique orbit of Pluto.
	*Pluto coordinates are first solved in heliocentric rectangular (X, Y, Z)
	*coordinates, which are then converted to heliocentric spherical
	*(R, Longitude, Latitude) coordinates, and finally translated to geocentric
	*(RA, Dec) coordinates.
	*/
	bool findPosition( long double Epoch, KSPlanet *Earth );
};

#endif
