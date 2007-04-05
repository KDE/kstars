/***************************************************************************
                          planetcatalog.h  -  description
                             -------------------
    begin                : Mon Feb 18 2002
    copyright          : (C) 2002 by Mark Hollomon
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

#ifndef PLANETCATALOG_H
#define PLANETCATALOG_H

/**@class PlanetCatalog
	*This class contains a QPtrList of the eight major planets, as well as pointers
	*to objects representing the Earth and Sun.  Note that the Sun also exists
	*in the QPtrList, the external pointer is just for convenience.
	*There are methods to search
	*the collection by name, identify if a given object pointer is a planet,
	*find the coordinates of a planet, and find the planet closest to a given
	*SkyPoint.
	*@short the collection of planet objects.
	*@author Mark Hollomon
	*@version 1.0
	*/

#include <qglobal.h>
#include <qobject.h>
#include <qptrlist.h>

class QString;
class KStarsData;
class KSNumbers;
class KSPlanetBase;
class KSPlanet;
class KSSun;
class SkyPoint;
class SkyObject;
class ObjectNameList;
class dms;

class PlanetCatalog : public QObject {
	Q_OBJECT

	public:
	/**Constructor. */
		PlanetCatalog(KStarsData *dat);

	/**Destructor. Delete the Earth object (all others auto-deleted by QPtrList)*/
		~PlanetCatalog();

	/**Loads all planetary data from files on disk into the appropriate objects. */
		bool initialize();

	/**Add pointers to the planetary objects to the ObjNames list. 
		*@p ObjNames the list of all named objects to which we will add the planets.
		*/
		void addObject( ObjectNameList &ObjNames ) const;

	/**Determine the coordinates for all of the planets
		*@param num pointer to a ksnumbers object for the target date/time
		*@param lat pointer to the geographic latitude
		*@param LST pointer to the local sidereal time
		*/
		void findPosition( const KSNumbers *num, const dms *lat, const dms *LST );

	/**@return pointer to the Sun. */
		const KSSun *planetSun() const { return Sun; }

	/**@return pointer to the Earth. (must not be const because we call findPosition on it in KSPlanetBase::updateCoords() )*/
		KSPlanet *earth() { return Earth; }

	/**Compute the present Horizontal coordinates of all planets. 
		*@p LST pointer to the current local sidereal time
		*@p lat pointer to the current geographic latitude
		*/
		void EquatorialToHorizontal( dms *LST, const dms *lat );

	/**@return true if the SkyObject argument is a planet. 
		*@p so pointer to the SkyObject to be tested
		*/
		bool isPlanet(SkyObject *so) const;

	/**@return a pointer to the KSPlanetBase of the planet named in the argument.
		*@p n the name of the planet to point to
		*@note if no planet with this name is found, return the NULL pointer.
		*/
		KSPlanetBase *findByName( const QString n) const;

	/**@return a pointer to the planet closest to the given SkyPoint 
		*(within a maximum angular search radius) 
		*@p p the Sky point to find a planet near
		*@p r the maximum angular search radius
		*/
		SkyObject *findClosest(const SkyPoint *p, double &r) const;

	private:
		QPtrList<KSPlanetBase> planets;
		KSPlanet *Earth;
		KSSun *Sun;
		KStarsData *kd;
};

#endif
