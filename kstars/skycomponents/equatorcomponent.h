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

#include "linelistcomponent.h"

class SkyComposite;
class KStarsData;
class SkyMap;

/**
	*@class EquatorComponent
	*Represents the celestial equator on the sky map.
	
	*@author Jason Harris
	*@version 0.1
	*/
class EquatorComponent: public LineListComponent
{
	public:

		/**
		 *@short Constructor
		 *@p parent pointer to the parent SkyComposite
		 */
		EquatorComponent(SkyComponent *parent );

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
		//		virtual void draw(KStars *ks, QPainter& psky, double scale);
		/**
		 *@short Initialize the Equator
		 *@p data Pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);

        bool selected();
};

#endif
