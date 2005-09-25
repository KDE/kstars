/***************************************************************************
                          asteroidscomponent.h  -  K Desktop Planetarium
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

#ifndef ASTEROIDSCOMPONENT_H
#define ASTEROIDSCOMPONENT_H

/**@class AsteroidsComponent
	*Represents the asteroids on the sky map.

	*@author Thomas Kabelmann
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

//FIXME: JH: this file doesn't exist (yet? or is the include outdated?)
#include "abstractplanetcomponent.h"

#include <QList>

class AsteroidsComponent: public AbstractPlanetComponent
{
	public:

	/**
		*@short Default constructor.
		*@p parent pointer to the parent SolarSystemComposite
		*@p visibleMethod 
		*@p msize
		*/
		AsteroidsComponent(SolarSystemComposite *parent, bool (*visibleMethod)(), int msize = 2);
		
	/**
		*Destructor.  Delete all list members
		*/
		virtual ~AsteroidsComponent();

	/**
		*@short Draw the asteroids onto the skymap
		*@p map pointer to the SkyMap widget
		*@p psky reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

	/**
		*@short Initialize the asteroids list.
		*Reads in the asteroids data from the asteroids.dat file.
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
		*@short Update the positions of list members
		*@p data Pointer to the KStarsData object
		*@p data Pointer to the KSNumbers object
		*@p needNewCoords set to true if objects need their positions recomputed
		*/
		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);

		/** from PlanetHelper */
		virtual void drawTrail(SkyMap *map, QPainter& psky, double scale);
		
	private:
		/**
			*@short Utility function for reading the asteroids.dat file
			*Populate the list of Asteroids from the data file.
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
		void readAsteroidData(KStarsData *data);
		
		QList<KSAsteroid*> asteroidList;
};

#endif
