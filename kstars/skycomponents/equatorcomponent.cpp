/***************************************************************************
                          equatorcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10 Sept. 2005
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

#include "equatorcomponent.h"

#include <QList>
#include <QPoint>
#include <QBrush>

#include "ksnumbers.h"
#include "kstarsdata.h"
#include "skypoint.h"

//DEBUG
#include "Options.h"
#include "kstars.h"
#include "skymap.h"

EquatorComponent::EquatorComponent(SkyComponent *parent ) : 
    LineListComponent(parent )
{}

EquatorComponent::~EquatorComponent()
{}

bool EquatorComponent::selected()
{
    return Options::showEquator();
}


void EquatorComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Creating equator" ) );

	setLabel( i18n("Equator") );
	setLabelPosition( LineListComponent::LeftEdgeLabel );
	setPen( QPen( QBrush( data->colorScheme()->colorNamed( "EqColor" ) ), 
										 1, Qt::SolidLine ) );

	for ( unsigned int i=0; i<=NCIRCLE; ++i ) {
		SkyPoint* p = new SkyPoint( i*24./NCIRCLE, 0.0 );
		p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
		appendP( p );
	}
}
