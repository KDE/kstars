/***************************************************************************
                          skymapdraw.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Mar 2 2003
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//This file contains drawing functions SkyMap class.

#include <iostream>

#include <QPainter>
#include <QPixmap>

#include "skymap.h"
#include "Options.h"
#include "fov.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "ksutils.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/starobject.h"
#include "skyobjects/ksplanetbase.h"
#include "simclock.h"
#include "observinglist.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/skylabeler.h"
#include "skycomponents/skymapcomposite.h"
#include "skyqpainter.h"
#include "projections/projector.h"
#include "projections/lambertprojector.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/devicemanager.h"
#include "indi/indimenu.h"
#include "indi/indiproperty.h"
#include "indi/indielement.h"
#include "indi/indidevice.h"
#endif

QPointF SkyMap::clipLine( SkyPoint *p1, SkyPoint *p2 )
{
    return m_proj->clipLine(p1,p2);
}

void SkyMap::drawOverlays( QPainter& p ) {
    if( !KStars::Instance() )
        return;

    //draw labels
    SkyLabeler::Instance()->draw(p);

    //draw FOV symbol
    foreach( FOV* fov, KStarsData::Instance()->visibleFOVs ) {
        fov->draw(p, Options::zoomFactor());
    }
    drawTelescopeSymbols( p );
    drawZoomBox( p );

    if ( angularDistanceMode ) {
        updateAngleRuler();
        drawAngleRuler( p );
    }
}

void SkyMap::drawAngleRuler( QPainter &p ) {
    p.setPen( QPen( data->colorScheme()->colorNamed( "AngularRuler" ), 3.0, Qt::DotLine ) );
    p.drawLine(
        toScreen( AngularRuler.point(0) ),
        toScreen( AngularRuler.point(1) ) );
}

void SkyMap::drawZoomBox( QPainter &p ) {
    //draw the manual zoom-box, if it exists
    if ( ZoomRect.isValid() ) {
        p.setPen( QPen( Qt::white, 1.0, Qt::DotLine ) );
        p.drawRect( ZoomRect.x(), ZoomRect.y(), ZoomRect.width(), ZoomRect.height() );
    }
}

void SkyMap::drawObjectLabels( QList<SkyObject*>& labelObjects ) {
    bool checkSlewing = ( slewing || ( clockSlewing && data->clock()->isActive() ) ) && Options::hideOnSlew();
    if ( checkSlewing && Options::hideLabels() ) return;

    float Width = m_Scale * width();
    float Height = m_Scale * height();

    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    skyLabeler->resetFont();      // use the zoom dependent font

    skyLabeler->setPen( data->colorScheme()->colorNamed( "UserLabelColor" ) );

    bool drawPlanets    = Options::showSolarSystem() && !(checkSlewing && Options::hidePlanets());
    bool drawComets     = drawPlanets && Options::showComets();
    bool drawAsteroids  = drawPlanets && Options::showAsteroids();
    bool drawMessier    = Options::showDeepSky() && ( Options::showMessier() || Options::showMessierImages() ) && !(checkSlewing && Options::hideMessier() );
    bool drawNGC        = Options::showDeepSky() && Options::showNGC() && !(checkSlewing && Options::hideNGC() );
    bool drawIC         = Options::showDeepSky() && Options::showIC() && !(checkSlewing && Options::hideIC() );
    bool drawOther      = Options::showDeepSky() && Options::showOther() && !(checkSlewing && Options::hideOther() );
    bool drawStars      = Options::showStars();
    bool hideFaintStars = checkSlewing && Options::hideStars();

    //Attach a label to the centered object
    if ( focusObject() != NULL && Options::useAutoLabel() ) {
        QPointF o = toScreen( focusObject() );
        skyLabeler->drawNameLabel( focusObject(), o );
    }

    foreach ( SkyObject *obj, labelObjects ) {
        //Only draw an attached label if the object is being drawn to the map
        //reproducing logic from other draw funcs here...not an optimal solution
        if ( obj->type() == SkyObject::STAR || obj->type() == SkyObject::CATALOG_STAR || obj->type() == SkyObject::MULT_STAR ) {
            if ( ! drawStars ) continue;
            //            if ( obj->mag() > Options::magLimitDrawStar() ) continue;
            if ( hideFaintStars && obj->mag() > Options::magLimitHideStar() ) continue;
        }
        if ( obj->type() == SkyObject::PLANET ) {
            if ( ! drawPlanets ) continue;
            if ( obj->name() == "Sun" && ! Options::showSun() ) continue;
            if ( obj->name() == i18n( "Mercury" ) && ! Options::showMercury() ) continue;
            if ( obj->name() == i18n( "Venus" ) && ! Options::showVenus() ) continue;
            if ( obj->name() == "Moon" && ! Options::showMoon() ) continue;
            if ( obj->name() == i18n( "Mars" ) && ! Options::showMars() ) continue;
            if ( obj->name() == i18n( "Jupiter" ) && ! Options::showJupiter() ) continue;
            if ( obj->name() == i18n( "Saturn" ) && ! Options::showSaturn() ) continue;
            if ( obj->name() == i18n( "Uranus" ) && ! Options::showUranus() ) continue;
            if ( obj->name() == i18n( "Neptune" ) && ! Options::showNeptune() ) continue;
            if ( obj->name() == i18n( "Pluto" ) && ! Options::showPluto() ) continue;
        }
        if ( (obj->type() >= SkyObject::OPEN_CLUSTER && obj->type() <= SkyObject::GALAXY) ||
             (obj->type() >= SkyObject::ASTERISM) ||
             (obj->type() <= SkyObject::QUASAR) )
        {
            if ( ((DeepSkyObject*)obj)->isCatalogM() && ! drawMessier ) continue;
            if ( ((DeepSkyObject*)obj)->isCatalogNGC() && ! drawNGC ) continue;
            if ( ((DeepSkyObject*)obj)->isCatalogIC() && ! drawIC ) continue;
            if ( ((DeepSkyObject*)obj)->isCatalogNone() && ! drawOther ) continue;
        }
        if ( obj->type() == SkyObject::COMET && ! drawComets ) continue;
        if ( obj->type() == SkyObject::ASTEROID && ! drawAsteroids ) continue;

        if ( ! checkVisibility( obj ) ) continue;
        QPointF o = toScreen( obj );
        if ( ! (o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) ) continue;

        skyLabeler->drawNameLabel( obj, o );
    }

    skyLabeler->useStdFont();   // use the StdFont for the guides.
}

void SkyMap::drawTelescopeSymbols(QPainter &psky)
{
#ifdef HAVE_INDI_H
    KStars* kstars = KStars::Instance();
    if( !kstars )
        return;

    INDI_P *eqNum;
    INDI_P *portConnect;
    INDI_E *lp;
    INDIMenu *devMenu = kstars->indiMenu();
    SkyPoint indi_sp;

    if (!Options::showTargetCrosshair() || devMenu == NULL)
        return;

    psky.setPen( QPen( QColor( data->colorScheme()->colorNamed("TargetColor" ) ) ) );
    psky.setBrush( Qt::NoBrush );
    float pxperdegree = Options::zoomFactor()/57.3;

    //fprintf(stderr, "in draw telescope function with managerssize of %d\n", devMenu->managers.size());
    for ( int i=0; i < devMenu->managers.size(); i++ ) {
        for ( int j=0; j < devMenu->managers.at(i)->indi_dev.size(); j++ ) {
            bool useAltAz = false;
            bool useJ2000 = false;

            // make sure the dev is on first
            if (devMenu->managers.at(i)->indi_dev.at(j)->isOn()) {
                portConnect = devMenu->managers.at(i)->indi_dev.at(j)->findProp("CONNECTION");
                if( !portConnect || portConnect->state == PS_BUSY )
                    return;

                eqNum = devMenu->managers.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_EOD_COORD");
                if (eqNum == NULL) {
                    eqNum = devMenu->managers.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_COORD");
                    if (eqNum == NULL) {
                        eqNum = devMenu->managers.at(i)->indi_dev.at(j)->findProp("HORIZONTAL_COORD");
                        if (eqNum == NULL)
                            continue;
                        else
                            useAltAz = true;
                    } else {
                        useJ2000 = true;
                    }
                }

                // make sure it has RA and DEC properties
                if ( eqNum) {
                    //fprintf(stderr, "Looking for RA label\n");
                    if (useAltAz) {
                        lp = eqNum->findElement("AZ");
                        if (!lp)
                            continue;

                        dms azDMS(lp->value);

                        lp = eqNum->findElement("ALT");
                        if (!lp)
                            continue;

                        dms altDMS(lp->value);
                        indi_sp.setAz(azDMS);
                        indi_sp.setAlt(altDMS);
                    } else {
                        lp = eqNum->findElement("RA");
                        if (!lp)
                            continue;

                        // express hours in degrees on the celestial sphere
                        dms raDMS(lp->value);
                        raDMS.setD ( raDMS.Degrees() * 15.0);

                        lp = eqNum->findElement("DEC");
                        if (!lp)
                            continue;

                        dms decDMS(lp->value);

                        indi_sp.setRA(raDMS);
                        indi_sp.setDec(decDMS);

                        if (useJ2000) {
                            indi_sp.setRA0(raDMS);
                            indi_sp.setDec0(decDMS);
                            indi_sp.apparentCoord( (double) J2000, data->ut().djd());
                        }

                        if ( Options::useAltAz() )
                            indi_sp.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                    }

                    QPointF P = toScreen( &indi_sp );
                    if ( Options::useAntialias() ) {
                        float s1 = 0.5*pxperdegree;
                        float s2 = pxperdegree;
                        float s3 = 2.0*pxperdegree;

                        float x0 = P.x();        float y0 = P.y();
                        float x1 = x0 - 0.5*s1;  float y1 = y0 - 0.5*s1;
                        float x2 = x0 - 0.5*s2;  float y2 = y0 - 0.5*s2;
                        float x3 = x0 - 0.5*s3;  float y3 = y0 - 0.5*s3;

                        //Draw radial lines
                        psky.drawLine( QPointF(x1, y0), QPointF(x3, y0) );
                        psky.drawLine( QPointF(x0+s2, y0), QPointF(x0+0.5*s1, y0) );
                        psky.drawLine( QPointF(x0, y1), QPointF(x0, y3) );
                        psky.drawLine( QPointF(x0, y0+0.5*s1), QPointF(x0, y0+s2) );
                        //Draw circles at 0.5 & 1 degrees
                        psky.drawEllipse( QRectF(x1, y1, s1, s1) );
                        psky.drawEllipse( QRectF(x2, y2, s2, s2) );

                        psky.drawText( QPointF(x0+s2+2., y0), QString(devMenu->managers.at(i)->indi_dev.at(j)->label) );
                    } else {
                        int s1 = int( 0.5*pxperdegree );
                        int s2 = int( pxperdegree );
                        int s3 = int( 2.0*pxperdegree );

                        int x0 = int(P.x());   int y0 = int(P.y());
                        int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
                        int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
                        int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

                        //Draw radial lines
                        psky.drawLine( QPoint(x1, y0),      QPoint(x3, y0) );
                        psky.drawLine( QPoint(x0+s2, y0),   QPoint(x0+s1/2, y0) );
                        psky.drawLine( QPoint(x0, y1),      QPoint(x0, y3) );
                        psky.drawLine( QPoint(x0, y0+s1/2), QPoint(x0, y0+s2) );
                        //Draw circles at 0.5 & 1 degrees
                        psky.drawEllipse( QRect(x1, y1, s1, s1) );
                        psky.drawEllipse( QRect(x2, y2, s2, s2) );

                        psky.drawText( QPoint(x0+s2+2, y0), QString(devMenu->managers.at(i)->indi_dev.at(j)->label) );
                    }
                }
            }
        }
    }
#endif
}

void SkyMap::exportSkyImage( QPaintDevice *pd ) {
    #warning Still have to port exportSkyImage()
    #if 0
    QPainter p;
    p.begin( pd );
    p.setRenderHint(QPainter::Antialiasing, Options::useAntialias() );

    //scale image such that it fills 90% of the x or y dimension on the paint device
    double xscale = double(p.device()->width()) / double(width());
    double yscale = double(p.device()->height()) / double(height());
    m_Scale = (xscale < yscale) ? xscale : yscale;

    //Now that we have changed the map scale, we need to re-run 
    //StarObject::initImages() to get scaled pixmaps
    SkyQPainter::initImages();

    int pdWidth = int( m_Scale * width() );
    int pdHeight = int( m_Scale * height() );
    int x1 = int( 0.5*(p.device()->width()  - pdWidth) );
    int y1 = int( 0.5*(p.device()->height()  - pdHeight) );

    p.setClipRect( QRect( x1, y1, pdWidth, pdHeight ) );
    p.setClipping( true );

    //Fill background with sky color
    p.fillRect( x1, y1, pdWidth, pdHeight, QBrush( data->colorScheme()->colorNamed( "SkyColor" ) ) );

    if ( x1 || y1 ) p.translate( x1, y1 );

    data->skyComposite()->draw( p );
    drawObservingList( p );

    p.end();

    //Reset scale for screen drawing
    m_Scale = 1.0;
    SkyQPainter::initImages();
    #endif
}

