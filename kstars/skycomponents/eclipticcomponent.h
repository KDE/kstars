//Added by qt3to4:
#include <Q3PointArray>
/***************************************************************************
                          eclipticcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16 Sept. 2005
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

#ifndef ECLIPTICCOMPONENT_H
#define ECLIPTICCOMPONENT_H

class QList;

/**
	*@class EclipticComponent
	*Represents the celestial equator on the sky map.
	
	*@author Jason Harris
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class EclipticComponent: public PointListComponent
{
	public:

		/**
		 *@short Constructor
		 *@p parent pointer to the parent SkyComposite object
		 */
		EclipticComponent(SkyComposite *parent, bool (*visibleMethod)());

		/**
		 *@short Destructor
		 */
		virtual ~EclipticComponent();

		/**
		 *@short draw the Ecliptic onto the sky
		 *@p ks pointer to the KStars object
		 *@p psky reference to the QPainter on which to draw
		 *@p scale the scaling factor (1.0 for screen draws)
		 */
		virtual void draw(KStars *ks, QPainter& psky, double scale);

		/**
		 *@short Initialize the Ecliptic
		 *@p data pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);

};

#endif
