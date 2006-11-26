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
#include "skyline.h"
#include "dms.h"
#include "Options.h"
#include "ksutils.h"
#include "ksfilereader.h"

#include "milkywaycomponent.h"

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

void MilkyWayComponent::addPoint( const SkyPoint &p )
{
	if ( lineList().size() == 0 ) {
		if ( ! FirstPoint ) {
			FirstPoint = new SkyPoint( p );
			return;
		}

		lineList().append( new SkyLine( *FirstPoint, p ) );
		return;
	}
	
	SkyPoint *pLast = lineList().last()->endPoint();
	lineList().append( new SkyLine( *pLast, p ) );
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

void MilkyWayComponent::drawInt( KStars *, QPainter&, double )
{
	/*
	SkyMap *map = ks->map();

	bool partVisible, onscreen, onscreenLast;
    bool visible, visibleLast;
    SkyPoint  *pLast, *pThis;

    int height = int( scale * map->height() );
    int width  = int( scale * map->width() );
    QPolygon polyMW;
    QPoint oThis, oLast, oMid;

    if ( Options::fillMilkyWay() ) {
        partVisible = true;

        pLast = pointList().at( 0 );
        oLast = map->toScreenI( pLast, scale, false, &onscreenLast );

        for ( int i=1; i < pointList().size(); ++i ) {
            pThis = pointList().at( i );
            oThis = map->toScreenI( pThis, scale, false, &onscreen );

            if ( oThis.x() >= 0 && oThis.x() <= width &&
                 oThis.y() >= 0 && oThis.y() <= height ) partVisible = true;

            if ( onscreen && onscreenLast ) {
               polyMW << oThis;
            }
            else if ( onscreenLast ) {
                oMid = map->clipLineI( pLast, pThis, scale );
                polyMW << oMid;
            }
            else if ( onscreen ){
                oMid = map->clipLineI( pThis, pLast, scale );
                polyMW << oMid;
                polyMW << oThis;
            }

            pLast = pThis;
            oLast = oThis;
            onscreenLast = onscreen;
        }
        if ( polyMW.size() && partVisible ) psky.drawPolygon( polyMW );
    }
    else {

        pLast = pointList().at( 0 );

        oLast = map->toScreenI( pLast, scale, false, &onscreenLast );

        visibleLast = ( oLast.x() >= 0 && oLast.x() <= width &&
                        oLast.y() >= 0 && oLast.y() <= height );

        int limit = pointList().size() - 1;   // because we appended 1st point
                                              // to end of list so we can
                                              // clip and wrap the polygons
        for ( int i=1 ; i < limit ; i++ ) {
            pThis = pointList().at( i );
            oThis = map->toScreenI( pThis, scale, false, &onscreen );
            visible = ( oThis.x() >= 0 && oThis.x() <= width &&
                        oThis.y() >= 0 && oThis.y() <= height );

            if ( ( visible || visibleLast ) && !skip.contains(i) ) {

                if ( onscreen && onscreenLast ) {
                    psky.drawLine( oLast.x(), oLast.y(), oThis.x(), oThis.y() );
                }
                else if ( onscreenLast ) {
                    oMid = map->clipLineI( pLast, pThis, scale );
                    psky.drawLine( oLast.x(), oLast.y(), oMid.x(), oMid.y() );
                }
                else if ( onscreen ) {
                    oMid = map->clipLineI( pThis, pLast, scale );
                    psky.drawLine( oMid.x(), oMid.y(), oThis.x(), oThis.y() );
                }
            }

            pLast = pThis;
            oLast = oThis;
            onscreenLast = onscreen;
            visibleLast = visible;
        }
    }
	*/
}

void MilkyWayComponent::drawFloat( KStars *ks, QPainter& psky, double scale )
{
	SkyMap *map = ks->map();
	QPolygonF polyMW;
	
	if ( Options::fillMilkyWay() ) {
		//Construct a QPolygonF from the on-screen line segments
		foreach ( SkyLine *sl, lineList() ) {
			//Last argument: don't clip lines that are partly off-screen
			QLineF screenline = map->toScreen( sl, scale, true, false );
			
			if ( ! screenline.isNull() ) {
				//Add both end-points if this is the first segment
				if ( ! polyMW.size() )
					polyMW << screenline.p1() << screenline.p2();
				else
					polyMW << screenline.p2();
			}
		}
		
		if ( polyMW.size() ) {
			psky.drawPolygon( polyMW );
		}

	} else { //Draw MW outline only
		//Draw all non-hidden, onscreen line segments
		
		int limit = lineList().size() - 1;   // because we appended 1st point
		// to end of list so we can
		// clip and wrap the polygons
		for ( int i=1 ; i < limit ; i++ ) {
			if ( ! skip.contains(i) )
				psky.drawLine( map->toScreen( lineList().at(i), scale ) );
		}
	}
}
