/***************************************************************************
                          ksplanet.h  -  K Desktop Planetarium
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

#include <qstring.h>
#include <qimage.h>
#include <qvector.h>
#include <qdict.h>
#include "ksplanetbase.h"
#include "dms.h"

#ifndef KSPLANET_H
#define KSPLANET_H

/**
	*A subclass of SkyObject that provides additional information
	*needed for solar system objects.  KSPlanet may be used
	*directly for all planets except Pluto.  The Sun are subclassed from
	*KSPlanet.
	*@short Provides necessary information about objects in the solar system.
  *@author Jason Harris
  *@version 0.9
  */

class KSPlanet : public KSPlanetBase {
public: 

/**
	*Constructor.  Calls SkyObject constructor with type=2 (planet),
	*coordinates=0.0, mag=0.0, primary name s, and all other QStrings empty.
	*@param s Name of planet
	*@param im the planet's image
	*/
	KSPlanet( QString s="unnamed", QString image_file="" );

/**
	*Destructor (empty)
	*/
	virtual ~KSPlanet() {}


/**
	*Calculate the RA, Dec coordinates of the Planet.
	*@param num time-dependent values for the desired date
	*@param Earth planet Earth (needed to calculate geocentric coords)
	*@returns true if position was successfully calculated.
	*/
	virtual bool findPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

/**
	*@short Preload the data used by findPosition.
	*/
	virtual bool loadData();

/**Calculate the ecliptic longitude and latitude of the planet for
	*the given date (expressed in Julian Millenia since J2000).  A reference
	*to the ecliptic coordinates is returned as the second object.
	*/
	virtual void calcEcliptic(double jm, EclipticPosition &ret) const;

protected:

	bool data_loaded;
	class OrbitData  {
		public:
			double A, B, C;
			OrbitData(double a, double b, double c) :
				A(a), B(b), C(c) {};
	};

	typedef QVector<OrbitData> OBArray[6];
	class OrbitDataColl {
		public:
			OrbitDataColl();

			OBArray Lon;
			OBArray Lat;
			OBArray Dst;
	};


	class OrbitDataManager {
		public:
			OrbitDataManager();

			OrbitDataColl *loadData(QString n);

		private:
			bool readOrbitData(QString fname, QVector<KSPlanet::OrbitData> *vector);
			QDict<OrbitDataColl> dict;
	};

	static OrbitDataManager odm;

};

#endif
