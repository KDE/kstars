/***************************************************************************
                          skymapevents.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 10 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//This file contains Event handlers for the SkyMap class.

#include <QCursor>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPaintEvent>

#include <kstatusbar.h>
#include <kio/job.h>

#include "skymap.h"
#include "skyqpainter.h"
#include "skyglpainter.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "simclock.h"
#include "kspopupmenu.h"
#include "skyobjects/ksplanetbase.h"
#include "widgets/infoboxwidget.h"

#include "projections/projector.h"

#include "skycomponents/skymapcomposite.h"

// TODO: Remove if debug key binding is removed
#include "skycomponents/skylabeler.h"
#include "skycomponents/starcomponent.h"


void SkyMap::resizeEvent( QResizeEvent * )
{
    computeSkymap = true; // skymap must be new computed

    //FIXME: No equivalent for this line in Qt4 ??
    //	if ( testWState( Qt::WState_AutoMask ) ) updateMask();

    // TODO: Hopefully the child QWidget / QGLWidget will scale automatically...
    // No, it doesn't seem to:
    m_SkyMapDraw->resize( size() );

    // Resize infoboxes container.
    // FIXME: this is not really pretty. Maybe there are some better way to this???
    m_iboxes->resize( size() );
}

void SkyMap::keyPressEvent( QKeyEvent *e ) {
    KStars* kstars = KStars::Instance();
    QString s;
    bool arrowKeyPressed( false );
    bool shiftPressed( false );
    float step = 1.0;
    if ( e->modifiers() & Qt::ShiftModifier ) { step = 10.0; shiftPressed = true; }

    //If the DBus resume key is not empty, then DBus processing is
    //paused while we wait for a keypress
    if ( ! data->resumeKey.isEmpty() && QKeySequence(e->key()) == data->resumeKey ) {
        //The resumeKey was pressed.  Signal that it was pressed by
        //resetting it to empty; this will break the loop in
        //KStars::waitForKey()
        data->resumeKey = QKeySequence();
        return;
    }

    switch ( e->key() ) {
    case Qt::Key_Left :
        if ( Options::useAltAz() ) {
            focus()->setAz( dms( focus()->az().Degrees() - step * MINZOOM/Options::zoomFactor() ).reduce() );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            focus()->setRA( focus()->ra().Hours() + 0.05*step * MINZOOM/Options::zoomFactor() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Right :
        if ( Options::useAltAz() ) {
            focus()->setAz( dms( focus()->az().Degrees() + step * MINZOOM/Options::zoomFactor() ).reduce() );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            focus()->setRA( focus()->ra().Hours() - 0.05*step * MINZOOM/Options::zoomFactor() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Up :
        if ( Options::useAltAz() ) {
            focus()->setAlt( focus()->alt().Degrees() + step * MINZOOM/Options::zoomFactor() );
            if ( focus()->alt().Degrees() > 90.0 ) focus()->setAlt( 90.0 );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            focus()->setDec( focus()->dec().Degrees() + step * MINZOOM/Options::zoomFactor() );
            if (focus()->dec().Degrees() > 90.0) focus()->setDec( 90.0 );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Down:
        if ( Options::useAltAz() ) {
            focus()->setAlt( focus()->alt().Degrees() - step * MINZOOM/Options::zoomFactor() );
            if ( focus()->alt().Degrees() < -90.0 ) focus()->setAlt( -90.0 );
            focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat() );
        } else {
            focus()->setDec( focus()->dec().Degrees() - step * MINZOOM/Options::zoomFactor() );
            if (focus()->dec().Degrees() < -90.0) focus()->setDec( -90.0 );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Plus:   //Zoom in
    case Qt::Key_Equal:
        zoomInOrMagStep( e->modifiers() );
        break;

    case Qt::Key_Minus:  //Zoom out
    case Qt::Key_Underscore:
        zoomOutOrMagStep( e->modifiers() );
        break;

    case Qt::Key_0: //center on Sun
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::SUN ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_1: //center on Mercury
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::MERCURY ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_2: //center on Venus
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::VENUS ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_3: //center on Moon
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::MOON ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_4: //center on Mars
        setClickedObject( data->skyComposite()->planet( KSPlanetBase:: MARS) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_5: //center on Jupiter
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::JUPITER ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_6: //center on Saturn
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::SATURN ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_7: //center on Uranus
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::URANUS ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_8: //center on Neptune
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::NEPTUNE ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_9: //center on Pluto
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::PLUTO ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_BracketLeft:   // Begin measuring angular distance
        if( !angularDistanceMode )
            slotBeginAngularDistance();
        break;
    case Qt::Key_Escape:        // Cancel angular distance measurement
        if( angularDistanceMode )
            slotCancelAngularDistance();
        break;
    case Qt::Key_Comma:  //advance one step backward in time
    case Qt::Key_Less:
        if ( data->clock()->isActive() )
            data->clock()->stop();
        data->clock()->setClockScale( -1.0 * data->clock()->scale() ); //temporarily need negative time step
        data->clock()->manualTick( true );
        data->clock()->setClockScale( -1.0 * data->clock()->scale() ); //reset original sign of time step
        update();
        qApp->processEvents();
        break;

    case Qt::Key_Period: //advance one step forward in time
    case Qt::Key_Greater:
        if ( data->clock()->isActive() ) data->clock()->stop();
        data->clock()->manualTick( true );
        update();
        qApp->processEvents();
        break;

    case Qt::Key_C: //Center clicked object
        if ( clickedObject() ) slotCenter();
        break;

    case Qt::Key_D: //Details window for Clicked/Centered object
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) { 
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        if ( clickedObject() ) {
            slotDetail();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_P: //Show Popup menu for Clicked/Centered object
        if ( shiftPressed ) {
            if ( focusObject() ) 
                focusObject()->showPopupMenu( pmenu, QCursor::pos() );
        } else {
            if ( clickedObject() )
                clickedObject()->showPopupMenu( pmenu, QCursor::pos() );
        }
        break;

    case Qt::Key_O: //Add object to Observing List
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) {
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        if ( clickedObject() ) {
            kstars->observingList()->slotAddObject();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_L: //Toggle User label on Clicked/Centered object
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) {
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        if ( clickedObject() ) {
            if ( isObjectLabeled( clickedObject() ) )
                slotRemoveObjectLabel();
            else
                slotAddObjectLabel();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_T: //Toggle planet trail on Clicked/Centered object (if solsys)
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) {
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        KSPlanetBase* planet = dynamic_cast<KSPlanetBase*>( clickedObject() );
        if( planet ) {
            if( planet->hasTrail() )
                slotRemovePlanetTrail();
            else
                slotAddPlanetTrail();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_R:
        {
            // Toggle relativistic corrections
            Options::setUseRelativistic( ! Options::useRelativistic() );
            kDebug() << "Relativistc corrections: " << Options::useRelativistic();
            forceUpdate();
            break;
        }
    case Qt::Key_A:
        Options::setUseAntialias( ! Options::useAntialias() );
        kDebug() << "Use Antialiasing: " << Options::useAntialias();
        forceUpdate();
        break;
    default:
        // We don't want to do anything in this case. Key is unknown
        return;
    }

    if ( arrowKeyPressed ) {
        stopTracking();
        if ( scrollCount > 10 ) {
            setDestination( focus() );
            scrollCount = 0;
        }
    }

    forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

//DEBUG_KIO_JOB
void SkyMap::slotJobResult( KJob *job ) {
    KIO::StoredTransferJob *stjob = (KIO::StoredTransferJob*)job;

    QPixmap pm;
    pm.loadFromData( stjob->data() );

    //DEBUG
    kDebug() << "Pixmap: " << pm.width() << "x" << pm.height();

}

void SkyMap::stopTracking() {
    KStars* kstars = KStars::Instance();
    emit positionChanged( focus() );
    if( kstars && Options::isTracking() )
        kstars->slotTrack();
}

void SkyMap::keyReleaseEvent( QKeyEvent *e ) {
    switch ( e->key() ) {
    case Qt::Key_Plus:   //Zoom in
    case Qt::Key_Equal:
    case Qt::Key_Minus:  //Zoom out
    case Qt::Key_Underscore:

    case Qt::Key_Left :  //no break; continue to Qt::Key_Down
    case Qt::Key_Right :  //no break; continue to Qt::Key_Down
    case Qt::Key_Up :  //no break; continue to Qt::Key_Down
    case Qt::Key_Down :
        slewing = false;
        scrollCount = 0;

        if ( Options::useAltAz() )
            setDestinationAltAz( focus()->alt(), focus()->az() );
        else
            setDestination( focus() );

        showFocusCoords();
        forceUpdate();  // Need a full update to draw faint objects that are not drawn while slewing.
        break;
    }
}

void SkyMap::mouseMoveEvent( QMouseEvent *e ) {
    if ( Options::useHoverLabel() ) {
        //First of all, if the transientObject() pointer is not NULL, then
        //we just moved off of a hovered object.  Begin fading the label.
        if ( transientObject() && ! TransientTimer.isActive() ) {
            fadeTransientLabel();
        }

        //Start a single-shot timer to monitor whether we are currently hovering.
        //The idea is that whenever a moveEvent occurs, the timer is reset.  It
        //will only timeout if there are no move events for HOVER_INTERVAL ms
        HoverTimer.start( HOVER_INTERVAL );
    }

    //Are we defining a ZoomRect?
    if ( ZoomRect.center().x() > 0 && ZoomRect.center().y() > 0 ) {
        //cancel operation if the user let go of CTRL
        if ( !( e->modifiers() & Qt::ControlModifier ) ) {
            ZoomRect = QRect(); //invalidate ZoomRect
            update();
        } else {
            //Resize the rectangle so that it passes through the cursor position
            QPoint pcenter = ZoomRect.center();
            int dx = abs(e->x() - pcenter.x());
            int dy = abs(e->y() - pcenter.y());
            if ( dx == 0 || float(dy)/float(dx) > float(height())/float(width()) ) {
                //Size rect by height
                ZoomRect.setHeight( 2*dy );
                ZoomRect.setWidth( 2*dy*width()/height() );
            } else {
                //Size rect by height
                ZoomRect.setWidth( 2*dx );
                ZoomRect.setHeight( 2*dx*height()/width() );
            }
            ZoomRect.moveCenter( pcenter ); //reset center

            update();
            return;
        }
    }

    if ( projector()->unusablePoint( e->pos() ) ) return;  // break if point is unusable

    //determine RA, Dec of mouse pointer
    setMousePoint( projector()->fromScreen( e->pos(), data->lst(), data->geo()->lat() ) );
    mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

    double dyPix = 0.5*height() - e->y();
    if ( midMouseButtonDown ) { //zoom according to y-offset
        float yoff = dyPix - y0;
        if (yoff > 10 ) {
            y0 = dyPix;
            slotZoomIn();
        }
        if (yoff < -10 ) {
            y0 = dyPix;
            slotZoomOut();
        }
    }

    if ( mouseButtonDown ) {
        // set the mouseMoveCursor and set slewing=true, if they are not set yet
        if( !mouseMoveCursor )
            setMouseMoveCursor();
        if( !slewing ) {
            slewing = true;
            stopTracking(); //toggle tracking off
        }

        //Update focus such that the sky coords at mouse cursor remain approximately constant
        if ( Options::useAltAz() ) {
            mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            clickedPoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            dms dAz  = mousePoint()->az()  - clickedPoint()->az();
            dms dAlt = mousePoint()->alt() - clickedPoint()->alt();
            focus()->setAz( focus()->az().Degrees() - dAz.Degrees() ); //move focus in opposite direction
            focus()->setAz( focus()->az().reduce() );
            focus()->setAlt(
                KSUtils::clamp( focus()->alt().Degrees() - dAlt.Degrees() , -90.0 , 90.0 ) );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            dms dRA  = mousePoint()->ra()  - clickedPoint()->ra();
            dms dDec = mousePoint()->dec() - clickedPoint()->dec();
            focus()->setRA( focus()->ra().Hours() - dRA.Hours() ); //move focus in opposite direction
            focus()->setRA( focus()->ra().reduce() );
            focus()->setDec(
                KSUtils::clamp( focus()->dec().Degrees() - dDec.Degrees() , -90.0 , 90.0 ) );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        ++scrollCount;
        if ( scrollCount > 4 ) {
            showFocusCoords();
            scrollCount = 0;
        }

        //redetermine RA, Dec of mouse pointer, using new focus
        setMousePoint( projector()->fromScreen( e->pos(), data->lst(), data->geo()->lat() ) );
        mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        setClickedPoint( mousePoint() );

        forceUpdate();  // must be new computed

    } else { //mouse button not down
        emit mousePointChanged( mousePoint() );
    }
}

void SkyMap::wheelEvent( QWheelEvent *e ) {
    if ( e->delta() > 0 ) 
        zoomInOrMagStep ( e->modifiers() );
    else
        zoomOutOrMagStep( e->modifiers() );
}

void SkyMap::mouseReleaseEvent( QMouseEvent * ) {
    if ( ZoomRect.isValid() ) {
        //Zoom in on center of Zoom Circle, by a factor equal to the ratio
        //of the sky pixmap's width to the Zoom Circle's diameter
        float factor = float(width()) / float(ZoomRect.width());

        stopTracking();

        SkyPoint newcenter = projector()->fromScreen( ZoomRect.center(), data->lst(), data->geo()->lat() );

        setFocus( &newcenter );
        setDestination( &newcenter );
        setDefaultMouseCursor();

        setZoomFactor( Options::zoomFactor() * factor );

        ZoomRect = QRect(); //invalidate ZoomRect
    } else {
        setDefaultMouseCursor();
        ZoomRect = QRect(); //just in case user Ctrl+clicked + released w/o dragging...
    }

    if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
    if (mouseButtonDown) { //false if double-clicked, because it's unset there.
        mouseButtonDown = false;
        if ( slewing ) {
            slewing = false;

            if ( Options::useAltAz() )
                setDestinationAltAz( focus()->alt(), focus()->az() );
            else
                setDestination( focus() );
        }
        forceUpdate();	// is needed because after moving the sky not all stars are shown
    }
    if ( midMouseButtonDown ) midMouseButtonDown = false;  // if middle button was pressed unset here

    scrollCount = 0;
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
    KStars* kstars = KStars::Instance();

    if ( ( e->modifiers() & Qt::ControlModifier ) && (e->button() == Qt::LeftButton) ) {
        ZoomRect.moveCenter( e->pos() );
        setZoomMouseCursor();
        update(); //refresh without redrawing skymap
        return;
    }

    // if button is down and cursor is not moved set the move cursor after 500 ms
    QTimer::singleShot(500, this, SLOT (setMouseMoveCursor()));

    // break if point is unusable
    if ( projector()->unusablePoint( e->pos() ) )
        return;

    if ( !midMouseButtonDown && e->button() == Qt::MidButton ) {
        y0 = 0.5*height() - e->y();  //record y pixel coordinate for middle-button zooming
        midMouseButtonDown = true;
    }

    if ( !mouseButtonDown ) {
        if ( e->button() == Qt::LeftButton ) {
            mouseButtonDown = true;
            scrollCount = 0;
        }

        //determine RA, Dec of mouse pointer
        setMousePoint( projector()->fromScreen( e->pos(), data->lst(), data->geo()->lat() ) );
        mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        setClickedPoint( mousePoint() );
        clickedPoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        //Find object nearest to clickedPoint()
        double maxrad = 1000.0/Options::zoomFactor();
        SkyObject* obj = data->skyComposite()->objectNearest( clickedPoint(), maxrad );
        setClickedObject( obj );
        if( obj )
            setClickedPoint(  obj );

        switch( e->button() ) {
        case Qt::LeftButton:
            if( kstars ) {
                QString name;
                if( clickedObject() )
                    name = clickedObject()->translatedLongName();
                else
                    name = i18n( "Empty sky" );
                kstars->statusBar()->changeItem(name, 0 );
            }
            break;
        case Qt::RightButton:
            if( angularDistanceMode ) {
                // Compute angular distance.
                slotEndAngularDistance();
            } else {
                // Show popup menu
                if( clickedObject() ) {
                    clickedObject()->showPopupMenu( pmenu, QCursor::pos() );
                } else {
                    pmenu->createEmptyMenu( clickedPoint() );
                    pmenu->popup( QCursor::pos() );
                }
            }
            break;
        default: ;
        }
    }
}

void SkyMap::mouseDoubleClickEvent( QMouseEvent *e ) {
    if ( e->button() == Qt::LeftButton && !projector()->unusablePoint( e->pos() ) ) {
        mouseButtonDown = false;
        if( e->x() != width()/2 || e->y() != height()/2 )
            slotCenter();
    }
}

/*
void SkyMap::initializeGL()
{
}

void SkyMap::resizeGL(int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    //do nothing since we resize in SkyGLPainter::paintGL()
}

void SkyMap::paintEvent( QPaintEvent *event )
{
    QPainter p;
    p.begin(this);
    p.beginNativePainting();

    setupProjector();
    if(m_framecount == 25) {
        float sec = m_fpstime.elapsed()/1000.;
        printf("FPS: %.2f\n", m_framecount/sec);
        m_framecount = 0;
        m_fpstime.restart();
    }
    SkyGLPainter psky(this);
    //FIXME: we may want to move this into the components.
    psky.begin();

    //Draw all sky elements
    psky.drawSkyBackground();
    data->skyComposite()->draw( &psky );
    //Finish up
    psky.end();
    
    p.endNativePainting();
    drawOverlays(p);
    p.end();

    ++m_framecount;
}
*/

