/***************************************************************************
                          plutocomponent.h  -  K Desktop Planetarium
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

#ifndef PLUTOCOMPONENT_H
#define PLUTOCOMPONENT_H

#include "kspluto.h"
#include "solarsystemsinglecomponent.h"

/**
	*@class PlutoComponent
	*This class encapsulates the Pluto
	*
	*@author Jason Harris
	*@version 0.1
	*/
class PlutoComponent : public SolarSystemSingleComponent
{
	public:

	/**
		*@short Default constructor.
		*@p parent pointer to the parent SolarSystemComposite
		*/
		PlutoComponent( SolarSystemComposite *parent );
		
	/**
		*Destructor.  Delete KSPluto member
		*/
		virtual ~PlutoComponent();

	/**
		*@short Draw Pluto onto the skymap
		*@p ks pointer to the KStars object
		*@p psky reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Initialize Pluto's orbital data.
		*Reads in the orbital data from files.
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	/**
		*@short Update the positions of planets and solar system bodies
		*@p data Pointer to the KStarsData object
		*@p data Pointer to the KSNumbers object
		*@p needNewCoords set to true if objects need their positions recomputed
		*/
		virtual void updatePlanets(KStarsData *data, KSNumbers *num, bool needNewCoords);
		
	/**
		*@short Update the position
		*@p data Pointer to the KStarsData object
		*@p data Pointer to the KSNumbers object
		*@p needNewCoords set to true if Pluto needs its position recomputed
		*/
		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);

		/** from PlanetHelper */
		virtual void drawTrail(SkyMap *map, QPainter& psky, double scale);
		
	private:
		KSPluto *pluto;
		bool HasTrail;
};
