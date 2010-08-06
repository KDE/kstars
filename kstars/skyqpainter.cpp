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

#include <QMap>

#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"

#include "skycomponents/linelist.h"
#include "skycomponents/skiplist.h"
#include "skycomponents/linelistlabel.h"

#include "skyobjects/deepskyobject.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/trailobject.h"

#include "projections/projector.h"
#include "ksutils.h"

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

SkyQPainter::SkyQPainter(SkyMap* sm, QPaintDevice *pd)
    : SkyPainter(sm), QPainter()
{
    //Set paint device pointer to pd or sm if pd = 0
    m_pd = (pd ? pd : sm);
}

SkyQPainter::~SkyQPainter()
{
}

void SkyQPainter::begin()
{
    QPainter::begin(m_pd);
    bool aa = !m_sm->isSlewing() && Options::useAntialias();
    setRenderHint(QPainter::Antialiasing, aa );
    setRenderHint(QPainter::HighQualityAntialiasing, aa);
    m_proj = m_sm->projector();
}

void SkyQPainter::end()
{
    QPainter::end();
}

void SkyQPainter::drawSkyBackground()
{
    //FIXME use projctor
    fillRect( 0, 0, m_sm->width(), m_sm->height(), KStarsData::Instance()->colorScheme()->colorNamed( "SkyColor" ) );
}

void SkyQPainter::setPen(const QPen& pen)
{
    QPainter::setPen(pen);
}

void SkyQPainter::setBrush(const QBrush& brush)
{
    QPainter::setBrush(brush);
}

void SkyQPainter::initStarImages()
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

void SkyQPainter::drawSkyLine(SkyPoint* a, SkyPoint* b)
{
    bool aVisible, bVisible;
    QPointF aScreen = m_proj->toScreen(a,true,&aVisible);
    QPointF bScreen = m_proj->toScreen(b,true,&bVisible);
    //THREE CASES:
    if( aVisible && bVisible ) {
        //Both a,b visible, so paint the line normally:
        drawLine(aScreen, bScreen);
    } else if( aVisible ) {
        //a is visible but b isn't:
        drawLine(aScreen, m_proj->clipLine(a,b));
    } else if( bVisible ) {
        //b is visible but a isn't:
        drawLine(bScreen, m_proj->clipLine(b,a));
    } //FIXME: what if both are offscreen but the line isn't?
}

void SkyQPainter::drawSkyPolyline(LineList* list, SkipList* skipList, LineListLabel* label)
{
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;
    QPointF   oLast = m_proj->toScreen( points->first(), true, &isVisibleLast );
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility( points->first() );

    QPointF oThis, oThis2;
    for ( int j = 1 ; j < points->size() ; j++ ) {
        SkyPoint* pThis = points->at( j );
        oThis2 = oThis = m_proj->toScreen( pThis, true, &isVisible );
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);
        bool doSkip = false;
        if( skipList ) {
            doSkip = skipList->skip(j);
        }
        //FIXME: check whether this works.
        if ( m_sm->onScreen( oThis, oLast) && !doSkip ) {
            if ( isVisible && isVisibleLast && label )
                label->updateLabelCandidates(oThis.x(), oThis.y(), list, j);
            if ( isVisible || isVisibleLast )
                drawLine( oLast, oThis);
        }

        oLast = oThis2;
        isVisibleLast = isVisible;
    }
}

