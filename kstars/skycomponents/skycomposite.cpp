/***************************************************************************
                          skycomposite.cpp  -  K Desktop Planetarium
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

#include "skycomposite.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "skyobject.h"

SkyComposite::SkyComposite(SkyComponent *parent, KStarsData *data ) 
: SkyComponent( parent )
{
}

SkyComposite::~SkyComposite()
{
	while ( !Components.isEmpty() )
		delete Components.takeFirst();
}

void SkyComposite::addComponent(SkyComponent *component)
{
	Components.append(component);
}

void SkyComposite::removeComponent(SkyComponent *component)
{
	int index = Components.indexOf(component);
	if (index != -1)
	{
		// component is in list
		Components.removeAt(index);
		delete component;
	}
}

void SkyComposite::draw(KStars *ks, QPainter& psky, double scale)
{
	foreach (SkyComponent *component, Components)
		component->draw(ks, psky, scale);
}

void SkyComposite::drawExportable(KStars *ks, QPainter& psky, double scale)
{
	foreach (SkyComponent *component, Components)
		component->drawExportable(ks, psky, scale);
}

void SkyComposite::init(KStarsData *data)
{
	foreach (SkyComponent *component, Components)
		component->init(data);
}

void SkyComposite::update(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, Components)
		component->update( data, num );
}

void SkyComposite::updatePlanets(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, Components)
		component->updatePlanets( data, num );
}

void SkyComposite::updateMoons(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component )
		component->updateMoons( data, num );
}

bool SkyComposite::addTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->addTrail( o ) ) return true;
	}
	//Did not find object o
	return false;
}

bool SkyComposite::hasTrail( SkyObject *o, bool &found ) {
	found = false;
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->hasTrail( o, found ) ) return true;
		//It's possible we found the object, but it had no trail:
		if ( found ) return false;
	}
	//Did not find object o
	return false;
}

bool SkyComposite::removeTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->removeTrail( o ) ) return true;
	}
	//Did not find object o
	return false;
}

SkyObject* SkyComposite::findByName( const QString &name ) {
	SkyObject *o = 0;
	foreach ( SkyComponent *comp, Components ) {
		o = comp->findByName( name );
		if ( o ) return o;
	}
	return 0;
}
