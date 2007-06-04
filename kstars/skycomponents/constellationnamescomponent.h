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

#include "listcomponent.h"

/**
	*@class ConstellationNamesComponent
	*Represents the constellation names on the sky map.

	*@author Thomas Kabelmann
	*@version 0.1
	*/

class KStarsData;

#include <QList>
#include "skyobject.h"

class ConstellationNamesComponent : public ListComponent
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		ConstellationNamesComponent(SkyComponent *parent, bool (*visibleMethod)());
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
		*Each line in the file is parsed according to column position:
		*@li 0-1     RA hours [int]
		*@li 2-3     RA minutes [int]
		*@li 4-5     RA seconds [int]
		*@li 6       Dec sign [char; '+' or '-']
		*@li 7-8     Dec degrees [int]
		*@li 9-10    Dec minutes [int]
		*@li 11-12   Dec seconds [int]
		*@li 13-15   IAU Abbreviation [string]  e.g., 'Ori' == Orion
		*@li 17-     Constellation name [string]
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);

};

#endif
