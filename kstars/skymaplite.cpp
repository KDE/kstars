/** *************************************************************************
                          skymaplite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 30/04/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skymaplite.h"
#include "kstarsdata.h"
#include "kstarslite.h"

#include "projections/projector.h"
#include "projections/lambertprojector.h"
#include "projections/gnomonicprojector.h"
#include "projections/stereographicprojector.h"
#include "projections/orthographicprojector.h"
#include "projections/azimuthalequidistantprojector.h"
#include "projections/equirectangularprojector.h"

#include "solarsystemsinglecomponent.h"
#include "Options.h"

//SkyItems
#include "kstarslite/skyitems/planetsitem.h"
#include "kstarslite/skyitems/asteroidsitem.h"
#include "kstarslite/skyitems/cometsitem.h"

#include "ksplanetbase.h"
#include "ksutils.h"

#include <QSGSimpleRectNode>
#include <QSGNode>
#include <QBitmap>
#include <QSGTexture>
#include <QQuickWindow>

namespace {

// Draw bitmap for zoom cursor. Width is size of pen to draw with.
QBitmap zoomCursorBitmap(int width) {
    QBitmap b(32, 32);
    b.fill(Qt::color0);
    int mx = 16, my = 16;
    // Begin drawing
    QPainter p;
    p.begin( &b );
    p.setPen( QPen( Qt::color1, width ) );
    p.drawEllipse( mx - 7, my - 7, 14, 14 );
    p.drawLine(    mx + 5, my + 5, mx + 11, my + 11 );
    p.end();
    return b;
}

// Draw bitmap for default cursor. Width is size of pen to draw with.
QBitmap defaultCursorBitmap(int width) {
    QBitmap b(32, 32);
    b.fill(Qt::color0);
    int mx = 16, my = 16;
    // Begin drawing
    QPainter p;
    p.begin( &b );
    p.setPen( QPen( Qt::color1, width ) );
    // 1. diagonal
    p.drawLine (mx - 2, my - 2, mx - 8, mx - 8);
    p.drawLine (mx + 2, my + 2, mx + 8, mx + 8);
    // 2. diagonal
    p.drawLine (mx - 2, my + 2, mx - 8, mx + 8);
    p.drawLine (mx + 2, my - 2, mx + 8, mx - 8);
    p.end();
    return b;
}
}

SkyMapLite *SkyMapLite::pinstance = 0;

int SkyMapLite::starColorMode = 0;

SkyMapLite::SkyMapLite(QQuickItem* parent)
    :QQuickItem(parent), m_proj(0), count(0), data(KStarsData::Instance()),
      nStarSizes(15), nSPclasses(7), m_planetsItem(new PlanetsItem(this)),
      m_asteroidsItem(new AsteroidsItem(this)), m_cometsItem(new CometsItem(this))
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemHasContents, true);

    // Whenever the wrapper's(parent) dimensions changed, change SkyMapLite too
    connect(parent, &QQuickItem::widthChanged, this, &SkyMapLite::resizeItem);
    connect(parent, &QQuickItem::heightChanged, this, &SkyMapLite::resizeItem);

    //Initialize images for stars
    initStarImages();
    // Set pinstance to yourself
    pinstance = this;
    /*textureCache = QVector<QVector<QSGTexture*>> (imageCache.length());

    for(int i = 0; i < textureCache.length(); ++i) {
        int length = imageCache[i].length();
        textureCache[i] = QVector<QSGTexture *>(length);
        for(int c = 1; c < length; ++c) {
           textureCache[i][c] = window()->createTextureFromImage(imageCache[i][c]->toImage());
        }
    }*/
}

QSGNode* SkyMapLite::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {
    /*if(!textureCache.length()) {
        textureCache = QVector<QVector<QSGTexture*>> (imageCache.length());

        for(int i = 0; i < textureCache.length(); ++i) {
            int length = imageCache[i].length();
            textureCache[i] = QVector<QSGTexture *>(length);
            for(int c = 1; c < length; ++c) {
                //textureCache[i][c] = window()->createTextureFromImage(imageCache[i][c]->toImage());
            }
        }
    }*/
    return oldNode;
}

