/***************************************************************************
                          listcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/01
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "listcomponent.h"

#include <QList>

#include "kstarsdata.h"
#include "skymap.h" 
#include "../skyobjects/skyobject.h"

ListComponent::ListComponent( SkyComponent *parent, bool (*visibleMethod)() )
        : SkyComponent( parent, visibleMethod ), m_CurrentIndex(0)
{}

ListComponent::ListComponent( SkyComponent *parent )
        : SkyComponent( parent ), m_CurrentIndex(0)
{}


ListComponent::~ListComponent()
{
    clear();
}

void ListComponent::clear() {
    while ( ! objectList().isEmpty() ) {
        SkyObject *o = objectList().takeFirst();
        int i = objectNames(o->type()).indexOf( o->name() );
        if ( i >= 0 ) objectNames(o->type()).removeAt( i );
        i = objectNames(o->type()).indexOf( o->longname() );
        if ( i >= 0 ) objectNames(o->type()).removeAt( i );

        delete o;
    }
}

void ListComponent::update( KStarsData *data, KSNumbers *num )
{
    if ( ! selected() ) return;

    foreach ( SkyObject *o, objectList() ) {
        if ( num ) o->updateCoords( num );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

SkyObject* ListComponent::findByName( const QString &name ) {
    foreach ( SkyObject *o, objectList() )
    if ( QString::compare( o->name(), name, Qt::CaseInsensitive ) == 0 || 
        QString::compare( o->longname(), name, Qt::CaseInsensitive ) == 0 || 
        QString::compare( o->name2(), name, Qt::CaseInsensitive ) == 0 )
        return o;

    //No object found
    return 0;
}

SkyObject* ListComponent::objectNearest( SkyPoint *p, double &maxrad ) {
    SkyObject *oBest = 0;

    if ( ! selected() ) return 0;

    foreach ( SkyObject *o, objectList() ) {
        double r = o->angularDistanceTo( p ).Degrees();
        if ( r < maxrad ) {
            oBest = o;
            maxrad = r;
        }
    }

    return oBest;
}


SkyObject* ListComponent::first() {
    m_CurrentIndex = 0;
    return ObjectList[m_CurrentIndex];
}

SkyObject* ListComponent::next() {
    m_CurrentIndex++;
    if ( m_CurrentIndex >= ObjectList.size() )
        return 0;
    else
        return ObjectList[m_CurrentIndex];
}
