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

#include <QList>

#include "kstarsdata.h"
#include "skymap.h"

SkyComposite::SkyComposite(SkyComposite *parent) : SkyComponent(parent)
{
	Components = new QList() <SkyComponent*>;
}

SkyComposite::~SkyComposite()
{
	while (!Components.isEmpty())
        	delete Components.takeFirst();

	delete Components;
}

void SkyComposite::addComponent(SkyComponent *component)
{
	Components->append(component);
}

void SkyComposite::removeComponent(SkyComponent *component)
{
	int index = Components->indexOf(component);
	if (index != -1)
	{
		// component is in list
		Components->removeAt(index);
		delete component;
	}
}

void SkyComposite::draw(SkyMap *map, QPainter& psky, double scale)
{
  foreach (SkyComponent *component, Components)
		component->draw(map, psky, scale);
}

void SkyComposite::drawExportable(SkyMap *map, QPainter& psky, double scale)
{
  foreach (SkyComponent *component, Components)
		component->drawExportable(map, psky, scale);
}

void SkyComposite::init(KStarsData *data)
{
  foreach (SkyComponent *component, Components)
		component->init(data);
}

void SkyComposite::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
  foreach (SkyComponent *component, Components)
		component->update(data, num, needNewCoords);
}

void SkyComposite::updatePlanets(KStarsData*, KSNumbers*, bool needNewCoords)
{
  foreach (SkyComponent *component, Components)
		component->updatePlanets(data, num, needNewCoords);
}}

void SkyComposite::updateMoons(KStarsData*, KSNumbers*, bool needNewCoords)
{
  foreach (SkyComponent *component, Components)
		component->updateMoons(data, num, needNewCoords);
}
