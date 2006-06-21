/***************************************************************************
                          milkywaycomponent.cpp  -  K Desktop Planetarium
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
#include <QPolygonF>
#include <QPainter>
#include <Q3PointArray>
#include <QFile>

#include "skycomposite.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h"
#include "dms.h"
#include "Options.h"
#include "ksutils.h"
#include "ksfilereader.h"

#include "milkywaycomponent.h"

MilkyWayComponent::MilkyWayComponent(SkyComponent *parent, bool (*visibleMethod)())
: PointListComponent(parent, visibleMethod)
{
}

MilkyWayComponent::~MilkyWayComponent()
{
}

void MilkyWayComponent::init(KStarsData *)
{
}

void MilkyWayComponent::addPoint( double ra, double dec )
{
    pointList().append( new SkyPoint( ra, dec ) );
}

void MilkyWayComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !visible() ) return;

    int thick(1);
	if ( ! Options::fillMilkyWay() ) thick=3;

	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "MWColor" ) ), thick, Qt::SolidLine ) );
	psky.setBrush( QBrush( QColor( ks->data()->colorScheme()->colorNamed( "MWColor" ) ) ) );

    // Uncomment these two lines to get more visible images for debugging.  -jbb
    //
    //psky.setPen( QPen( QColor( "red" ), 1, Qt::SolidLine ) );
    //psky.setBrush( QBrush( QColor("green"  ) ) );

    if ( Options::useAntialias() ) {
        drawFloat( ks, psky, scale );
    }
    else {
        drawInt( ks, psky, scale );
    }
}

void MilkyWayComponent::drawInt( KStars *ks, QPainter& psky, double scale )
{
	SkyMap *map = ks->map();

    bool partVisible, clipped, clippedLast;
    bool visible, visibleLast;
    SkyPoint  *pLast, *pThis;

    int height = int( scale * map->height() );
    int width  = int( scale * map->width() );
    QPolygon polyMW;
    QPoint oThis, oLast, oMid;

    if ( Options::fillMilkyWay() ) {
        partVisible = true;

        pLast = pointList().at( 0 );
        oLast = map->toScreenI( pLast, scale, false, &clippedLast );

        for ( int i=1; i < pointList().size(); ++i ) {
            pThis = pointList().at( i );
            oThis = map->toScreenI( pThis, scale, false, &clipped );

            if ( oThis.x() >= 0 && oThis.x() <= width &&
                 oThis.y() >= 0 && oThis.y() <= height ) partVisible = true;

            if ( !clipped && !clippedLast ) {
               polyMW << oThis;
            }
            else if ( !clippedLast ) {
                oMid = map->clipLineI( pLast, pThis, scale );
                polyMW << oMid;
            }
            else if ( !clipped ){
                oMid = map->clipLineI( pThis, pLast, scale );
                polyMW << oMid;
                polyMW << oThis;
            }

            pLast = pThis;
            oLast = oThis;
            clippedLast = clipped;
        }
        if ( polyMW.size() && partVisible ) psky.drawPolygon( polyMW );
    }
    else {

        pLast = pointList().at( 0 );

        oLast = map->toScreenI( pLast, scale, false, &clippedLast );

        visibleLast = ( oLast.x() >= 0 && oLast.x() <= width &&
                        oLast.y() >= 0 && oLast.y() <= height );

        int limit = pointList().size() - 1;   // because we appended 1st point
                                              // to end of list so we can
                                              // clip and wrap the polygons
        for ( int i=1 ; i < limit ; i++ ) {
            pThis = pointList().at( i );
            oThis = map->toScreenI( pThis, scale, false, &clipped );
            visible = ( oThis.x() >= 0 && oThis.x() <= width &&
                        oThis.y() >= 0 && oThis.y() <= height );

            if ( ( visible || visibleLast ) && !skip.contains(i) ) {

                if ( !clipped && !clippedLast ) {
                    psky.drawLine( oLast.x(), oLast.y(), oThis.x(), oThis.y() );
                }
                else if ( !clippedLast ) {
                    oMid = map->clipLineI( pLast, pThis, scale );
                    psky.drawLine( oLast.x(), oLast.y(), oMid.x(), oMid.y() );
                }
                else if ( !clipped ) {
                    oMid = map->clipLineI( pThis, pLast, scale );
                    psky.drawLine( oMid.x(), oMid.y(), oThis.x(), oThis.y() );
                }
            }

            pLast = pThis;
            oLast = oThis;
            clippedLast = clipped;
            visibleLast = visible;
        }
    }
}

void MilkyWayComponent::drawFloat( KStars *ks, QPainter& psky, double scale )
{
	SkyMap *map = ks->map();

    bool partVisible, clipped, clippedLast;
    bool visible, visibleLast;
    SkyPoint  *pLast, *pThis;

    float height =  scale * map->height();
    float width  = scale * map->width();
    QPolygonF polyMW;
    QPointF oThis, oLast, oMid;


    if ( Options::fillMilkyWay() ) {
        partVisible = true;

        pLast = pointList().at( 0 );
        oLast = map->toScreen( pLast, scale, false, &clippedLast );

        for ( int i=1; i < pointList().size(); ++i ) {
            pThis = pointList().at( i );
            oThis = map->toScreen( pThis, scale, false, &clipped );

            if ( oThis.x() >= 0 && oThis.x() <= width &&
                 oThis.y() >= 0 && oThis.y() <= height ) partVisible = true;

            if ( !clipped && !clippedLast ) {
               polyMW << oThis;
            }
            else if ( !clippedLast ) {
                oMid = map->clipLine( pLast, pThis, scale);
                polyMW << oMid;
            }
            else if ( !clipped ){
                oMid = map->clipLine( pThis, pLast, scale);
                polyMW << oMid;
                polyMW << oThis;
            }

            pLast = pThis;
            oLast = oThis;
            clippedLast = clipped;
        }
        if ( polyMW.size() && partVisible ) psky.drawPolygon( polyMW );
    }
    else {

        pLast = pointList().at( 0 );

        oLast = map->toScreen( pLast, scale, false, &clippedLast );

        visibleLast = (oLast.x() >= 0 && oLast.x() <= width &&
                       oLast.y() >= 0 && oLast.y() <= height );

        int limit = pointList().size() - 1;   // because we appended 1st point
                                              // to end of list so we can
                                              // clip and wrap the polygons
        for ( int i=1 ; i < limit ; i++ ) {
            pThis = pointList().at( i );
            oThis = map->toScreen( pThis, scale, false, &clipped );
            visible = ( oThis.x() >= 0 && oThis.x() <= width &&
                        oThis.y() >= 0 && oThis.y() <= height );

            if ( ( visible || visibleLast ) && ! skip.contains(i) ) {

                if ( !clipped && !clippedLast ) {
                    psky.drawLine( oLast, oThis );
                }
                else if ( !clippedLast ) {
                    oMid = map->clipLine( pLast, pThis, scale );
                    psky.drawLine( oLast, oMid );
                }
                else if ( !clipped ) {
                    oMid = map->clipLine( pThis, pLast, scale );
                    psky.drawLine( oMid, oThis );
                }
            }

            pLast = pThis;
            oLast = oThis;
            clippedLast = clipped;
            visibleLast = visible;
        }
    }
}
