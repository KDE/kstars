/***************************************************************************
                          mooncomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/06
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

#ifndef MOONCOMPONENT_H
#define MOONCOMPONENT_H

class SolarSystemComposite;
class KSNumbers;
class SkyMap;
class KStarsData;
class KSMoon;

#include "abstractplanetcomponent.h"

/**@class MoonComponent
* Represents the moon.

*@author Thomas Kabelmann
*@version 0.1
*/

class MoonComponent : AbstractPlanetComponent
{
	public:
	
		MoonComponent(SolarSystemComposite*, bool (*visibleMethod)(), int msize = 8);
		
		virtual ~MoonComponent();
		
		virtual void draw(SkyMap *map, QPainter& psky, double scale);
		
		virtual void drawTrail(SkyMap *map, QPainter& psky, double scale);
		
		virtual void init(KStarsData *data);
		
		virtual void updateMoons(KStarsData*, KSNumbers*, bool needNewCoords);

		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);
	
	
	private:
	
		KSMoon *Moon;

		earth()
};

#endif
