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

#include <qstring.h>
#include "ksplanetbase.h"
#include "ksplanet.h"
#include "kssun.h"
#include "skyobjectname.h"
#include "objectnamelist.h"

/**This class contains a QList of the eight major planets, as well as pointers
	*to objects representing the Earth and Sun.  Note that the Sun also exists
	*in the QList, the external pointer is just for convenience.
	*There are methods to search
	*the collection by name, identify if a given object pointer is a planet,
	*find the coordinates of a planet, and find the planet closest to a given
	*SkyPoint.
	*@short the collection of planet objects.
	*@author Mark Hollomon
	*@version 0.9
 */

#include <qglobal.h>
#if (QT_VERSION > 299)
#include <qptrlist.h>
#else
#include <qlist.h>
#endif

class KStars;

class PlanetCatalog : public QObject {
	Q_OBJECT

	public:
	/**Constructor. */
		PlanetCatalog(KStars *ks);

	/**Destructor. Delete the Earth object (all others auto-deleted by QList)*/
		~PlanetCatalog();

	/**Loads all planetary data from files on disk into the appropriate objects. */
		bool initialize();

	/**Add pointers to the planetary objects to the ObjNames list. */
		void addObject( ObjectNameList &ObjNames ) const;

	/**Determine the coordinates for all of the planets, given a KSNumbers object
		*appropriate for the desired DateTime. */
		void findPosition( const KSNumbers *num);

	/**@returns pointer to the Sun. */
		const KSSun *planetSun() const { return Sun; };

	/**@returns pointer to the Earth. (must not be const because we call findPosition on it in KSPlanetBase::updateCoords() )*/
		KSPlanet *earth() { return Earth; };

	/**Compute the present Horizontal coordinates of all planets. */
		void EquatorialToHorizontal( const dms LST, const dms lat );

	/**@returns true if the SkyObject argument is a planet. */
		bool isPlanet(SkyObject *so) const;

	/**@returns a pointer to the KSPlanetBase of the planet named in the argument.*/
		KSPlanetBase *findByName( const QString n) const;

	/**@returns a pointer to the planet closest to the given SkyPoint (within max. radius r) */
		SkyObject *findClosest(const SkyPoint *p, double &r) const;

	private:
		QList<KSPlanetBase> planets;
		KSPlanet *Earth;
		KSSun *Sun;
		KStars *kstars;
};

#endif
