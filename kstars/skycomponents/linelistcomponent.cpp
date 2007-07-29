/****************************************************************************
                          linelistcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2006/11/01
    copyright            : (C) 2006 by Jason Harris
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

#include "linelistcomponent.h"

#include <math.h> //fabs()
#include <QtAlgorithms>
#include <QPainter>

#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "dms.h"
#include "Options.h"


LineListComponent::LineListComponent( SkyComponent *parent )
	: SkyComponent( parent ), LabelPosition( NoLabel ), Label( QString() )
{}

LineListComponent::~LineListComponent()
{}

void LineListComponent::update( KStarsData *data, KSNumbers *num )
{
    if ( ! num ) return;
    if ( ! selected() ) return;

    foreach ( SkyPoint* p, pointList ) {
        p->updateCoords( num );
        p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

void LineListComponent::draw( KStars *ks, QPainter &psky, double scale ) {
	if ( ! selected() ) return;

	SkyMap *map = ks->map();

	psky.setPen( pen() );

	bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;

	QList<QPointF> pList; // for sending to drawLabels()


    if ( Options::useAntialias() ) {

        QPointF oThis, oLast, oMid;

        pLast = points()->at( 0 );

        oLast = map->toScreen( pLast, scale, false, &isVisibleLast );

        int limit = points()->size();

        for ( int i=1 ; i < limit ; i++ ) {
            pThis = points()->at( i );
            oThis = map->toScreen( pThis, scale, false, &isVisible );

            if ( map->onScreen(oThis, oLast ) ) {
                if ( isVisible && isVisibleLast ) {
                    psky.drawLine( oLast, oThis );
                }

                else if ( isVisibleLast && ! isVisible ) {
                    oMid = map->clipLine( pLast, pThis, scale );
                    // -jbb printf("oMid: %4d %4d\n", oMid.x(), oMid.y());
                    psky.drawLine( oLast, oMid );
                }
                else if ( isVisible && ! isVisibleLast ) {
                    oMid = map->clipLine( pThis, pLast, scale );
                    psky.drawLine( oMid, oThis );
                }
            }

            pLast = pThis;
            oLast = oThis;
            isVisibleLast = isVisible;
        }
    }
    else {

        QPoint oThis, oLast, oMid;
        pLast = points()->at( 0 );

        oLast = map->toScreenI( pLast, scale, false, &isVisibleLast );

        int limit = points()->size();

        for ( int i=1 ; i < limit ; i++ ) {
            pThis = points()->at( i );
            oThis = map->toScreenI( pThis, scale, false, &isVisible );

            if ( map->onScreen(oThis, oLast ) ) {
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
/***********************************
	//Each SkyLine is a series of line segments in the celestial coordinate system.
	//Transform each segment to screen coordinates, and draw the segments that are on-screen
	pList = map->toScreen( &m_SkyLine, scale, Options::useRefraction(), true) //clip offscreen segments );

	//highZoomFactor = true when FOV is less than 1 degree (1/57.3 radians)
	bool highZoomFactor( false );
	if ( Options::zoomFactor() > map->width()*57.3 )
		highZoomFactor = true;

	QPointF pLast( -10000000., -10000000. );
	foreach ( QPointF p, pList ) {
		if ( ! map->isPointNull( pLast ) && ! map->isPointNull( p ) ) {

			if ( fabs(p.x()-pLast.x()) < map->width() || highZoomFactor )
				psky.drawLine( pLast, p );

*********************************/
/********
			//Next, identify the index of the point whose segment brackets
			//the left-edge position (x=20), the right-edge position (x=width()-60),
			//the top-edge position (y=20), and the bottom-edge pos. (y=height()-30)
			float xLeft = 20.;
			float xRight = psky.window().width() - 60.;
			float yTop = 20.;
			float yBottom = psky.window().height() - 30.;
			if ( (p.x() >= xLeft && pLast.x() < xLeft) || (pLast.x() >= xLeft && p.x() < xLeft) )
				iLeft = pList.indexOf( p );
			if ( (p.x() >= xRight && pLast.x() < xRight) || (pLast.x() >= xRight && p.x() < xRight) )
				iRight = pList.indexOf( p );
			if ( (p.y() >= yTop && pLast.y() < yTop) || (pLast.y() >= yTop && p.y() < yTop) )
				iTop = pList.indexOf( p );
			if ( (p.y() >= yBottom && pLast.y() < yBottom) || (pLast.y() >= yBottom && p.y() < yBottom) )
				iBottom = pList.indexOf( p );
        }
		pLast = p;
        drawLabels(ks, psky, scale, pList);
    }
}
********/


