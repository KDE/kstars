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

#include <QList>
#include <QPoint>
#include <QPainter>

#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

EclipticComponent::EclipticComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

EclipticComponent::~EclipticComponent()
{
  while ( ! Ecliptic.isEmpty() ) delete Ecliptic.takeFirst();
}

// was KStarsData::initGuides(KSNumbers *num)
// needs dms *LST, *HourAngle from KStarsData
// TODO: ecliptic + equator needs partial code of algorithm
// -> solution:
//	-all 3 objects in 1 component (this is messy)
//	-3 components which share a algorithm class
void EclipticComponent::init(KStarsData *data)
{
	// Define the Ecliptic
  KSNumbers num( data->ut().djd() );
  dms elat(0.0);
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
	  dms elng( double( i ) );
		SkyPoint *o = new SkyPoint( 0.0, 0.0 );
		o->setFromEcliptic( num.obliquity(), &elng, &elat );
		o->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		Ecliptic.append( o );
	}
}

void EclipticComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( Options::showEcliptic() ) {
	  foreach ( SkyPoint *p, Ecliptic ) {
			p->EquatorialToHorizontal( LST, data->geo()->lat() );
		}
	}
}
