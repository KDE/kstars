/***************************************************************************
                          ksmoon.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 26 2001
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

#ifndef KSMOON_H
#define KSMOON_H

#include <ksplanet.h>
#include "dms.h"

/**
	*A subclass of SkyObject that provides information
	*needed for the Moon.  Specifically, KSMoon provides a moon-specific
	*findPosition() function.  Also, there is a method findPhase(), which returns
	*the lunar phase as a floating-point number between 0.0 and 1.0.
	*@short Provides necessary information about the Moon.
  *@author Jason Harris
	*@version 0.9
  */

class KSMoon : public KSPlanet  {
public: 
	/**
		*Default constructor.  Set name="Moon", calculate position for the given date.
		*@param Epoch the initial date for which to calculate the position
		*/
	KSMoon( long double Epoch );

	/**Destructor (empty). */
	~KSMoon() {}

	/**
    *Reimplemented from KSPlanet, this function employs unique algorithms for
    *estimating the lunar coordinates.  Finding the position of the moon is
    *much more difficult than the other planets.  For one thing, the Moon is
    *a lot closer, so we can detect smaller deviations in its orbit.  Also,
    *the Earth has a significant effect on the Moon's orbit, and their
    *interaction is complex and nonlinear.  As a result, the positions as
    *calculated by findPosition() are only accurate to about 10 arcseconds
		*(10 times less precise than the planets' positions!)
		*@short moon-specific coordinate finder
		*@param Epoch current Julian Date
		*/
	void findPosition( long double Epoch );

	/**
	  *@short Calls findPosition( Epoch ), then localizeCoords( lat, LST ).
		*@param Epoch current Julian Date
		*@param lat The geographic latitude
		*@param LST The local sidereal time
		*/
	void findPosition( long double Epoch, dms lat, dms LST );

	/**
		*Convert geocentric coordinates to local (topographic) coordinates,
		*by correcting for the effect of parallax (a.k.a "figure of the Earth").
		*@param lat The geographic latitude
		*@param LST The local sidereal time
		*/
	void localizeCoords( dms lat, dms LST );

	/**
		*Determine the phase angle of the moon, and assign the appropriate
		*moon image
		*@param Sun The current Sun object.
		*/
	void findPhase( KSSun *Sun );

	/**
		*@returns the moon's current distance from the Earth
		*/
	double distance( void ) { return Rearth; }

	/**
		*@returns the moon's current phase angle, as a dms angle
		*/
	dms phase( void ) { return Phase; }
	
private:
	double Rearth; //Distance from Earth, in km.
	dms Phase;
};

#endif
