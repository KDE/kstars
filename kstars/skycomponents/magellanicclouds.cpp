/***************************************************************************
                          magellanicclouds.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 12 Nov. 2005
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

#include "magellanicclouds.h"

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QPainter>

#include <klocale.h>

#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skypoint.h"
#include "dms.h"
#include "Options.h"
#include "ksfilereader.h"

#include "skymesh.h"


MagellanicClouds::MagellanicClouds( SkyComponent *parent ) :
        SkipListIndex( parent, i18n("Magellanic Clouds") )
{}

void MagellanicClouds::init() {
    intro();
    loadSkipLists("lmc.dat", i18n("Loading Large Magellanic Clouds"));
    loadSkipLists("smc.dat", i18n("Loading Small Magellanic Clouds"));
    summary();
}

bool MagellanicClouds::selected()
{
    return Options::showMilkyWay() &&
           ! ( Options::hideOnSlew() && Options::hideMilkyWay() && SkyMap::IsSlewing() );
}

void MagellanicClouds::draw( QPainter& psky )
{
    if ( !selected() ) return;

    KStarsData *data = KStarsData::Instance();

    QColor color =  QColor( data->colorScheme()->colorNamed( "MWColor" ) );

    psky.setPen( QPen( color, 3, Qt::SolidLine ) );
    psky.setBrush( QBrush( color ) );

    if ( Options::fillMilkyWay() ) {
        drawFilled( psky );
    }
    else {
        drawLines( psky );
    }
}

