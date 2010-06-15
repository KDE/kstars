/*

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

#ifndef SKYQPAINTER_H
#define SKYQPAINTER_H

#include "skypainter.h"

/** @short The QPainter-based painting backend.
    This class implements the SkyPainter interface using a QPainter.
    For documentation, @see SkyPainter. */
class SkyQPainter : public SkyPainter, public QPainter
{
public:
    SkyQPainter(SkyMap *sm);
    virtual ~SkyQPainter();
    virtual void setPen(const QPen& pen);
    virtual void setBrush(const QBrush& brush);
    /** Recalculates the star pixmaps. */
    static void initImages();
    //Screen drawing functions
    virtual void drawScreenRect(float x, float y, float w, float h);
    virtual void drawScreenPolyline(const QPolygonF& polyline);
    virtual void drawScreenPolygon(const QPolygonF& polygon);
    virtual void drawScreenLine(const QPointF& a, const QPointF& b);
    virtual void drawScreenLine(float x1, float y1, float x2, float y2);
    virtual void drawScreenEllipse(float x, float y, float width, float height, float theta);
protected:
    virtual void drawScreenStar(const QPointF& pos, float size, char sp);
};

#endif
