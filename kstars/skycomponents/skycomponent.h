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
		virtual void init(KStarsData *data);
		
		/**For optimization there are 3 update methods. Long operations, like moon updates
		  *can be called separately, so it's not necessary to process it every update call.
		  *updatePlanets() is used by planets, comets, ecliptic.
		  *updateMoons() is used by the moon and the jupiter moons.
		  *update() is used by all components and for updating Alt/Az coordinates.
		*/
		virtual void updatePlanets(KStarsData*, KSNumbers*, bool needNewCoords) {};

		virtual void updateMoons(KStarsData*, KSNumbers*, bool needNewCoords) {};

		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords) {};
	
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
		
		/** Draws the label of the object.
		* This is an default implementation and can be overridden when needed.
		* For most cases it's just needed to reimplement the labelSize() method.
		* Currently only stars have an own implementation of drawNameLabel().
		*/
		virtual void drawNameLabel(QPainter &psky, SkyObject *obj, int x, int y, double scale);
		
		/** Returns the size for drawing the label. It's used by generic
		* drawNameLabel() method.
		*/
		virtual int labelSize() { return 1; };

	private:
		SkyComposite *Parent;
};

#endif
