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

#include <QColor>

/**
	*@class SolarSystemSingleComponent
	*This class encapsulates some methods which are shared between
	*all single-object solar system components (Sun, Moon, Planet, Pluto)
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/

#include "singlecomponent.h"

class SolarSystemComposite;
class KSNumbers;
class KSPlanet;
class KSPlanetBase;
class KStarsData;
class SkyLabeler;

class SolarSystemSingleComponent : public SingleComponent
{
	public:
		/** Initialize visible method, minimum size and sizeScale. */
		SolarSystemSingleComponent(SolarSystemComposite*, KSPlanetBase *kspb, bool (*visibleMethod)(), int msize);

		~SolarSystemSingleComponent();

		/**
			*@short Initialize the solar system body
			*Reads in the orbital data from data files.
			*@p data Pointer to the KStarsData object
			*/
			virtual void init(KStarsData *data);
	
		virtual void update( KStarsData *data, KSNumbers *num );

		/**
			*@short Update the coordinates of the planet.
			*
			*This function updates the position of the moving solar system body.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*/
		virtual void updatePlanets( KStarsData *data, KSNumbers *num );

		void draw( KStars *ks, QPainter &psky, double scale );

		/**
			*@short Set the size scale. Default value is 1.0 and only
			*Saturn uses a scale of 2.5.
			*/
		void setSizeScale(float scale);
		
	protected:
		
		KSPlanet* earth() { return m_Earth; }
		
		KSPlanetBase *ksp() { return (KSPlanetBase*)skyObject(); }

		/** 
			*@short Draws the planet's trail, if necessary.
			*/
		void drawTrails( KStars *ks, QPainter& psky, double scale );
		
	/**
		*@short Add a Trail to the specified SkyObject.
		*@p o Pointer to the SkyObject to which a Trail will be added
		*/
		virtual bool addTrail( SkyObject *o );

	/**
		*@return true if the specified SkyObject is a member of this component, and it contains a Trail.
		*@p o Pointer to the SkyObject to which a Trail will be added
		*/
		virtual bool hasTrail( SkyObject *o, bool& found );
		virtual bool removeTrail( SkyObject *o );
		virtual void clearTrailsExcept( SkyObject *exOb );

	private:

		// minimum size for drawing name labels
		int minsize;
		// scale for drawing name labels
		// only Saturn has a scale of 2.5
		float sizeScale;

		QColor m_Color;
		KSPlanet *m_Earth;
        SkyLabeler* m_skyLabeler;
};

#endif
