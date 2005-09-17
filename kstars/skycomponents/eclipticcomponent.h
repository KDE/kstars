/***************************************************************************
                          eclipticcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16 Sept. 2005
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

#ifndef ECLIPTICCOMPONENT_H
#define ECLIPTICCOMPONENT_H

class QList;

/**@class EclipticComponent
*Represents the celestial equator on the sky map.

*@author Jason Harris
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class EclipticComponent: public SkyComponent
{
	public:

		EclipticComponent(SkyComposite*);
		
		virtual ~EclipticComponent();

		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:
		// the points of the equator
		QList<SkyPoint*> Ecliptic;
		
		QPointArray *pts;

};

#endif
