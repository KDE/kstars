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

/**
	*@class ConstellationNamesComponent
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

class ConstellationNamesComponent : public ListComponent
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComposite object
		*/
		ConstellationNamesComponent(SkyComposite *parent, bool (*visibleMethod)());
	/**
		*@short Destructor.  Delete list members
		*/
		~ConstellationNamesComponent();
		
	/**
		*@short Draw constellation names on the sky map.
		*@p ks pointer to the KStars object
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Initialize the Constellation names component
		*Reads the constellation names data from cnames.dat
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);

};

#endif
