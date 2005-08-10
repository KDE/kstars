/***************************************************************************
                          constellationnamescomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/08
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

#ifndef CONSTELLATIONNAMESCOMPONENT_H
#define CONSTELLATIONNAMESCOMPONENT_H

#include "skycomponent.h"

/**@class ConstellationNamesComponent
*Represents the constellation names on the sky map.

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

// TODO change cnameList into QList* and the list should be list of type SkyObject*
#include <QList>
#include "skyobject.h"

class ConstellationNamesComponent : SkyComponent
{
	public:
		
		ConstellationNamesComponent(SkyComposite*);
		
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init();
	
		virtual void updateTime(KStarsData*, KSNumbers*, bool needNewCoords);
		
	private:
		QList<SkyObject> cnameList;
};

#endif
