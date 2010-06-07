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

#ifndef SKYPAINTER_H
#define SKYPAINTER_H

#include <QPainter>

#include "skycomponents/typedef.h"

class SkyPoint;
class SkyMap;


/** @short Draws things on the sky, without regard to backend.
    This class serves as an interface to draw objects onto the sky without
    worrying about whether we are using a QPainter or OpenGL.
. */
class SkyPainter
{
public:
    /** @short Constructor.
        @param sm A pointer to SkyMap object on which to paint.
        */
    SkyPainter(SkyMap *sm) { m_sm = sm; }
    virtual ~SkyPainter() {}
    
    /** @short Get the SkyMap on which the painter operates */
    SkyMap* skyMap() const { return m_sm; }

    /** @short Set the pen of the painter **/
    virtual void setPen(const QPen& pen) = 0;

    /** @short Set the brush of the painter **/
    virtual void setBrush(const QBrush& brush) = 0;

    ////////////////////////////////////
    //                                //
    // SKY DRAWING FUNCTIONS:         //
    //                                //
    ////////////////////////////////////

    /** @short Draw a line between points in the sky.
        @param a the first point
        @param b the second point
        @note this function will skip lines not on screen and clip lines
               that are only partially visible. */
    void drawSkyLine(SkyPoint* a, SkyPoint* b);

    /** @short Draw a polyline in the sky.
        @param points a list of points in the sky
        @note it's faster to use this than repeated calls to drawSkyLine(),
               because it avoids an extra points->size() -2 projections.
        FIXME: Add support for skipping points...
        */
    void drawSkyPolyline(SkyList* points);
    
    /** @short Draw a polygon in the sky.
        @param points a list of points in the sky
        @note this function is identical to drawSkyPolyline() except that it
               closes (and may fill) the polyline.
        @see drawSkyPolyline()
        */
    void drawSkyPolygon(SkyList* points);

    
#if 0
//uncomment later
    /** @short Draw a star.
        @param loc the location of the star in the sky
        @param mag the magnitude of the star
        @param sp the spectral class of the star
        */
    virtual void drawStar(SkyPoint *loc, float mag, char sp) = 0;
#endif


    ////////////////////////////////////
    //                                //
    // SCREEN DRAWING FUNCTIONS:      //
    //                                //
    ////////////////////////////////////


    /** @short Draw an unprojected ellipse in screen coordinates.
        @param x the x coordinate of the centre of the ellipse
        @param y the y coordinate of the centre of the ellipse
        @param width the width of the ellipse
        @param height the height of the ellipse
        @param dashed draws a dashed line if true
        @param theta the angle (in radians) to draw at.
        */
    virtual void drawScreenEllipse(float x, float y,
                                      float width, float height,
                                      float theta =0) = 0;
    /** @see drawScreenEllipse() */
    virtual void drawScreenEllipse(int x, int y,
                                      int width, int height,
                                      float theta =0) = 0;

    /** @short Draw an unprojected line in screen coordinates.
        @param x1 the x coordinate of the first point
        @param y1 the y coordinate of the first point
        @param x2 the x coordinate of the second point
        @param y2 the y coordinate of the second point
        */
    virtual void drawScreenLine(float x1, float y1, float x2, float y2) = 0;
    /** @see drawScreenLine() */
    virtual void drawScreenLine(const QPointF& a, const QPointF& b) = 0;
    /** @see drawScreenLine() */
    virtual void drawScreenLine(int x1, int y1, int x2, int y2) = 0;
    /** @see drawScreenLine() */
    virtual void drawScreenLine(const QPoint& a, const QPoint& b) = 0;

    /** @short Draw an unprojected polyline in screen coordinates.
        @param polyline a list of points that make up the polyline
        */
    virtual void drawScreenPolyline(const QPolygon& polyline) = 0;
    /** @see drawScreenPolyLine() */
    virtual void drawScreenPolyline(const QPolygonF& polyline) = 0;

    /** @short Draw an unprojected rectangle in screen coordinates.
        @param x the x coordinate of the top left point
        @param y the y coordinate of the top left point
        @param w the width of the rectangle
        @param h the height of the rectangle
        */
    virtual void drawScreenRect(float x, float y, float w, float h) = 0;
    /** @see drawScreenRect() */
    virtual void drawScreenRect(int x, int y, int w, int h) = 0;

    /** @short Draw an unprojected polygon in screen coordinates.
        @param polygon the polygon to be drawn.
        */
    virtual void drawScreenPolygon(const QPolygonF& polygon) =0;
    /** @see drawScreenPolygon() */
    virtual void drawScreenPolygon(const QPolygon& polygon) =0;


private:
    inline QPolygonF makePolyline(SkyList* points);
    SkyMap *m_sm;
};

#endif // SKYPAINTER_H
