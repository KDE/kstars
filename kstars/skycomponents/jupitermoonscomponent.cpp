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

#include "skyobjects/jupitermoons.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skypoint.h" 
#include "skyobjects/trailobject.h" 
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

    for ( uint i=0; i<4; i++ ) 
        objectNames(SkyObject::MOON).append( jmoons->name(i) );
}

void JupiterMoonsComponent::update( KStarsData *data, KSNumbers * )
{
    if ( visible() )
        jmoons->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void JupiterMoonsComponent::updateMoons( KStarsData *, KSNumbers *num )
{
    if ( visible() )
        jmoons->findPosition( num, (KSPlanet*)(m_Jupiter->skyObject()), (KSSun*)(parent()->findByName( "Sun" )) );
}

SkyObject* JupiterMoonsComponent::findByName( const QString &name ) { 
    for ( uint i=0; i<4; ++i ) {
        TrailObject *moon = jmoons->moon(i);
        if ( QString::compare( moon->name(), name, Qt::CaseInsensitive ) == 0 || 
            QString::compare( moon->longname(), name, Qt::CaseInsensitive ) == 0 ||
            QString::compare( moon->name2(), name, Qt::CaseInsensitive ) == 0 )
            return moon;
    }

    return 0;
}

SkyObject* JupiterMoonsComponent::objectNearest( SkyPoint *p, double &maxrad ) { 
    SkyObject *oBest = 0;
    for ( uint i=0; i<4; ++i ) {
        SkyObject *moon = (SkyObject*)(jmoons->moon(i));
        double r = moon->angularDistanceTo( p ).Degrees();
        if ( r < maxrad ) {
            maxrad = r;
            oBest = moon;
        }
    }

    return oBest;

}

bool JupiterMoonsComponent::addTrail( SkyObject *o ) {
    for ( uint i=0; i<4; i++ ) {
        if ( o == jmoons->moon(i) ) {
            jmoons->moon(i)->addToTrail();
            return true;
        }
    }

    return false;
}

bool JupiterMoonsComponent::hasTrail( SkyObject *o, bool &found ) {
    for ( uint i=0; i<4; i++ ) {
        if ( o == jmoons->moon(i) ) {
            found = true;
            return jmoons->moon(i)->hasTrail();
        }
    }

    return false;
}

bool JupiterMoonsComponent::removeTrail( SkyObject *o ) {
    for ( uint i=0; i<4; i++ ) {
        if ( o == jmoons->moon(i) ) {
            jmoons->moon(i)->clearTrail();
            return true;
        }
    }

    return false;
}

void JupiterMoonsComponent::clearTrailsExcept( SkyObject *exOb ) {
    for ( uint i=0; i<4; i++ ) 
        if ( exOb != jmoons->moon(i) ) 
            jmoons->moon(i)->clearTrail();
}

void JupiterMoonsComponent::draw( QPainter& psky )
{
    if ( ! Options::showJupiter() ) return;

    SkyMap *map = SkyMap::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    psky.setPen( QPen( QColor( "white" ) ) );

    if ( Options::zoomFactor() <= 10.*MINZOOM ) return;

    //In order to get the z-order right for the moons and Jupiter,
    //we need to first draw the moons that are further away than Jupiter,
    //then re-draw Jupiter, then draw the moons nearer than Jupiter.
    QList<QPointF> frontMoons;
    for ( unsigned int i=0; i<4; ++i ) {
        QPointF o = map->toScreen( jmoons->moon(i) );

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
    m_Jupiter->draw( psky );

    //Now draw the remaining moons, as stored in frontMoons
    psky.setPen( QPen( QColor( "white" ) ) );
    foreach ( const QPointF &o, frontMoons ) {
        psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
    }

    //Draw Moon name labels if at high zoom
    if ( ! (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM) ) return;
    for ( unsigned int i=0; i<4; ++i ) {
        QPointF o = map->toScreen( jmoons->moon(i) );

        if ( ! map->onScreen( o ) ) continue;

        SkyLabeler::AddLabel( o, jmoons->moon(i), SkyLabeler::JUPITER_MOON_LABEL );
    }
}

void JupiterMoonsComponent::drawTrails( QPainter& psky ) {
    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    QColor tcolor1 = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    QColor tcolor2 = QColor( data->colorScheme()->colorNamed( "SkyColor" ) );

    for ( uint i=0; i<4; ++i ) {
        TrailObject *moon = jmoons->moon(i);

        if ( ! visible() || ! moon->hasTrail() ) continue;

        SkyPoint p = moon->trail().first();
        QPointF o = map->toScreen( &p );
        QPointF oLast( o );

        bool doDrawLine(false);
        int j = 0;
        int n = moon->trail().size();

        if ( ( o.x() >= -1000. && o.x() <= Width+1000.
                && o.y() >= -1000. && o.y() <= Height+1000. ) ) {
            //		psky.moveTo(o.x(), o.y());
            doDrawLine = true;
        }

        psky.setPen( QPen( tcolor1, 1 ) );
        bool firstPoint( true );
        foreach ( p, moon->trail() ) {
            if ( firstPoint ) { firstPoint = false; continue; } //skip first point

            if ( Options::fadePlanetTrails() ) {
                //Define interpolated color
                QColor tcolor = QColor(
                                    (j*tcolor1.red()   + (n-j)*tcolor2.red())/n,
                                    (j*tcolor1.green() + (n-j)*tcolor2.green())/n,
                                    (j*tcolor1.blue()  + (n-j)*tcolor2.blue())/n );
                ++j;
                psky.setPen( QPen( tcolor, 1 ) );
            }

            o = map->toScreen( &p );
            if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

                //Want to disable line-drawing if this point and the last are both outside bounds of display.
                //FIXME: map->rect() should return QRectF
                if ( ! map->rect().contains( o.toPoint() ) && ! map->rect().contains( oLast.toPoint() ) ) doDrawLine = false;
    
                if ( doDrawLine ) {
                    psky.drawLine( oLast, o );
                } else {
                    doDrawLine = true;
                }
            }
    
            oLast = o;
        }
    }
}

