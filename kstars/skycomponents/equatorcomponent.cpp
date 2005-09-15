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
#include <QPainter>

#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

EquatorComponent::EquatorComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

EquatorComponent::~EquatorComponent()
{
}

// was KStarsData::initGuides(KSNumbers *num)
// needs dms *LST, *HourAngle from KStarsData
// TODO: ecliptic + equator needs partial code of algorithm
// -> solution:
//	-all 3 objects in 1 component (this is messy)
//	-3 components which share a algorithm class
void EquatorComponent::init(KStarsData *data)
{
	// Define the Celestial Equator
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint *o = new SkyPoint( i*24./NCIRCLE, 0.0 );
		o->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		Equator.append( o );
	}
}

void EquatorComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( Options::showEquator() ) {
		for ( SkyPoint *p = Equator.first(); p; p = Equator.next() ) {
			p->EquatorialToHorizontal( LST, data->geo()->lat() );
		}
	}
}
