/***************************************************************************
                          jupitermoonscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/13/08
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

#include "jupitermoonscomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "jupitermoons.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "solarsystemsinglecomponent.h"
#include "solarsystemcomposite.h"
#include "skylabeler.h"

JupiterMoonsComponent::JupiterMoonsComponent( SkyComponent *p,
        SolarSystemSingleComponent *jupiterComponent,
        bool (*visibleMethod)() ) : SkyComponent( p, visibleMethod )
{
    jmoons = 0;
    m_Jupiter = jupiterComponent;
}

JupiterMoonsComponent::~JupiterMoonsComponent()
{
    delete jmoons;
}

void JupiterMoonsComponent::init(KStarsData *)
{
    jmoons = new JupiterMoons();
}

void JupiterMoonsComponent::update( KStarsData *data, KSNumbers * )
{
    if ( visible() )
        jmoons->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void JupiterMoonsComponent::updateMoons( KStarsData *, KSNumbers *num )
{
    //TODO findPosition should named updatePosition
    if ( visible() )
        jmoons->findPosition( num, (KSPlanet*)(m_Jupiter->skyObject()), (KSSun*)(parent()->findByName( "Sun" )) );
}

void JupiterMoonsComponent::draw( KStars *ks, QPainter& psky )
{
    if ( ! Options::showJupiter() ) return;

    SkyMap *map = ks->map();
    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    psky.setPen( QPen( QColor( "white" ) ) );

    if ( Options::zoomFactor() <= 10.*MINZOOM ) return;

    //In order to get the z-order right for the moons and Jupiter,
    //we need to first draw the moons that are further away than Jupiter,
    //then re-draw Jupiter, then draw the moons nearer than Jupiter.
    QList<QPointF> frontMoons;
    for ( unsigned int i=0; i<4; ++i ) {
        QPointF o = map->toScreen( jmoons->pos(i) );

        if ( ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) ) {
            if ( jmoons->z(i) < 0.0 ) { //Moon is nearer than Jupiter
                frontMoons.append( o );
            } else {
                //Draw Moons that are further than Jupiter
                psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
            }
        }
    }

    //Now redraw Jupiter
    m_Jupiter->draw( ks, psky );

    //Now draw the remaining moons, as stored in frontMoons
    psky.setPen( QPen( QColor( "white" ) ) );
    foreach ( QPointF o, frontMoons ) {
        psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
    }

    //Draw Moon name labels if at high zoom
    if ( ! (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM) ) return;
    for ( unsigned int i=0; i<4; ++i ) {
        QPointF o = map->toScreen( jmoons->pos(i) );

        if ( ! map->onScreen( o ) ) continue;
        //if ( ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) ) {
        float offset = 3.0 * map->scale();

        SkyLabeler::AddLabel( QPointF( o.x() + offset, o.y() + offset),
                              jmoons->name(i), JUPITER_MOON_LABEL );
    }
}
