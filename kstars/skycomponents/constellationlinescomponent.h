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

#include "skycomponent.h"

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
#include "skyobject.h"

class ConstellationLinesComponent : public PointListComponent
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComposite object
		*/
		ConstellationLinesComponent(SkyComposite*, bool (*visibleMethod)());
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
		*Reads the constellation lines data from clines.dat
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	private:

		QList<QChar> m_CLineModeList;

};

#endif
