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

#include <ksplanetbase.h>
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

class KStars;
class KSMoon : public KSPlanetBase  {
public: 
	/**
		*Default constructor.  Set name="Moon".
		*/
	KSMoon(KStars *ks);

	/**Destructor (empty). */
	~KSMoon();

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
		*@param jd current Julian Date
		*/
	virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth = 0);

	/**
	  *@short Calls findPosition( Epoch ), then localizeCoords( lat, LST ).
		*@param jd current Julian Date
		*@param lat The geographic latitude
		*@param LST The local sidereal time
		*/
	void findPosition( KSNumbers *num, const dms *lat, const dms *LST );

	/**
		*Convert geocentric coordinates to local (topographic) coordinates,
		*by correcting for the effect of parallax (a.k.a "figure of the Earth").
		*@param lat The geographic latitude
		*@param LST The local sidereal time
		*/
	void localizeCoords( const dms *lat, const dms *LST );

	/**
		*Determine the phase angle of the moon, and assign the appropriate
		*moon image
		*@param Sun The current Sun object.
		*/
	void findPhase( const KSSun *Sun );

	/**
		*@returns the moon's current distance from the Earth
		*/
	double distance( void ) { return Rearth; }

	/**
		*@returns the moon's current phase angle, as a dms angle
		*/
	dms phase( void ) { return Phase; }

	/**
	 * reimplemented from KSPlanetBase
	 */
	virtual bool loadData();

protected:

private:
	double Rearth; //Distance from Earth, in km.
	dms Phase;
	static bool data_loaded;

/**Class to encapsulate the Longitude and radius terms of the sums
	*used to compute the moon's position.
	*@short Moon Longitude and radius data object
	*@author Mark Hollomon
	*@version 0.9
	*/
	class MoonLRData {
		public:
			int nd;
			int nm;
			int nm1;
			int nf;
			double Li;
			double Ri;
			
			MoonLRData( int pnd, int pnm, int pnm1, int pnf, double pLi, double pRi ):
				nd(pnd), nm(pnm), nm1(pnm1), nf(pnf), Li(pLi), Ri(pRi) {};
	};

	static QList<MoonLRData> LRData;

/**Class to encapsulate the Latitude terms of the sums
	*used to compute the moon's position.
	*@short Moon Latitude data object
	*@author Mark Hollomon
	*@version 0.9
	*/
	class MoonBData {
		public:
			int nd;
			int nm;
			int nm1;
			int nf;
			double Bi;
			
			MoonBData( int pnd, int pnm, int pnm1, int pnf, double pBi ):
				nd(pnd), nm(pnm), nm1(pnm1), nf(pnf), Bi(pBi) {};
	};

	static QList<MoonBData> BData;
};

#endif
