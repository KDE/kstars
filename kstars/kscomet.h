/***************************************************************************
                          kscomet.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
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

#ifndef KSCOMET_H
#define KSCOMET_H

#include <klocale.h>

#include <qstring.h>
#include <qimage.h>

#include "ksplanetbase.h"

/** KSComet is a subclass of KSPlanetBase that implements comets.  
 *  The orbital elements are stored as private member variables, and 
 *  it provides methods to compute the ecliptic coordinates for any 
 *  time from the orbital elements.
 *  
 *  The orbital elements are:
 *  
 *  JD    Epoch of element values
 *  q     perihelion distance (AU)
 *  e     eccentricity of orbit
 *  i     inclination angle (with respect to J2000.0 ecliptic plane)
 *  w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
 *  N     longitude of ascending node (J2000.0 ecliptic)
 *  Tp    time of perihelion passage (YYYYMMDD.DDD)
 *
 *@short Encapsulates a comet.
 *@author Jason Harris
 *@version 0.9
 */

class KStars;
class KSNumbers;
class dms;

class KSComet : public KSPlanetBase
{
	public:
		KSComet( KStars *ks, QString s, QString image_file,
			long double JD, double q, double e, dms i, dms w, dms N, double Tp );
		
		virtual ~KSComet() {}
		
		virtual bool loadData();


/**
	*Calculate the RA, Dec coordinates of the Comet.
	*@param num time-dependent values for the desired date
	*@param Earth planet Earth (needed to calculate geocentric coords)
	*@returns true if position was successfully calculated.
	*/
		virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );
	
	private:
		KStars *ks;
		long double JD, JDp;
		double q, e, a, P;
		dms i, w, N;
		
};

#endif
