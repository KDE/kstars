/***************************************************************************
                          horizoncomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "horizoncomponent.h"

#include <QList>
#include <QPointF>

#include "Options.h"
#include "kstarsdata.h"
#include "ksnumbers.h"

#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif

#include "skyobjects/skypoint.h" 
#include "dms.h"
#include "skylabeler.h"
#include "skypainter.h"

#include "projections/projector.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

HorizonComponent::HorizonComponent(SkyComposite *parent )
        : PointListComponent( parent )
{
    KStarsData *data = KStarsData::Instance();
    emitProgressText( i18n("Creating horizon" ) );

    //Define Horizon
    for ( unsigned int i=0; i<NCIRCLE; ++i ) {
        SkyPoint *o = new SkyPoint();
        o->setAz( i*360./NCIRCLE );
        o->setAlt( 0.0 );

        o->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        pointList().append( o );
    }
}

HorizonComponent::~HorizonComponent()
{}

bool HorizonComponent::selected()
{
    return Options::showHorizon() || Options::showGround();
}

void HorizonComponent::update( KSNumbers * )
{
    if ( ! selected() )
        return;
    KStarsData *data = KStarsData::Instance();
    foreach ( SkyPoint *p, pointList() ) {
        p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
    }
}

//Only half of the Horizon circle is ever valid, the invalid half is "behind" the observer.
//To select the valid half, we start with the azimuth of the central focus point.
//The valid horizon points have azimuth between this az +- 90
//This is true for Equatorial or Horizontal coordinates
void HorizonComponent::draw( SkyPainter *skyp )
{
    if( !selected() )
        return;

    KStarsData *data = KStarsData::Instance();
    
    skyp->setPen( QPen( QColor( data->colorScheme()->colorNamed( "HorzColor" ) ), 2, Qt::SolidLine ) );

    if ( Options::showGround() )
        skyp->setBrush( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );
    else
        skyp->setBrush( Qt::NoBrush );

    SkyPoint labelPoint;
    bool drawLabel;
    skyp->drawHorizon( Options::showGround(), &labelPoint, &drawLabel );

    if( drawLabel ) {
        SkyPoint labelPoint2;
        labelPoint2.setAlt(0.0);
        labelPoint2.setAz( labelPoint.az().Degrees() + 1.0 );
        labelPoint2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );
    }

    drawCompassLabels();
}

void HorizonComponent::drawCompassLabels() {
#ifndef KSTARS_LITE
    SkyPoint c;
    QPointF cpoint;
    bool visible;

    const Projector *proj = SkyMap::Instance()->projector();
    KStarsData *data = KStarsData::Instance();
    
    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    // Set proper color for labels
    QColor color( data->colorScheme()->colorNamed( "CompassColor" ) );
    skyLabeler->setPen( QPen( QBrush(color), 1, Qt::SolidLine) );

    double az = -0.01;
    static QString name[8];
    name[0] = i18nc( "Northeast", "NE" );
    name[1] = i18nc( "East", "E" );
    name[2] = i18nc( "Southeast", "SE" );
    name[3] = i18nc( "South", "S" );
    name[4] = i18nc( "Southwest", "SW" );
    name[5] = i18nc( "West", "W" );
    name[6] = i18nc( "Northwest", "NW" );
    name[7] = i18nc( "North", "N" );

    for ( int i = 0; i < 8; i++ ) {
        az += 45.0;
        c.setAz( az );
        c.setAlt( 0.0 );
        if ( !Options::useAltAz() ) {
            c.HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        }
        
        cpoint = proj->toScreen( &c, false, &visible );
        if ( visible && proj->onScreen(cpoint) ) {
            skyLabeler->drawGuideLabel( cpoint, name[i], 0.0 );
        }
    }
#endif
}
