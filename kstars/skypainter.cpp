/*
    SkyPainter: class for painting onto the sky for KStars
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include "skypainter.h"
#include "skymap.h"

void SkyPainter::drawSkyLine(SkyPoint* a, SkyPoint* b)
{
    bool aVisible, bVisible;
    QPointF aScreen = m_sm->toScreen(a,true,&aVisible);
    QPointF bScreen = m_sm->toScreen(b,true,&bVisible);
    if( !m_sm->onScreen(aScreen,bScreen) )
        return;
    //THREE CASES:
    if( aVisible && bVisible ) {
        //Both a,b visible, so paint the line normally:
        drawScreenLine(aScreen, bScreen);
    } else if( aVisible ) {
        //a is visible but b isn't:
        drawScreenLine(aScreen, m_sm->clipLine(a,b));
    } else if( bVisible ) {
        //b is visible but a isn't:
        drawScreenLine(bScreen, m_sm->clipLine(b,a));
    } //FIXME: what if both are offscreen but the line isn't?
}

void SkyPainter::drawSkyPolyline(SkyList* points)
{
    bool isVisible, isVisibleLast;
    SkyPoint* pLast = points->first();
    QPointF   oLast = m_sm->toScreen( pLast, true, &isVisibleLast );

    QPointF oThis, oThis2;
    for ( int j = 1 ; j < points->size() ; j++ ) {
        SkyPoint* pThis = points->at( j );
        oThis2 = oThis = m_sm->toScreen( pThis, true, &isVisible );

        if ( m_sm->onScreen( oThis, oLast) /*&& ! skipAt( lineList, j )*/ ) {

            if ( isVisible && isVisibleLast && m_sm->onscreenLine( oLast, oThis ) ) {
                drawScreenLine( oLast, oThis );
                //updateLabelCandidates( oThis, lineList, j );
            } else if ( isVisibleLast ) {
                QPointF oMid = m_sm->clipLine( pLast, pThis );
                drawScreenLine( oLast, oMid );
            } else if ( isVisible ) {
                QPointF oMid = m_sm->clipLine( pThis, pLast );
                drawScreenLine( oMid, oThis );
            }
        }

        pLast = pThis;
        oLast = oThis2;
        isVisibleLast = isVisible;
    }
}

void SkyPainter::drawSkyPolygon(SkyList* points)
{
    bool isVisible, isVisibleLast;
    SkyPoint* pLast = points->last();
    QPointF   oLast = m_sm->toScreen( pLast, true, &isVisibleLast );

    QPolygonF polygon;
    for ( int i = 0; i < points->size(); ++i ) {
        SkyPoint* pThis = points->at( i );
        QPointF oThis = m_sm->toScreen( pThis, true, &isVisible );

        if ( isVisible && isVisibleLast ) {
            polygon << oThis;
        } else if ( isVisibleLast ) {
            QPointF oMid = m_sm->clipLine( pLast, pThis );
            polygon << oMid;
        } else if ( isVisible ) {
            QPointF oMid = m_sm->clipLine( pThis, pLast );
            polygon << oMid;
            polygon << oThis;
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }

    if ( polygon.size() )
        drawScreenPolygon(polygon);
}


