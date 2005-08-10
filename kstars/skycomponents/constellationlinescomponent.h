/***************************************************************************
                          constellationlinescomponent.h  -  K Desktop Planetarium
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

#ifndef CONSTELLATIONLINESCOMPONENT_H
#define CONSTELLATIONLINESCOMPONENT_H

#include "skycomponent.h"

/**@class ConstellationLinesComponent
*Represents the constellation lines on the sky map.

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

// TODO change QLists into QList* of SkyObject*/QChar*
#include <QList>
#include <QChar>
#include "skyobject.h"

class ConstellationNamesComponent : SkyComponent
{
	public:
		
		ConstellationLinesComponent(SkyComposite*);
		
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init();
	
	private:

		QList<SkyPoint> clineList;

		QList<QChar> clineModeList;

};

#endif
