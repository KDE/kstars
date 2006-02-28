/***************************************************************************
                          telescopecomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2006/02/26
    copyright            : (C) 2006 by Jasem Mutlaq
    email                : mutlaqja@ikarusteh.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TELESCOPECOMPONENT_H
#define TELESCOPECOMPONENT_H

/**@class TelescopeComponent
*Represents the telescopes markers on the sky.

*@author Jasem Mutlaq
*@version 0.1
*/

#include "listcomponent.h"

class SkyComponent;
class KStars;
class KStarsData;
class SkyMap;
class KSNumbers;
class SkyObject;

class TelescopeComponent: public ListComponent
{
	public:

		TelescopeComponent(SkyComponent*, bool (*visibleMethod)());
		
		virtual ~TelescopeComponent();

		virtual void TelescopeComponent::init(KStarsData *data);

		virtual void draw(KStars *ks, QPainter& psky, double scale);

		virtual void update( KStarsData*, KSNumbers* );

		virtual void addTelescopeMarker(SkyObject *o);
		virtual void removeTelescopeMarker(SkyObject *o);

};

#endif
