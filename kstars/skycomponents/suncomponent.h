/***************************************************************************
                          suncomponent.h  -  K Desktop Planetarium
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

#ifndef SUNCOMPONENT_H
#define SUNCOMPONENT_H

/**
	*@class SunComponent
	*Represents the Sun on the sky map
	*
	*@author Jason Harris
	*@version 0.1
	*/

#include "solarsystemsinglecomponent.h"

class SunComponent : public SolarSystemSingleComponent
{
	public:

	/**
		*@short Default constructor.
		*@p parent pointer to the parent SolarSystemComposite
		*@p visibleMethod 
		*@p msize
		*/
		SunComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize = 2);
		
	/**
		*Destructor.  Delete all list members
		*/
		virtual ~SunComponent();

	/**
		*@short Draw the Sun onto the skymap
		*@p ks pointer to the KStars object
		*@p psky reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Initialize the Sun.
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	/**
		*@short Update the position of the Sun
		*@p data Pointer to the KStarsData object
		*@p data Pointer to the KSNumbers object
		*@p needNewCoords set to true if the Sun needs its position recomputed
		*/
		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);

	private:
		KSSun *sun;
		bool HasTrail;
};
