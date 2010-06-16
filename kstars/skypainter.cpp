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
#include "Options.h"
#include "kstarsdata.h"
#include "skycomponents/skiplist.h"
#include "skycomponents/linelistlabel.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/ksplanetbase.h"

SkyPainter::SkyPainter(SkyMap* sm)
    : m_sizeMagLim(10.),
      m_sm(sm)
{

}

SkyPainter::~SkyPainter()
{

}

SkyMap* SkyPainter::skyMap() const
{
    return m_sm;
}

void SkyPainter::setSizeMagLimit(float sizeMagLim)
{
    m_sizeMagLim = sizeMagLim;
}

float SkyPainter::starWidth(float mag) const
{
    //adjust maglimit for ZoomLevel
    const double maxSize = 10.0;

    double lgmin = log10(MINZOOM);
//    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());

    float sizeFactor = maxSize + (lgz - lgmin);

    float size = ( sizeFactor*( m_sizeMagLim - mag ) / m_sizeMagLim ) + 1.;
    if( size <= 1.0 ) size = 1.0;
    if( size > maxSize ) size = maxSize;

    return size;
}

bool SkyPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    //Check if it's even visible before doing anything
    if( !m_sm->checkVisibility(obj) ) return false;

    QPointF pos = m_sm->toScreen(obj);
     //FIXME: is this check necessary?
    if( !m_sm->onScreen(pos) ) return false;

    float positionAngle = m_sm->findPA( obj, pos.x(), pos.y() );

    //Draw Image
    bool imgdrawn = false;
    if ( drawImage && Options::zoomFactor() > 5.*MINZOOM ) {
        imgdrawn = drawScreenDeepSkyImage(pos, obj, positionAngle);
    }

    //Draw Symbol
    drawScreenDeepSkySymbol(pos, obj, positionAngle);
    return true;
}

bool SkyPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    //Check if it's even visible before doing anything
    if( !m_sm->checkVisibility(loc) ) return false;
    
    QPointF pos = m_sm->toScreen(loc);
     //FIXME: is this check necessary?
    if( m_sm->onScreen(pos) ) {
        drawScreenPointSource(pos,starWidth(mag),sp);
        return true;
    } else {
        return false;
    }
}

bool SkyPainter::drawComet(KSComet* comet)
{
    if( !m_sm->checkVisibility(comet) ) return false;

    QPointF pos = m_sm->toScreen(comet);
     //FIXME: is this check necessary?
    if( !m_sm->onScreen(pos) ) return false;

    drawScreenComet(pos,comet);
    return true;
}

bool SkyPainter::drawPlanet(KSPlanetBase* planet)
{
    if( !m_sm->checkVisibility(planet) ) return false;

    QPointF pos = m_sm->toScreen(planet);
     //FIXME: is this check necessary?
    if( !m_sm->onScreen(pos) ) return false;

    float fakeStarSize = ( 10.0 + log10( Options::zoomFactor() ) - log10( MINZOOM ) ) * ( 10 - planet->mag() ) / 10;
    if( fakeStarSize > 15.0 )
        fakeStarSize = 15.0;

    float size = planet->angSize() * m_sm->scale() * dms::PI * Options::zoomFactor()/10800.0;
    if( size < fakeStarSize && planet->name() != "Sun" && planet->name() != "Moon" ) {
        // Draw them as bright stars of appropriate color instead of images
        char spType;
        //FIXME: do these need i18n?
        if( planet->name() == i18n("Mars") ) {
            spType = 'K';
        } else if( planet->name() == i18n("Jupiter") || planet->name() == i18n("Mercury") || planet->name() == i18n("Saturn") ) {
            spType = 'F';
        } else {
            spType = 'B';
        }
        drawScreenPointSource(pos,fakeStarSize,spType);
    } else {
        drawScreenPlanet(pos,planet);
    }
    return true;
}

bool SkyPainter::drawAsteroid(KSAsteroid *ast)
{
    if( !m_sm->checkVisibility(ast) ) return false;

    QPointF pos = m_sm->toScreen(ast);
     //FIXME: is this check necessary?
    if( !m_sm->onScreen(pos) ) return false;

    drawScreenAsteroid(pos,ast);
    return true;
}

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

void SkyPainter::drawSkyPolyline(LineList* list, SkipList* skipList, LineListLabel* label)
{
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;
    SkyPoint* pLast = points->first();
    QPointF   oLast = m_sm->toScreen( pLast, true, &isVisibleLast );

    QPointF oThis, oThis2;
    for ( int j = 1 ; j < points->size() ; j++ ) {
        SkyPoint* pThis = points->at( j );
        oThis2 = oThis = m_sm->toScreen( pThis, true, &isVisible );
        bool doSkip = false;
        if( skipList ) {
            doSkip = skipList->skip(j);
        }

        if ( m_sm->onScreen( oThis, oLast) && !doSkip ) {

            if ( isVisible && isVisibleLast && m_sm->onscreenLine( oLast, oThis ) ) {
                drawScreenLine( oLast, oThis );
                if( label ) {
                    label->updateLabelCandidates(oThis.x(), oThis.y(), list, j);
                }
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

void SkyPainter::drawSkyPolygon(LineList* list)
{
    SkyList *points = list->points();
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


