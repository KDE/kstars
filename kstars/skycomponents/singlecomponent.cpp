/***************************************************************************
                          singlecomponent.cpp  -  K Desktop Planetarium
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

#include "singlecomponent.h"
#include "listcomponent.h"

#include <QList>

#include "kstarsdata.h"
#include "skymap.h" 
#include "skyobjects/skyobject.h"

SingleComponent::SingleComponent(SkyComponent *parent, bool (*visibleMethod)())
        : SkyComponent( parent, visibleMethod )
{
}

SingleComponent::~SingleComponent()
{
    int i = objectNames(m_StoredObject->type()).indexOf( m_StoredObject->name() );
    if ( i >= 0 )
        objectNames(m_StoredObject->type()).removeAt( i );

    i = objectNames(m_StoredObject->type()).indexOf( m_StoredObject->longname() );
    if ( i >= 0 )
        objectNames(m_StoredObject->type()).removeAt( i );

    delete m_StoredObject;
}

void SingleComponent::update( KSNumbers *num )
{
    KStarsData *data = KStarsData::Instance();
    if ( visible() ) {
        if ( num ) skyObject()->updateCoords( num );
        skyObject()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

SkyObject* SingleComponent::findByName( const QString &name ) {
    if ( QString::compare( skyObject()->name(), name, Qt::CaseInsensitive ) == 0 ||
        QString::compare( skyObject()->longname(), name, Qt::CaseInsensitive ) == 0 ||
        QString::compare( skyObject()->name2(), name, Qt::CaseInsensitive ) == 0 )
        return skyObject();

    return 0;
}

SkyObject* SingleComponent::objectNearest( SkyPoint *p, double &maxrad ) {
    double r = skyObject()->angularDistanceTo( p ).Degrees();
    if ( r < maxrad ) {
        maxrad = r;
        return skyObject();
    }

    return 0;
}

SkyObject* SingleComponent::first() {
    return skyObject();
}

SkyObject* SingleComponent::next() {
    return 0;
}
