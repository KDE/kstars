/***************************************************************************
                         saturnmoonscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Mar 13 2009
    copyright            : (C) 2009 by Vipul Kumar Singh
    email                : vipulkrsingh@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "saturnmoonscomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "skyobjects/saturnmoons.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skypoint.h" 
#include "skyobjects/trailobject.h" 
#include "dms.h"
#include "Options.h"
#include "solarsystemsinglecomponent.h"
#include "solarsystemcomposite.h"
#include "skylabeler.h"

SaturnMoonsComponent::SaturnMoonsComponent( SkyComponent *p,
        SolarSystemSingleComponent *saturnComponent,
        bool (*visibleMethod)() ) : SkyComponent( p, visibleMethod )
{
    smoons = 0;
    m_Saturn = saturnComponent;
}

SaturnMoonsComponent::~SaturnMoonsComponent()
{
    delete smoons;
}

void SaturnMoonsComponent::init(KStarsData *)
{
    smoons = new SaturnMoons();

    for ( uint i=0; i<8; i++ ) 
        objectNames(SkyObject::MOON).append( smoons->name(i) );
}

void SaturnMoonsComponent::update( KStarsData *data, KSNumbers * )
{
    if ( visible() )
        smoons->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void SaturnMoonsComponent::updateMoons( KStarsData *, KSNumbers *num )
{
    if ( visible() )
        smoons->findPosition( num, (KSPlanet*)(m_Saturn->skyObject()), (KSSun*)(parent()->findByName( "Sun" )) );
}

SkyObject* SaturnMoonsComponent::findByName( const QString &name ) { 
    for ( uint i=0; i<8; ++i ) {
        TrailObject *moon = smoons->moon(i);
        if ( QString::compare( moon->name(), name, Qt::CaseInsensitive ) == 0 || 
            QString::compare( moon->longname(), name, Qt::CaseInsensitive ) == 0 ||
            QString::compare( moon->name2(), name, Qt::CaseInsensitive ) == 0 )
            return moon;
    }

    return 0;
}

SkyObject* SaturnMoonsComponent::objectNearest( SkyPoint *p, double &maxrad ) { 
    SkyObject *oBest = 0;
    for ( uint i=0; i<8; ++i ) {
        SkyObject *moon = (SkyObject*)(smoons->moon(i));
        double r = moon->angularDistanceTo( p ).Degrees();
        if ( r < maxrad ) {
            maxrad = r;
            oBest = moon;
        }
    }

    return oBest;

}

bool SaturnMoonsComponent::addTrail( SkyObject *o ) {
    for ( uint i=0; i<8; i++ ) {
        if ( o == smoons->moon(i) ) {
            smoons->moon(i)->addToTrail();
            return true;
        }
    }

    return false;
}

bool SaturnMoonsComponent::hasTrail( SkyObject *o, bool &found ) {
    for ( uint i=0; i<8; i++ ) {
        if ( o == smoons->moon(i) ) {
            found = true;
            return smoons->moon(i)->hasTrail();
        }
    }

    return false;
}

bool SaturnMoonsComponent::removeTrail( SkyObject *o ) {
    for ( uint i=0; i<8; i++ ) {
        if ( o == smoons->moon(i) ) {
            smoons->moon(i)->clearTrail();
            return true;
        }
    }

    return false;
}

void SaturnMoonsComponent::clearTrailsExcept( SkyObject *exOb ) {
    for ( uint i=0; i<8; i++ ) 
        if ( exOb != smoons->moon(i) ) 
            smoons->moon(i)->clearTrail();
}

void SaturnMoonsComponent::draw( QPainter& psky )
{
    if ( ! Options::showSaturn() ) return;

    SkyMap *map = SkyMap::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    psky.setPen( QPen( QColor( "white" ) ) );

    if ( Options::zoomFactor() <= 10.*MINZOOM ) return;

    //In order to get the z-order right for the moons and Saturn,
    //we need to first draw the moons that are further away than Saturn,
    //then re-draw Saturn, then draw the moons nearer than Saturn.
    QList<QPointF> frontMoons;
    for ( unsigned int i=0; i<8; ++i ) {
        QPointF o = map->toScreen( smoons->moon(i) );

        if ( ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) ) {
            if ( smoons->z(i) < 0.0 ) { //Moon is nearer than Saturn
                frontMoons.append( o );
            } else {
                //Draw Moons that are further than Saturn
                psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
            }
        }
    }

    //Now redraw Saturn
    m_Saturn->draw( psky );

    //Now draw the remaining moons, as stored in frontMoons
    psky.setPen( QPen( QColor( "white" ) ) );
    foreach ( const QPointF &o, frontMoons ) {
        psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
    }

    //Draw Moon name labels if at high zoom
    if ( ! (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM) ) return;
    for ( unsigned int i=0; i<8; ++i ) {
        QPointF o = map->toScreen( smoons->moon(i) );

        if ( ! map->onScreen( o ) ) continue;

        SkyLabeler::AddLabel( o, smoons->moon(i), SkyLabeler::SATURN_MOON_LABEL );
    }
}

void SaturnMoonsComponent::drawTrails( QPainter& psky ) {
    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    QColor tcolor1 = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    QColor tcolor2 = QColor( data->colorScheme()->colorNamed( "SkyColor" ) );

    for ( uint i=0; i<8; ++i ) {
        TrailObject *moon = smoons->moon(i);

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
