/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

//This file contains Event handlers for the SkyMap class.

#include "skymap.h"

#include "ksplanetbase.h"
#include "kspopupmenu.h"
#include "kstars.h"
#include "observinglist.h"
#include "Options.h"
#include "skyglpainter.h"
#include "skyqpainter.h"
#include "printing/simplefovexporter.h"
#include "skycomponents/skylabeler.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/starcomponent.h"
#include "widgets/infoboxwidget.h"

#include <QGestureEvent>
#include <QStatusBar>
#include <QToolTip>

void SkyMap::resizeEvent(QResizeEvent *)
{
    computeSkymap = true; // skymap must be new computed

    //FIXME: No equivalent for this line in Qt4 ??
    //	if ( testWState( Qt::WState_AutoMask ) ) updateMask();

    // Resize the widget that draws the sky map.
    // FIXME: The resize event doesn't pass to children. Any better way of doing this?
    m_SkyMapDraw->resize(size());

    // Resize infoboxes container.
    // FIXME: this is not really pretty. Maybe there are some better way to this???
    m_iboxes->resize(size());
}

void SkyMap::keyPressEvent(QKeyEvent *e)
{
    bool arrowKeyPressed(false);
    bool shiftPressed(false);
    float step = 1.0;
    if (e->modifiers() & Qt::ShiftModifier)
    {
        step         = 10.0;
        shiftPressed = true;
    }

    //If the DBus resume key is not empty, then DBus processing is
    //paused while we wait for a keypress
    if (!data->resumeKey.isEmpty() && QKeySequence(e->key()) == data->resumeKey)
    {
        //The resumeKey was pressed.  Signal that it was pressed by
        //resetting it to empty; this will break the loop in
        //KStars::waitForKey()
        data->resumeKey = QKeySequence();
        return;
    }

    if (m_previewLegend)
    {
        slotCancelLegendPreviewMode();
    }

    switch (e->key())
    {
        case Qt::Key_Left:
            if (Options::useAltAz())
            {
                focus()->setAz(dms(focus()->az().Degrees() - step * MINZOOM / Options::zoomFactor()).reduce());
                focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
            }
            else
            {
                focus()->setRA(focus()->ra().Hours() + 0.05 * step * MINZOOM / Options::zoomFactor());
                focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            }

            arrowKeyPressed = true;
            slewing         = true;
            break;

        case Qt::Key_Right:
            if (Options::useAltAz())
            {
                focus()->setAz(dms(focus()->az().Degrees() + step * MINZOOM / Options::zoomFactor()).reduce());
                focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
            }
            else
            {
                focus()->setRA(focus()->ra().Hours() - 0.05 * step * MINZOOM / Options::zoomFactor());
                focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            }

            arrowKeyPressed = true;
            slewing         = true;
            break;

        case Qt::Key_Up:
            if (Options::useAltAz())
            {
                focus()->setAltRefracted(focus()->altRefracted().Degrees() + step * MINZOOM / Options::zoomFactor());
                if (focus()->alt().Degrees() > 90.0)
                    focus()->setAlt(90.0);
                focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
            }
            else
            {
                focus()->setDec(focus()->dec().Degrees() + step * MINZOOM / Options::zoomFactor());
                if (focus()->dec().Degrees() > 90.0)
                    focus()->setDec(90.0);
                focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            }

            arrowKeyPressed = true;
            slewing         = true;
            break;

        case Qt::Key_Down:
            if (Options::useAltAz())
            {
                focus()->setAltRefracted(focus()->altRefracted().Degrees() - step * MINZOOM / Options::zoomFactor());
                if (focus()->alt().Degrees() < -90.0)
                    focus()->setAlt(-90.0);
                focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
            }
            else
            {
                focus()->setDec(focus()->dec().Degrees() - step * MINZOOM / Options::zoomFactor());
                if (focus()->dec().Degrees() < -90.0)
                    focus()->setDec(-90.0);
                focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            }

            arrowKeyPressed = true;
            slewing         = true;
            break;

        case Qt::Key_Plus: //Zoom in
        case Qt::Key_Equal:
            zoomInOrMagStep(e->modifiers());
            break;

        case Qt::Key_Minus: //Zoom out
        case Qt::Key_Underscore:
            zoomOutOrMagStep(e->modifiers());
            break;

        case Qt::Key_0: //center on Sun
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::SUN));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_1: //center on Mercury
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::MERCURY));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_2: //center on Venus
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::VENUS));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_3: //center on Moon
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::MOON));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_4: //center on Mars
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::MARS));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_5: //center on Jupiter
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::JUPITER));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_6: //center on Saturn
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::SATURN));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_7: //center on Uranus
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::URANUS));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        case Qt::Key_8: //center on Neptune
            setClickedObject(data->skyComposite()->planet(KSPlanetBase::NEPTUNE));
            setClickedPoint(clickedObject());
            slotCenter();
            break;

        /*case Qt::Key_9: //center on Pluto
            setClickedObject( data->skyComposite()->planet( KSPlanetBase::PLUTO ) );
            setClickedPoint( clickedObject() );
            slotCenter();
            break;*/

        case Qt::Key_BracketLeft: // Begin measuring angular distance
            if (!rulerMode)
                slotBeginAngularDistance();
            break;
        case Qt::Key_Escape: // Cancel angular distance measurement
        {
            if (rulerMode)
                slotCancelRulerMode();

            if (m_fovCaptureMode)
                slotFinishFovCaptureMode();
            break;
        }

        case Qt::Key_C: //Center clicked object
            if (clickedObject())
                slotCenter();
            break;

        case Qt::Key_D: //Details window for Clicked/Centered object
        {
            SkyObject *orig = nullptr;
            if (shiftPressed)
            {
                orig = clickedObject();
                setClickedObject(focusObject());
            }

            if (clickedObject())
            {
                slotDetail();
            }

            if (orig)
            {
                setClickedObject(orig);
            }
            break;
        }

        case Qt::Key_P: //Show Popup menu for Clicked/Centered object
            if (shiftPressed)
            {
                if (focusObject())
                    focusObject()->showPopupMenu(pmenu, QCursor::pos());
            }
            else
            {
                if (clickedObject())
                    clickedObject()->showPopupMenu(pmenu, QCursor::pos());
            }
            break;

        case Qt::Key_O: //Add object to Observing List
        {
            SkyObject *orig = nullptr;
            if (shiftPressed)
            {
                orig = clickedObject();
                setClickedObject(focusObject());
            }

            if (clickedObject())
            {
                data->observingList()->slotAddObject();
            }

            if (orig)
            {
                setClickedObject(orig);
            }
            break;
        }

        case Qt::Key_L: //Toggle User label on Clicked/Centered object
        {
            SkyObject *orig = nullptr;
            if (shiftPressed)
            {
                orig = clickedObject();
                setClickedObject(focusObject());
            }

            if (clickedObject())
            {
                if (isObjectLabeled(clickedObject()))
                    slotRemoveObjectLabel();
                else
                    slotAddObjectLabel();
            }

            if (orig)
            {
                setClickedObject(orig);
            }
            break;
        }

        case Qt::Key_T: //Toggle planet trail on Clicked/Centered object (if solsys)
        {
            SkyObject *orig = nullptr;
            if (shiftPressed)
            {
                orig = clickedObject();
                setClickedObject(focusObject());
            }

            KSPlanetBase *planet = dynamic_cast<KSPlanetBase *>(clickedObject());
            if (planet)
            {
                if (planet->hasTrail())
                    slotRemovePlanetTrail();
                else
                    slotAddPlanetTrail();
            }

            if (orig)
            {
                setClickedObject(orig);
            }
            break;
        }

        case Qt::Key_R:
        {
            // Toggle relativistic corrections
            Options::setUseRelativistic(!Options::useRelativistic());
            qDebug() << "Relativistic corrections: " << Options::useRelativistic();
            forceUpdate();
            break;
        }

        case Qt::Key_A:
            Options::setUseAntialias(!Options::useAntialias());
            qDebug() << "Use Antialiasing: " << Options::useAntialias();
            forceUpdate();
            break;

        case Qt::Key_K:
        {
            if (m_fovCaptureMode)
                slotCaptureFov();
            break;
        }

        case Qt::Key_PageUp:
        {
            KStars::Instance()->selectPreviousFov();
            break;
        }

        case Qt::Key_PageDown:
        {
            KStars::Instance()->selectNextFov();
            break;
        }

        default:
            // We don't want to do anything in this case. Key is unknown
            return;
    }

    if (arrowKeyPressed)
    {
        stopTracking();
        setDestination(*focus());
    }

    forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::stopTracking()
{
    KStars *kstars = KStars::Instance();

    emit positionChanged(focus());
    if (kstars && Options::isTracking())
        kstars->slotTrack();
}

