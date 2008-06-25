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

#include <stdlib.h> // abs
#include <math.h> //log10()
#include <iostream>

#include <QPainter>
#include <QPixmap>

#include "skymap.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobject.h"
#include "deepskyobject.h"
#include "starobject.h"
#include "ksplanetbase.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "ksmoon.h"
#include "jupitermoons.h"
#include "infoboxes.h"
#include "simclock.h"
#include "devicemanager.h"
#include "indimenu.h"
#include "indiproperty.h"
#include "indielement.h"
#include "indidevice.h"
#include "observinglist.h"
#include "skycomponents/constellationboundary.h"
#include "skycomponents/skylabeler.h"

void toXYZ(SkyPoint* p, double *x, double *y, double *z) {
    double sinRa, sinDec, cosRa, cosDec;

    p->ra()->SinCos( sinRa, cosRa );
    p->dec()->SinCos( sinDec, cosDec );
    *x = cosDec * cosRa;
    *y = cosDec * sinRa;
    *z = sinDec;
}

QPointF SkyMap::clipLine( SkyPoint *p1, SkyPoint *p2 )
{
    /* ASSUMES p1 was not clipped but p2 was.
     * Return the QPoint that barely clips in the line twixt p1 and p2.
     */              
    int iteration = 15;          // For "perfect" clipping:
    // 2^interations should be >= max pixels/line
    bool isVisible = true;       // so we start at midpoint
    SkyPoint mid;
    QPointF oMid;
    double x, y, z, dx, dy, dz, ra, dec;
    int newx, newy, oldx, oldy;
    oldx = oldy = -10000;        // any old value that is not the first omid

    toXYZ( p1, &x, &y, &z );
    // -jbb printf("\np1: %6.4f %6.4f %6.4f\n", x, y, z);

    toXYZ( p2, &dx, &dy, &dz );

    // -jbb printf("p2: %6.4f %6.4f %6.4f\n", dx, dy, dz);
    dx -= x;
    dy -= y;
    dz -= z;
    // Successive approximation to point on line that just clips.
    while(iteration-- > 0) {
        dx *= .5;
        dy *= .5;
        dz *= .5;
        if ( ! isVisible ) {              // move back toward visible p1
            x -= dx;
            y -= dy;
            z -= dz;
        }
        else {                        // move out toward clipped p2
            x += dx;
            y += dy;
            z += dz;
        }

        // -jbb printf("  : %6.4f %6.4f %6.4f\n", x, y, z);
        // [x, y, z] => [ra, dec]
        ra = atan2( y, x );
        dec = asin( z / sqrt(x*x + y*y + z*z) );

        mid = SkyPoint( ra * 12. / dms::PI, dec * 180. / dms::PI );
        mid.EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        oMid = toScreen( &mid, false, &isVisible );
        newx = (int) oMid.x();
        newy = (int) oMid.y();

        // -jbb printf("new x/y: %4d %4d", newx, newy);
        if ( (oldx == newx) && (oldy == newy) ) {
            break;
        }
        oldx = newx;
        oldy = newy;
    }
    return  oMid;
}

QPoint SkyMap::clipLineI( SkyPoint *p1, SkyPoint *p2 ) {
    QPointF qpf = clipLine( p1, p2 );
    return QPoint( (int) qpf.x(), (int) qpf.y() );
}


void SkyMap::drawOverlays( QPixmap *pm ) {
    if ( ks ) { //only if we are drawing in the GUI window
        QPainter p;
        p.begin( pm );
        p.setRenderHint(QPainter::Antialiasing, Options::useAntialias() );

        if ( Options::showHighlightedCBound() )
            drawHighlightConstellation( p );

        showFocusCoords( true );
        drawBoxes( p );

        //draw FOV symbol
        ks->data()->fovSymbol.draw( p, (float)(Options::fOVSize() * Options::zoomFactor()/57.3/60.0) );
        drawTelescopeSymbols( p );
        drawObservingList( p );
        drawZoomBox( p );

        if ( transientObject() ) drawTransientLabel( p );
        if (isAngleMode()) {
            updateAngleRuler();
            drawAngleRuler( p );
        }
    }
}

void SkyMap::drawAngleRuler( QPainter &p ) {
    p.setPen( QPen( data->colorScheme()->colorNamed( "AngularRuler" ), 3.0, Qt::DotLine ) );

    QPointF startPoint = toScreen( AngularRuler.point(0) );
    QPointF endPoint = toScreen( AngularRuler.point(1) );

    p.drawLine( startPoint, endPoint );
}

