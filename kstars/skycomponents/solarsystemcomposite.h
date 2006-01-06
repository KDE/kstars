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

class KSPlanet;
class AsteroidsComponent;
class CometsComponent;

/**@class SolarSystemComposite
* The solar system composite manages all planets, asteroids and comets.
* As every sub component of solar system needs the earth , the composite
* is managing this object by itself.
*
*@author Thomas Kabelmann
*@version 0.1
*/

class SolarSystemComposite : public SkyComposite
{
	public:
		SolarSystemComposite(SkyComponent *parent, KStarsData *data);
		~SolarSystemComposite();
		
		KSPlanet* earth() { return Earth; }
		QList<SkyObject*>& asteroids();
		QList<SkyObject*>& comets();

		
		virtual void init(KStarsData *data);

		virtual void updatePlanets( KStarsData *data, KSNumbers *num );
		
		virtual void updateMoons( KStarsData *data, KSNumbers *num );

		virtual void draw(KStars *ks, QPainter& psky, double scale);
		
		//Do the drawing in this class, since we are keeping the list of objects 
		//with Trails here
		void drawTrails( KStars *ks, QPainter& psky, double scale );

		void reloadAsteroids( KStarsData *data );
		void reloadComets( KStarsData *data );

	private:
		KSPlanet *Earth;

		AsteroidsComponent *m_AsteroidsComponent;
		CometsComponent *m_CometsComponent;
};

#endif