bool SkyMap::event(QEvent *event)
{
#if !defined(KSTARS_LITE)
    if (event->type() == QEvent::TouchBegin)
    {
        m_touchMode = true;
        m_pinchScale = -1;
    }

    if (event->type() == QEvent::Gesture && m_touchMode)
    {
        QGestureEvent* gestureEvent = static_cast<QGestureEvent*>(event);

        if (QPinchGesture *pinch = static_cast<QPinchGesture*>(gestureEvent->gesture(Qt::PinchGesture)))
        {
            QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();

            m_pinchMode = true;
            if (changeFlags & QPinchGesture::ScaleFactorChanged)
            {
                if (m_pinchScale == -1)
                {
                    m_pinchScale = pinch->totalScaleFactor();
                    return true;
                }
                if (pinch->totalScaleFactor() - m_pinchScale > 0.1)
                {
                    m_pinchScale = pinch->totalScaleFactor();
                    zoomInOrMagStep(0);
                    return true;
                }
                if (pinch->totalScaleFactor() - m_pinchScale < -0.1)
                {
                    m_pinchScale = pinch->totalScaleFactor();
                    zoomOutOrMagStep(0);
                    return true;
                }
            }
        }
        if (QTapAndHoldGesture *tapAndHold = static_cast<QTapAndHoldGesture*>(gestureEvent->gesture(Qt::TapAndHoldGesture)))
        {
            m_tapAndHoldMode = true;
            if (tapAndHold->state() == Qt::GestureFinished)
            {
                if (clickedObject())
                {
                    clickedObject()->showPopupMenu(pmenu, tapAndHold->position().toPoint());
                }
                else
                {
                    pmenu->createEmptyMenu(clickedPoint());
                    pmenu->popup(tapAndHold->position().toPoint());
                }
                m_touchMode = false;
                m_pinchMode = false;
                m_tapAndHoldMode = false;
            }
        }
        return true;
    }
#endif
    return QGraphicsView::event(event);
}

