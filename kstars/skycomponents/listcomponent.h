/***************************************************************************
                          listcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/01
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

#ifndef LISTCOMPONENT_H
#define LISTCOMPONENT_H

/**
 *@class ListComponent
 *An abstract parent class, to be inherited by SkyComponents that store a QList
 *of SkyObjects.
 *
 *@author Jason Harris
 *@version 0.1
 */

class SkyMap;

#include "skycomponent.h"

class ListComponent : public SkyComponent
{
	public:
	
		ListComponent(SkyComposite*);
		
		virtual ~ListComponent();
		
		/**Draw the list of objects on the SkyMap*/
		virtual void draw(SkyMap *map, QPainter& psky, double scale) {};
		
		/**Draw the object, if it is exportable to an image
		*@see isExportable()
		*/
		void drawExportable(SkyMap *map, QPainter& psky, double scale);
		
		/**
		 *@short Add a Trail to the specified SkyObject.
		 *@p o Pointer to the SkyObject to which a Trail will be added
		 *@note This function simply returns false; it is overridden by 
		 *the Solar System components
		 */
		bool addTrail( SkyObject *o );
		bool removeTrail( SkyObject *o );

		QList<SkyObject*>& objectList() { return ObjectList; }
		QList<SkyObject*>& trailList() { return TrailList; }

	private:
		SkyComposite *Parent;
		QList<SkyObject*> ObjectList;
		QList<SkyObject*> TrailList;
};

#endif
