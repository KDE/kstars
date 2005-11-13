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

#include <QPainter>
#include <klocale.h>

#include "solarsystemcomposite.h"
#include "solarsystemsinglecomponent.h"
#include "asteroidscomponent.h"
#include "cometscomponent.h"
#include "skycomponent.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "ksplanet.h"
#include "kssun.h"
#include "ksmoon.h"
#include "kspluto.h"
#include "jupitermoonscomponent.h"

SolarSystemComposite::SolarSystemComposite(SkyComponent *parent, KStarsData *data)
  : SkyComposite(parent)
{
	Earth = new KSPlanet( data, I18N_NOOP( "Earth" ), "", 12756.28 /*diameter in km*/ );

	//FIXME: KSSun and KSPluto ctors doesn't need filename and diameter args!
	addComponent( new SolarSystemSingleComponent( this, new KSSun(data), Options::showSun, 8 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSMoon(data), Options::showMoon, 8 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP("Mercury"), "mercury.png", 4879.4 /*diameter in km*/ ), Options::showMercury, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP("Venus"), "venus.png", 12103.6 /*diameter in km*/ ), Options::showVenus, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Mars" ), "mars.png", 6792.4 /*diameter in km*/ ), Options::showMars, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Jupiter" ), "jupiter.png", 142984. /*diameter in km*/ ), Options::showJupiter, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Saturn" ), "saturn.png", 120536. /*diameter in km*/ ), Options::showSaturn, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Uranus" ), "uranus.png", 51118. /*diameter in km*/ ), Options::showUranus, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPlanet( data, I18N_NOOP( "Neptune" ), "neptune.png", 49572. /*diameter in km*/ ), Options::showNeptune, 4 ) );
	addComponent( new SolarSystemSingleComponent( this, new KSPluto(data), Options::showPluto, 4 ) );

	addComponent( new AsteroidsComponent( this, Options::showAsteroids ) );
	addComponent( new CometsComponent( this, Options::showComets ) );
}

SolarSystemComposite::~SolarSystemComposite()
{
	delete Earth;
}

void SolarSystemComposite::init(KStarsData *data)
{
	if (!Earth->loadData())
		return; //stop initializing

	emitProgressText( i18n("Loading solar system" ) );

	//init all sub components
	SkyComposite::init(data);
}

void SolarSystemComposite::updatePlanets( KStarsData *data, KSNumbers *num )
{
	Earth->findPosition(num);
	SkyComposite::updatePlanets( data, num );
}

void SolarSystemComposite::updateMoons( KStarsData *data, KSNumbers *num )
{
	Earth->findPosition(num);
	SkyComposite::updateMoons( data, num );
}

void SolarSystemComposite::draw(KStars *ks, QPainter& psky, double scale)
{
	//FIXME: first draw the objects which are far away
	//(Thomas had been doing this by keeping separate pointers to 
	//inner solar system objects, but I'd rather handle it here in the draw 
	//function if possible
	SkyComposite::draw(ks, psky, scale);
}

void SolarSystemComposite::drawTrails(KStars *ks, QPainter& psky, double scale )
{
	foreach ( SkyComponent *comp, components() ) {
		comp->drawTrails( ks, psky, scale );
	}
}