void SkyMap::keyReleaseEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        case Qt::Key_Plus: //Zoom in
        case Qt::Key_Equal:
        case Qt::Key_Minus: //Zoom out
        case Qt::Key_Underscore:

        case Qt::Key_Left:  //no break; continue to Qt::Key_Down
        case Qt::Key_Right: //no break; continue to Qt::Key_Down
        case Qt::Key_Up:    //no break; continue to Qt::Key_Down
        case Qt::Key_Down:
            slewing = false;

            if (Options::useAltAz())
                setDestinationAltAz(focus()->alt(), focus()->az(), false);
            else
                setDestination(*focus());

            showFocusCoords();
            forceUpdate(); // Need a full update to draw faint objects that are not drawn while slewing.
            break;
    }
}

void SkyMap::mouseMoveEvent(QMouseEvent *e)
{
#if !defined(KSTARS_LITE)
    // Skip touch points
    if (m_pinchMode || m_tapAndHoldMode || (m_touchMode && e->globalX() == 0 && e->globalY() == 0))
        return;
#endif

    if (Options::useHoverLabel())
    {
        //Start a single-shot timer to monitor whether we are currently hovering.
        //The idea is that whenever a moveEvent occurs, the timer is reset.  It
        //will only timeout if there are no move events for HOVER_INTERVAL ms
        m_HoverTimer.start(HOVER_INTERVAL);
        QToolTip::hideText();
    }

    //Are we defining a ZoomRect?
    if (ZoomRect.center().x() > 0 && ZoomRect.center().y() > 0)
    {
        //cancel operation if the user let go of CTRL
        if (!(e->modifiers() & Qt::ControlModifier))
        {
            ZoomRect = QRect(); //invalidate ZoomRect
            update();
        }
        else
        {
            //Resize the rectangle so that it passes through the cursor position
            QPoint pcenter = ZoomRect.center();
            int dx         = abs(e->x() - pcenter.x());
            int dy         = abs(e->y() - pcenter.y());
            if (dx == 0 || float(dy) / float(dx) > float(height()) / float(width()))
            {
                //Size rect by height
                ZoomRect.setHeight(2 * dy);
                ZoomRect.setWidth(2 * dy * width() / height());
            }
            else
            {
                //Size rect by height
                ZoomRect.setWidth(2 * dx);
                ZoomRect.setHeight(2 * dx * height() / width());
            }
            ZoomRect.moveCenter(pcenter); //reset center

            update();
            return;
        }
    }

    if (projector()->unusablePoint(e->pos()))
        return; // break if point is unusable

    //determine RA, Dec of mouse pointer
    m_MousePoint = projector()->fromScreen(e->pos(), data->lst(), data->geo()->lat());

    double dyPix = 0.5 * height() - e->y();
    if (midMouseButtonDown) //zoom according to y-offset
    {
        float yoff = dyPix - y0;
        if (yoff > 10)
        {
            y0 = dyPix;
            slotZoomIn();
        }
        if (yoff < -10)
        {
            y0 = dyPix;
            slotZoomOut();
        }
    }

    if (mouseButtonDown)
    {
        // set the mouseMoveCursor and set slewing=true, if they are not set yet
        if (!mouseMoveCursor)
            setMouseMoveCursor();
        if (!slewing)
        {
            slewing = true;
            stopTracking(); //toggle tracking off
        }

        //Update focus such that the sky coords at mouse cursor remain approximately constant
        if (Options::useAltAz())
        {
            m_MousePoint.EquatorialToHorizontal(data->lst(), data->geo()->lat());
            clickedPoint()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            dms dAz  = m_MousePoint.az() - clickedPoint()->az();
            dms dAlt = m_MousePoint.altRefracted() - clickedPoint()->altRefracted();
            focus()->setAz(focus()->az().Degrees() - dAz.Degrees()); //move focus in opposite direction
            focus()->setAz(focus()->az().reduce());
            focus()->setAltRefracted(KSUtils::clamp(focus()->altRefracted().Degrees() - dAlt.Degrees(), -90.0, 90.0));
            focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat());
        }
        else
        {
            dms dRA  = m_MousePoint.ra() - clickedPoint()->ra();
            dms dDec = m_MousePoint.dec() - clickedPoint()->dec();
            focus()->setRA(focus()->ra().Hours() - dRA.Hours()); //move focus in opposite direction
            focus()->setRA(focus()->ra().reduce());
            focus()->setDec(KSUtils::clamp(focus()->dec().Degrees() - dDec.Degrees(), -90.0, 90.0));
            focus()->EquatorialToHorizontal(data->lst(), data->geo()->lat());
        }
        showFocusCoords();

        //redetermine RA, Dec of mouse pointer, using new focus
        m_MousePoint = projector()->fromScreen(e->pos(), data->lst(), data->geo()->lat());
        setClickedPoint(&m_MousePoint);

        forceUpdate(); // must be new computed
    }
    else //mouse button not down
    {
        if (Options::useAltAz())
            m_MousePoint.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        emit mousePointChanged(&m_MousePoint);
    }
}

