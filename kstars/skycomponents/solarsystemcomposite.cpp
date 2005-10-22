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

#include "solarsystemcomposite.h"

#include "jupitermoonscomponent.h"

SolarSystemComposite::SolarSystemComposite(SkyComposite *parent, KStarsData *data)
  : SkyComposite(parent)
{
	Earth = new KSPlanet( kd, I18N_NOOP( "Earth" ), "", 12756.28 /*diameter in km*/ );

	//FIXME: KSSun and KSPluto ctors doesn't need filename and diameter args!
	addComponent( new PlanetComponent( this, new KSSun(data), Options::showSun(), 8 ) );
	addComponent( new PlanetComponent( this, new KSMoon(data), Options::showMoon(), 8 ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18n_NOOP("Mercury"), "mercury.png", 4879.4 /*diameter in km*/ ), Options::showMercury() ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18N_NOOP("Venus"), "venus.png", 12103.6 /*diameter in km*/ ), Options::showVenus() ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18N_NOOP( "Mars" ), "mars.png", 6792.4 /*diameter in km*/ ), Options::showMars() ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18N_NOOP( "Jupiter" ), "jupiter.png", 142984. /*diameter in km*/ ), Options::showJupiter() ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18N_NOOP( "Saturn" ), "saturn.png", 120536. /*diameter in km*/ ), Options::showSaturn() ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18N_NOOP( "Uranus" ), "uranus.png", 51118. /*diameter in km*/ ), Options::showUranus() ) );
	addComponent( new PlanetComponent( this, new KSPlanet( data, I18N_NOOP( "Neptune" ), "neptune.png", 49572. /*diameter in km*/ ), Options::showNeptune() ) );
	addComponent( new PlanetComponent( this, new KSPluto(data), Options::showPluto() ) );

	addComponent(new AsteroidsComponent(this, Options::showAsteroids()));
	addComponent(new CometsComponent(this, Options::showComets()));
}

SolarSystemComposite::~SolarSystemComposite()
{
  //JH: is this necessary to call, or is it done automatically?
  SkyComposite::~SkyComposite();

	delete Earth;
	delete Sun;
	delete Venus;
	delete Mercury;
	delete Moon;
}

SolarSystemComposite::init(KStarsData *data)
{
	if (!Earth->loadData())
		return; //stop initializing

	//init all sub components
	SkyComposite::init(data);
}

void SolarSystemComposite::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	Sun->update(data, num, needNewCoords);
	Venus->update(data, num, needNewCoords);
	Mercury->update(data, num, needNewCoords);
	Moon->update(data, num, needNewCoords);
	
	SkyComposite::update(data, num, needNewCoords);
}

void SolarSystemComposite::updatePlanets(KStarsData*, KSNumbers*, bool needNewCoords)
{
	Earth->findPosition(num);

	Sun->updatePlanets(data, num, needNewCoords);
	Venus->updatePlanets(data, num, needNewCoords);
	Mercury->updatePlanets(data, num, needNewCoords);
	Moon->updatePlanets(data, num, needNewCoords);
	
	SkyComposite::updatePlanets(data, num, needNewCoords);
}

void SolarSystemComposite::updateMoons(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	Sun->updateMoons(data, num, needNewCoords);
	Venus->updateMoons(data, num, needNewCoords);
	Mercury->updateMoons(data, num, needNewCoords);
	Moon->updateMoons(data, num, needNewCoords);
	
	SkyComposite::updateMoons(data, num, needNewCoords);
}

void SolarSystemComposite::draw(KStars *ks, QPainter& psky, double scale)
{
	//FIXME: first draw the objects which are far away
	//(Thomas had been doing this by keeping separate pointers to 
	//inner solar system objects, but I'd rather handle it here in the draw 
	//function if possible
	SkyComposite::draw(ks, psky, scale);
}

bool SolarSystemComposite::addTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, components() ) {
		if ( comp->addTrail( o ) ) return true;
	}
	
	return false; //The SkyObject o was not found
}
