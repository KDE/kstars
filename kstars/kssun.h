/***************************************************************************
                          kssun.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
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

#ifndef KSSUN_H
#define KSSUN_H

#include <qstring.h>
#include "dms.h"
#include "ksplanet.h"

/**
	*Child class of KSPlanet; encapsulates information about the Sun.
	*@short Provides necessary information about the Sun.
  *@author Jason Harris
  *@version 0.4
  */

class KSSun : public KSPlanet  {
public:
/**
	*Default constructor.  Defines constants needed by findPosition().
	*Sets Ecliptic coordinates appropriate for J2000.
	*/
	KSSun();
/**
	*Constructor.  Defines constants needed by findPosition().
	*Sets Ecliptic coordinates according to Epoch given as the argument.
	*@param Epoch current Julian Date
	*/
	KSSun( long double Epoch );
/**
	*Destructor (empty)
	*/
	~KSSun();
/**
	*Determine RA, Dec coordinates for the Epoch given in the argument.
	*@param Epoch current Julian Date
	*/
	bool findPosition( long double jd );	

private:
	long double JD0;
	double eclong0, plong0, e0;
};

#endif