void SkyMap::drawZoomBox( QPainter &p ) {
    //draw the manual zoom-box, if it exists
    if ( ZoomRect.isValid() ) {
        p.setPen( QPen( Qt::white, 1.0, Qt::DotLine ) );
        p.drawRect( ZoomRect.x(), ZoomRect.y(), ZoomRect.width(), ZoomRect.height() );
    }
}

void SkyMap::drawHighlightConstellation( QPainter &psky ) {
    const QPolygonF* cbound =
        ConstellationBoundary::Instance()->constellationPoly( focus() );

    if ( ! cbound ) return;

    psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CBoundHighColor" ) ), 3, Qt::SolidLine ) );

    QPolygonF clippedPoly;
    bool needClip( false );
    //If the solid ground is being drawn, then we may need to clip the
    //polygon at the Horizon.  cbound will be the clipped polygon
    if ( Options::useAltAz() && Options::showGround() ) {
        QPolygonF horizPolygon;

        for ( int i = 0; i < cbound->size(); i++ ) {
            QPointF node = cbound->at( i );
            SkyPoint sp( node.x(), node.y() );
            sp.EquatorialToHorizontal( data->LST, data->geo()->lat() );
            horizPolygon << QPointF( sp.az()->Degrees(), sp.alt()->Degrees() );
        }

        QRectF rBound = horizPolygon.boundingRect();
        //If the bounding rectangle's top edge is below the horizon,
        //then we don't draw the polygon at all.  Note that the rectangle's
        //top edge is returned by the bottom() function!
        if ( rBound.bottom() < 0.0 )
            return;

        //We need to clip if the bounding rect intersects the horizon
        if ( rBound.contains( QPointF( rBound.center().x(), 0.0 ) ) ) {
            needClip = true;
            QPolygonF clipper;
            clipper << QPointF( rBound.left(), -0.5 ) << QPointF( rBound.right(), -0.5 )
            << QPointF( rBound.right(), rBound.top() ) //top() is bottom ;)
            << QPointF( rBound.left(), rBound.top() );

            horizPolygon = horizPolygon.subtracted( clipper );

            //Now, convert the clipped horizPolygon vertices back to equatorial
            //coordinates, and store them back in cbound

            foreach ( const QPointF &node, horizPolygon ) {
                SkyPoint sp;
                sp.setAz( node.x() );
                sp.setAlt( node.y() );
                sp.HorizontalToEquatorial( data->LST, data->geo()->lat() );
                clippedPoly << QPointF( sp.ra()->Hours(), sp.dec()->Degrees() );
            }
        }
    }

    //Transform the constellation boundary polygon to the screen and clip
    // against the celestial horizon

    const QPolygonF* bound = needClip ? &clippedPoly : cbound;

    if (bound->size() < 2) return;

    bool isVisible, isVisibleLast;
    SkyPoint pThis, pLast;
    QPointF oThis, oLast, oMid;

    QPointF node = bound->at( 0 );
    pLast.set( node.x(), node.y() );

    pLast.EquatorialToHorizontal( data->LST, data->geo()->lat() );
    oLast = toScreen( &pLast, Options::useRefraction(), &isVisibleLast );

    int limit = bound->size();

    for ( int i=1 ; i < limit ; i++ ) {
        node = bound->at( i );
        pThis.set( node.x(), node.y() );
        pThis.EquatorialToHorizontal( data->LST, data->geo()->lat() );
        oThis = toScreen( &pThis, Options::useRefraction(), &isVisible );

        if ( isVisible && isVisibleLast ) {
            psky.drawLine( oLast, oThis );
        }

        else if ( isVisibleLast ) {
            oMid = clipLineI( &pLast, &pThis );
            // -jbb printf("oMid: %4d %4d\n", oMid.x(), oMid.y());
            psky.drawLine( oLast, oMid );
        }
        else if ( isVisible ) {
            oMid = clipLineI( &pThis, &pLast );
            psky.drawLine( oMid, oThis );
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }
    /****
    	QPolygonF poly;
        bool isVisible;
    	foreach ( const QPointF &node, cbound ) {
    		SkyPoint sp( node.x(), node.y() );
    		sp.EquatorialToHorizontal( data->LST, data->geo()->lat() );
    		QPointF v = toScreen( &sp, scale, Options::useRefraction() );

    		poly << v;
    	}

    	psky.drawPolygon( poly );
    ****/
}

void SkyMap::drawObjectLabels( QList<SkyObject*>& labelObjects, QPainter &psky ) {
    bool checkSlewing = ( slewing || ( clockSlewing && data->clock()->isActive() ) ) && Options::hideOnSlew();
    if ( checkSlewing && Options::hideLabels() ) return;

    float Width = m_Scale * width();
    float Height = m_Scale * height();

    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    skyLabeler->resetFont( psky );      // use the zoom dependent font

    psky.setPen( data->colorScheme()->colorNamed( "UserLabelColor" ) );

    bool drawPlanets( Options::showSolarSystem() && !(checkSlewing && Options::hidePlanets() ) );
    bool drawComets( drawPlanets && Options::showComets() );
    bool drawAsteroids( drawPlanets && Options::showAsteroids() );
    bool drawMessier( Options::showDeepSky() && ( Options::showMessier() || Options::showMessierImages() ) && !(checkSlewing && Options::hideMessier() ) );
    bool drawNGC( Options::showDeepSky() && Options::showNGC() && !(checkSlewing && Options::hideNGC() ) );
    bool drawIC( Options::showDeepSky() && Options::showIC() && !(checkSlewing && Options::hideIC() ) );
    bool drawOther( Options::showDeepSky() && Options::showOther() && !(checkSlewing && Options::hideOther() ) );
    bool drawStars = ( Options::showStars() );
    bool hideFaintStars( checkSlewing && Options::hideStars() );

    //Attach a label to the centered object
    if ( focusObject() != NULL && Options::useAutoLabel() ) {
        QPointF o = toScreen( focusObject() );

        focusObject()->drawNameLabel( psky, o );
    }

    foreach ( SkyObject *obj, labelObjects ) {
        //Only draw an attached label if the object is being drawn to the map
        //reproducing logic from other draw funcs here...not an optimal solution
        if ( obj->type() == SkyObject::STAR ) {
            if ( ! drawStars ) continue;
            //            if ( obj->mag() > Options::magLimitDrawStar() ) continue;
            if ( hideFaintStars && obj->mag() > Options::magLimitHideStar() ) continue;
        }
        if ( obj->type() == SkyObject::PLANET ) {
            if ( ! drawPlanets ) continue;
            if ( obj->name() == "Sun" && ! Options::showSun() ) continue;
            if ( obj->name() == "Mercury" && ! Options::showMercury() ) continue;
            if ( obj->name() == "Venus" && ! Options::showVenus() ) continue;
            if ( obj->name() == "Moon" && ! Options::showMoon() ) continue;
            if ( obj->name() == "Mars" && ! Options::showMars() ) continue;
            if ( obj->name() == "Jupiter" && ! Options::showJupiter() ) continue;
            if ( obj->name() == "Saturn" && ! Options::showSaturn() ) continue;
            if ( obj->name() == "Uranus" && ! Options::showUranus() ) continue;
            if ( obj->name() == "Neptune" && ! Options::showNeptune() ) continue;
            if ( obj->name() == "Pluto" && ! Options::showPluto() ) continue;
        }
        if ( obj->type() >= SkyObject::OPEN_CLUSTER && obj->type() <= SkyObject::GALAXY ) {
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

        obj->drawNameLabel( psky, o );
    }

    skyLabeler->useStdFont( psky );   // use the StdFont for the guides.
}

void SkyMap::drawTransientLabel( QPainter &p ) {
    if ( ! transientObject() || ! checkVisibility( transientObject() ) ) return;

    QPointF o = toScreen( transientObject() );
    if ( ! onScreen( o ) ) return;

    p.setPen( TransientColor );
    transientObject()->drawRudeNameLabel( p, o );
}

//FIXME: Implement Options::boxBGMode()
void SkyMap::drawBoxes( QPainter &p ) {
    if ( ks ) { //only if we are drawing in the GUI window
        ks->infoBoxes()->drawBoxes( p,
                                    data->colorScheme()->colorNamed( "BoxTextColor" ),
                                    data->colorScheme()->colorNamed( "BoxGrabColor" ),
                                    data->colorScheme()->colorNamed( "BoxBGColor" ), 1 /*Options::boxBGMode()*/ );
    }
}

void SkyMap::drawObservingList( QPainter &psky ) {
    int penWidth = int(m_Scale);
    psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "ObsListColor" ) ), penWidth ) );

    if ( ks && ks->observingList()->obsList().size() ) {
        foreach ( SkyObject* obj, ks->observingList()->obsList() ) {
            if ( checkVisibility( obj ) ) {
                QPointF o = toScreen( obj );

                // label object if it is currently on screen
                if (o.x() >= 0. && o.x() <= width()*m_Scale && o.y() >=0. && o.y() <= height()*m_Scale ) {
                    if ( Options::obsListSymbol() ) {
                        if ( Options::useAntialias() ) {
                            float size = 20.*m_Scale;
                            float x1 = o.x() - 0.5*size;
                            float y1 = o.y() - 0.5*size;
                            psky.drawArc( QRectF(x1, y1, size, size), -60*16, 120*16 );
                            psky.drawArc( QRectF(x1, y1, size, size), 120*16, 120*16 );
                        } else {
                            int size = 20*int(m_Scale);
                            int x1 = int(o.x()) - size/2;
                            int y1 = int(o.y()) - size/2;
                            psky.drawArc( QRect(x1, y1, size, size), -60*16, 120*16 );
                            psky.drawArc( QRect(x1, y1, size, size), 120*16, 120*16 );
                        }
                    }
                    if ( Options::obsListText() ) {
                        obj->drawRudeNameLabel( psky, o );
                    }
                }
            }
        }
    }
}

