/***************************************************************************
                          constellationboundarycomponent.h  -  K Desktop Planetarium
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

#ifndef CONSTELLATIONBOUNDARYCOMPONENT_H
#define CONSTELLATIONBOUNDARYCOMPONENT_H

#include "skycomponent.h"

/**
	*@class ConstellationBoundaryComponent
	*Represents the constellation lines on the sky map.

	*@author Jason Harris
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

#include <QList>
#include <QChar>
#include "csegment.h"

class ConstellationBoundaryComponent : public SkyComponent
{
	public:
		
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComposite object
		*/
		ConstellationBoundaryComponent(SkyComposite *parent);
	/**
		*@short Destructor.  Delete list members
		*/
		~ConstellationBoundaryComponent();
		
	/**
		*@short Draw constellation boundaries on the sky map.
		*@p ks pointer to the KStars object
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Initialize the Constellation boundary component
		*Reads the constellation boundary data from cbounds.dat
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);
	
	/**
		*@short Update the current positions of the constellation boundaries
		*@p data Pointer to the KStarsData object
		*@p num Pointer to the KSNumbers object
		*@p needNewCoords set to true if objects need their positions recomputed
		*/
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:

		QList<CSegment*> csegmentList;
};

#endif