void SkyQPainter::drawSkyPolygon(LineList* list)
{
    SkyList *points = list->points();
    bool isVisible, isVisibleLast;
    SkyPoint* pLast = points->last();
    QPointF   oLast = m_proj->toScreen( pLast, true, &isVisibleLast );
    // & with the result of checkVisibility to clip away things below horizon
    isVisibleLast &= m_proj->checkVisibility(pLast);


    QPolygonF polygon;
    for ( int i = 0; i < points->size(); ++i ) {
        SkyPoint* pThis = points->at( i );
        QPointF oThis = m_proj->toScreen( pThis, true, &isVisible );
        // & with the result of checkVisibility to clip away things below horizon
        isVisible &= m_proj->checkVisibility(pThis);


        if ( isVisible && isVisibleLast ) {
            polygon << oThis;
        } else if ( isVisibleLast ) {
            QPointF oMid = m_proj->clipLine( pLast, pThis );
            polygon << oMid;
        } else if ( isVisible ) {
            QPointF oMid = m_proj->clipLine( pThis, pLast );
            polygon << oMid;
            polygon << oThis;
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }

    if ( polygon.size() )
        drawPolygon(polygon);
}

bool SkyQPainter::drawPlanet(KSPlanetBase* planet)
{
    if( !m_proj->checkVisibility(planet) ) return false;

    bool visible = false;
    QPointF pos = m_proj->toScreen(planet,true,&visible);
    if( !visible || !m_proj->onScreen(pos) ) return false;

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
        drawPointSource(pos,fakeStarSize,spType);
    } else {
        float sizemin = 1.0;
        if( planet->name() == "Sun" || planet->name() == "Moon" )
            sizemin = 8.0;
        sizemin *= skyMap()->scale();

        float size = planet->angSize() * skyMap()->scale() * dms::PI * Options::zoomFactor()/10800.0;
        if ( size < sizemin )
            size = sizemin;
        if ( Options::showPlanetImages() && !planet->image()->isNull() ) {
            dms pa( m_proj->findPA( planet, pos.x(), pos.y() ) );

            //FIXME: Need to figure out why the size is sometimes NaN.
            Q_ASSERT( !isnan( size ) && "Core dumps are good for you NaNs");

            //Because Saturn has rings, we inflate its image size by a factor 2.5
            if( planet->name() == "Saturn" )
                size = int(2.5*size);

            planet->scaleRotateImage( size, pa.Degrees() );
            float x1 = pos.x() - 0.5*planet->image()->width();
            float y1 = pos.y() - 0.5*planet->image()->height();
            drawImage( QPointF(x1, y1), *( planet->image() ) );
        } else { //Otherwise, draw a simple circle.
            drawEllipse( pos, size, size );
        }
    }
    return true;
}

bool SkyQPainter::drawPointSource(SkyPoint* loc, float mag, char sp)
{
    //Check if it's even visible before doing anything
    if( !m_proj->checkVisibility(loc) ) return false;

    bool visible = false;
    QPointF pos = m_proj->toScreen(loc,true,&visible);
    if( visible && m_proj->onScreen(pos) ) {
        drawPointSource(pos, starWidth(mag), sp);
        return true;
    } else {
        return false;
    }
}

void SkyQPainter::drawPointSource(const QPointF& pos, float size, char sp)
{
    int isize = qMin(static_cast<int>(size), 14);
    QPixmap* im = imageCache[ harvardToIndex(sp) ][isize];
    float offset = 0.5 * im->width();
    drawPixmap( QPointF(pos.x()-offset, pos.y()-offset), *im );
}

bool SkyQPainter::drawDeepSkyObject(DeepSkyObject* obj, bool drawImage)
{
    if( !m_proj->checkVisibility(obj) ) return false;

    bool visible = false;
    QPointF pos = m_proj->toScreen(obj, true, &visible);
    if( !visible || !m_proj->onScreen(pos) ) return false;

    float positionAngle = m_proj->findPA( obj, pos.x(), pos.y() );

    //Draw Image
    if ( drawImage && Options::zoomFactor() > 5.*MINZOOM )
        drawDeepSkyImage(pos, obj, positionAngle);

    //Draw Symbol
    drawDeepSkySymbol(pos, obj, positionAngle);
    return true;
}

bool SkyQPainter::drawDeepSkyImage(const QPointF& pos, DeepSkyObject* obj, float positionAngle)
{
    float x = pos.x();
    float y = pos.y();
    QImage *image = obj->readImage();
    QImage ScaledImage;

    if ( !image ) return false;
    
    double scale = skyMap()->scale();
    float zoom = Options::zoomFactor();
    float w = obj->a() * scale * dms::PI * zoom/10800.0;

    float h = w*image->height()/image->width(); //preserve image's aspect ratio
    float dx = 0.5*w;
    float dy = 0.5*h;
    ScaledImage = image->scaled( int(w), int(h) );
    save();
    translate( x, y );
    rotate( positionAngle );  //rotate the coordinate system

    if ( Options::useAntialias() )
        drawImage( QPointF( -dx, -dy ), ScaledImage );
    else
        drawImage( QPoint( -1*int(dx), -1*int(dy) ), ScaledImage );

    restore();
        
    return true;
}

