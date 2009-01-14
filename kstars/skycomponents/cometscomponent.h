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

class KStarsData;
class SkyLabeler;

#include "solarsystemlistcomponent.h"
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
    	*@p psky reference to the QPainter on which to paint
    	*/
    virtual void draw( QPainter& psky );

    /**
    	*@short Initialize the asteroids list.
    	*Reads in the asteroids data from the asteroids.dat file.
    	*@p data Pointer to the KStarsData object
    	*
    	*Populate the list of Comets from the data file.
    	*Each line in the data file is parsed as follows:
    	*@li 3-37 Name [string]
    	*@li 38-42 Modified Julian Day of orbital elements [int]
    	*@li 44-53 semi-major axis of orbit in AU [double]
    	*@li 55-64 eccentricity of orbit [double]
    	*@li 66-74 inclination angle of orbit in degrees [double]
    	*@li 76-84 argument of perihelion in degrees [double]
    	*@li 86-94 Longitude of the Ascending Node in degrees [double]
    	*@li 82-93 Date of most proximate perihelion passage (YYYYMMDD.DDD) [double]
        *@li 124-127 Absolute magnitude [float]
        *@li 129-132 Slope parameter [float]
        *
        *@note See KSComet constructor for more details.
    	*/
    virtual void init(KStarsData *data);

};

#endif
