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

#include "skyqpainter.h"

#include <math.h>
#define RAD2DEG (180.0/M_PI)

#include <QMap>

#include "Options.h"


namespace {

    // Convert spectral class to numerical index.
    // If spectral class is invalid return index for white star (A class)
    int harvardToIndex(char c) {
        switch( c ) {
        case 'o': case 'O': return 0;
        case 'b': case 'B': return 1;
        case 'a': case 'A': return 2;
        case 'f': case 'F': return 3;
        case 'g': case 'G': return 4;
        case 'k': case 'K': return 5;
        case 'm': case 'M': return 6;
        // For unknown spectral class assume A class (white star)
        default: return 2;
        }
    }

    // Total number of sizes of stars.
    const int nStarSizes = 15;
    // Total number of specatral classes
    // N.B. Must be in sync with harvardToIndex
    const int nSPclasses = 7;

    // Cache for star images.
    //
    // These pixmaps are never deallocated. Not really good...
    QPixmap* imageCache[nSPclasses][nStarSizes] = {{0}};
}

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

void SkyQPainter::initImages()
{

    QMap<char, QColor> ColorMap;
    const int starColorIntensity = Options::starColorIntensity();

    switch( Options::starColorMode() ) {
    case 1: // Red stars.
        ColorMap.insert( 'O', QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( 'B', QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( 'A', QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( 'F', QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( 'G', QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( 'K', QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( 'M', QColor::fromRgb( 255,   0,   0 ) );
        break;
    case 2: // Black stars.
        ColorMap.insert( 'O', QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( 'B', QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( 'A', QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( 'F', QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( 'G', QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( 'K', QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( 'M', QColor::fromRgb(   0,   0,   0 ) );
        break;
    case 3: // White stars
        ColorMap.insert( 'O', QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( 'B', QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( 'A', QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( 'F', QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( 'G', QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( 'K', QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( 'M', QColor::fromRgb( 255, 255, 255 ) );
    case 0:  // Real color
    default: // And use real color for everything else
        ColorMap.insert( 'O', QColor::fromRgb(   0,   0, 255 ) );
        ColorMap.insert( 'B', QColor::fromRgb(   0, 200, 255 ) );
        ColorMap.insert( 'A', QColor::fromRgb(   0, 255, 255 ) );
        ColorMap.insert( 'F', QColor::fromRgb( 200, 255, 100 ) );
        ColorMap.insert( 'G', QColor::fromRgb( 255, 255,   0 ) );
        ColorMap.insert( 'K', QColor::fromRgb( 255, 100,   0 ) );
        ColorMap.insert( 'M', QColor::fromRgb( 255,   0,   0 ) );
    }

    foreach( char color, ColorMap.keys() ) {
        QPixmap BigImage( 15, 15 );
        BigImage.fill( Qt::transparent );

        QPainter p;
        p.begin( &BigImage );

        if ( Options::starColorMode() == 0 ) {
            qreal h, s, v, a;
            p.setRenderHint( QPainter::Antialiasing, false );
            QColor starColor = ColorMap[color];
            starColor.getHsvF(&h, &s, &v, &a);
            for (int i = 0; i < 8; i++ ) {
                for (int j = 0; j < 8; j++ ) {
                    qreal x = i - 7;
                    qreal y = j - 7;
                    qreal dist = sqrt( x*x + y*y ) / 7.0;
                    starColor.setHsvF(h,
                                      qMin( qreal(1), dist < (10-starColorIntensity)/10.0 ? 0 : dist ),
                                      v,
                                      qMax( qreal(0), dist < (10-starColorIntensity)/20.0 ? 1 : 1-dist ) );
                    p.setPen( starColor );
                    p.drawPoint( i, j );
                    p.drawPoint( 14-i, j );
                    p.drawPoint( i, 14-j );
                    p.drawPoint (14-i, 14-j);
                }
            }
        } else {
            p.setRenderHint(QPainter::Antialiasing, true );
            p.setPen( QPen(ColorMap[color], 2.0 ) );
            p.setBrush( p.pen().color() );
            p.drawEllipse( QRectF( 2, 2, 10, 10 ) );
        }
        p.end();

        // Cahce array slice
        QPixmap** pmap = imageCache[ harvardToIndex(color) ];
        for( int size = 1; size < nStarSizes; size++ ) {
            if( !pmap[size] )
                pmap[size] = new QPixmap();
            *pmap[size] = BigImage.scaled( size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
        }
    }
}

void SkyQPainter::drawScreenStar(const QPointF& pos, float size, char sp)
{
    int isize = qMin(static_cast<int>(size), 14);
    QPixmap* im = imageCache[ harvardToIndex(sp) ][isize];
    float offset = 0.5 * im->width();
    drawPixmap( QPointF(pos.x()-offset, pos.y()-offset), *im );
}



#if 0
void SkyQPainter::drawStar(SkyPoint* loc, float mag, char sp)
{
//SKELETON
}
#endif