/***************************************************************************
                          coordinategridcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/08
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

#ifndef COORDINATEGRIDCOMPONENT_H
#define COORDINATEGRIDCOMPONENT_H

#include "skycomponent.h"

/**@class CoordinateGridComponent
*Represents one circle on the coordinate grid

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;

class CoordinateGridComponent : public SkyComponent
{
	public:
		
		CoordinateGridComponent( SkyComposite*, bool isParallel, double coord );
		~CoordinateGridComponent();
		
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);

		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:
		QList<SkyPoint*> gridList;

		double Coordinate;
		bool Parallel;
};
