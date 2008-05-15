/***************************************************************************
              solarsystemlistcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/22/09
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

#include "solarsystemlistcomponent.h"
#include "solarsystemcomposite.h"

#include <QPainter>
#include <QPen>
#include <klocale.h>

#include "Options.h"
#include "ksplanet.h"
#include "ksplanetbase.h"
#include "kstarsdata.h"
#include "skymap.h"

SolarSystemListComponent::SolarSystemListComponent( SolarSystemComposite *p, bool (*visibleMethod)(), int msize )
        : ListComponent( (SkyComponent*)p, visibleMethod )
{
    m_Earth = p->earth();
    minsize = msize;
}

SolarSystemListComponent::~SolarSystemListComponent()
{
    //Object deletes handled by parent class (ListComponent)
}

void SolarSystemListComponent::update(KStarsData *data, KSNumbers * ) {
    if ( visible() ) {
        foreach ( SkyObject *o, objectList() ) {
            KSPlanetBase *p = (KSPlanetBase*)o;
            p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }
    }
}

void SolarSystemListComponent::updatePlanets(KStarsData *data, KSNumbers *num ) {
    if ( visible() ) {
        foreach ( SkyObject *o, objectList() ) {
            KSPlanetBase *p = (KSPlanetBase*)o;
            p->findPosition( num, data->geo()->lat(), data->lst(), earth() );
            p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

            if ( p->hasTrail() )
                p->updateTrail( data->lst(), data->geo()->lat() );
        }
    }
}

bool SolarSystemListComponent::addTrail( SkyObject *oTarget ) {
  //DEBUG
  kDebug() << oTarget->name() << endl;

    foreach( SkyObject *o, objectList() ) {
        if ( o == oTarget ) {
            ((KSPlanetBase*)o)->addToTrail();
            m_TrailList.append( o );
            return true;
        }
    }
    return false;
}

bool SolarSystemListComponent::hasTrail( SkyObject *oTarget, bool &found ) {
    foreach( SkyObject *o, m_TrailList ) {
        if ( o == oTarget ) {
            found = true;
            return ((KSPlanetBase*)o)->hasTrail();
        }
    }
    return false;
}

bool SolarSystemListComponent::removeTrail( SkyObject *oTarget ) {
    foreach( SkyObject *o, m_TrailList ) {
        if ( o == oTarget ) {
            ((KSPlanetBase*)o)->clearTrail();
            if ( m_TrailList.indexOf( o ) >= 0 )
                m_TrailList.removeAt( m_TrailList.indexOf( o ) );
            return true;
        }
    }
    return false;
}

void SolarSystemListComponent::clearTrailsExcept( SkyObject *exOb ) {
    foreach( SkyObject *o, m_TrailList ) {
        if ( o != exOb ) {
            ((KSPlanetBase*)o)->clearTrail();
            if ( m_TrailList.indexOf( o ) >= 0 )
                m_TrailList.removeAt( m_TrailList.indexOf( o ) );
        }
    }
}

void SolarSystemListComponent::drawTrails( QPainter& psky ) {
    if ( ! visible() ) return;

    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    QColor tcolor1 = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    QColor tcolor2 = QColor( data->colorScheme()->colorNamed( "SkyColor" ) );

    foreach ( SkyObject *obj, m_TrailList ) {
        //DEBUG
        kDebug() << obj->name() << endl;

        KSPlanetBase *ksp = (KSPlanetBase*)obj;
        if ( ! ksp->hasTrail() ) continue;

        SkyPoint p = ksp->trail().first();
        QPointF o = map->toScreen( &p );
        QPointF oLast( o );

        bool doDrawLine(false);
        int i = 0;
        int n = ksp->trail().size();

        if ( ( o.x() >= -1000. && o.x() <= Width+1000. && o.y() >=-1000. && o.y() <= Height+1000. ) ) {
            doDrawLine = true;
        }

        psky.setPen( QPen( tcolor1, 1 ) );
        bool firstPoint( true );
        foreach ( p, ksp->trail() ) {
            if ( firstPoint ) { firstPoint = false; continue; } //skip first point

            if ( Options::fadePlanetTrails() ) {
                //Define interpolated color
                QColor tcolor = QColor(
                                    (i*tcolor1.red()   + (n-i)*tcolor2.red())/n,
                                    (i*tcolor1.green() + (n-i)*tcolor2.green())/n,
                                    (i*tcolor1.blue()  + (n-i)*tcolor2.blue())/n );
                ++i;
                psky.setPen( QPen( tcolor, 1 ) );
            }

            o = map->toScreen( &p );
            if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

                //Want to disable line-drawing if this point and the last are both outside bounds of display.
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
