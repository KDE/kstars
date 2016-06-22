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

#include "skylabeler.h"
#include "Options.h"
#include "skymesh.h"

#include "kstarslite/skyitems/rootnode.h"
#include "kstarslite/skyitems/skynodes/skynode.h"

#include "ksplanetbase.h"
#include "ksutils.h"

#include <QSGSimpleRectNode>
//#include <QSGNode>
#include <QBitmap>
#include <QSGTexture>
#include <QQuickWindow>
#include <QLinkedList>

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
      nStarSizes(15), nSPclasses(7), pinch(false), m_loadingFinished(false), m_sizeMagLim(10.0)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemHasContents, true);

    midMouseButtonDown = false;
    mouseButtonDown = false;
    slewing = false;
    clockSlewing = false;

    ClickedObject = NULL;
    FocusObject = NULL;

    setupProjector();

    // Whenever the wrapper's(parent) dimensions changed, change SkyMapLite too
    connect(parent, &QQuickItem::widthChanged, this, &SkyMapLite::resizeItem);
    connect(parent, &QQuickItem::heightChanged, this, &SkyMapLite::resizeItem);

    resizeItem(); /* Set initial size. Without it on Android SkyMapLite is not displayed until screen
    orientation is not changed */

    //Initialize images for stars
    initStarImages();
    // Set pinstance to yourself
    pinstance = this;

}

QSGNode* SkyMapLite::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {
    Q_UNUSED(updatePaintNodeData);
    RootNode *n = static_cast<RootNode*>(oldNode);

    qDeleteAll(m_deleteNodes);
    m_deleteNodes.clear();


    if(m_loadingFinished) {
        if(!n) {
            n = new RootNode();
        }
        n->update();
    }

    //Debug
    /*if(m_loadingFinished) {
        if(!n) {
            n = new RootNode();
        }
        n->testLeakAdd();
        n->update();
        m_loadingFinished = false;
    } else {
        if (n) {
            n->testLeakDelete();
        }
        m_loadingFinished = true;
    }*/

    return n;
}

