//Added by qt3to4:
#include <Q3PointArray>
/***************************************************************************
                          equatorcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 10 Sept. 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef EQUATORCOMPONENT_H
#define EQUATORCOMPONENT_H

class QList;

/**@class EquatorComponent
*Represents the celestial equator on the sky map.

*@author Jason Harris
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class EquatorComponent: public SkyComponent
{
	public:

		EquatorComponent(SkyComposite*);
		
		virtual ~EquatorComponent();

		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:
		// the points of the equator
		QList<SkyPoint*> Equator;
		
		Q3PointArray *pts;

};

#endif
