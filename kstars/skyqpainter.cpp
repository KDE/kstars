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

#include <math.h>
#define RAD2DEG (180.0/M_PI)

#include "skyqpainter.h"

SkyQPainter::SkyQPainter(SkyMap* sm)
    : SkyPainter(sm), QPainter()
{
    //
}

SkyQPainter::~SkyQPainter()
{
}

void SkyQPainter::drawScreenRect(int x, int y, int w, int h)
{
    drawRect(x,y,w,h);
}

void SkyQPainter::drawScreenRect(float x, float y, float w, float h)
{
    drawRect(QRectF(x,y,w,h));
}

void SkyQPainter::drawScreenPolyline(const QPolygon& polyline)
{
    drawPolyline(polyline);
}

void SkyQPainter::drawScreenPolyline(const QPolygonF& polyline)
{
    drawPolyline(polyline);
}

void SkyQPainter::drawScreenPolygon(const QPolygonF& polygon)
{
    drawPolygon(polygon);
}

void SkyQPainter::drawScreenPolygon(const QPolygon& polygon)
{
    drawPolygon(polygon);
}


void SkyQPainter::drawScreenLine(int x1, int y1, int x2, int y2)
{
    drawLine(x1,y1,x2,y2);
}

void SkyQPainter::drawScreenLine(float x1, float y1, float x2, float y2)
{
    drawLine(QLineF(x1,y1,x2,y2));
}

void SkyQPainter::drawScreenLine(const QPoint& a, const QPoint& b)
{
    drawLine(a,b);
}

void SkyQPainter::drawScreenLine(const QPointF& a, const QPointF& b)
{
    drawLine(a,b);
}


void SkyQPainter::drawScreenEllipse(int x, int y, int width, int height, float theta)
{
    if(theta == 0.0) {
        drawEllipse(QPoint(x,y),width/2,height/2);
        return;
    }
    save();
    translate(x,y);
    rotate(theta*RAD2DEG);
    drawEllipse(QPoint(0,0),width/2,height/2);
    restore();
}

void SkyQPainter::drawScreenEllipse(float x, float y, float width, float height, float theta)
{
    if(theta == 0.0) {
        drawEllipse(QPointF(x,y),width/2,height/2);
        return;
    }
    save();
    translate(x,y);
    rotate(theta*RAD2DEG);
    drawEllipse(QPointF(0,0),width/2,height/2);
    restore();
}

void SkyQPainter::setPen(const QPen& pen)
{
    QPainter::setPen(pen);
}

void SkyQPainter::setBrush(const QBrush& brush)
{
    QPainter::setBrush(brush);
}




#if 0
void SkyQPainter::drawStar(SkyPoint* loc, float mag, char sp)
{
//SKELETON
}
#endif