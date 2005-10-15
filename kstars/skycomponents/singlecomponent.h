/***************************************************************************
                          singlecomponent.h  -  K Desktop Planetarium
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

#ifndef SINGLECOMPONENT_H
#define SINGLECOMPONENT_H

/**
 *@class SingleComponent
 *An abstract parent class, to be inherited by SkyComponents that store a 
 *SkyObject.
 *
 *@author Jason Harris
 *@version 0.1
 */

class SkyMap;

#include "skycomponent.h"

class SingleComponent : public SkyComponent
{
	public:
		SingleComponent(SkyComposite*);
		
		virtual ~SingleComponent();
		
		/**Draw the object on the SkyMap*/
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

		SkyObject* skyObject() { return StoredObject; }

	private:
		SkyComposite *Parent;
		SkyObject* StoredObject;
		bool HasTrail;
};

#endif
