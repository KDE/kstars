/***************************************************************************
                          kssun.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jan 29 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSSUN_H_
#define KSSUN_H_

#include <QString>
#include "ksplanet.h"

/** @class KSSun
	*Child class of KSPlanetBase; encapsulates information about the Sun.
	*@short Provides necessary information about the Sun.
	*@author Mark Hollomon
	*@version 1.0
	*/

class KSSun : public KSPlanet  {
public:
    /** Constructor.  Defines constants needed by findPosition().
    	*Sets Ecliptic coordinates appropriate for J2000.
    	*@param kd pointer to KStarsData object
    	*/
    KSSun();

    virtual KSSun* clone() const;
    virtual SkyObject::UID getUID() const;

    /** Destructor (empty)
    	*/
    virtual ~KSSun() {}

    /** Read orbital data from disk
    	*@note reimplemented from KSPlanet
    	*@note we actually read Earth's orbital data.  The Sun's geocentric
    	*ecliptic coordinates are by definition exactly the opposite of the 
    	*Earth's heliocentric ecliptic coordinates.
    	*/
    virtual bool loadData();


protected:
    /** Determine geocentric RA, Dec coordinates for the Epoch given in the argument.
    	*@p Epoch current Julian Date
    	*@p Earth pointer to earth object
    	*/
    virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );
private:
    virtual void findMagnitude(const KSNumbers*);
};
long double equinox(int year, int d, int m, int angle);

#endif
