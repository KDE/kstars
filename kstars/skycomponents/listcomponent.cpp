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
#include "skyobject.h"

ListComponent::ListComponent( SkyComponent *parent, bool (*visibleMethod)() )
: SkyComponent( parent, visibleMethod )
{
}

ListComponent::~ListComponent()
{
	while ( ! objectList().isEmpty() )
		delete objectList().takeFirst();
}

void ListComponent::update( KStarsData *data, KSNumbers *num )
{
	if ( visible() ) {
		foreach ( SkyObject *o, objectList() ) {
			if ( num ) o->updateCoords( num );
			o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
		}
	}
}

SkyObject* ListComponent::findByName( const QString &name ) {
	foreach ( SkyObject *o, objectList() ) 
		if ( o->name() == name || o->longname() == name || o->name2() == name )
			return o;

	//No object found
	return 0;
}
