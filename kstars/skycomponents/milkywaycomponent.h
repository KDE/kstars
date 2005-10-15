/***************************************************************************
                          milkywaycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/08/08
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

#ifndef MILKYWAYCOMPONENT_H
#define MILKYWAYCOMPONENT_H

#define NMWFILES  11

#include "skycomponent.h"

#include <QList>
//Added by qt3to4:
#include <Q3PointArray>

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

/**
	*@class MilkyWayComponent
	*Represents the milky way contour.
	
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class MilkyWayComponent : public SkyComponent
{
	public:
		
		/**
		 *@short Constructor
		 *@p parent pointer to the parent SkyComposite
		 */
		MilkyWayComponent(SkyComposite*);

		/**
		 *@short Destructor
		 */
		~MilkyWayComponent();

		/**
		 *@short Draw the Milky Way on the sky map
		 *@p map Pointer to the SkyMap object
		 *@p psky Reference to the QPainter on which to paint
		 *@p scale the scaling factor for drawing (1.0 for screen draws)
		 */
		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		/**
		 *@short Initialize the Milky Way
		 *@p data Pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);
	
		/**
		 *@short Update the coordinates of the Milky Way
		 *@p data Pointer to the KStarsData object
		 *@p num Pointer to the KSNumbers object
		 *@p needNewCoords true if cordinates should be recomputed
		 */
		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);
		
	private:
		QList<SkyPoint> MilkyWay[NMWFILES];
		
		// optimization: don't realloc every draw the array
		Q3PointArray *pts;
};

#endif
