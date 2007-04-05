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
#include <qptrvector.h>
#include <qdict.h>

#include "ksplanetbase.h"
#include "dms.h"

#ifndef KSPLANET_H
#define KSPLANET_H

/**@class KSPlanet
	*A subclass of KSPlanetBase for seven of the major planets in the solar system
	*(Earth and Pluto have their own specialized classes derived from KSPlanetBase).  
	*@note The Sun is subclassed from KSPlanet.
	*
	*KSPlanet contains internal classes to manage the computations of a planet's position.
	*The position is computed as a series of sinusoidal sums, similar to a Fourier
	*transform.  See "Astronomical Algorithms" by Jean Meeus or the file README.planetmath
	*for details.
	*@short Provides necessary information about objects in the solar system.
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsData;

class KSPlanet : public KSPlanetBase {
public:

/**Constructor.  
	*@param kd Some kind of data
	*@param s Name of planet
	*@param image_file filename of the planet's image
	*@param pSize physical diameter of the planet, in km
	*
	*@todo figure out what @p kd does.
	*/
	KSPlanet( KStarsData *kd, QString s="unnamed", QString image_file="", double pSize=0 );

/**Destructor (empty)
	*/
	virtual ~KSPlanet() {}

/**@short Preload the data used by findPosition.
	*/
	virtual bool loadData();

/**Calculate the ecliptic longitude and latitude of the planet for
	*the given date (expressed in Julian Millenia since J2000).  A reference
	*to the ecliptic coordinates is returned as the second object.
	*@param jm Julian Millenia (=jd/1000)
	*@param ret The ecliptic coordinates are returned by reference through this argument.
	*/
	virtual void calcEcliptic(double jm, EclipticPosition &ret) const;

protected:

	bool data_loaded;

/**Calculate the geocentric RA, Dec coordinates of the Planet.
	*@note reimplemented from KSPlanetBase
	*@param num pointer to object with time-dependent values for the desired date
	*@param Earth pointer to the planet Earth (needed to calculate geocentric coords)
	*@return true if position was successfully calculated.
	*/
	virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

/**@class OrbitData 
	*This class contains doubles A,B,C which represent a single term in a planet's
	*positional expansion sums (each sum-term is A*COS(B+C*T)).
	*@author Mark Hollomon
	*@version 1.0
	*/
	class OrbitData  {
		public:
			/**Constructor 
				*@p a the A value
				*@p b the B value
				*@p c the C value
				*/
			OrbitData(double a, double b, double c) :
				A(a), B(b), C(c) {}
			
			double A, B, C;
	};

	typedef QPtrVector<OrbitData> OBArray[6];

/**OrbitDataColl contains three groups of six QPtrVectors.  Each QPtrVector is a
	*list of OrbitData objects, representing a single sum used in computing
	*the planet's position.  A set of six of these vectors comprises the large
	*"meta-sum" which yields the planet's Longitude, Latitude, or Distance value.
	*@author Mark Hollomon
	*@version 1.0
	*/
	class OrbitDataColl {
		public:
			/**Constructor*/
			OrbitDataColl();

			OBArray Lon;
			OBArray Lat;
			OBArray Dst;
	};


/**OrbitDataManager places the OrbitDataColl objects for all planets in a QDict
	*indexed by the planets' names.  It also loads the positional data of each planet
	*from disk.
	*@author Mark Hollomon
	*@version 1.0
	*/
	class OrbitDataManager {
		public:
			/**Constructor*/
			OrbitDataManager();

			/**Load orbital data for a planet from disk.
				*The data is stored on disk in a series of files named 
				*"name.[LBR][0...5].vsop", where "L"=Longitude data, "B"=Latitude data,
				*and R=Radius data.
				*@p n the name of the planet whose data is to be loaded from disk.
				*@return pointer to the OrbitDataColl containing the planet's orbital data.
				*/
			OrbitDataColl *loadData(QString n);

		private:
			/**Read a single orbital data file from disk into an OrbitData vector.
			*The data files are named "name.[LBR][0...5].vsop", where 
			*"L"=Longitude data, "B"=Latitude data, and R=Radius data.
			*@p fname the filename to be read.
			*@p vector pointer to the OrbitData vector to be filled with these data.
			*/
			bool readOrbitData(QString fname, QPtrVector<KSPlanet::OrbitData> *vector);
			
			QDict<OrbitDataColl> dict;
	};

	static OrbitDataManager odm;

};

#endif
