/***************************************************************************
                          skycomponent.h  -  K Desktop Planetarium
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

#ifndef SKYCOMPONENT_H
#define SKYCOMPONENT_H

class GeoLocation;
class SkyMap;
class KStarsData;

/**@class SkyComponent
*SkyComponent represents an object on the sky map. This may be a star, a planet or
*the equator too.
*The SkyComponent uses the composite design pattern. So if a new object is needed,
*it has to be derived from this class and must be added into the hierarchy of components.
*
*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class SkyComponent
{
	public:
	
		SkyComponent(SkyComposite*);
		
		virtual ~SkyComponent();
		
		/**Draw the object on the SkyMap*/
		virtual void draw(SkyMap *map, QPainter& psky, double scale) {};
		
		/**Draw the object, if it is exportable to an image
		*@see isExportable()
		*/
		void drawExportable(SkyMap *map, QPainter& psky, double scale);
		
		/**Initialize the component - loading data from file etc.*/
		// TODO pass a splashscreen as parameter to init, so it can update the splashscreen -> should use the new KSplashScreen?
		virtual void init() {};
		
		/**Update the component with new data*/
		virtual void updateTime(KStarsData*, KSNumbers*, bool needNewCoords) {};
	
		/**The parent of a component may be a composite or nothing.
		*It's useful to know it's parent, if a component want to add
		*a component to it's parent, for example a star want to add/remove
		*a trail to it's parent.
		*/
		void parent() { return Parent; }
		
	protected:

		/**Is the object exportable to the sky map?
		*Overwrite the implementation, if the object should not exported.
		*Default: object is exportable
		*/
		virtual bool isExportable();
		
	private:
		SkyComposite *Parent;
};

#endif