void SkyMap::paintEvent( QPaintEvent *e ) {
    // Do nothing for now.
    kDebug() << "Was here";
}

double SkyMap::zoomFactor( const int modifier ) {
    double factor = ( modifier & Qt::ControlModifier) ? DZOOM : 2.0; 
    if ( modifier & Qt::ShiftModifier ) 
        factor = sqrt( factor );
    return factor;
}

void SkyMap::zoomInOrMagStep( const int modifier ) {
    if ( modifier & Qt::AltModifier )
        incMagLimit( modifier );
    else
        setZoomFactor( Options::zoomFactor() * zoomFactor( modifier ) );
}

    
void SkyMap::zoomOutOrMagStep( const int modifier ) {
    if ( modifier & Qt::AltModifier )
        decMagLimit( modifier );
    else
        setZoomFactor( Options::zoomFactor() / zoomFactor (modifier ) );
}

double SkyMap::magFactor( const int modifier ) {
    double factor = ( modifier & Qt::ControlModifier) ? 0.2 : 1.0; 
    if ( modifier & Qt::ShiftModifier ) 
        factor /= 2.0;
    return factor;
}

void SkyMap::incMagLimit( const int modifier ) {
    double limit = 2.222 * log10(static_cast<double>( Options::starDensity() )) + 0.35;
    limit += magFactor( modifier );
    if ( limit > 7.94 ) limit = 7.94;
    Options::setStarDensity( pow( 10, ( limit - 0.35 ) / 2.222) );
    //printf("maglim set to %3.1f\n", limit);
    forceUpdate();
}

void SkyMap::decMagLimit( const int modifier ) {
    double limit = 2.222 * log10(static_cast<double>( Options::starDensity() )) + 0.35;
    limit -= magFactor( modifier );
    if ( limit < 3.55 ) limit = 3.55;
    Options::setStarDensity( pow( 10, ( limit - 0.35 ) / 2.222) );
    //printf("maglim set to %3.1f\n", limit);
    forceUpdate();
}

