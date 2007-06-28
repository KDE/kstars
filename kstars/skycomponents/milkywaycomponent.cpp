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

/****************************************************************************
 * The filled polygon code below implements the Sutherland-Hodgman's Polygon
 * clipping algorithm.  Please don't mess with it unless you ensure that
 * the Milky Way clipping continues to work.
 *
 * Since the clipping code is a bit messy, there are four versions of the
 * inner loop for Filled/Outline * Integer/Float.  This moved these two
 * decisions out of the inner loops to make them a bit faster and less
 * messy.
 *
 * -- James B. Bowlin
 *
 ****************************************************************************/
#include "milkywaycomponent.h"

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QPainter>
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

MilkyWayComponent::MilkyWayComponent(SkyComponent *parent, bool (*visibleMethod)())
  : LineListComponent(parent, visibleMethod), FirstPoint(0)
{
}

MilkyWayComponent::~MilkyWayComponent()
{
	delete FirstPoint;
}

void MilkyWayComponent::init(KStarsData *)
{
}

void MilkyWayComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !visible() ) return;

	int thick(1);
	if ( ! Options::fillMilkyWay() ) thick = 3;

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

void MilkyWayComponent::drawInt( KStars *ks, QPainter& psky, double scale)
{

	SkyMap *map = ks->map();

	bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;

    QPolygon polyMW;
    QPoint oThis, oLast, oMid;


    if ( Options::fillMilkyWay() ) {
        //partVisible = true;

        pLast = lineList().at( 0 );
        oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

        for ( int i=1; i < lineList().size(); ++i ) {
            pThis = lineList().at( i );
            oThis = map->toScreenI( pThis, scale, false, &isVisible );

            if ( isVisible && isVisibleLast ) {
               polyMW << oThis;
            }
            else if ( isVisibleLast ) {
                oMid = map->clipLineI( pLast, pThis, scale );
                polyMW << oMid;
            }
            else if ( isVisible ) {
                oMid = map->clipLineI( pThis, pLast, scale );
                polyMW << oMid;
                polyMW << oThis;
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }

        if ( polyMW.size() ) psky.drawPolygon( polyMW );
    }
    else {

        pLast = lineList().at( 0 );

        oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

        int limit = lineList().size() - 1;   // because we appended 1st point
                                              // to end of list so we can
                                              // clip and wrap the polygons
        for ( int i=1 ; i < limit ; i++ ) {
            pThis = lineList().at( i );
            oThis = map->toScreenI( pThis, scale, false, &isVisible );

            if ( map->onScreen(oThis, oLast) && !skip.contains(i) ) {
                if ( isVisible && isVisibleLast ) {
                    psky.drawLine( oLast.x(), oLast.y(), oThis.x(), oThis.y() );
                }

                else if ( isVisibleLast ) {
                    oMid = map->clipLineI( pLast, pThis, scale );
                    // -jbb printf("oMid: %4d %4d\n", oMid.x(), oMid.y());
                    psky.drawLine( oLast.x(), oLast.y(), oMid.x(), oMid.y() );
                }
                else if ( isVisible ) {
                    oMid = map->clipLineI( pThis, pLast, scale );
                    psky.drawLine( oMid.x(), oMid.y(), oThis.x(), oThis.y() );
                }
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
    }

}
void MilkyWayComponent::drawFloat( KStars *ks, QPainter& psky, double scale )
{
	SkyMap *map = ks->map();
	QPolygonF polyMW;
	bool isVisible, isVisibleLast;
    bool onScreen, onScreenLast;
    SkyPoint  *pLast, *pThis;
    QPointF oThis, oLast, oMid;

	if ( Options::fillMilkyWay() ) {
         pLast = lineList().at( 0 );

        oLast = map->toScreen( pLast, scale, false, &isVisibleLast );

        for ( int i=1; i < lineList().size(); ++i ) {
            pThis = lineList().at( i );
            oThis = map->toScreen( pThis, scale, false, &isVisible );

            if ( isVisible && isVisibleLast ) {
               polyMW << oThis;
            }
            else if ( isVisibleLast ) {
                oMid = map->clipLine( pLast, pThis, scale );
                polyMW << oMid;
            }
            else if ( isVisible ) {
                oMid = map->clipLine( pThis, pLast, scale );
                polyMW << oMid;
                polyMW << oThis;
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
        if ( polyMW.size() ) psky.drawPolygon( polyMW );
    }
    else {

        pLast = lineList().at( 0 );

        oLast = map->toScreen( pLast, scale, false, &isVisibleLast );

        int limit = lineList().size() - 1;   // because we appended 1st point
                                              // to end of list so we can
                                              // clip and wrap the polygons
        for ( int i=1 ; i < limit ; i++ ) {
            pThis = lineList().at( i );
            oThis = map->toScreen( pThis, scale, false, &isVisible );
            onScreen = map->onScreen(oThis);

            if ( map->onScreen(oThis, oLast ) && !skip.contains(i) ) {
                if ( isVisible && isVisibleLast ) {
                    psky.drawLine( oLast, oThis );
                }

                else if ( isVisibleLast ) {
                    oMid = map->clipLine( pLast, pThis, scale );
                    // -jbb printf("oMid: %4d %4d\n", oMid.x(), oMid.y());
                    psky.drawLine( oLast, oMid );
                }
                else if ( isVisible ) {
                    oMid = map->clipLine( pThis, pLast, scale );
                    psky.drawLine( oMid, oThis );
                }
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
    }
}

