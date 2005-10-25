/***************************************************************************
                          solarsystemcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/01/09
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

#ifndef SOLARSYSTEMCOMPOSITE_H
#define SOLARSYSTEMCOMPOSITE_H

#include "skycomposite.h"

/**@class SolarSystemComposite
* The solar system composite manages all planets, asteroids and comets.
* As every sub component of solar system needs the earth , the composite
* is managing this object by itself.
*
*@author Thomas Kabelmann
*@version 0.1
*/

class SolarSystemComposite : SkyComposite
{
	public:
		SolarSystemComposite(SkyComponent *parent, KStarsData *data);
		~SolarSystemComposite();
		
		KSPlanet* earth() { return Earth; }
		
		virtual void updatePlanets( KStarsData *data, KSNumbers *num );
		
		virtual void updateMoons( KStarsData *data, KSNumbers *num );

		virtual void draw(KStars *ks, QPainter& psky, double scale);
		
		//Do the drawing in this class, since we are keeping the list of objects 
		//with Trails here
		void drawTrails( KStars *ks, QPainter& psky, double scale );

		/**
		 *@short Add a Trail to the specified SkyObject.
		 *@p o Pointer to the SkyObject to which a Trail will be added.
		 */
		bool addTrail( SkyObject *o );
		bool hasTrail( SkyObject *o );
		bool removeTrail( SkyObject *o );

	private:
		KSPlanet *Earth;

};

#endif
