/***************************************************************************
                          solarsystemsinglecomponent.h  -  K Desktop Planetarium
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

#ifndef SOLARSYSTEMSINGLECOMPONENT_H
#define SOLARSYSTEMSINGLECOMPONENT_H

/**
	*@class SolarSystemSingleComponent
	*This class encapsulates some methods which are shared between
	*all single-object solar system components (Sun, Moon, Planet, Pluto)
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/

#include "singlecomponent.h"

class QColor;

class SolarSystemComposite;
class KSNumbers;
class KSPlanet;
class KSPlanetBase;
class KStarsData;
class SkyMap;

class SolarSystemSingleComponent : public SingleComponent
{
	public:
		/** Initialize visible method, minimum size and sizeScale. */
		SolarSystemSingleComponent(SolarSystemComposite*, KSPlanet *earth, bool (*visibleMethod)(), int msize);

		/** Set the size scale. Default value is 1.0 and only
		* Saturn uses a scale of 2.5.
		*(JH: this should be in PlanetComponent, no?)
		*/
		void setSizeScale(float scale);
		
	protected:
		
		KSPlanet* earth() { return m_Earth; }
		
		/** 
			*@short Draws the planet's trail, if necessary.
			*/
		void drawTrail(KStars *ks, QPainter& psky, KSPlanetBase *ksp, double scale);
		
		//FIXME: try to remove color (DONE), zoommin, and resize_mult args to match baseclass draw declaration
		//FIXME: Move to child classes (SUn, Moon, Planet, Pluto)
		void draw(KStars *ks, QPainter &psky, KSPlanetBase *p, QColor c, double zoommin, int resize_mult, double scale);

		// calculate the label size for drawNameLabel()
		virtual int labelSize(SkyObject*);

		// use the function pointer to call the Option::XXX() method passed in ctor - this method is then called visible()
		bool (*visible)();
		
	private:

		// minimum size for drawing name labels
		int minsize;
		// scale for drawing name labels
		// only Saturn has a scale of 2.5
		float sizeScale;

		QColor m_Color;
		KSPlanet *m_Earth;
};

#endif
