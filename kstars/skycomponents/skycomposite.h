/***************************************************************************
                          skycomposite.h  -  K Desktop Planetarium
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

#ifndef SKYCOMPOSITE_H
#define SKYCOMPOSITE_H

#include "skycomponent.h"

class QList;
class SkyMap;
class KStarsData;
class GeoLocation;

/**@class SkyComposite
*SkyComposite is a complex component, which just delegates all requests to
*it's sub components. Sub components may be SkyComponents or SkyComposites.
*The SkyComposite uses the composite design pattern.
*
*Performance tuning: Sometimes it will be better to overwrite a virtual function and do
*the work in the composite instead of delegating the request to all sub components.
*@author Thomas Kabelmann
*@version 0.1
*/

class QList;
class KStarsData;
class SkyMap;
class KSNumbers;

class SkyComposite : SkyComponent
{
	public:
		SkyComposite(SkyComposite*);
		
		virtual ~SkyComposite();
		
		/**Delegates draw requests to it's sub components*/
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		/**Delegates drawExportable requests to it's sub components*/
		virtual void drawExportable(SkyMap *map, QPainter& psky, double scale);
		
		/**Delegates init requests to it's sub components*/
		virtual void init();
	
		/**Delegates updateTime requests to it's sub components*/
		virtual void updateTime(KStarsData *data, KSNumbers*, bool needNewCoords);
	
		/**Add a new sub component to the composite*/
		void addComponent(SkyComponent*);
		
		/**Remove a sub component from the composite*/
		void removeComponent(SkyComponent*);
		
	protected:
		
		/**Returns the list of components (for internal use)*/
		QList<SkyComponent*>* components();
		
	private:
		/**All sub components are stored here*/
		QList<SkyComponent*> *Components;
};

#endif
