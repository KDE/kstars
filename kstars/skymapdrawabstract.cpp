/***************************************************************************
                 skymapdrawabstract.cpp  -  K Desktop Planetarium
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

// This file implements the class SkyMapDrawAbstract, and is almost
// identical to the older skymapdraw.cpp file, written by Jason Harris

#include <iostream>

#include <QPainter>
#include <QPixmap>

#include "skymapdrawabstract.h"
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

SkyMapDrawAbstract::SkyMapDrawAbstract( SkyMap *sm ) : 
    m_KStarsData( KStarsData::Instance() ), m_SkyMap( sm ) {
    m_fpstime.start();
    m_framecount = 0;
}

void SkyMapDrawAbstract::drawOverlays( QPainter& p ) {
    if( !KStars::Instance() )
        return;

    //draw labels
    SkyLabeler::Instance()->draw(p);

    //draw FOV symbol
    foreach( FOV* fov, m_KStarsData->getVisibleFOVs() ) {
        fov->draw(p, Options::zoomFactor());
    }
    drawTelescopeSymbols( p );
    drawZoomBox( p );

    // FIXME: Maybe we should take care of this differently. Maybe
    // drawOverlays should remain in SkyMap, since it just calls
    // certain drawing functions which are implemented in
    // SkyMapDrawAbstract. Really, it doesn't draw anything on its
    // own.
    if ( m_SkyMap->rulerMode ) {
        m_SkyMap->updateAngleRuler();
        drawAngleRuler( p );
    }
}

void SkyMapDrawAbstract::drawAngleRuler( QPainter &p ) {
    //FIXME use sky painter.
    p.setPen( QPen( m_KStarsData->colorScheme()->colorNamed( "AngularRuler" ), 3.0, Qt::DotLine ) );
    p.drawLine(
               m_SkyMap->m_proj->toScreen( m_SkyMap->AngularRuler.point(0) ), // FIXME: More ugliness. m_proj should probably be a single-instance class, or we should have our own instance etc.
               m_SkyMap->m_proj->toScreen( m_SkyMap->AngularRuler.point(1) ) ); // FIXME: Again, AngularRuler should be something better -- maybe a class in itself. After all it's used for more than one thing after we integrate the StarHop feature.
}

void SkyMapDrawAbstract::drawZoomBox( QPainter &p ) {
    //draw the manual zoom-box, if it exists
    if ( m_SkyMap->ZoomRect.isValid() ) {
        p.setPen( QPen( Qt::white, 1.0, Qt::DotLine ) );
        p.drawRect( m_SkyMap->ZoomRect.x(), m_SkyMap->ZoomRect.y(), m_SkyMap->ZoomRect.width(), m_SkyMap->ZoomRect.height() );
    }
}

void SkyMapDrawAbstract::drawObjectLabels( QList<SkyObject*>& labelObjects ) {
    bool checkSlewing = ( m_SkyMap->slewing || ( m_SkyMap->clockSlewing && m_KStarsData->clock()->isActive() ) ) && Options::hideOnSlew();
    if ( checkSlewing && Options::hideLabels() ) return;

    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    skyLabeler->resetFont();      // use the zoom dependent font

    skyLabeler->setPen( m_KStarsData->colorScheme()->colorNamed( "UserLabelColor" ) );

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
    if ( m_SkyMap->focusObject() != NULL && Options::useAutoLabel() ) {
        QPointF o = m_SkyMap->m_proj->toScreen( m_SkyMap->focusObject() ); // FIXME: Same thing. m_proj should be accessible here.
        skyLabeler->drawNameLabel( m_SkyMap->focusObject(), o );
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
             (obj->type() <= SkyObject::QUASAR) ||
             (obj->type() <= SkyObject::RADIO_SOURCE))
        {
            if ( ((DeepSkyObject*)obj)->isCatalogM() && ! drawMessier ) continue;
            if ( ((DeepSkyObject*)obj)->isCatalogNGC() && ! drawNGC ) continue;
            if ( ((DeepSkyObject*)obj)->isCatalogIC() && ! drawIC ) continue;
            if ( ((DeepSkyObject*)obj)->isCatalogNone() && ! drawOther ) continue;
        }
        if ( obj->type() == SkyObject::COMET && ! drawComets ) continue;
        if ( obj->type() == SkyObject::ASTEROID && ! drawAsteroids ) continue;

        if ( ! m_SkyMap->m_proj->checkVisibility( obj ) ) continue; // FIXME: m_proj should be a member of this class.
        QPointF o = m_SkyMap->m_proj->toScreen( obj );
        if ( ! m_SkyMap->m_proj->onScreen(o) ) continue;

        skyLabeler->drawNameLabel( obj, o );
    }

    skyLabeler->useStdFont();   // use the StdFont for the guides.
}

void SkyMapDrawAbstract::drawTelescopeSymbols(QPainter &psky)
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

    psky.setPen( QPen( QColor( m_KStarsData->colorScheme()->colorNamed("TargetColor" ) ) ) );
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
                        raDMS.setH(lp->value);

                        lp = eqNum->findElement("DEC");
                        if (!lp)
                            continue;

                        dms decDMS(lp->value);

                        indi_sp.setRA(raDMS);
                        indi_sp.setDec(decDMS);

                        if (useJ2000) {
                            indi_sp.setRA0(raDMS);
                            indi_sp.setDec0(decDMS);
                            indi_sp.apparentCoord( (double) J2000, m_KStarsData->ut().djd());
                        }

                        if ( Options::useAltAz() )
                            indi_sp.EquatorialToHorizontal( m_KStarsData->lst(), m_KStarsData->geo()->lat() );
                    }

                    QPointF P = m_SkyMap->m_proj->toScreen( &indi_sp );
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

void SkyMapDrawAbstract::exportSkyImage( QPaintDevice *pd ) {
    SkyQPainter p(m_SkyMap, pd); // FIXME: Really, we should be doing this differently. We shouldn't be passing m_SkyMap, but rather, this widget.
    p.begin();
    p.setRenderHint(QPainter::Antialiasing, Options::useAntialias() );

    //scale image such that it fills 90% of the x or y dimension on the paint device
    double xscale = double(p.device()->width()) / double(m_SkyMap->width());
    double yscale = double(p.device()->height()) / double(m_SkyMap->height());
    double scale = qMin(xscale,yscale);

    p.scale(scale,scale);

    p.drawSkyBackground();
    m_KStarsData->skyComposite()->draw( &p );
    drawOverlays( p );
    p.end();
}

void SkyMapDrawAbstract::calculateFPS()
{
    if(m_framecount == 25) {
        float sec = m_fpstime.elapsed()/1000.;
        kDebug() << "FPS " << m_framecount/sec;
        m_framecount = 0;
        m_fpstime.restart();
    }
    ++m_framecount;
}