QSGTexture* SkyMapLite::getCachedTexture(int size, char spType) {
    return textureCache[harvardToIndex(spType)][size];
}

SkyMapLite* SkyMapLite::createInstance(QQuickItem* parent) {
    delete pinstance;
    pinstance = new SkyMapLite(parent);
    return pinstance;
}

SkyMapLite::~SkyMapLite() {
    // Delete image cache
    foreach(QVector<QPixmap*> imgCache, imageCache) {
        foreach(QPixmap* img, imgCache) delete img;
    }
    // Delete textures generated from image cache
    foreach(QVector<QSGTexture*> tCache, textureCache) {
        foreach(QSGTexture* t, tCache) delete t;
    }
}

void SkyMapLite::setFocus( SkyPoint *p ) {
    setFocus( p->ra(), p->dec() );
}

void SkyMapLite::setFocus( const dms &ra, const dms &dec ) {
    Options::setFocusRA(  ra.Hours() );
    Options::setFocusDec( dec.Degrees() );

    focus()->set( ra, dec );
    focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void SkyMapLite::setFocusAltAz( const dms &alt, const dms &az) {
    Options::setFocusRA( focus()->ra().Hours() );
    Options::setFocusDec( focus()->dec().Degrees() );
    focus()->setAlt(alt);
    focus()->setAz(az);
    focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );

    slewing = false;
    forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMapLite::setDestination( const SkyPoint& p ) {
    setDestination( p.ra(), p.dec() );
}

void SkyMapLite::setDestination( const dms &ra, const dms &dec ) {
    destination()->set( ra, dec );
    destination()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    emit destinationChanged();
}

void SkyMapLite::setDestinationAltAz( const dms &alt, const dms &az) {
    destination()->setAlt(alt);
    destination()->setAz(az);
    destination()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
    emit destinationChanged();
}

void SkyMapLite::setClickedPoint( SkyPoint *f ) {
    ClickedPoint = *f;
}

void SkyMapLite::setClickedObject( SkyObject *o ) {
    ClickedObject = o;
}

void SkyMapLite::setFocusObject( SkyObject *o ) {
    FocusObject = o;
    if ( FocusObject )
        Options::setFocusObject( FocusObject->name() );
    else
        Options::setFocusObject( i18n( "nothing" ) );
}

void SkyMapLite::slotClockSlewing() {
    //If the current timescale exceeds slewTimeScale, set clockSlewing=true, and stop the clock.
    if( (fabs( data->clock()->scale() ) > Options::slewTimeScale())  ^  clockSlewing ) {
        data->clock()->setManualMode( !clockSlewing );
        clockSlewing = !clockSlewing;
        // don't change automatically the DST status
        KStarsLite* kstars = KStarsLite::Instance();
        if( kstars )
            kstars->updateTime( false );
    }
}

/*void SkyMapLite::updateFocus() {
    if( slewing )
        return;

    //Tracking on an object
    if ( Options::isTracking() && focusObject() != NULL ) {
        if ( Options::useAltAz() ) {
            //Tracking any object in Alt/Az mode requires focus updates
            focusObject()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            setFocusAltAz( focusObject()->altRefracted(), focusObject()->az() );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
            setDestination( *focus() );
        } else {
            //Tracking in equatorial coords
            setFocus( focusObject() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            setDestination( *focus() );
        }

    //Tracking on empty sky
    } else if ( Options::isTracking() && focusPoint() != NULL ) {
        if ( Options::useAltAz() ) {
            //Tracking on empty sky in Alt/Az mode
            setFocus( focusPoint() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            setDestination( *focus() );
        }

    // Not tracking and not slewing, let sky drift by
    // This means that horizontal coordinates are constant.
    } else {
        focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat() );
    }
}*/

void SkyMapLite::addPlanetItem(SolarSystemSingleComponent* parentComp) {
    m_planetsItem->addPlanet(parentComp);
}

void SkyMapLite::resizeItem() {
    setWidth(parentItem()->width());
    setHeight(parentItem()->height());
    forceUpdate();
}

void SkyMapLite::slotZoomIn() {
    setZoomFactor( Options::zoomFactor() * DZOOM );
}

void SkyMapLite::slotZoomOut() {
    setZoomFactor( Options::zoomFactor() / DZOOM );
}

void SkyMapLite::slotZoomDefault() {
    setZoomFactor( DEFAULTZOOM );
}

void SkyMapLite::setZoomFactor(double factor) {
    Options::setZoomFactor(  KSUtils::clamp(factor, MINZOOM, MAXZOOM)  );
    forceUpdate();
    emit zoomChanged();
}

void SkyMapLite::forceUpdate() {
    setupProjector();
    m_planetsItem->update();
    m_asteroidsItem->update();
    m_cometsItem->update();
}

void SkyMapLite::setupProjector() {
    //Update View Parameters for projection
    ViewParams p;
    p.focus         = focus();
    p.height        = height();
    p.width         = width();
    p.useAltAz      = Options::useAltAz();
    p.useRefraction = Options::useRefraction();
    p.zoomFactor    = Options::zoomFactor();
    p.fillGround    = Options::showGround();
    Options::setProjection(Projector::Lambert);
    //Check if we need a new projector
    if( m_proj && Options::projection() == m_proj->type() )
        m_proj->setViewParams(p);
    else {
        delete m_proj;
        switch( Options::projection() ) {
            case Projector::Gnomonic:
                m_proj = new GnomonicProjector(p);
                break;
            case Projector::Stereographic:
                m_proj = new StereographicProjector(p);
                break;
            case Projector::Orthographic:
                m_proj = new OrthographicProjector(p);
                break;
            case Projector::AzimuthalEquidistant:
                m_proj = new AzimuthalEquidistantProjector(p);
                break;
            case Projector::Equirectangular:
                m_proj = new EquirectangularProjector(p);
                break;
            case Projector::Lambert: default:
                //TODO: implement other projection classes
                m_proj = new LambertProjector(p);
                break;
        }
    }
}

void SkyMapLite::setZoomMouseCursor()
{
    mouseMoveCursor = false;	// no mousemove cursor
    QBitmap cursor = zoomCursorBitmap(2);
    QBitmap mask   = zoomCursorBitmap(4);
    setCursor( QCursor(cursor, mask) );
}

void SkyMapLite::setDefaultMouseCursor()
{
    mouseMoveCursor = false;        // no mousemove cursor
    QBitmap cursor = defaultCursorBitmap(2);
    QBitmap mask   = defaultCursorBitmap(3);
    setCursor( QCursor(cursor, mask) );
}

void SkyMapLite::setMouseMoveCursor()
{
    if (mouseButtonDown)
    {
        setCursor(Qt::SizeAllCursor);	// cursor shape defined in qt
        mouseMoveCursor = true;
    }
}

bool SkyMapLite::isSlewing() const  {
    return (slewing || ( clockSlewing && data->clock()->isActive() ) );
}

int SkyMapLite::harvardToIndex(char c) {
    // Convert spectral class to numerical index.
    // If spectral class is invalid return index for white star (A class)

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

QVector<QVector<QPixmap*>> SkyMapLite::getImageCache()
{
    return imageCache;
}

void SkyMapLite::initStarImages()
{
    imageCache = QVector<QVector<QPixmap*>>(nSPclasses);

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
        //Add new spectral class

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
        //[nSPclasses][nStarSizes];
        // Cache array slice

        QVector<QPixmap *> pmap = imageCache[ harvardToIndex(color) ];
        pmap.append(new QPixmap());
        for( int size = 1; size < nStarSizes; size++ ) {
            //if( !pmap[size] ) {
            pmap.append(new QPixmap());
            *pmap[size] = BigImage.scaled( size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
            //}
        }
        imageCache[ harvardToIndex(color) ] = pmap;
    }
    starColorMode = Options::starColorMode();
}
