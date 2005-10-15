/***************************************************************************
                          solarsystemcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/01/09
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SOLARSYSTEMCOMPOSITE_H
#define SOLARSYSTEMCOMPOSITE_H

#include "skycomposite.h"

/**@class SolarSystemComposite
* The solar system composite manages all planets, asteroids and comets.
* As every sub component of solar system needs the earth , the composite
* is managing this object by itself.
*
*@author Thomas Kabelmann
*@version 0.1
*/

class SolarSystemComposite : SkyComposite
{
	public:
		SolarSystemComposite(SkyComposite*);
		
		virtual ~SolarSystem();
		
		KSPlanet* earth() { return Earth; }
		
//		KSPlanet* sun() { return Sun->data(); }

		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);
		
		virtual void updatePlanets(KStarsData*, KSNumbers*, bool needNewCoords);
		
		virtual void updateMoons(KStarsData *data, KSNumbers *num, bool needNewCoords);

		virtual void draw(SkyMap *map, QPainter& psky, double scale);
		
		/**
		 *@short Add a Trail to the specified SkyObject.
		 *@p o Pointer to the SkyObject to which a Trail will be added.
		 */
		bool addTrail( SkyObject *o );

	private:
		KSPlanet *Earth;
		
		/** Sun, inner planets and moon are a little bit tricky.
		  * We have to care about the order of drawing them, so we
		  * don't put them to the other components.
		  */
		SunComponent *Sun;
		PlanetComponent *Venus;
		PlanetComponent *Mercury;
		MoonComponent *Moon;
};

#endif
