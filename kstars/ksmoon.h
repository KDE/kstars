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

#include "ksplanetbase.h"
#include "dms.h"

/**@class KSMoon
	*A subclass of SkyObject that provides information
	*needed for the Moon.  Specifically, KSMoon provides a moon-specific
	*findPosition() function.  Also, there is a method findPhase(), which returns
	*the lunar phase as a floating-point number between 0.0 and 1.0.
	*@short Provides necessary information about the Moon.
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsData;
class KSSun;

class KSMoon : public KSPlanetBase  {
public:
	/**
		*Default constructor.  Set name="Moon".
		*/
	KSMoon(KStarsData *kd);

	/**Destructor (empty). */
	~KSMoon();

	/**
		*Determine the phase angle of the moon, and assign the appropriate
		*moon image
		*@param Sun The current Sun object.
		*/
	void findPhase( const KSSun *Sun );

	/**@return the moon's current phase angle, as a dms angle
		*/
	dms phase( void ) const { return Phase; }

	/**@return the illuminated fraction of the Moon as seen from Earth
		*/
	double illum( void ) const { return 0.5*(1.0 - cos( phase().radians() ) ); }
	
	/**@return a short string describing the moon's phase
		*/
	QString phaseName( void ) const;

	/** reimplemented from KSPlanetBase
		*/
	virtual bool loadData();

protected:
	/**Reimplemented from KSPlanetBase, this function employs unique algorithms for
		*estimating the lunar coordinates.  Finding the position of the moon is
		*much more difficult than the other planets.  For one thing, the Moon is
		*a lot closer, so we can detect smaller deviations in its orbit.  Also,
		*the Earth has a significant effect on the Moon's orbit, and their
		*interaction is complex and nonlinear.  As a result, the positions as
		*calculated by findPosition() are only accurate to about 10 arcseconds
		*(10 times less precise than the planets' positions!)
		*@short moon-specific coordinate finder
		*@param num KSNumbers pointer for the target date/time
		*@note we don't use the Earth pointer here
		*/
	virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase* );

private:
	dms Phase;
	static bool data_loaded;

/**@class MoonLRData
	*Encapsulates the Longitude and radius terms of the sums
	*used to compute the moon's position.
	*@short Moon Longitude and radius data object
	*@author Mark Hollomon
	*@version 1.0
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
				nd(pnd), nm(pnm), nm1(pnm1), nf(pnf), Li(pLi), Ri(pRi) {}
	};

	static QPtrList<MoonLRData> LRData;

/**@class MoonBData
	*Encapsulates the Latitude terms of the sums
	*used to compute the moon's position.
	*@short Moon Latitude data object
	*@author Mark Hollomon
	*@version 1.0
	*/
	class MoonBData {
		public:
			int nd;
			int nm;
			int nm1;
			int nf;
			double Bi;

			MoonBData( int pnd, int pnm, int pnm1, int pnf, double pBi ):
				nd(pnd), nm(pnm), nm1(pnm1), nf(pnf), Bi(pBi) {}
	};

	static QPtrList<MoonBData> BData;
};

#endif
