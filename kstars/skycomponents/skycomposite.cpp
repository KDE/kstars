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

#include "skyobjects/skyobject.h"

SkyComposite::SkyComposite(SkyComponent *parent )
        : SkyComponent( parent ), m_CurrentIndex(0)
{
}

SkyComposite::SkyComposite(SkyComponent *parent, bool (*visibleMethod)())
        : SkyComponent( parent, visibleMethod ), m_CurrentIndex(0)
{
}


SkyComposite::~SkyComposite()
{
    qDeleteAll( components() );
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

void SkyComposite::draw( QPainter& psky )
{
    foreach ( SkyComponent *component, components() )
        component->draw( psky );
}

void SkyComposite::drawExportable( QPainter& psky )
{
    foreach ( SkyComponent *component, components() )
        component->drawExportable( psky );
}

void SkyComposite::init()
{
    foreach ( SkyComponent *component, components() )
        component->init();
}

void SkyComposite::update( KSNumbers *num )
{
    foreach (SkyComponent *component, components())
        component->update( num );
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

    if ( ! selected() ) return 0;

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