void SkyMap::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0)
        zoomInOrMagStep(e->modifiers());
    else if (e->delta() < 0)
        zoomOutOrMagStep(e->modifiers());
}

void SkyMap::mouseReleaseEvent(QMouseEvent *e)
{
#if !defined(KSTARS_LITE)
    if (m_touchMode)
    {
        m_touchMode = false;
        m_pinchMode = false;
        m_tapAndHoldMode = false;
    }
#endif

    if (ZoomRect.isValid())
    {
        stopTracking();
        SkyPoint newcenter = projector()->fromScreen(ZoomRect.center(), data->lst(), data->geo()->lat());
        setFocus(&newcenter);
        setDestination(newcenter);

        //Zoom in on center of Zoom Circle, by a factor equal to the ratio
        //of the sky pixmap's width to the Zoom Circle's diameter
        float factor = float(width()) / float(ZoomRect.width());
        setZoomFactor(Options::zoomFactor() * factor);
    }

    setMouseCursorShape(static_cast<Cursor>(Options::defaultCursor()));

    ZoomRect = QRect(); //invalidate ZoomRect

    if (m_previewLegend)
    {
        slotCancelLegendPreviewMode();
    }

    //false if double-clicked, because it's unset there.
    if (mouseButtonDown)
    {
        mouseButtonDown = false;
        if (slewing)
        {
            slewing = false;
            if (Options::useAltAz())
                setDestinationAltAz(focus()->alt(), focus()->az(), false);
            else
                setDestination(*focus());
        }
        else if (Options::leftClickSelectsObject())
            mouseDoubleClickEvent(e);
        forceUpdate(); // is needed because after moving the sky not all stars are shown
    }
    // if middle button was pressed unset here
    midMouseButtonDown = false;
}

