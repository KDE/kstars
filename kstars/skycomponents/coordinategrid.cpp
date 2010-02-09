/***************************************************************************
                          coordinategrid.h  -  K Desktop Planetarium
                             -------------------
    begin                : 15 Sept. 2005
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

#include "coordinategrid.h"

#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPainter>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "linelist.h"
#include "dms.h"

CoordinateGrid::CoordinateGrid( SkyComposite *parent )
        : NoPrecessIndex(parent, i18n("Coordinate Grid") )
{
    KStarsData *data = KStarsData::Instance();
    //emitProgressText( i18n("Loading coordinate grid" ) );
    intro();

    // start the new fangled way here

    double eps    =   0.1;
    double minRa  =   0.0;
    double maxRa  =  23.0;
    double dRa    =   2.0;
    double minDec = -80.0;
    double maxDec =  90.0;
    double dDec   =  20.0;
    double dDec2  =   4.0;
    double dRa2   =   0.2;

    double max, dec, dec2, ra, ra2;

    LineList* lineList;

    for ( ra = minRa; ra < maxRa; ra += dRa ) {
        for ( dec = -90.0; dec < maxDec - eps; dec += dDec ) {
            lineList = new LineList();
            max = dec + dDec;
            if ( max > 90.0 ) max = 90.0;
            for ( dec2 = dec; dec2 <= max + eps; dec2 += dDec2 ) {
                SkyPoint* p = new SkyPoint( ra, dec2 );
                p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                lineList->append( p );
            }
            appendLine( lineList );
        }
    }

    for ( dec = minDec; dec < maxDec + eps; dec += dDec ) {
        // Adjust point density
        int nPoints = int(round( fabs(cos(dec* dms::PI / 180.0)) * dRa / dRa2 ));
        if ( nPoints < 5 )
            nPoints = 5;
        double dRa3 = dRa / nPoints;

        for ( ra = minRa; ra < maxRa + eps; ra += dRa ) {
            lineList = new LineList();
            for ( ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa3 ) {
                SkyPoint* p = new SkyPoint( ra2, dec );
                p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                lineList->append( p );
            }
            appendLine( lineList );
        }
    }
    summary();
}

bool CoordinateGrid::selected()
{
    return Options::showGrid() &&
           ! ( Options::hideOnSlew() && Options::hideGrid() && SkyMap::IsSlewing() );
}

void CoordinateGrid::preDraw( QPainter &psky )
{
    KStarsData *data = KStarsData::Instance();
    QColor color = data->colorScheme()->colorNamed( "GridColor" );
    psky.setPen( QPen( QBrush( color ), 1, Qt::DotLine ) );
}
