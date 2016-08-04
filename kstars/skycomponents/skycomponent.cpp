/***************************************************************************
                          skycomponent.cpp  -  K Desktop Planetarium
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

#include "skycomponent.h"
#include "skycomposite.h"

#include <QList>

#include "Options.h"
#include "ksnumbers.h"
#include "skyobjects/skyobject.h"

SkyComponent::SkyComponent( SkyComposite *parent ) :
    m_parent( parent )
{}

SkyComponent::~SkyComponent()
{}

//Hand the message up to SkyMapComposite
void SkyComponent::emitProgressText( const QString &message ) {
    parent()->emitProgressText( message );
}

SkyObject* SkyComponent::findByName( const QString & ) {
    return 0;
}

SkyObject* SkyComponent::objectNearest( SkyPoint *, double & ) {
    return 0;
}

void SkyComponent::drawTrails( SkyPainter* )
{}

void SkyComponent::objectsInArea( QList<SkyObject*>&, const SkyRegion& )
{}


QHash<int, QStringList>& SkyComponent::getObjectNames() {
    return parent()->objectNames();
}

QHash<int, QVector<QPair<QString, const SkyObject *>>>& SkyComponent::getObjectLists() {
    return parent()->objectLists();
}

void SkyComponent::removeFromNames(const SkyObject* obj) {
    QStringList& names = getObjectNames()[obj->type()];
    int i;
    i = names.indexOf( obj->name() );
    if ( i >= 0 )
        names.removeAt( i );

    i = names.indexOf( obj->longname() );
    if ( i >= 0 )
        names.removeAt( i );
}

void SkyComponent::removeFromLists(const SkyObject* obj) {
    QVector<QPair<QString, const SkyObject*>>& names = getObjectLists()[obj->type()];
    int i;
    i = names.indexOf( QPair<QString, const SkyObject*>(obj->name(), obj) );
    if ( i >= 0 )
        names.removeAt( i );

    i = names.indexOf( QPair<QString, const SkyObject*>(obj->longname(),obj) );
    if ( i >= 0 )
        names.removeAt( i );
}
