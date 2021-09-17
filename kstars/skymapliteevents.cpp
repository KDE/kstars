/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "skymaplite.h"
#include "kstarsdata.h"
#include "kstarslite.h"

#include <QtMath>

#include "kstarslite/skypointlite.h"
#include "kstarslite/skyobjectlite.h"

#include "kstarslite/skyitems/planetsitem.h"
#include "Options.h"
#include "projections/projector.h"
#include "skymapcomposite.h"
#include "ksutils.h"
//#include <QTapSensor>

void SkyMapLite::mousePressEvent(QMouseEvent *e)
{
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
        double maxrad  = 1000.0 / Options::zoomFactor();
        SkyObject *obj = data->skyComposite()->objectNearest(clickedPoint(), maxrad);
        setClickedObject(obj);
        if (obj)
            setClickedPoint(obj);

        switch (e->button())
        {
            case Qt::LeftButton:
            {
                /*QString name;
                if( clickedObject() )
                    //name = clickedObject()->translatedLongName();
                else
                    //name = i18n( "Empty sky" );
                //kstars->statusBar()->changeItem(name, 0 );
                //kstars->statusBar()->showMessage(name, 0 );

                emit positionClicked(&m_MousePoint);*/
            }

            break;
            case Qt::RightButton:
                /*if( rulerMode ) {
                    // Compute angular distance.
                    slotEndRulerMode();
                } else {*/
                // Show popup menu
                if (clickedObject())
                {
                    emit objectLiteChanged();
                }
                else
                {
                    emit pointLiteChanged();
                    /* pmenu->createEmptyMenu( clickedPoint() );
                        pmenu->popup( QCursor::pos() );*/
                }
                //}
                break;
            default:
                ;
        }
    }
}

void SkyMapLite::mouseReleaseEvent(QMouseEvent *)
{
    /*if ( ZoomRect.isValid() ) {
        stopTracking();
        SkyPoint newcenter = projector()->fromScreen( ZoomRect.center(), data->lst(), data->geo()->lat() );
        setFocus( &newcenter );
        setDestination( newcenter );

        //Zoom in on center of Zoom Circle, by a factor equal to the ratio
        //of the sky pixmap's width to the Zoom Circle's diameter
        float factor = float(width()) / float(ZoomRect.width());
        setZoomFactor( Options::zoomFactor() * factor );
    }*/
    setDefaultMouseCursor();
    ZoomRect = QRect(); //invalidate ZoomRect

    /*if(m_previewLegend) {
        slotCancelLegendPreviewMode();
    }*/

    //false if double-clicked, because it's unset there.
    if (mouseButtonDown)
    {
        mouseButtonDown = false;
        if (getSlewing())
        {
            setSlewing(false);
            if (Options::useAltAz())
                setDestinationAltAz(focus()->alt(), focus()->az(), false);
            else
                setDestination(*focus());
        }
        forceUpdate(); // is needed because after moving the sky not all stars are shown
    }
    // if middle button was pressed unset here
    midMouseButtonDown = false;
}

void SkyMapLite::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !projector()->unusablePoint(e->pos()))
    {
        mouseButtonDown = false;
        if (e->x() != width() / 2 || e->y() != height() / 2)
            slotCenter();
    }
}

void SkyMapLite::mouseMoveEvent(QMouseEvent *e)
{
    if (Options::useHoverLabel())
    {
        //Start a single-shot timer to monitor whether we are currently hovering.
        //The idea is that whenever a moveEvent occurs, the timer is reset.  It
        //will only timeout if there are no move events for HOVER_INTERVAL ms
        m_HoverTimer.start(HOVER_INTERVAL);
        //DELETE? QToolTip::hideText();
    }

    //Are we defining a ZoomRect?
    /*if ( ZoomRect.center().x() > 0 && ZoomRect.center().y() > 0 ) {
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
    */
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
        // set the mouseMoveCursor and set slewing to true, if they are not set yet
        if (!mouseMoveCursor)
            setMouseMoveCursor();
        if (!getSlewing())
        {
            setSlewing(true);
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
        //showFocusCoords();

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

void SkyMapLite::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0)
    {
        zoomInOrMagStep(e->modifiers());
        //setRotation(rotation() + 0.1); TEST
    }
    else if (e->delta() < 0)
    {
        zoomOutOrMagStep(e->modifiers());
        //setRotation(rotation() - 0.1); TEST
    }
}

