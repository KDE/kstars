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

QPolygonF SkyPainter::makePolyline(SkyList* points)
{
    //The polyline to be returned
    QPolygonF polyline;
    //We need at least two points to draw.
    //We need to check that the list is nonempty for prevPoint also.
    if(points->size() < 2)
        return polyline;
    SkyPoint* prevPoint = points->at(0);
    bool prevVisible;
    QPointF prevScreen = m_sm->toScreen(prevPoint,true,&prevVisible);
    if( prevVisible ) { polyline << prevScreen; }
    
    for(int i = 1; i < points->size(); ++i) {
        SkyPoint* curPoint = points->at(i);
        bool curVisible;
        QPointF curScreen = m_sm->toScreen(curPoint,true,&curVisible);
        //THREE CASES:
        if( prevVisible && curVisible ) {
            //Both visible, so add the next point:
            polyline << curScreen;
        } else if( prevVisible ) {
            //Current point invisible, so add the clipped point:
            polyline << m_sm->clipLine(prevPoint, curPoint);
        } else if( curVisible ) {
            //last point is invisible, so add the clipped point and the current point:
            polyline << m_sm->clipLine(curPoint,  prevPoint);
            polyline << curScreen;
        } 
        //Now update the prev* variables for the next line segment
        prevPoint   = curPoint;
        prevScreen  = curScreen;
        prevVisible = curVisible;
    }
    return polyline;
}

void SkyPainter::drawSkyPolyline(SkyList* points)
{
    QPolygonF polyline = makePolyline(points);
    drawScreenPolyline(polyline);
}

void SkyPainter::drawSkyPolygon(SkyList* points)
{
    QPolygonF polygon = makePolyline(points);
    drawScreenPolygon(polygon);
}


