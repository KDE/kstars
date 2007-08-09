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


// I don't think the ecliptic or the celestial equator should precess. -jbb
void LineListComponent::update( KStarsData *data, KSNumbers *num )
{
    if ( ! selected() ) return;

    foreach ( SkyPoint* p, pointList ) {
        //if ( num ) p->updateCoords( num );
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

	// These are used to keep track of the element that is farrthest left,
	// farthest right, etc.
	float xLeft  = 100000.;
	float xRight = 0.;
    float yTop   = 100000.;
	float yBot   = 0.;

	// These are the indices of the farthest left point, farthest right point,
	// etc.  The are data members so the drawLabels() routine can use them.
	// Zero indicates an index that was never set and is considered invalid
	// inside of drawLabels().
	m_iLeft = m_iRight = m_iTop = m_iBot = 0;

	// We don't draw the label here but we need the proper font in order to set
	// the margins correctly.  Since psky no contains the zoom dependent font as
	// its default font, we need to play the little dance below.
	m_skyLabeler->useStdFont( psky );
	QFontMetricsF fm( psky.font() );
	m_skyLabeler->resetFont( psky );

	// Create the margins within which is is okay to draw the label
	float margin      = fm.width("MMM");
	float rightMargin = psky.window().width() - margin - fm.width( Label );
	float leftMargin  = margin;
	float topMargin   = fm.height();
	float botMargin   = psky.window().height() - 4.0 * fm.height();

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
					
					// Keep track of index of leftmost, rightmost, etc point.
					// Only allow points that fit within the margins.
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

	SkyMap *map = kstars->map();

	double comfyAngle = 40.0;  // the first valid candidate with an angle
							   // smaller than this gets displayed.  If you set
							   // this to > 90. then the first valid candidate
							   // will be displayed, regardless of angle.

	// We store info about the four candidate points in arrays to make several
	// of the steps easier, particularly choosing the valid candiate with the
	// smallest angle from the horizontal.

	int     i[4];                                  // index of candidate
	double  a[4] = { 360.0, 360.0, 360.0, 360.0 }; // angle, default to large value
	QPointF o[4];                                  // candidate point
	bool okay[4] = { true, true, true, true };     // flag  candidate false if it
	                                               // overlaps a previous label.

	// Try candidate in different orders depending on if the label was to be
	// near the left or right side of the screen.
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
	int minI = 0;

	for ( ; minI < 4; minI++ ) {
		if ( i[minI] ) break;
	}

	// return if there are no valid candidates
	if ( minI >= 4 ) return;

	// Try the points in order and print the label if we can draw it at
	// a comfortable angle for viewing;
	for ( int j = minI; j < 4; j++ ) {
		o[j] = angleAt( map, i[j], &a[j], scale );
		if ( fabs( a[j] ) > comfyAngle ) continue;
		if ( drawTheLabel( psky, o[j], a[j] ) ) return;
		okay[j] = false;
	}

	//--- No angle was comfy so pick the one with the smallest angle ---

	// Index of the index/angle/point that gets displayed	
	int ii = minI;

	// find first valid candidate that does not overlap existing labels
	for ( ; ii < 4; ii++ ) {
		if ( i[ii] && okay[ii] ) break;
	}

	// return if all candiates either overlap or are invalid
	if ( ii >= 4 ) return;

	// find the valid non-overlap candidate with the smallest angle
	for ( int j = ii + 1; j < 4; j++ ) {
		if ( i[j] && okay[j] && fabs(a[j]) < fabs(a[ii]) ) ii = j;
	}

	drawTheLabel( psky, o[ii], a[ii] );
}

bool LineListComponent::drawTheLabel( QPainter& psky, QPointF& o, double angle )
{
	m_skyLabeler->useStdFont( psky );
	QFontMetricsF fm( psky.font() );

	// Create bounding rectangle by rotating the (height x width) rectangle
	qreal h = fm.height();
	qreal w = fm.width(Label);
	qreal s = sin( angle * dms::PI / 180.);
	qreal c = cos( angle * dms::PI / 180.);

	qreal top, bot, left, right;
	
	// These numbers really do depend on the sign of the angle like this
	if ( angle >= 0.0 ) {
		top   =  o.y();
		bot   =  o.y() + c * h + s * w;
		left  =  o.x() - s * h;
		right =  o.x() + c * w;
	}
	else {
		top   = o.y() + s * w;
		bot   = o.y() + c * h;
		left  = o.x();
		right = o.x() + c * w - s * h;
	}

	// return false if label would overlap existing label
	if ( ! m_skyLabeler->markRegion( top, bot, left, right ) )
		return false;

	// otherwise draw the label and return true
	psky.save();
	psky.translate( o );

	psky.rotate( angle );                        //rotate the coordinate system
	psky.drawText( QPointF( 0.0, fm.height() ), Label );
	psky.restore();                              //reset coordinate system

	m_skyLabeler->resetFont( psky );

	return true;
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

	// Never draw the label upside down
	if ( *angle < -90.0 ) *angle += 180.0;
	if ( *angle >  90.0 ) *angle -= 180.0;

	return oThis;
}


