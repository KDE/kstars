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
	bool inFront( int id ) const { return InFront[id]; }
	QString name( int id ) const { return Name[id]; }
	int moonNamed( const QString &name ) const;
	void EquatorialToHorizontal( const dms *LSTh, const dms *lat );
	double x( int i ) const { return XJ[i]; }
	double y( int i ) const { return YJ[i]; }
	double z( int i ) const { return ZJ[i]; }
private:
	SkyPoint Pos[4];
	QString Name[4];
	bool InFront[4];
	//the rectangular position, relative to Jupiter.  X-axis is equator of Jupiter; usints are Jup. Radius
	double XJ[4], YJ[4], ZJ[4];
};

#endif

