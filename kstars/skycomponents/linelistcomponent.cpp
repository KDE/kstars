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
#include "skylabeler.h"


LineListComponent::LineListComponent( SkyComponent *parent )
	: SkyComponent( parent ), LabelPosition( NoLabel ), Label( QString() )
{
	m_skyLabeler = ((SkyMapComposite*) parent)->skyLabeler();
}

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

void LineListComponent::draw( KStars *ks, QPainter &psky, double scale )
{
	if ( ! selected() ) return;

	SkyMap *map = ks->map();

	psky.setPen( pen() );

	bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;

	float xLeft  = 100000.;
	float xRight = 0.;
    float yTop   = 100000.;
	float yBot   = 0.;

	m_skyLabeler->useStdFont( psky );
	QFontMetricsF fm( psky.font() );
	m_skyLabeler->resetFont( psky );

	float margin = fm.width("MMM");
	float rightMargin = psky.window().width() - margin - fm.width( Label );
	float leftMargin  = margin;
	float topMargin   = fm.height();
	float botMargin   = psky.window().height() - 4.0 * fm.height();

	m_iLeft = m_iRight = m_iTop = m_iBot = 0;

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

					qreal x = oThis.x();
					qreal y = oThis.y();
					if ( x > leftMargin && x < rightMargin &&
						 y < botMargin  && y > topMargin) {

						if ( x < xLeft ) {
							m_iLeft = i;
							xLeft = x;
						}
						if ( x > xRight ) {
							m_iRight = i;
							xRight = x;
						}
						if (  y > yBot ) {
							m_iBot = i;
							yBot = y;
						}
						if ( y < yTop ) {
							m_iTop = i;
							yTop = y;
						}
					}
                }

                else if ( isVisibleLast && ! isVisible ) {
                    oMid = map->clipLine( pLast, pThis, scale );
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

	drawLabels( ks, psky, scale );
}

void LineListComponent::drawLabels( KStars* kstars, QPainter& psky, double scale )
{

	if ( LabelPosition == NoLabel ) return;

	int     i[4];
	double  a[4] = { 360.0, 360.0, 360.0, 360.0 };
	QPointF o[4];

	if ( LabelPosition == LeftEdgeLabel ) {
		i[0] = m_iLeft;
		i[1] = m_iTop;
		i[2] = m_iBot;
		i[3] = m_iRight;
	}
	else {
		i[0] = m_iRight;
		i[1] = m_iBot;
		i[2] = m_iTop;
		i[3] = m_iLeft;
	}

	// Make sure we have at least one valid point
	for ( int j = 1; j < 4; j++) {
		if ( i[0] ) break;
		i[0] = i[j];
	}
	if ( i[0] == 0 ) return;

	SkyMap *map = kstars->map();
	double comfyAngle = 45.0;

	// Try the points in order and print the label if we can draw it at
	// a comfortable angle for viewing;

	for ( int j = 0; j < 4; j++) {
		o[j] = angleAt( map, i[j], &a[j], scale );
		//printf("a[%d]=%.f  ", j, a[j]);
		if ( fabs( a[j] ) < comfyAngle ) {
			//printf("\n");
			return drawTheLabel( psky, o[j], a[j] );
		}
	}
	//printf("\n");

	// No angle was great so pick the best angle
	int ii = 0;
	for ( int j = 1; j < 4; j++) {
		if ( fabs(a[j]) < fabs(a[ii]) ) ii = j;
	}

	return drawTheLabel( psky, o[ii], a[ii] );
}

void LineListComponent::drawTheLabel( QPainter& psky, QPointF& o, double angle )
{
	m_skyLabeler->useStdFont( psky );
	QFontMetricsF fm( psky.font() );

	psky.save();
	psky.translate( o );

	psky.rotate( double( angle ) );              //rotate the coordinate system
	psky.drawText( QPointF( 0., fm.height() ), Label );
	psky.restore();                              //reset coordinate system

	m_skyLabeler->resetFont( psky );
}

QPointF LineListComponent::angleAt( SkyMap* map, int i, double *angle, double scale )
{
	SkyPoint* pThis = points()->at( i );
	SkyPoint* pLast = points()->at( i - 1 );
	QPointF oThis = map->toScreen( pThis, scale, false );
	QPointF oLast = map->toScreen( pLast, scale, false );
	double sx = double( oThis.x() - oLast.x() );
	double sy = double( oThis.y() - oLast.y() );
	*angle = atan2( sy, sx ) * 180.0 / dms::PI;
	if ( *angle < -90.0 ) *angle += 180.0;
	if ( *angle >  90.0 ) *angle -= 180.0;
	return oThis;
}


