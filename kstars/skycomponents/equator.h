/***************************************************************************
                          equator.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EQUATOR_H
#define EQUATOR_H

#include "labeledlistindex.h"

class KStarsData;

/**
	*@class Equator
	*Represents the equator on the sky map.
	
	*@author James B. Bowlin
	*@version 0.1
	*/
class Equator : public LabeledListIndex
{
	public:

		/* @short Constructor
		 * @p parent pointer to the parent SkyComponent object
		 * name is the name of the subclass
		 */
		Equator( SkyComponent *parent );

		void preDraw( KStars *kstars, QPainter& psky );

		/**@short Initialize the Equator
		 *@p data pointer to the KStarsData object
		 */
		virtual void init( KStarsData *data );

        bool selected();
};

#endif
