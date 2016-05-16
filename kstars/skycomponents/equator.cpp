/***************************************************************************
                             equator.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "equator.h"

#include "ksnumbers.h"
#include "kstarsdata.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "skyobjects/skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "linelist.h"
#include "skylabeler.h"

#include "skypainter.h"
#include "projections/projector.h"

Equator::Equator(SkyComposite *parent ) :
        NoPrecessIndex( parent, i18n("Equator") ),
        m_label( LineListIndex::name() )
{
    KStarsData *data = KStarsData::Instance();
    KSNumbers num( data->ut().djd() );

    const double eps    =   0.1;
    const double minRa  =   0.0;
    const double maxRa  =  23.0;
    const double dRa    =   2.0;
    const double dRa2   =  .5 / 5.;

    for(double ra = minRa; ra < maxRa; ra += dRa ) {
        LineList* lineList = new LineList();
        for(double ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa2 ) {
            SkyPoint* o = new SkyPoint( ra2, 0.0 );
            o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            lineList->append( o );
        }
        appendLine( lineList );
    }
}

bool Equator::selected()
{
    return Options::showEquator();
}

void Equator::preDraw( SkyPainter *skyp )
{
    KStarsData *data = KStarsData::Instance();
    QColor color( data->colorScheme()->colorNamed( "EqColor" ) );
    skyp->setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );
}

void Equator::draw( SkyPainter *skyp )
{
    if ( ! selected() ) return;
    
    m_label.reset();
    NoPrecessIndex::draw( skyp );

    KStarsData *data = KStarsData::Instance();
    QColor color( data->colorScheme()->colorNamed( "EqColor" ) );
    SkyLabeler::Instance()->setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );
    m_label.draw();

    drawCompassLabels();
}

void Equator::drawCompassLabels() {
#ifndef KSTARS_LITE
    QString label;

    const Projector *proj  = SkyMap::Instance()->projector();
    KStarsData *data       = KStarsData::Instance();
    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    // Set proper color for labels
    QColor color( data->colorScheme()->colorNamed( "CompassColor" ) );
    skyLabeler->setPen( QPen( QBrush(color), 1, Qt::SolidLine) );

    KSNumbers num( data->ut().djd() );
    for( int ra = 0; ra < 23; ra += 2 ) {
        SkyPoint o( ra, 0.0 );
        o.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        bool visible;
        QPointF cpoint = proj->toScreen( &o, false, &visible );
        if ( visible && proj->checkVisibility( &o ) ) {
            label.setNum( o.ra().hour() );
            skyLabeler->drawGuideLabel( cpoint, label, 0.0 );
        }
    }
#endif
}
