/***************************************************************************
                          cometscomponent.h  -  K Desktop Planetarium
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

#ifndef COMETSCOMPONENT_H
#define COMETSCOMPONENT_H

class SkyComposite;
class KStars;
class KStarsData;
class KSNumbers;

#include "soalrsystemlistcomponent.h"
#include <QList>

/**
	*@class CometsComponent
	*This class encapsulates the Comets
	*
	*@author Jason Harris
	*@version 0.1
	*/
class CometsComponent : public SolarSystemListComponent
{
	public:

	/**
		*@short Default constructor.
		*@p parent pointer to the parent SolarSystemComposite
		*@p visibleMethod 
		*@p msize
		*/
		CometsComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize = 2);
		
	/**
		*Destructor.  Delete all list members
		*/
		virtual ~CometsComponent();

	/**
		*@short Draw the asteroids onto the skymap
		*@p ks pointer to the KStars object
		*@p psky reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale);

	/**
		*@short Initialize the asteroids list.
		*Reads in the asteroids data from the asteroids.dat file.
		*@p data Pointer to the KStarsData object
		*
		*@short Utility function for reading the asteroids.dat file
		*Populate the list of Comets from the data file.
		*Each line in the data file is parsed as follows:
		*@li 6-23 Name [string]
		*@li 24-29 Modified Julian Day of orbital elements [int]
		*@li 30-39 semi-major axis of orbit in AU [double]
		*@li 41-51 eccentricity of orbit [double]
		*@li 52-61 inclination angle of orbit in degrees [double]
		*@li 62-71 argument of perihelion in degrees [double]
		*@li 72-81 Longitude of the Ascending Node in degrees [double]
		*@li 82-93 Mean Anomaly in degrees [double]
		*@li 94-98 Magnitude [double]
		*/
		virtual void init(KStarsData *data);
	
};