void SkyMapLite::touchEvent(QTouchEvent *e)
{
    QList<QTouchEvent::TouchPoint> points = e->touchPoints();

    bool autoMode = false; //Always false for devices that doesn't support automatic mode
#if defined(Q_OS_ANDROID)
    autoMode = getAutomaticMode();
#endif

    if (points.length() == 2)
    {
        //Set tapBegan to false because user doesn't tap but either pans or pinches to zoom
        tapBegan = false;
        if (projector()->unusablePoint(points[0].pos()) || projector()->unusablePoint(points[1].pos()))
            return;

        //Pinch to zoom
        double x_old_diff = qFabs(points[1].lastPos().x() - points[0].lastPos().x());
        double y_old_diff = qFabs(points[1].lastPos().y() - points[0].lastPos().y());

        //Manhattan distance of old points
        double md_old = x_old_diff + y_old_diff;

        double x_diff = qFabs(points[1].pos().x() - points[0].pos().x());
        double y_diff = qFabs(points[1].pos().y() - points[0].pos().y());

        //Manhattan distance of new points
        double md_new = x_diff + y_diff;

        int zoomThreshold = 5;

        if (md_old - md_new < -zoomThreshold)
            zoomInOrMagStep(Qt::MetaModifier);
        else if (md_old - md_new > zoomThreshold)
            zoomOutOrMagStep(Qt::MetaModifier);

        double x_min = points[1].pos().x() < points[0].pos().x() ? points[1].pos().x() : points[0].pos().x();
        double y_min = points[1].pos().y() < points[0].pos().y() ? points[1].pos().y() : points[0].pos().y();

        //Center point on the line between 2 touch points used for moveEvent
        QPointF pinchCenter(x_min + x_diff / 2, y_min + y_diff / 2);

        //Pinch(turn) to rotate
        QPointF old_vec = points[1].lastPos() - points[0].lastPos();
        QPointF new_vec = points[1].pos() - points[0].pos();

        double old_vec_len = sqrt((old_vec.x() * old_vec.x()) + (old_vec.y() * old_vec.y()));
        double new_vec_len = sqrt((new_vec.x() * new_vec.x()) + (new_vec.y() * new_vec.y()));

        //Normalize vectors
        old_vec = QPointF(old_vec.x() / old_vec_len, old_vec.y() / old_vec_len);
        new_vec = QPointF(new_vec.x() / new_vec_len, new_vec.y() / new_vec_len);

        double old_atan = qAtan2(old_vec.y(), old_vec.x());
        double new_atan = qAtan2(new_vec.y(), new_vec.x());

        /*Workaround to solve the strange bug when sometimes signs of qAtan2 results
        for 2 vectors are different*/
        if ((new_atan > 0 && old_atan < 0) || (new_atan < 0 && old_atan > 0))
        {
            old_atan *= -1;
        }

        //Get the angle between two vectors
        /*double delta = new_atan - old_atan;

        //Rotation is under construction
        if(rotation() > 360) {
            setRotation(0);
        } else if(rotation() < 0) {
            setRotation(360);
        }

        //Scale the angle to speed up the rotation
        delta *= 100;
        setRotation(rotation() + delta);
        //update(); //Apply rotation*/

        //Allow movement of SkyMapLite while rotating or zooming
        if (!getCenterLocked() && !autoMode)
        {
            QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, pinchCenter, Qt::LeftButton, Qt::LeftButton,
                                                 Qt::ControlModifier);
            if (!pinch)
            {
                m_MousePoint = projector()->fromScreen(pinchCenter, data->lst(), data->geo()->lat());
                setClickedPoint(&m_MousePoint);
                mouseButtonDown = true;
                pinch           = true;
            }
            mouseMoveEvent(event);
            delete event;
        }

        if (e->touchPointStates() & Qt::TouchPointReleased)
        {
            setSlewing(false);
            if (pinch)
            {
                pinch           = false;
                mouseButtonDown = false;
            }
        }
    }
    else if (points.length() == 1 && !pinch && !autoMode)
    {
        QPointF point = points[0].screenPos();
        //Set clicked point (needed for pan)
        if (e->type() == QEvent::TouchBegin)
        {
            m_MousePoint = projector()->fromScreen(point, data->lst(), data->geo()->lat());
            setClickedPoint(&m_MousePoint);
            mouseButtonDown = true;
        }
        else if (e->type() == QEvent::TouchEnd)
        {
            mouseButtonDown = false;
            if (getSlewing())
            {
                setSlewing(false);
                if (Options::useAltAz())
                    setDestinationAltAz(focus()->alt(), focus()->az(), false);
                else
                    setDestination(*focus());
            }
        }

        if (!projector()->unusablePoint(points[0].screenPos()))
        {
            if (!tapBegan && (e->touchPointStates() & Qt::TouchPointPressed))
            {
                //We set tapBegan to true whenever user tapped on the screen
                tapBegan = true;
                m_tapBeganTimer.start(100);
            }
            else if ((e->touchPointStates() & Qt::TouchPointMoved) || getSlewing())
            {
                //Set tapBegan to false because user doesn't tap but either pans or pinches to zoom
                if (m_tapBeganTimer.remainingTime() > 0)
                {
                    return;
                }
                else
                {
                    m_tapBeganTimer.stop();
                }
                tapBegan = false;

                QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton,
                                                     Qt::ControlModifier);
                mouseMoveEvent(event);
                delete event;

                //If user didn't pan and pinch to zoom tapBegan will be true
            }
            else if ((e->touchPointStates() & Qt::TouchPointReleased) && tapBegan)
            {
                if (getSlewing())
                    setSlewing(false);
                tapBegan = false;
                //Show tap animation
                emit posClicked(point);
                //determine RA, Dec of touch
                m_MousePoint = projector()->fromScreen(point, data->lst(), data->geo()->lat());
                setClickedPoint(&m_MousePoint);

                //Find object nearest to clickedPoint()
                double maxrad =
                    1000.0 /
                    Options::
                    zoomFactor(); /* On high zoom-level it is very hard to select the object using touch screen.
                                            That's why radius remains constant*/
                maxrad         = qMax(maxrad, 2.5);
                SkyObject *obj = data->skyComposite()->objectNearest(clickedPoint(), maxrad);
                setClickedObject(obj);
                if (obj)
                    setClickedPoint(obj);

                if (clickedObject())
                {
                    emit objectLiteChanged();
                }
                else
                {
                    emit pointLiteChanged();
                }
            }
        }
    }
}

