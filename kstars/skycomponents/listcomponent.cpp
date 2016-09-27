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
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "skyobjects/skyobject.h"

ListComponent::ListComponent( SkyComposite *parent ) :
    SkyComponent( parent )
{}

ListComponent::~ListComponent() {
    clear();
}

void ListComponent::clear() {
    while ( ! m_ObjectList.isEmpty() ) {
        SkyObject *o = m_ObjectList.takeFirst();
        removeFromNames( o );
        delete o;
    }
}

void ListComponent::update( KSNumbers *num )
{
    if ( ! selected() )
        return;
    KStarsData *data = KStarsData::Instance();
    foreach ( SkyObject *o, m_ObjectList ) {
        if( num )
            o->updateCoords( num );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

SkyObject* ListComponent::findByName( const QString &name ) {
    foreach( SkyObject *o, m_ObjectList ) {
        if( QString::compare( o->name(),     name, Qt::CaseInsensitive ) == 0 ||
            QString::compare( o->longname(), name, Qt::CaseInsensitive ) == 0 ||
            QString::compare( o->name2(),    name, Qt::CaseInsensitive ) == 0 )
            return o;
    }
    //No object found
    return 0;
}

SkyObject* ListComponent::objectNearest( SkyPoint *p, double &maxrad ) {
    if ( ! selected() )
        return 0;

    SkyObject *oBest = 0;
    foreach ( SkyObject *o, m_ObjectList ) {
        double r = o->angularDistanceTo( p ).Degrees();
        if ( r < maxrad ) {
            oBest = o;
            maxrad = r;
        }
    }
    return oBest;
}
