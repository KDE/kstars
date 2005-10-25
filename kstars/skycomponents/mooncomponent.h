/***************************************************************************
                          mooncomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/09/06
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

#ifndef MOONCOMPONENT_H
#define MOONCOMPONENT_H

class SolarSystemComposite;
class KSNumbers;
class SkyMap;
class KStarsData;
class KSMoon;

#include "solarsystemsinglecomponent.h"

/**
	*@class MoonComponent
	* Represents the moon.
	
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class MoonComponent : SolarSystemSingleComponent
{
	public:
		/**
		 *@short Constructor.
		 *@p parent pointer to the parent SolarSystemComposite
		 *@p visibleMethod pointer to the function which returns whether the Moon 
		 *should be drawn
		 *@p msize
		 */
		MoonComponent( SolarSystemComposite *parent, bool (*visibleMethod)(), int msize=8 );
		
		/**
		 *Destructor.
		 */
		virtual ~MoonComponent();
		
		/**
		 *@short draw the Moon onto the skymap
		 *@p ks pointer to the KStars object
		 *@p psky reference to the QPainter on which to paint
		 *@p scale scaling factor (1.0 for screen draws)
		 */
		virtual void draw( KStars *ks, QPainter& psky, double scale );
		
		/**
		 *@short Initialize the moon; read orbital data from disk
		 *@p data pointer to the KStarsData object
		 */
		virtual void init(KStarsData *data);
		
		/**
		 *@short Update the position of the Moon
		 *@p data pointer to the KStarsData object
		 *@p num pointer to the KSNumbers object
		 */
		virtual void updateMoons( KStarsData *data, KSNumbers *num );
	
	
	private:
	
		KSMoon *Moon;
};

#endif
