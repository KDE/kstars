/***************************************************************************
                          coordinategridcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/08
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

#ifndef COORDINATEGRIDCOMPONENT_H
#define COORDINATEGRIDCOMPONENT_H

#include "skycomponent.h"

/**
	*@class CoordinateGridComponent
	*Represents one circle on the coordinate grid

	*@author Thomas Kabelmann
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;

class CoordinateGridComponent : public PointListComponent
{
	public:
		
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		CoordinateGridComponent( SkyComponent*, bool (*visibleMethod)(), bool isParallel, double coord );
	/**
		*@short Destructor.  Delete list members
		*/
		~CoordinateGridComponent();
		
	/**
		*@short Draw constellation names on the sky map.
		*@p ks pointer to the KStars object
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw(KStars *ks, QPainter& psky, double scale);

	/**
		*@short Initialize the Constellation names component
		*Reads the constellation names data from cnames.dat
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);

	private:
		double Coordinate;
		bool Parallel;
};

#endif
