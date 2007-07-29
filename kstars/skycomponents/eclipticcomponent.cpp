/***************************************************************************
                          eclipticcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 16 Sept. 2005
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

#include "eclipticcomponent.h"

#include <QPainter>

#include "ksnumbers.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

EclipticComponent::EclipticComponent(SkyComponent *parent ) : 
    LineListComponent( parent )
{}

EclipticComponent::~EclipticComponent()
{}

bool EclipticComponent::selected()
{
    return Options::showEcliptic();
}

void EclipticComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Creating ecliptic" ) );

	setLabel( i18n("Ecliptic") );
	setLabelPosition( LineListComponent::RightEdgeLabel );
	setPen( QPen( QBrush( data->colorScheme()->colorNamed( "EclColor" ) ), 
										 1, Qt::SolidLine ) );

	KSNumbers num( data->ut().djd() );
	dms elat(0.0), elng(0.0);

	for ( unsigned int i=0; i<=NCIRCLE; ++i ) {
		elng.setD( double( i ) );
		SkyPoint* o = new SkyPoint();
		o->setFromEcliptic( num.obliquity(), &elng, &elat );
		o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
		appendP( o );
	}
}