void SkyMap::drawTelescopeSymbols(QPainter &psky)
{
    if ( ks ) { //ks doesn't exist in non-GUI mode!
        INDI_P *eqNum;
        INDI_P *portConnect;
        INDI_E *lp;
        INDIMenu *devMenu = ks->getINDIMenu();
        bool useJ2000 (false), useAltAz(false);
        SkyPoint indi_sp;

        if (!Options::indiCrosshairs() || devMenu == NULL)
            return;

        psky.setPen( QPen( QColor( data->colorScheme()->colorNamed("TargetColor" ) ) ) );
        psky.setBrush( Qt::NoBrush );
        float pxperdegree = Options::zoomFactor()/57.3;

        //fprintf(stderr, "in draw telescope function with managerssize of %d\n", devMenu->managers.size());
        for ( int i=0; i < devMenu->managers.size(); i++ )
        {
            for ( int j=0; j < devMenu->managers.at(i)->indi_dev.size(); j++ )
            {
                useAltAz = false;
                useJ2000 = false;

                // make sure the dev is on first
                if (devMenu->managers.at(i)->indi_dev.at(j)->isOn())
                {
                    portConnect = devMenu->managers.at(i)->indi_dev.at(j)->findProp("CONNECTION");

                    if (!portConnect)
                        return;

                    if (portConnect->state == PS_BUSY)
                        return;

                    eqNum = devMenu->managers.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_EOD_COORD");

                    if (eqNum == NULL)
                    {
                        eqNum = devMenu->managers.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_COORD");
                        if (eqNum == NULL)
                        {
                            eqNum = devMenu->managers.at(i)->indi_dev.at(j)->findProp("HORIZONTAL_COORD");
                            if (eqNum == NULL) continue;
                            else
                                useAltAz = true;
                        }
                        else
                            useJ2000 = true;
                    }

                    // make sure it has RA and DEC properties
                    if ( eqNum)
                    {
                        //fprintf(stderr, "Looking for RA label\n");
                        if (useAltAz)
                        {
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

                        }
                        else
                        {

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


                            //kDebug() << "the KStars RA is " << raDMS.toHMSString();
                            //kDebug() << "the KStars DEC is " << decDMS.toDMSString() << "\n****************";

                            indi_sp.setRA(raDMS);
                            indi_sp.setDec(decDMS);

                            if (useJ2000)
                            {
                                indi_sp.setRA0(raDMS);
                                indi_sp.setDec0(decDMS);
                                indi_sp.apparentCoord( (double) J2000, ks->data()->ut().djd());
                            }

                            if ( Options::useAltAz() )
                                indi_sp.EquatorialToHorizontal( ks->LST(), ks->geo()->lat() );

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
    }
}

void SkyMap::exportSkyImage( QPaintDevice *pd ) {
    QPainter p;
    p.begin( pd );
    p.setRenderHint(QPainter::Antialiasing, Options::useAntialias() );

    //scale image such that it fills 90% of the x or y dimension on the paint device
    double xscale = double(p.device()->width()) / double(width());
    double yscale = double(p.device()->height()) / double(height());
    m_Scale = (xscale < yscale) ? xscale : yscale;

    //Now that we have changed the map scale, we need to re-run 
    //StarObject::initImages() to get scaled pixmaps
    StarObject::initImages();

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
    StarObject::initImages();
}

void SkyMap::setMapGeometry() {
    m_Guidemax = Options::zoomFactor()/10.0;

    isPoleVisible = false;
    if ( Options::useAltAz() ) {
        XRange = 1.2*fov()/cos( focus()->alt()->radians() );
        Ymax = fabs( focus()->alt()->Degrees() ) + fov();
    } else {
        XRange = 1.2*fov()/cos( focus()->dec()->radians() );
        Ymax = fabs( focus()->dec()->Degrees() ) + fov();
    }
    if ( Ymax >= 90. ) isPoleVisible = true;

    //at high zoom, double FOV for guide lines so they don't disappear.
    guideFOV = fov();
    guideXRange = XRange;
    if ( Options::zoomFactor() > 10.*MINZOOM ) { guideFOV *= 2.0; guideXRange *= 2.0; }
}
