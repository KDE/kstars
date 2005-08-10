/***************************************************************************
                          horizoncomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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

#ifndef HORIZONCOMPONENT_H
#define HORIZONCOMPONENT_H

class QList;

/**@class HorizonComponent
*Represents the horizon on the sky map.

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class HorizonComponent: SkyComponent
{
	public:

		HorizonComponent(SkyComposite*);
		
		virtual ~HorizonComponent();

		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init();
	
		virtual void updateTime(KStarsData*, KSNumbers*, bool needNewCoords);

	private:
		// the points of the horizon
		QList<SkyPoint*> Horizon;
		
		QPointArray *pts;

};

#endif
