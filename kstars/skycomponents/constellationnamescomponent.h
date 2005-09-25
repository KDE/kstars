/***************************************************************************
                          constellationnamescomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/08
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

#ifndef CONSTELLATIONNAMESCOMPONENT_H
#define CONSTELLATIONNAMESCOMPONENT_H

#include "skycomponent.h"

/**@class ConstellationNamesComponent
	*Represents the constellation names on the sky map.

	*@author Thomas Kabelmann
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

#include <QList>
#include "skyobject.h"

class ConstellationNamesComponent : public SkyComponent
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComposite object
		*/
		ConstellationNamesComponent(SkyComposite *parent);
	/**
		*@short Destructor.  Delete list members
		*/
		~ConstellationNamesComponent();
		
	/**
		*@short Draw constellation names on the sky map.
		*@p map pointer to the SkyMap widget
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

	/**
		*@short Initialize the Constellation names component
		*Reads the constellation names data from cnames.dat
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	/**
		*@short Update the current positions of the constellation names
		*@p data Pointer to the KStarsData object
		*@p num Pointer to the KSNumbers object
		*@p needNewCoords set to true if positions need to be recomputed
		*/
		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);
		
	private:
		QList<SkyObject*> cnameList;
};

#endif
