/***************************************************************************
                          jupitermoonscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/13/08
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

#ifndef JUPITERMOONSCOMPONENT_H
#define JUPITERMOONSCOMPONENT_H

class QList;

/**@class JupiterMoonsComponent
*Represents the jupitermoons on the sky map.

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;
class JupiterMoons;

class JupiterMoonsComponent: public SkyComponent
{
	public:

		JupiterMoonsComponent(SkyComposite*);
		
		virtual ~JupiterMoonsComponent();

		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void updateMoons(KStarsData*, KSNumbers*, bool needNewCoords) {};
		
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:

		JupiterMoons *jmoons;
};

#endif
