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

#ifndef KSSUN_H
#define KSSUN_H

#include <qstring.h>
#include "dms.h"
#include "ksplanet.h"

/**
	*Child class of KSPlanetBase; encapsulates information about the Sun.
	*@short Provides necessary information about the Sun.
  *@author Mark Hollomon
  *@version 0.9
  */

class KSSun : public KSPlanet  {
public:
/**
	*Default constructor.  Defines constants needed by findPosition().
	*Sets Ecliptic coordinates appropriate for J2000.
	*/
	KSSun(QString fn = "");

/**
	*Destructor (empty)
	*/
	virtual ~KSSun() {}

	/**
	 *  reimplemented from KSPlanet
	 */
	virtual bool loadData();

/**
	*Determine RA, Dec coordinates for the Epoch given in the argument.
	*@param Epoch current Julian Date
	*/
	virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );	
};

#endif