void SkyMapLite::deleteSkyNode(SkyNode *skyNode) {
    m_deleteNodes.append(skyNode);
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

void SkyMapLite::slewFocus() {
    //Don't slew if the mouse button is pressed
    //Also, no animated slews if the Manual Clock is active
    //08/2002: added possibility for one-time skipping of slew with snapNextFocus
    if ( !mouseButtonDown ) {
        bool goSlew =  ( Options::useAnimatedSlewing() && ! data->snapNextFocus() ) &&
                !( data->clock()->isManualMode() && data->clock()->isActive() );
        if ( goSlew  ) {
            double dX, dY;
            double maxstep = 10.0;
            if ( Options::useAltAz() ) {
                dX = destination()->az().Degrees() - focus()->az().Degrees();
                dY = destination()->alt().Degrees() - focus()->alt().Degrees();
            } else {
                dX = destination()->ra().Degrees() - focus()->ra().Degrees();
                dY = destination()->dec().Degrees() - focus()->dec().Degrees();
            }

            //switch directions to go the short way around the celestial sphere, if necessary.
            dX = KSUtils::reduceAngle(dX, -180.0, 180.0);

            double r0 = sqrt( dX*dX + dY*dY );
            if ( r0 < 20.0 ) { //smaller slews have smaller maxstep
                maxstep *= (10.0 + 0.5*r0)/20.0;
            }
            double step  = 0.5;
            double r  = r0;
            while ( r > step ) {
                //DEBUG
                //qDebug() << step << ": " << r << ": " << r0 << endl;
                double fX = dX / r;
                double fY = dY / r;

                if ( Options::useAltAz() ) {
                    focus()->setAlt( focus()->alt().Degrees() + fY*step );
                    focus()->setAz( dms( focus()->az().Degrees() + fX*step ).reduce() );
                    focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
                } else {
                    fX = fX/15.; //convert RA degrees to hours
                    SkyPoint newFocus( focus()->ra().Hours() + fX*step, focus()->dec().Degrees() + fY*step );
                    setFocus( &newFocus );
                    focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
                }

                slewing = true;

                forceUpdate();
                qApp->processEvents(); //keep up with other stuff

                if ( Options::useAltAz() ) {
                    dX = destination()->az().Degrees() - focus()->az().Degrees();
                    dY = destination()->alt().Degrees() - focus()->alt().Degrees();
                } else {
                    dX = destination()->ra().Degrees() - focus()->ra().Degrees();
                    dY = destination()->dec().Degrees() - focus()->dec().Degrees();
                }

                //switch directions to go the short way around the celestial sphere, if necessary.
                dX = KSUtils::reduceAngle(dX, -180.0, 180.0);
                r = sqrt( dX*dX + dY*dY );

                //Modify step according to a cosine-shaped profile
                //centered on the midpoint of the slew
                //NOTE: don't allow the full range from -PI/2 to PI/2
                //because the slew will never reach the destination as
                //the speed approaches zero at the end!
                double t = dms::PI*(r - 0.5*r0)/(1.05*r0);
                step = cos(t)*maxstep;
            }
        }

        //Either useAnimatedSlewing==false, or we have slewed, and are within one step of destination
        //set focus=destination.
        if ( Options::useAltAz() ) {
            setFocusAltAz( destination()->alt(), destination()->az() );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            setFocus( destination() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        slewing = false;

        //Turn off snapNextFocus, we only want it to happen once
        if ( data->snapNextFocus() ) {
            data->setSnapNextFocus(false);
        }

        //Start the HoverTimer. if the user leaves the mouse in place after a slew,
        //we want to attach a label to the nearest object.
        if ( Options::useHoverLabel() )
            m_HoverTimer.start( HOVER_INTERVAL );

        forceUpdate();
    }
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

    // We delay one draw cycle before re-indexing
    // we MUST ensure CLines do not get re-indexed while we use DRAW_BUF
    // so we do it here.
    //m_CLines->reindex( &m_reindexNum );
    // This queues re-indexing for the next draw cycle
    //m_reindexNum = KSNumbers( data->updateNum()->julianDay() );

    // This ensures that the JIT updates are synchronized for the entire draw
    // cycle so the sky moves as a single sheet.  May not be needed.
    data->syncUpdateIDs();

    SkyMesh *m_skyMesh = SkyMesh::Instance(3);
    if(m_skyMesh) {
        // prepare the aperture
        // FIXME_FOV: We may want to rejigger this to allow
        // wide-angle views --hdevalence
        double radius = m_proj->fov();
        if ( radius > 180.0 )
            radius = 180.0;


        if ( m_skyMesh->inDraw() ) {
            printf("Warning: aborting concurrent SkyMapComposite::draw()\n");
            return;
        }

        //m_skyMesh->inDraw( true );
        m_skyMesh->aperture( &Focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing

        // create the no-precess aperture if needed
        if ( Options::showEquatorialGrid() || Options::showHorizontalGrid() || Options::showCBounds() || Options::showEquator() ) {
            m_skyMesh->index( &Focus, radius + 1.0, NO_PRECESS_BUF );
        }
    }
    update();
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

QSGTexture *SkyMapLite::textToTexture(QString text, QColor color, bool zoomFont) {
    QFont f;

    if(zoomFont) {
        f = SkyLabeler::Instance()->drawFont();
    } else {
        f = SkyLabeler::Instance()->stdFont();
    }

    QFontMetrics fm(f);

    int width = fm.width(text);
    int height = fm.height();

    QImage label(width, height, QImage::Format_ARGB32_Premultiplied);
    label.fill(Qt::transparent);

    m_painter.begin(&label);

    m_painter.setFont(f);

    m_painter.setPen( color );
    m_painter.drawText(0,height - fm.descent(),text);

    m_painter.end();

    QSGTexture *texture = window()->createTextureFromImage(label,
                                                           QQuickWindow::TextureCanUseAtlas);

    /*texture->setFiltering(QSGTexture::Linear);
    texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    texture->setVerticalWrapMode(QSGTexture::ClampToEdge);*/

    return texture;
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

        QVector<QPixmap *> *pmap = &imageCache[ harvardToIndex(color) ];
        pmap->append(new QPixmap(BigImage));
        for( int size = 1; size < nStarSizes; size++ ) {
            pmap->append(new QPixmap(BigImage.scaled( size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation )));
        }

    }
    //}
    starColorMode = Options::starColorMode();
}
