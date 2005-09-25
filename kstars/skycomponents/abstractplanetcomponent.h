/***************************************************************************
                          abstractplanetcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

#ifndef ABSTRACTPLANETCOMPONENT_H
#define ABSTRACTPLANETCOMPONENT_H

/**
	*@class AbstractPlanetComponent
	*This class encapsulates some methods which are shared between
	*all classes derived from KSPlanetBase (like asteroids, planets and comets).
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/

#include "skycomponent.h"

class SolarSystemComposite;
class KStarsData;
class SkyMap;
class KSNumbers;
class KSPlanet;

class AbstractPlanetComponent : public SkyComponent
{
	public:
		/** Initialize visible method, minimum size and sizeScale. */
		AbstractPlanetComponent(SolarSystemComposite*, KSPlanet *earth, bool (*visibleMethod)(), int msize);

		/** Set the size scale. Default value is 1.0 and only
		* Saturn uses a scale of 2.5.
		*(JH: this should be in PlanetComponent, no?)
		*/
		void setSizeScale(float scale);
		
		/** Draws the trail. This method is abstract, so you have to
		* implement your own draw method calling drawPlanetTrail with
		* the corresponding planet.
		* (JH: I don't think this needs to be abstract...)
		*/
		virtual void drawTrail(SkyMap *map, QPainter& psky, double scale) = 0;
		
	protected:
	
		// we are sure that only a solarsystemcomposite is the parent (see ctor)
		KSPlanet* earth() { ((SolarSystemComposite*)parent)->earth(); }
		
		/** Draws the trail of an planet base object. */
		void drawPlanetTrail(SkyMap *map, QPainter& psky, KSPlanetBase *ksp, double scale);
		
		void drawPlanet(SkyMap *map, QPainter &psky, KSPlanetBase *p, QColor c, double zoommin, int resize_mult, double scale);

		// calculate the label size for drawNameLabel()
		virtual int labelSize(SkyObject*);

		// use the function pointer to call the Option::XXX() method passed in ctor - this method is then called visible()
		bool (*visible)();
		
		// store pointer to earth method
		KSPlanet* (*earth)();
		
	private:

		// minimum size for drawing name labels
		int minsize;
		// scale for drawing name labels
		// only Saturn has a scale of 2.5
		float sizeScale;
};

#endif
