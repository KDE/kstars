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

#include <QPainter>

#include "skypainter.h"

/** @short The QPainter-based painting backend.
    This class implements the SkyPainter interface using a QPainter.
    For documentation, @see SkyPainter. */
class SkyQPainter : public SkyPainter, public QPainter
{
public:
    SkyQPainter(SkyMap *sm);
    virtual ~SkyQPainter();
    //virtual void drawStar(SkyPoint* loc, float mag, char sp);
    virtual void drawScreenRect(int x, int y, int w, int h);
    virtual void drawScreenRect(float x, float y, float w, float h);
    virtual void drawScreenPolyLine(const QList< QPoint >& points);
    virtual void drawScreenPolyLine(const QList< QPointF >& points);
    virtual void drawScreenLine(int x1, int y1, int x2, int y2);
    virtual void drawScreenLine(float x1, float y1, float x2, float y2);
    virtual void drawScreenEllipse(int x, int y, int width, int height, float theta);
    virtual void drawScreenEllipse(float x, float y, float width, float height, float theta);
};
