/***************************************************************************
                      horizontalcoordinategrid.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 01 Mar 2012
    copyright            : (C) 2012 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "horizontalcoordinategrid.h"

#include <QPen>
#include <QBrush>
#include <QColor>

#include "Options.h"
#include "kstarsdata.h"

#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif

#include "linelist.h"
#include "dms.h"

#include "skypainter.h"

HorizontalCoordinateGrid::HorizontalCoordinateGrid( SkyComposite *parent )
        : CoordinateGrid( parent, i18n("Horizontal Coordinate Grid" ) )
{
    //KStarsData *data = KStarsData::Instance();
    
    intro();

    double eps    =   0.1;
    double minAz  =   0.0;
    double maxAz  = 359.0;
    double dAz    =  30.0;
    double minAlt = -80.0;
    double maxAlt =  90.0;
    double dAlt   =  20.0;
    double dAlt2  =   4.0;
    double dAz2   =   0.2;

    double max, alt, alt2, az, az2;

    LineList* lineList;
    
   
    for ( az = minAz; az < maxAz; az += dAz ) {
        for ( alt = -90.0; alt < maxAlt - eps; alt += dAlt ) {
            lineList = new LineList();
            max = alt + dAlt;
            if ( max > 90.0 ) max = 90.0;
            for ( alt2 = alt; alt2 <= max + eps; alt2 += dAlt2 ) {
                SkyPoint* p = new SkyPoint();
                p->setAz( az );
                p->setAlt( alt2 );
                //p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
                lineList->append( p );
            }
            appendLine( lineList );
        }
    }

    for ( alt = minAlt; alt < maxAlt + eps; alt += dAlt ) {
        // Do not paint the line on the horizon
        if ( alt < 0.1 && alt > -0.1 )
            continue;
        
        // Adjust point density
        int nPoints = int(round( fabs(cos(alt* dms::PI / 180.0)) * dAz / dAz2 ));
        if ( nPoints < 5 )
            nPoints = 5;
        double dAz3 = dAz / nPoints;

        for ( az = minAz; az < maxAz + eps; az += dAz ) {
            lineList = new LineList();
            for ( az2 = az; az2 <= az + dAz + eps; az2 += dAz3 ) {
                SkyPoint* p = new SkyPoint();
                p->setAz( az2 );
                p->setAlt( alt );
                //p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
                lineList->append( p );
            }
            appendLine( lineList );
        }
    }
    summary();
}

bool HorizontalCoordinateGrid::selected()
{
    if ( Options::autoSelectGrid() )
        return( Options::useAltAz() );
    else
#ifndef KSTARS_LITE
        return( Options::showHorizontalGrid() &&
            ! ( Options::hideOnSlew() && Options::hideGrids() && SkyMap::IsSlewing() ) );
#else
        return( Options::showHorizontalGrid() &&
            ! ( Options::hideOnSlew() && Options::hideGrids() && SkyMapLite::IsSlewing() ) );
#endif
}

void HorizontalCoordinateGrid::preDraw( SkyPainter* skyp )
{
    KStarsData *data = KStarsData::Instance();
    QColor color = data->colorScheme()->colorNamed( "HorizontalGridColor" );
    skyp->setPen( QPen( QBrush( color ), 2, Qt::DotLine ) );
}

void HorizontalCoordinateGrid::update( KSNumbers* )
{
    KStarsData *data = KStarsData::Instance();

    for ( int i=0; i<listList().count(); i++ ) {
        for ( int j=0; j<listList().at( i )->points()->count(); j++ ) {
            listList().at( i )->points()->at( j )->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        }
    }
}
