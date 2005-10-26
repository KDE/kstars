/***************************************************************************
                          skymapcomposite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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

#include "skymapcomposite.h"

#include "horizoncomponent.h"
#include "milkywaycomponent.h"
#include "coordinategridcomponent.h"

SkyMapComposite::SkyMapComposite(SkyComponent *parent) : SkyComposite(parent)
{
	//Add all components
	// beware the order of adding components
	// first added component will be drawn first
	// so horizon should be one of the last components
	addComponent( new MilkyWayComponent( this, &Options::showMilkyWay ) );
	addComponent( new CoordinateGridComposite( this, &Options::showGrid ) );
	addComponent( new ConstellationBoundaryComponent( this, &Options::showCBounds ) );
	addComponent( new ConstellationLinesComponent( this, &Options::showCLines ) );
	addComponent( new ConstellationNamesComponent( this, &Options::showCNames ) );
	addComponent( new EquatorComponent( this, &Options::showEquator ) );
	addComponent( new EclipticComponent( this, &Options::showEcliptic ) );
	addComponent( new DeepSkyComponent( this, &Options::showDeepSky ) );
	addComponent( new CustomCatalogComponent( this, &Options::showOther ) );
	addComponent( new StarComponent( this, &Options::showStars ) );

	m_SSComposite = new SolarSystemComposite( this, data );
	addComponent( m_SSComposite );

	addComponent( new JupiterMoonsComponent( this, &Options::showJupiter) );
	addComponent( new HorizonComponent(this) );
}

void SkyMapComposite::updatePlanets(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, solarSystem())
		component->updatePlanets( data, num );
}

void SkyMapComposite::updateMoons(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, solarSystem() )
		component->updateMoons( data, num );
}

bool SkyMapComposite::addTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->addTrail( o ) ) return true;
	}
	//Did not find object o
	return false;
}

bool SkyMapComposite::hasTrail( SkyObject *o, bool &found ) {
	found = false;
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->hasTrail( o, found ) ) return true;
		//It's possible we found the object, but it had no trail:
		if ( found ) return false;
	}
	//Did not find object o
	return false;
}

bool SkyMapComposite::removeTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->removeTrail( o ) ) return true;
	}
	//Did not find object o
	return false;
}