//FIXME: Do we really need two versions of all of this code? (int/float)
void SkyQPainter::drawDeepSkySymbol(const QPointF& pos, DeepSkyObject* obj, float positionAngle)
{
    float x = pos.x();
    float y = pos.y();
    float zoom = Options::zoomFactor();
    // if size is 0.0 set it to 1.0, this are normally stars (type 0 and 1)
    // if we use size 0.0 the star wouldn't be drawn
    float majorAxis = obj->a();
    if ( majorAxis == 0.0 ) {   majorAxis = 1.0; }

    double scale = SkyMap::Instance()->scale();
    float size = scale * majorAxis * dms::PI * zoom / 10800.0;
    int isize = int(size);

    float dx1 = -0.5*size;
    float dx2 =  0.5*size;
    float dy1 = -1.0*obj->e()*size/2.;
    float dy2 = obj->e()*size/2.;
    float x1 = x + dx1;
    float x2 = x + dx2;
    float y1 = y + dy1;
    float y2 = y + dy2;

    float dxa = -size/4.;
    float dxb =  size/4.;
    float dya = -1.0*obj->e()*size/4.;
    float dyb = obj->e()*size/4.;
    float xa = x + dxa;
    float xb = x + dxb;
    float ya = y + dya;
    float yb = y + dyb;

    float psize;

    QBrush tempBrush;

    switch ( obj->type() ) {
    case 0:
    case 1: //catalog star
        //Some NGC/IC objects are stars...changed their type to 1 (was double star)
        if (size<2.) size = 2.;
        if ( Options::useAntialias() )
            drawEllipse( QRectF(x1, y1, size/2., size/2.) );
        else
            drawEllipse( QRect(int(x1), int(y1), int(size/2), int(size/2)) );
        break;
    case 2: //Planet
        break;
    case 3: //Open cluster; draw circle of points
    case 13: // Asterism
        tempBrush = brush();
        setBrush( pen().color() );
        psize = 2.;
        if ( size > 50. )  psize *= 2.;
        if ( size > 100. ) psize *= 2.;
        if ( Options::useAntialias() ) {
            drawEllipse( QRectF(xa, y1, psize, psize) );
            drawEllipse( QRectF(xb, y1, psize, psize) );
            drawEllipse( QRectF(xa, y2, psize, psize) );
            drawEllipse( QRectF(xb, y2, psize, psize) );
            drawEllipse( QRectF(x1, ya, psize, psize) );
            drawEllipse( QRectF(x1, yb, psize, psize) );
            drawEllipse( QRectF(x2, ya, psize, psize) );
            drawEllipse( QRectF(x2, yb, psize, psize) );
        } else {
            int ix1 = int(x1); int iy1 = int(y1);
            int ix2 = int(x2); int iy2 = int(y2);
            int ixa = int(xa); int iya = int(ya);
            int ixb = int(xb); int iyb = int(yb);
            drawEllipse( QRect(ixa, iy1, int(psize), int(psize)) );
            drawEllipse( QRect(ixb, iy1, int(psize), int(psize)) );
            drawEllipse( QRect(ixa, iy2, int(psize), int(psize)) );
            drawEllipse( QRect(ixb, iy2, int(psize), int(psize)) );
            drawEllipse( QRect(ix1, iya, int(psize), int(psize)) );
            drawEllipse( QRect(ix1, iyb, int(psize), int(psize)) );
            drawEllipse( QRect(ix2, iya, int(psize), int(psize)) );
            drawEllipse( QRect(ix2, iyb, int(psize), int(psize)) );
        }
        setBrush( tempBrush );
        break;
    case 4: //Globular Cluster
        if (size<2.) size = 2.;
        save();
        translate( x, y );
        rotate( positionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            drawEllipse( QRectF(dx1, dy1, size, obj->e()*size) );
            drawLine( QPointF(0., dy1), QPointF(0., dy2) );
            drawLine( QPointF(dx1, 0.), QPointF(dx2, 0.) );
            restore(); //reset coordinate system
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            drawEllipse( QRect(idx1, idy1, isize, int(obj->e()*size)) );
            drawLine( QPoint(0, idy1), QPoint(0, idy2) );
            drawLine( QPoint(idx1, 0), QPoint(idx2, 0) );
            restore(); //reset coordinate system
        }
        break;

    case 5: //Gaseous Nebula
    case 15: // Dark Nebula
        if (size <2.) size = 2.;
        save();
        translate( x, y );
        rotate( positionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            drawLine( QPointF(dx1, dy1), QPointF(dx2, dy1) );
            drawLine( QPointF(dx2, dy1), QPointF(dx2, dy2) );
            drawLine( QPointF(dx2, dy2), QPointF(dx1, dy2) );
            drawLine( QPointF(dx1, dy2), QPointF(dx1, dy1) );
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            drawLine( QPoint(idx1, idy1), QPoint(idx2, idy1) );
            drawLine( QPoint(idx2, idy1), QPoint(idx2, idy2) );
            drawLine( QPoint(idx2, idy2), QPoint(idx1, idy2) );
            drawLine( QPoint(idx1, idy2), QPoint(idx1, idy1) );
        }
        restore(); //reset coordinate system
        break;
    case 6: //Planetary Nebula
        if (size<2.) size = 2.;
        save();
        translate( x, y );
        rotate( positionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            drawEllipse( QRectF(dx1, dy1, size, obj->e()*size) );
            drawLine( QPointF(0., dy1), QPointF(0., dy1 - obj->e()*size/2. ) );
            drawLine( QPointF(0., dy2), QPointF(0., dy2 + obj->e()*size/2. ) );
            drawLine( QPointF(dx1, 0.), QPointF(dx1 - size/2., 0.) );
            drawLine( QPointF(dx2, 0.), QPointF(dx2 + size/2., 0.) );
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            drawEllipse( QRect( idx1, idy1, isize, int(obj->e()*size) ) );
            drawLine( QPoint(0, idy1), QPoint(0, idy1 - int(obj->e()*size/2) ) );
            drawLine( QPoint(0, idy2), QPoint(0, idy2 + int(obj->e()*size/2) ) );
            drawLine( QPoint(idx1, 0), QPoint(idx1 - int(size/2), 0) );
            drawLine( QPoint(idx2, 0), QPoint(idx2 + int(size/2), 0) );
        }

        restore(); //reset coordinate system
        break;
    case 7: //Supernova remnant
        if (size<2) size = 2;
        save();
        translate( x, y );
        rotate( positionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            drawLine( QPointF(0., dy1), QPointF(dx2, 0.) );
            drawLine( QPointF(dx2, 0.), QPointF(0., dy2) );
            drawLine( QPointF(0., dy2), QPointF(dx1, 0.) );
            drawLine( QPointF(dx1, 0.), QPointF(0., dy1) );
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            drawLine( QPoint(0, idy1), QPoint(idx2, 0) );
            drawLine( QPoint(idx2, 0), QPoint(0, idy2) );
            drawLine( QPoint(0, idy2), QPoint(idx1, 0) );
            drawLine( QPoint(idx1, 0), QPoint(0, idy1) );
        }

        restore(); //reset coordinate system
        break;
    case 8: //Galaxy
    case 16: // Quasar
        if ( size <1. && zoom > 20*MINZOOM ) size = 3.; //force ellipse above zoomFactor 20
        if ( size <1. && zoom > 5*MINZOOM ) size = 1.; //force points above zoomFactor 5
        if ( size>2. ) {
            save();
            translate( x, y );
            rotate( positionAngle );  //rotate the coordinate system

            if ( Options::useAntialias() ) {
                drawEllipse( QRectF(dx1, dy1, size, obj->e()*size) );
            } else {
                int idx1 = int(dx1); int idy1 = int(dy1);
                drawEllipse( QRect(idx1, idy1, isize, int(obj->e()*size)) );
            }

            restore(); //reset coordinate system

        } else if ( size>0. ) {
            drawPoint( QPointF(x, y) );
        }
        break;
    case 14: // Galaxy cluster - draw a circle of + marks
        tempBrush = brush();
        setBrush( pen().color() );
        psize = 1.;
        if ( size > 50. )  psize *= 2.;

        if ( Options::useAntialias() ) {
            drawLine( QLineF( xa-psize, y1, xa+psize, y1 ) );
            drawLine( QLineF( xa, y1-psize, xa, y1+psize ) );
            drawLine( QLineF( xb-psize, y1, xa+psize, y1 ) );
            drawLine( QLineF( xb, y1-psize, xa, y1+psize ) );
            drawLine( QLineF( xa-psize, y2, xa+psize, y1 ) );
            drawLine( QLineF( xa, y2-psize, xa, y1+psize ) );
            drawLine( QLineF( xb-psize, y2, xa+psize, y1 ) );
            drawLine( QLineF( xb, y2-psize, xa, y1+psize ) );
            drawLine( QLineF( x1-psize, ya, xa+psize, y1 ) );
            drawLine( QLineF( x1, ya-psize, xa, y1+psize ) );
            drawLine( QLineF( x1-psize, yb, xa+psize, y1 ) );
            drawLine( QLineF( x1, yb-psize, xa, y1+psize ) );
            drawLine( QLineF( x2-psize, ya, xa+psize, y1 ) );
            drawLine( QLineF( x2, ya-psize, xa, y1+psize ) );
            drawLine( QLineF( x2-psize, yb, xa+psize, y1 ) );
            drawLine( QLineF( x2, yb-psize, xa, y1+psize ) );
        } else {
            int ix1 = int(x1); int iy1 = int(y1);
            int ix2 = int(x2); int iy2 = int(y2);
            int ixa = int(xa); int iya = int(ya);
            int ixb = int(xb); int iyb = int(yb);
            drawLine( QLine( ixa - int(psize), iy1, ixa + int(psize), iy1 ) );
            drawLine( QLine( ixa, iy1 - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ixb - int(psize), iy1, ixa + int(psize), iy1 ) );
            drawLine( QLine( ixb, iy1 - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ixa - int(psize), iy2, ixa + int(psize), iy1 ) );
            drawLine( QLine( ixa, iy2 - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ixb - int(psize), iy2, ixa + int(psize), iy1 ) );
            drawLine( QLine( ixb, iy2 - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ix1 - int(psize), iya, ixa + int(psize), iy1 ) );
            drawLine( QLine( ix1, iya - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ix1 - int(psize), iyb, ixa + int(psize), iy1 ) );
            drawLine( QLine( ix1, iyb - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ix2 - int(psize), iya, ixa + int(psize), iy1 ) );
            drawLine( QLine( ix2, iya - int(psize), ixa, iy1 + int(psize) ) );
            drawLine( QLine( ix2 - int(psize), iyb, ixa + int(psize), iy1 ) );
            drawLine( QLine( ix2, iyb - int(psize), ixa, iy1 + int(psize) ) );
        }
        setBrush( tempBrush );
        break;
    }
}

void SkyQPainter::drawObservingList(const QList< SkyObject* >& obs)
{
    foreach ( SkyObject* obj, obs ) {
        bool visible = false;
        QPointF o = m_proj->toScreen( obj, true, &visible );
        if( !visible || !m_proj->onScreen(o) ) continue;
        
        float size = 20.;
        float x1 = o.x() - 0.5*size;
        float y1 = o.y() - 0.5*size;
        drawArc( QRectF(x1, y1, size, size), -60*16, 120*16 );
        drawArc( QRectF(x1, y1, size, size), 120*16, 120*16 );
    }
}

void SkyQPainter::drawHorizon(bool filled, SkyPoint* labelPoint, bool* drawLabel)
{
    QVector<Vector2f> ground = m_proj->groundPoly(labelPoint, drawLabel);
    QPolygonF groundPoly(ground.size());
    for(int i = 0; i < ground.size(); ++i)
        groundPoly[i] = KSUtils::vecToPoint(ground[i]);
    if( filled )
        drawPolygon(groundPoly);
    else {
        groundPoly.append( groundPoly.first() );
        drawPolyline(groundPoly);
    }
}

