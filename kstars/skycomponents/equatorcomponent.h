/***************************************************************************
                          equatorcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 10 Sept. 2005
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

#ifndef EQUATORCOMPONENT_H
#define EQUATORCOMPONENT_H

//Added by qt3to4:
#include <Q3PointArray>

class QList;
class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;


/**
	*@class EquatorComponent
	*Represents the celestial equator on the sky map.
	
	*@author Jason Harris
	*@version 0.1
	*/
class EquatorComponent: public SkyComponent
{
	public:

		/**
		 *@short Constructor
		 *@p parent pointer to the parent SkyComposite
		 */
		EquatorComponent(SkyComposite *parent);

		/**
		 *@short Destructor
		 */
		virtual ~EquatorComponent();

		/**
		 *@short Draw the Equator on the sky map
		 *@p map Pointer to the SkyMap object
		 *@p psky Reference to the QPainter on which to paint
		 *@p scale the scaling factor for drawing (1.0 for screen draws)
		 */
		virtual void draw(KStars *ks, QPainter& psky, double scale);
		/**
		 *@short Initialize the Equator
		 *@p data Pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);
		
		/**
		 *@short Update the coordinates of the Equator
		 *@p data Pointer to the KStarsData object
		 *@p num Pointer to the KSNumbers object
		 *@p needNewCoords true if cordinates should be recomputed
		 */
		virtual void update(KStarsData *data, KSNumbers *num, bool needNewCoords);

	private:
		// the points of the equator
		QList<SkyPoint*> Equator;
		
		Q3PointArray *pts;

};

#endif
