/***************************************************************************
                          planetcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/24/09
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

#ifndef PLANETCOMPONENT_H
#define PLANETCOMPONENT_H

#include "solarsystemsinglecomponent.h"

class KSPlanet;

/**
	*@class PlanetComponent
	*This class encapsulates a single major planet, such as Mars.
	*
	*@author Jason Harris
	*@version 0.1
	*/
class PlanetComponent : public SolarSystemSingleComponent
{
	public:
	/**
		*@short Constructor.
		*@p parent pointer to the parent SolarSystemComposite
		*@p ksp pointer to the KSPlanetBase object encapsulating the planet
		*@p visibleMethod 
		*@p msize
		*/
		PlanetComponent( SolarSystemComposite *parent, KSPlanetBase *ksp, bool (*visibleMethod)(), int msize = 2 );

	/**
		*Destructor.  Delete KSPlanet member
		*/
		~PlanetComponent();

	/**
		*@short Draw the planet onto the skymap
		*@p ks pointer to the KStars object
		*@p psky reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Initialize the planet
		*Reads in the orbital data from data files.
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	/**
		*@short Update the positions of planets and solar system bodies
		*@p data Pointer to the KStarsData object
		*@p data Pointer to the KSNumbers object
		*@p needNewCoords set to true if objects need their positions recomputed
		(JH: Do we need this function?  How is it different from update()? )
		*/
		virtual void updatePlanets(KStarsData *data, KSNumbers *num, bool needNewCoords);
		
	/**
		*@short Update the position of the planet
		*@p data Pointer to the KStarsData object
		*@p data Pointer to the KSNumbers object
		*@p needNewCoords set to true if objects need their positions recomputed
		*/
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

		/**
			*@short Add a Trail to the specified SkyObject.
			*
			*@p o Pointer to the SkyObject to which a Trail will be added
			*@return true if this is the specified object (a Trail is added 
			*in this case); otherwise return false
			*/
		bool addTrail( SkyObject *o );
		bool removeTrail( SkyObject *o );

		KSPlanet* planet() { return p; }

	private:
		KSPlanet *p;
		bool HasTrail;
};
