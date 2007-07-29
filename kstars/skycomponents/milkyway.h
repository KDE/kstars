/***************************************************************************
                           milkyway.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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

#ifndef MILKYWAY_H
#define MILKYWAY_H

#include "skiplistindex.h"

/**
	*@class MlkyWay
    *This  stores a SkipList for each chunk.  
	*
	*@author Jason Harris
	*@version 0.1
	*/
class MilkyWay : public SkipListIndex
{
	public:
		/**
			*@short Constructor
			*@p parent pointer to the parent SkyComponent
			*/
		MilkyWay( SkyComponent *parent );

        void init( KStarsData *data );

		/**
			*@short Draw the Milky Way on the sky map
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		void draw( KStars *ks, QPainter& psky, double scale );
       
		void draw( QPainter& psky );

        void update( KStarsData *data, KSNumbers *num );

        bool selected();
};
#endif