void SkyMap::mousePressEvent(QMouseEvent *e)
{
    KStars *kstars = KStars::Instance();

    if ((e->modifiers() & Qt::ControlModifier) && (e->button() == Qt::LeftButton))
    {
        ZoomRect.moveCenter(e->pos());
        setZoomMouseCursor();
        update(); //refresh without redrawing skymap
        return;
    }

    // if button is down and cursor is not moved set the move cursor after 500 ms
    QTimer::singleShot(500, this, SLOT(setMouseMoveCursor()));

    // break if point is unusable
    if (projector()->unusablePoint(e->pos()))
        return;

    if (!midMouseButtonDown && e->button() == Qt::MidButton)
    {
        y0                 = 0.5 * height() - e->y(); //record y pixel coordinate for middle-button zooming
        midMouseButtonDown = true;
    }

    if (!mouseButtonDown)
    {
        if (e->button() == Qt::LeftButton)
        {
            mouseButtonDown = true;
        }

        //determine RA, Dec of mouse pointer
        m_MousePoint = projector()->fromScreen(e->pos(), data->lst(), data->geo()->lat());
        setClickedPoint(&m_MousePoint);

        //Find object nearest to clickedPoint()
        double maxrad  = 5000.0 / Options::zoomFactor();
        SkyObject *obj = data->skyComposite()->objectNearest(clickedPoint(), maxrad);
        setClickedObject(obj);
        if (obj)
            setClickedPoint(obj);

        switch (e->button())
        {
            case Qt::LeftButton:
            {
                QString name;
                if (clickedObject())
                {
                    name = clickedObject()->translatedLongName();
                    emit objectClicked(clickedObject());
                }
                else
                    name = i18n("Empty sky");
                //kstars->statusBar()->changeItem(name, 0 );
                kstars->statusBar()->showMessage(name, 0);

                emit positionClicked(&m_MousePoint);
            }

            break;
            case Qt::RightButton:
                if (rulerMode)
                {
                    // Compute angular distance.
                    slotEndRulerMode();
                }
                else
                {
                    // Show popup menu
                    if (clickedObject())
                    {
                        clickedObject()->showPopupMenu(pmenu, QCursor::pos());
                    }
                    else
                    {
                        pmenu->createEmptyMenu(clickedPoint());
                        pmenu->popup(QCursor::pos());
                    }
                }
                break;
            default:
                ;
        }
    }
}

void SkyMap::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !projector()->unusablePoint(e->pos()))
    {
        mouseButtonDown = false;
        if (e->x() != width() / 2 || e->y() != height() / 2)
            slotCenter();
    }
}

double SkyMap::zoomFactor(const int modifier)
{
    double factor = (modifier & Qt::ControlModifier) ? DZOOM : (Options::zoomScrollFactor() + 1);
    if (modifier & Qt::ShiftModifier)
        factor = sqrt(factor);
    return factor;
}

void SkyMap::zoomInOrMagStep(const int modifier)
{
    if (modifier & Qt::AltModifier)
        incMagLimit(modifier);
    else
        setZoomFactor(Options::zoomFactor() * zoomFactor(modifier));
}

void SkyMap::zoomOutOrMagStep(const int modifier)
{
    if (modifier & Qt::AltModifier)
        decMagLimit(modifier);
    else
        setZoomFactor(Options::zoomFactor() / zoomFactor(modifier));
}

double SkyMap::magFactor(const int modifier)
{
    double factor = (modifier & Qt::ControlModifier) ? 0.1 : 0.5;
    if (modifier & Qt::ShiftModifier)
        factor *= 2.0;
    return factor;
}

void SkyMap::incMagLimit(const int modifier)
{
    double limit = 2.222 * log10(static_cast<double>(Options::starDensity())) + 0.35;
    limit += magFactor(modifier);
    if (limit > 5.75954)
        limit = 5.75954;
    Options::setStarDensity(pow(10, (limit - 0.35) / 2.222));
    //printf("maglim set to %3.1f\n", limit);
    forceUpdate();
}

void SkyMap::decMagLimit(const int modifier)
{
    double limit = 2.222 * log10(static_cast<double>(Options::starDensity())) + 0.35;
    limit -= magFactor(modifier);
    if (limit < 1.18778)
        limit = 1.18778;
    Options::setStarDensity(pow(10, (limit - 0.35) / 2.222));
    //printf("maglim set to %3.1f\n", limit);
    forceUpdate();
}
