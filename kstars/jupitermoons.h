/***************************************************************************
                          jupitermoons.h  -  description
                             -------------------
    begin                : Fri Oct 18 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef JUPITERMOONS_H
#define JUPITERMOONS_H

#include "skypoint.h"
#include "ksplanet.h"
#include "kssun.h"

/**Implements the four largest moons of Jupiter.
	*See Chapter 43 of "Astronomical Algorithms"by Jean Meeus for details
  *@author Jason Harris
  */

class JupiterMoons {
public: 
	JupiterMoons();
	~JupiterMoons();

	void findPosition( const KSNumbers *num, const KSPlanet *jup, const KSSun *sun );
	SkyPoint* pos( int id ) { return &Pos[id]; }
	bool inFront( int id ) { return InFront[id]; }
	void EquatorialToHorizontal( dms LSTh, dms lat );
private:
	SkyPoint Pos[4];
	bool InFront[4];
};

#endif

