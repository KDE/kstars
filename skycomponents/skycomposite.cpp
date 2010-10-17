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

SkyComposite::SkyComposite(SkyComposite *parent ) :
    SkyComponent( parent )
{ }

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
        delete components().takeAt(index);
}

void SkyComposite::draw( QPainter& psky )
{
    if( selected() )
        foreach ( SkyComponent *component, components() )
            component->draw( psky );
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