double SkyMapLite::zoomFactor(const int modifier)
{
    double factor = (modifier & Qt::ControlModifier) ? DZOOM : 2.0;
    if (modifier & Qt::ShiftModifier)
        factor = sqrt(factor);
    return factor;
}

void SkyMapLite::zoomInOrMagStep(const int modifier)
{
    if (modifier & Qt::AltModifier)
        incMagLimit(modifier);
    else if (modifier & Qt::MetaModifier) //Used in pinch-to-zoom gesture
    {
        setZoomFactor(Options::zoomFactor() + Options::zoomFactor() * 0.05);
    }
    else
        setZoomFactor(Options::zoomFactor() * zoomFactor(modifier));
}

void SkyMapLite::zoomOutOrMagStep(const int modifier)
{
    if (modifier & Qt::AltModifier)
        decMagLimit(modifier);
    else if (modifier & Qt::MetaModifier) //Used in pinch-to-zoom gesture
    {
        setZoomFactor(Options::zoomFactor() - Options::zoomFactor() * 0.05);
    }
    else
        setZoomFactor(Options::zoomFactor() / zoomFactor(modifier));
}

double SkyMapLite::magFactor(const int modifier)
{
    double factor = (modifier & Qt::ControlModifier) ? 0.1 : 0.5;
    if (modifier & Qt::ShiftModifier)
        factor *= 2.0;
    return factor;
}

void SkyMapLite::setMagLim(double magLim)
{
    if (m_magLim != magLim)
    {
        m_magLim = magLim;
        if (m_magLim > 5.75954)
            m_magLim = 5.75954;
        if (m_magLim < 1.18778)
            m_magLim = 1.18778;
        emit magLimChanged(m_magLim);

        Options::setStarDensity(pow(10, (m_magLim - 0.35) / 2.222));
        //printf("maglim set to %3.1f\n", m_magLim);
        forceUpdate();
    }
}

void SkyMapLite::stopTracking()
{
    KStarsLite *kstars = KStarsLite::Instance();
    emit positionChanged();
    if (kstars && Options::isTracking())
        kstars->slotTrack();
}

uint SkyMapLite::projType() const
{
    return m_proj->type();
}

void SkyMapLite::incMagLimit(const int modifier)
{
    m_magLim = 2.222 * log10(static_cast<double>(Options::starDensity())) + 0.35;
    m_magLim += magFactor(modifier);
    if (m_magLim > 5.75954)
        m_magLim = 5.75954;
    emit magLimChanged(m_magLim);
    Options::setStarDensity(pow(10, (m_magLim - 0.35) / 2.222));
    //printf("maglim set to %3.1f\n", m_magLim);
    forceUpdate();
}

void SkyMapLite::decMagLimit(const int modifier)
{
    m_magLim = 2.222 * log10(static_cast<double>(Options::starDensity())) + 0.35;
    m_magLim -= magFactor(modifier);
    if (m_magLim < 1.18778)
        m_magLim = 1.18778;
    emit magLimChanged(m_magLim);
    Options::setStarDensity(pow(10, (m_magLim - 0.35) / 2.222));
    //printf("maglim set to %3.1f\n", m_magLim);
    forceUpdate();
}
