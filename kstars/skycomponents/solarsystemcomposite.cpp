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

SolarSystemComposite::SolarSystemComposite(SkyComposite *parent)
  : SkyComposite(parent)
{
	Earth = 0;
	Sun = new SunComponent(this, "sun.png", 1392000. /*diameter in km*/ );;
	Venus = PlanetComponent(this, I18N_NOOP("Venus"), "venus.png", 12103.6 /*diameter in km*/ );
 	Mercury = new PlanetComponent(this, I18N_NOOP("Mercury"), "mercury.png", 4879.4 /*diameter in km*/ );
	Moon = new MoonComponent(this, Options::showMoon());
	// TODO addComponent(new PlanetComponent(this, "Mars" ...);
// 	addComponent(new JupiterComposite(this)); // TODO composite knows jupiter + moons
	addComponent(new AsteroidsComponent(this, Options::showAsteroids()));
	addComponent(new CometsComponent(this, Options::showComets()));
}

SolarSystemComposite::~SolarSystemComposite::()
{
	delete Earth;
	delete Sun;
	delete Venus;
	delete Mercury;
	delete Moon;
}

SolarSystemComposite::init(KStarsData *data)
{
	Earth = new KSPlanet(data, I18N_NOOP("Earth"), "", 12756.28 /*diameter in km*/ );
	if (!Earth->loadData())
		//return false;
		return; // TODO stop initializing

	Sun->init(data);
	Venus->init(data);
	Mercury->init(data);
	Moon->init(data);
	
	// now init all sub components (outer planets, asteroids and comets)
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

void SolarSystemComposite::draw(SkyMap *map, QPainter& psky, double scale)
{
	// TODO drawTrail() of all components
	
	// first draw the objects which are far away
	SkyComposite::draw(map, psky, scale);
	
	Sun->draw(map, psky, scale);
	Venus->draw(map, psky, scale);
	Mercury->draw(map, psky, scale);
	Moon->draw(map, psky, scale);
}
