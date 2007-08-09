/***************************************************************************
                          ecliptic.h  -  K Desktop Planetarium
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

#ifndef ECLIPTIC_H
#define ECLIPTIC_H

#include "labeledlistindex.h"

class KStarsData;

/**
	*@class Ecliptic
	*Represents the ecliptic on the sky map.
	
	*@author James B. Bowlin
	*@version 0.1
	*/
class Ecliptic : public LabeledListIndex
{
	public:

		/* @short Constructor
		 * @p parent pointer to the parent SkyComponent object
		 * name is the name of the subclass
		 */
		Ecliptic( SkyComponent *parent );

		void preDraw( KStars *kstars, QPainter& psky );

		/**@short Initialize the Ecliptic
		 *@p data pointer to the KStarsData object
		 */
		virtual void init( KStarsData *data );

        bool selected();
};

#endif
