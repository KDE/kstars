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

#include "linelistcomponent.h"

class KStarsData;

/**
	*@class EclipticComponent
	*Represents the celestial equator on the sky map.
	
	*@author Jason Harris
	*@version 0.1
	*/
class EclipticComponent: public LineListComponent
{
	public:

		/**
		 *@short Constructor
		 *@p parent pointer to the parent SkyComponent object
		 */
		EclipticComponent(SkyComponent *parent );

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
		//		virtual void draw(KStars *ks, QPainter& psky, double scale);

		/**
		 *@short Initialize the Ecliptic
		 *@p data pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);

        bool selected();
};

#endif
