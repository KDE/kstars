/***************************************************************************
                          solarsystemsinglecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
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

#include "solarsystemsinglecomponent.h"
#include "solarsystemcomposite.h"
#include "skycomponent.h"

#include <QPainter>

#include "dms.h"
#include "kstarsdata.h"
#include "skyobjects/starobject.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksplanet.h"
#include "skymap.h"
#include "Options.h"
#include "skylabeler.h"

SolarSystemSingleComponent::SolarSystemSingleComponent(SolarSystemComposite *parent, KSPlanetBase *kspb, bool (*visibleMethod)(), int msize) :
    SingleComponent( parent, visibleMethod )
{
    setStoredObject( kspb );
    m_Earth = parent->earth();
}

SolarSystemSingleComponent::~SolarSystemSingleComponent()
{
    //Object deletes handled by parent class (SingleComponent)
}

void SolarSystemSingleComponent::init() {
    ksp()->loadData();
    if ( ! ksp()->name().isEmpty() )
        objectNames(ksp()->type()).append( ksp()->name() );
    if ( ! ksp()->longname().isEmpty() && ksp()->longname() != ksp()->name() )
        objectNames(ksp()->type()).append( ksp()->longname() );
}

void SolarSystemSingleComponent::update(KSNumbers *) {
    KStarsData *data = KStarsData::Instance(); 
    if ( visible() )
        ksp()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void SolarSystemSingleComponent::updatePlanets(KSNumbers *num) {
    if ( visible() ) {
        KStarsData *data = KStarsData::Instance(); 
        ksp()->findPosition( num, data->geo()->lat(), data->lst(), m_Earth );
        ksp()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        if ( ksp()->hasTrail() )
            ksp()->updateTrail( data->lst(), data->geo()->lat() );
    }
}

bool SolarSystemSingleComponent::addTrail( SkyObject *o ) {
    if ( o == skyObject() ) {
        ksp()->addToTrail();
        return true;
    }
    return false;
}

bool SolarSystemSingleComponent::hasTrail( SkyObject *o, bool &found ) {
    if ( o == skyObject() ) {
        found = true;
        return ksp()->hasTrail();
    }
    return false;
}

bool SolarSystemSingleComponent::removeTrail( SkyObject *o ) {
    if ( o == skyObject() ) {
        ksp()->clearTrail();
        return true;
    }
    return false;
}

void SolarSystemSingleComponent::clearTrailsExcept( SkyObject *exOb ) {
    if ( exOb != skyObject() )
        ksp()->clearTrail();
}

void SolarSystemSingleComponent::draw( QPainter &psky ) {
    if( ! visible() )
        return;

    SkyMap *map = SkyMap::Instance();

    //TODO: default values for 2nd & 3rd arg. of SkyMap::checkVisibility()
    if ( ! map->checkVisibility( ksp() ) )
        return;

    float Width = map->scale() * map->width();

    float sizemin = 1.0;
    if( ksp()->name() == "Sun" || ksp()->name() == "Moon" )
        sizemin = 8.0;
    sizemin *= map->scale();

    //TODO: KSPlanetBase needs a color property, and someone has to set the planet's color
    psky.setPen( ksp()->color() );
    psky.setBrush( ksp()->color() );
    QPointF o = map->toScreen( ksp() );

    //Is planet onscreen?
    if( ! map->onScreen( o ) )
        return;

    float fakeStarSize = ( 10.0 + log10( Options::zoomFactor() ) - log10( MINZOOM ) ) * ( 10 - ksp()->mag() ) / 10;
    if( fakeStarSize > 15.0 ) 
        fakeStarSize = 15.0;
    float size = ksp()->angSize() * map->scale() * dms::PI * Options::zoomFactor()/10800.0;
    if ( size < fakeStarSize && ksp()->name() != "Sun" && ksp()->name() != "Moon" ) {
        // Draw them as bright stars of appropriate color instead of images
        QString SpecType;
        KStarsData *data = KStarsData::Instance();
        SpecType = "B0";
        if( ksp()->name() == i18n("Mars") ) {
            SpecType = "K5";
        }
        if( ksp()->name() == i18n("Jupiter") || ksp()->name() == i18n("Mercury") || ksp()->name() == i18n("Saturn") ) {
            SpecType = "F5";
        }
        StarObject fakeStar( 0, 0, 0, QString(), QString(), SpecType, 0.0, 0.0, 0.0, false, false, 0); // TODO: Choose the spectral type based on the colour
        if( size < 1.0 )
            size = 1.0;
        fakeStar.draw( psky, o.x(), o.y(), fakeStarSize, (data->colorScheme()->starColorMode() == 0), data->colorScheme()->starColorIntensity(), false );
    }
    else {
        //Draw planet image if:
        if( size < sizemin )
            size = sizemin;
        if( Options::showPlanetImages() &&  //user wants them,
             //FIXME:					int(Options::zoomFactor()) >= int(zoommin) &&  //zoomed in enough,
             ! ksp()->image()->isNull() &&  //image loaded ok,
             size < Width ) {  //and size isn't too big.

            //Image size must be modified to account for possibility that rotated image's
            //size is bigger than original image size.  The rotated image is a square
            //superscribed on the original image.  The superscribed square is larger than
            //the original square by a factor of (cos(t) + sin(t)) where t is the angle by
            //which the two squares are rotated (in our case, equal to the position angle +
            //the north angle, reduced between 0 and 90 degrees).
            //The proof is left as an exercise to the student :)
            dms pa( map->findPA( ksp(), o.x(), o.y() ) );
            double spa, cpa;
            pa.SinCos( spa, cpa );
            cpa = fabs(cpa);
            spa = fabs(spa);
            size = size * (cpa + spa);

            //Quick and dirty fix to prevent a crash.
            //FIXME: Need to figure out why the size is sometimes NaN
            if ( isnan( size ) ) {
                size = 10.0;
            }

            //Because Saturn has rings, we inflate its image size by a factor 2.5
            if ( ksp()->name() == "Saturn" ) size = int(2.5*size);

            //FIXME: resize_mult ??
            //				if (resize_mult != 1) {
            //					size *= resize_mult;
            //				}

            ksp()->scaleRotateImage( size, pa.Degrees() );
            float x1 = o.x() - 0.5*ksp()->image()->width();
            float y1 = o.y() - 0.5*ksp()->image()->height();
            psky.drawImage( QPointF(x1, y1), *( ksp()->image() ) );
        }
        else { //Otherwise, draw a simple circle.
            psky.drawEllipse( QRectF(o.x()-0.5*size, o.y()-0.5*size, size, size) );
        }
    }
    //draw Name
    if ( ! Options::showPlanetNames() )
        return;

    SkyLabeler::AddLabel( o, ksp(), SkyLabeler::PLANET_LABEL );
}

void SolarSystemSingleComponent::drawTrails( QPainter& psky ) {
    if( ! visible() || ! ksp()->hasTrail() )
        return;

    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    SkyPoint p = ksp()->trail().first();
    QPointF o = map->toScreen( &p );
    QPointF oLast( o );

    bool doDrawLine(false);
    int i = 0;
    int n = ksp()->trail().size();

    if ( ( o.x() >= -1000. && o.x() <= Width+1000.
            && o.y() >= -1000. && o.y() <= Height+1000. ) ) {
        //		psky.moveTo(o.x(), o.y());
        doDrawLine = true;
    }

    bool firstPoint( true );
    QColor tcolor = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    psky.setPen( QPen( tcolor, 1 ) );
    foreach ( p, ksp()->trail() ) {
        if( firstPoint ) {
            firstPoint = false;
            continue;
        } //skip first point

        if ( Options::fadePlanetTrails() ) {
            tcolor.setAlphaF(static_cast<qreal>(i)/static_cast<qreal>(n));
            ++i;
            psky.setPen( QPen( tcolor, 1 ) );
        }

        o = map->toScreen( &p );
        if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

            //Want to disable line-drawing if this point and the last are both outside bounds of display.
            //FIXME: map->rect() should return QRectF
            if( !map->rect().contains( o.toPoint() ) && ! map->rect().contains( oLast.toPoint() ) )
                doDrawLine = false;

            if ( doDrawLine ) {
                psky.drawLine( oLast, o );
            } else {
                doDrawLine = true;
            }
        }

        oLast = o;
    }
}
