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

SkyComposite::SkyComposite(SkyComponent *parent ) 
: SkyComponent( parent ), m_CurrentIndex(0)
{
}

SkyComposite::~SkyComposite()
{
	while ( !components().isEmpty() )
		delete components().takeFirst();
}

void SkyComposite::addComponent(SkyComponent *component)
{
	components().append(component);
}

void SkyComposite::removeComponent(SkyComponent *component)
{
	int index = components().indexOf(component);
	if (index != -1)
	{
		// component is in list
		components().removeAt(index);
		delete component;
	}
}

void SkyComposite::draw(KStars *ks, QPainter& psky, double scale)
{
	foreach (SkyComponent *component, components())
		component->draw(ks, psky, scale);
}

void SkyComposite::drawExportable(KStars *ks, QPainter& psky, double scale)
{
	foreach (SkyComponent *component, components())
		component->drawExportable(ks, psky, scale);
}

void SkyComposite::init(KStarsData *data)
{
	int nCount=0;
	foreach (SkyComponent *component, components()) 
		component->init(data);
}

void SkyComposite::update(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, components())
		component->update( data, num );
}

SkyObject* SkyComposite::findByName( const QString &name ) {
	SkyObject *o = 0;
	foreach ( SkyComponent *comp, components() ) {
		o = comp->findByName( name );
		if ( o ) return o;
	}
	return 0;
}

SkyObject* SkyComposite::objectNearest( SkyPoint *p, double &maxrad ) {
	SkyObject *oTry = 0;
	SkyObject *oBest = 0;

	foreach ( SkyComponent *comp, components() ) {
		oTry = comp->objectNearest( p, maxrad );
		if ( oTry ) oBest = oTry; //found a closer object
	}

	return oBest; //will be 0 if no object nearer than maxrad was found
}

SkyObject* SkyComposite::first() {
	m_CurrentIndex = 0;
	return m_Components[m_CurrentIndex]->first();
}

SkyObject* SkyComposite::next() {
	SkyObject *result = m_Components[m_CurrentIndex]->next();

	if ( !result ) {
		m_CurrentIndex++;
		if ( m_CurrentIndex >= m_Components.size() ) return 0;
		return m_Components[m_CurrentIndex]->first();
	}

	return result;
}
