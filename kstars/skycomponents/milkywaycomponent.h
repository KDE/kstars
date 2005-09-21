/***************************************************************************
                          milkywaycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/08/08
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

#ifndef MILKYWAYCOMPONENT_H
#define MILKYWAYCOMPONENT_H

#define NMWFILES  11

#include "skycomponent.h"

#include <QList>
//Added by qt3to4:
#include <Q3PointArray>

/**@class MilkyWayComponent
*Represents the milky way.

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class MilkyWayComponent : public SkyComponent
{
	public:
		
		MilkyWayComponent(SkyComposite*);
		
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);
		
	private:
		QList<SkyPoint> MilkyWay[NMWFILES];
		
		// optimization: don't realloc every draw the array
		Q3PointArray *pts;
};

#endif