void LineListComponent::drawLabels( KStars *ks, QPainter &psky, double scale, QList<QPointF> &pList ) {

    int iLeft = -1;   //the segment that intersects the left edge
	int iRight = -1;  //the segment that intersects the right edge
	int iTop = -1;    //the segment that intersects the top edge
	int iBottom = -1; //the segment that intersects the bottom edge

	//Now label the LineListComponent.  We have stored the index of line segments
	//near each edge of the screen.  Draw the label near one of these segments,
	//according to the value of LabelPosition
	if ( LabelPosition != NoLabel ) {
		//Make sure we have at least one edge segment defined
		if ( iLeft < 0 && iRight < 0 && iTop < 0 && iBottom < 0 ) return;

		//Draw the label at segLeft or segRight.  If this segment is undefined,
		//choose segTop or segBottom, wheichever is further left or right
		int iLabel;
		double tx;
		double ty;

		if ( LabelPosition == LeftEdgeLabel ) {
			if ( iLeft >= 0 ) {
				iLabel = iLeft;
				tx = 20.0;
				ty = pList[iLabel].y() - 5.0;

			//if iLeft is undefined, try the top or bottom edge, whichever is further left
			} else {
				if ( iTop >= 0 && iBottom >= 0 ) {
					if ( pList[iTop].x() < pList[iBottom].x() )
						iLabel = iTop;
					else
						iLabel = iBottom;
				} else if ( iTop >= 0 ) {
					iLabel = iTop;

				} else if ( iBottom >= 0 ) {
					iLabel = iBottom;
				}
                //FIXME: -jbb to prevent crashes on index out of range

				//tx = pList[iLabel].x();
				//ty = pList[iLabel].y() - 5.0;
			}
		}

		if ( LabelPosition == RightEdgeLabel ) {
			if ( iRight >= 0 ) {
				iLabel = iRight;
				tx = psky.window().width() - 60.0;
				ty = pList[iLabel].y() - 5.0;

			//if iRight is undefined, try the top or bottom edge, whichever is further right
			} else {
				if ( iTop >= 0 && iBottom >= 0 ) {
					if ( pList[iTop].x() > pList[iBottom].x() )
						iLabel = iTop;
					else
						iLabel = iBottom;
				} else if ( iTop >= 0 ) {
					iLabel = iTop;

				} else if ( iBottom >= 0 ) {
					iLabel = iBottom;
				}

                //FIXME: -jbb to prevent crashes on index out of range

				//tx = pList[iLabel].x();
				//ty = pList[iLabel].y() - 5.0;
			}
		}

		if ( iLabel == 0 )
			iLabel = 1;


		//double sx = double( pList[iLabel].x() - pList[iLabel-1].x() );
		//double sy = double( pList[iLabel].y() - pList[iLabel-1].y() );
		//double angle = atan2( sy, sx )*180.0/dms::PI;
		//if ( sx < 0.0 ) angle -= 180.0;

		//psky.save();
		// FIXME: -jbb psky.translate( tx, ty );

		//psky.rotate( double( angle ) );  //rotate the coordinate system
		//psky.drawText( 0, 0, Label );
		//psky.restore(); //reset coordinate system
	}
}
