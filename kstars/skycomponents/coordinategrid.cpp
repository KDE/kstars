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

#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPainter>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "coordinategrid.h"
#include "linelist.h"

CoordinateGrid::CoordinateGrid( SkyComponent *parent ) 
	: LineListIndex(parent, "Coordinate Grid") 
{}

bool CoordinateGrid::selected()
{
    return Options::showGrid();
}

void CoordinateGrid::preDraw( KStars *kstars, QPainter &psky )
{
    QColor color = kstars->data()->colorScheme()->colorNamed( "GridColor" ); 
    psky.setPen( QPen( QBrush( color ), 1, Qt::DotLine ) );
}

void CoordinateGrid::init( KStarsData *data )
{
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
    double dRa2   =  2. / 5.;

    double max, dec, dec2, ra, ra2;

    LineList* lineList;

    for ( ra = minRa; ra < maxRa; ra += dRa ) {
        for ( dec = -90.0; dec < maxDec - eps; dec += dDec ) {
           lineList = new LineList();
           //printf("%6.2f: ", ra);
           max = dec + dDec;
           if ( max > 90.0 ) max = 90.0;
           for ( dec2 = dec; dec2 <= max + eps; dec2 += dDec2 ) {
                //printf("%4d %6.2f ", components().size(), dec2);
                SkyPoint* p = new SkyPoint( ra, dec2 );
                p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                lineList->points()->append( p );
            }

            appendLine( lineList );
            //printf("\n");
        }
    }

    for ( dec = minDec; dec < maxDec + eps; dec += dDec ) {
        for ( ra = minRa; ra < maxRa + eps; ra += dRa ) {
           lineList = new LineList();
           //printf("%6.2f: ", dec);
           for ( ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa2 ) {
                //printf("%4d %6.2f ", components().size(), ra2);
                SkyPoint* p = new SkyPoint( ra2, dec );
                p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                lineList->points()->append( p );
           }
           appendLine( lineList );
           //printf("\n");
        }
    }

    summary();
}

// Don't precess the coordinate grid
void CoordinateGrid::JITupdate( KStarsData *data, LineList* lineList )
{
    lineList->updateID = data->updateID();
    SkyList* points = lineList->points();
    for (int i = 0; i < points->size(); i++ ) {
        points->at( i )->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}


