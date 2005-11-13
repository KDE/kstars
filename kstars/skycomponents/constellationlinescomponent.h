/***************************************************************************
                          constellationlinescomponent.h  -  K Desktop Planetarium
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

#ifndef CONSTELLATIONLINESCOMPONENT_H
#define CONSTELLATIONLINESCOMPONENT_H

#include "pointlistcomponent.h"

/**
	*@class ConstellationLinesComponent
	*Represents the constellation lines on the sky map.

	*@author Thomas Kabelmann
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

#include <QList>
#include <QChar>

class ConstellationLinesComponent : public PointListComponent
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		ConstellationLinesComponent(SkyComponent*, bool (*visibleMethod)());
	/**
		*@short Destructor.  Delete list members
		*/
		~ConstellationLinesComponent();
		
	/**
		*@short Draw constellation lines on the sky map.
		*@p ks pointer to the KStars object
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Initialize the Constellation lines component
		*
		*Reads the constellation lines data from clines.dat.
		*Each line in the file contains a command character ("M" means move to 
		*this position without drawing a line, "D" means draw a line from 
		*the previous position to this one), followed by the genetive name of 
		*a star, which marks the position of the constellation node.
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	private:

		QList<QChar> m_CLineModeList;

};

#endif
