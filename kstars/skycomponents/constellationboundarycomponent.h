/***************************************************************************
                          constellationboundarycomponent.h  -  K Desktop Planetarium
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

#ifndef CONSTELLATIONBOUNDARYCOMPONENT_H
#define CONSTELLATIONBOUNDARYCOMPONENT_H

#include "skycomponent.h"

/**@class ConstellationSegmentComponent
*Represents the constellation lines on the sky map.

*@author Jason Harris
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

#include <QList>
#include <QChar>
#include "csegment.h"

class ConstellationBoundaryComponent : public SkyComponent
{
	public:
		
		ConstellationBoundaryComponent(SkyComposite*);
		~ConstellationBoundaryComponent();
		
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:

		QList<CSegment*> csegmentList;
};

#endif
