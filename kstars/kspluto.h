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

#include <ksplanetbase.h>

/**
	*A subclass of KSPlanetBase that provides a custom findPosition() function
	*needed for the unique orbit of Pluto.  The Pluto ephemeris gives its
	*Heliocentric coordinates in rectangular (X,Y,Z), which must be converted
	*to (R, Ecl. Longitude, Ecl. Latitude)
	*@short Provides necessary information about Pluto.
  *@author Jason Harris
  *@version 0.9
  */

class KSPluto : public KSPlanetBase  {
public:
/**
	*Default constructor.  Calls KSPlanetBase constructor with name="Pluto" and
	*a null image.
	*/
	KSPluto(QString fn="");

/**Destructor */
	virtual ~KSPluto() {};

/**
	*A custom findPosition() function needed for the unique orbit of Pluto.
	*Pluto coordinates are first solved in heliocentric rectangular (X, Y, Z)
	*coordinates, which are then converted to heliocentric spherical
	*(R, EcLong, EcLat) coordinates, and finally translated to geocentric
	*(RA, Dec) coordinates.
	*/
	virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

	virtual bool loadData();

protected:
	virtual bool loadData(QString n);

private:
	class XYZData {
		public:
			double ac, as;
			XYZData(double AC = 0.0, double AS = 0.0) : ac(AC), as(AS) {};
	};
	static int DATAARRAYSIZE;
	static bool data_loaded;
	static double *freq;
	static XYZData *xdata;
	static XYZData *ydata;
	static XYZData *zdata;

	class XYZpos {
		public:
			double X;
			double Y;
			double Z;
			XYZpos(double pX = 0.0, double pY = 0.0, double pZ = 0.0) :
				X(pX), Y(pY), Z(pZ) {};
	};

	XYZpos calcRectCoords(double jc);
};

#endif
