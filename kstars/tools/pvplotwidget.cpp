/***************************************************************************
                          pvplotwidget.cpp
                             -------------------
    begin                : Sat 17 Dec 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "pvplotwidget.h"

#include <cmath> //for sqrt()

#include <QApplication>
#include <qglobal.h>
#include <KPlotObject>
#include <KPlotPoint>
#include <QDebug>

#include "planetviewer.h"

PVPlotWidget::PVPlotWidget( QWidget *parent ) :
        KPlotWidget( parent ),
        mouseButtonDown(false), oldx(0), oldy(0), factor(2)
{
    setFocusPolicy( Qt::StrongFocus );
    setMouseTracking (true);
    setAntialiasing(true);
    //FIXME: Evil cast!
    pv = (PlanetViewer*)topLevelWidget();
}

PVPlotWidget::~ PVPlotWidget() {}

void PVPlotWidget::keyPressEvent( QKeyEvent *e ) {
    double xc = (dataRect().right() + dataRect().x())*0.5;
    double yc = (dataRect().bottom() + dataRect().y())*0.5;
    double xstep = 0.01*(dataRect().right() - dataRect().x());
    double ystep = 0.01*(dataRect().bottom() - dataRect().y());
    double dx = 0.5*dataRect().width();
    double dy = 0.5*dataRect().height();

    switch ( e->key() ) {
    case Qt::Key_Left:
        if ( xc - xstep > -AUMAX ) {
            setLimits( dataRect().x() - xstep, dataRect().right() - xstep, dataRect().y(), dataRect().bottom() );
            pv->setCenterPlanet(QString());
            update();
        }
        break;

    case Qt::Key_Right:
        if ( xc + xstep < AUMAX ) {
            setLimits( dataRect().x() + xstep, dataRect().right() + xstep, dataRect().y(), dataRect().bottom() );
            pv->setCenterPlanet(QString());
            update();
        }
        break;

    case Qt::Key_Down:
        if ( yc - ystep > -AUMAX ) {
            setLimits( dataRect().x(), dataRect().right(), dataRect().y() - ystep, dataRect().bottom() - ystep );
            pv->setCenterPlanet(QString());
            update();
        }
        break;

    case Qt::Key_Up:
        if ( yc + ystep < AUMAX ) {
            setLimits( dataRect().x(), dataRect().right(), dataRect().y() + ystep, dataRect().bottom() + ystep );
            pv->setCenterPlanet(QString());
            update();
        }
        break;

    case Qt::Key_Plus:
    case Qt::Key_Equal:
        {
            updateFactor( e->modifiers() );
            slotZoomIn();
            break;
        }

    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        {
            updateFactor( e->modifiers() );
            slotZoomOut();
            break;
        }

    case Qt::Key_0: //Sun
        setLimits( -dx, dx, -dy, dy );
        pv->setCenterPlanet( "Sun" );
        update();
        break;

    case Qt::Key_1: //Mercury
        {
            KPlotPoint *p = plotObjects().at(10)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Mercury" );
            update();
            break;
        }

    case Qt::Key_2: //Venus
        {
            KPlotPoint *p = plotObjects().at(11)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Venus" );
            update();
            break;
        }

    case Qt::Key_3: //Earth
        {
            KPlotPoint *p = plotObjects().at(12)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Earth" );
            update();
            break;
        }

    case Qt::Key_4: //Mars
        {
            KPlotPoint *p = plotObjects().at(13)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Mars" );
            update();
            break;
        }

    case Qt::Key_5: //Jupiter
        {
            KPlotPoint *p = plotObjects().at(14)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Jupiter" );
            update();
            break;
        }

    case Qt::Key_6: //Saturn
        {
            KPlotPoint *p = plotObjects().at(15)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Saturn" );
            update();
            break;
        }

    case Qt::Key_7: //Uranus
        {
            KPlotPoint *p = plotObjects().at(16)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Uranus" );
            update();
            break;
        }

    case Qt::Key_8: //Neptune
        {
            KPlotPoint *p = plotObjects().at(17)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Neptune" );
            update();
            break;
        }

    case Qt::Key_9: //Pluto
        {
            KPlotPoint *p = plotObjects().at(18)->points().at(0);
            setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
            pv->setCenterPlanet( "Pluto" );
            update();
            break;
        }

    default:
        e->ignore();
        break;
    }
}

void PVPlotWidget::mousePressEvent( QMouseEvent *e ) {
    mouseButtonDown = true;
    oldx = e->x();
    oldy = e->y();

    double xscale = dataRect().width()/( width() - leftPadding() - rightPadding() );
    double yscale = dataRect().height()/( height() - topPadding() - bottomPadding() );

    double xaxis = ( oldx - width()/2 - 10 ) * xscale ;
    double yaxis = ( height()/2 - oldy - 10 ) * yscale ;

    double xadjust = ( dataRect().bottomLeft().x() + dataRect().bottomRight().x() ) / 2;
    double yadjust = ( dataRect().topLeft().y() + dataRect().bottomLeft().y() ) / 2;

    double clickedX = xaxis + xadjust;
    double clickedY = yaxis + yadjust;

    distancePoints.append( new KPlotObject( "red", KPlotObject::Points, 3, KPlotObject::Circle ) );
    distancePoints[ distancePoints.length() - 1 ]->addPoint( clickedX ,  clickedY , "clicked" );
    this->addPlotObject( distancePoints[ distancePoints.length() - 1 ] );
    update();
}

void PVPlotWidget::mouseReleaseEvent( QMouseEvent * ) {
    mouseButtonDown = false;
    update();
}

void PVPlotWidget::mouseMoveEvent( QMouseEvent *e ) {
    if ( mouseButtonDown && !pv->distanceButtonPressed() ) {
        //Determine how far we've moved
        double xc = (dataRect().right() + dataRect().x())*0.5;
        double yc = (dataRect().bottom() + dataRect().y())*0.5;
        double xscale = dataRect().width()/( width() - leftPadding() - rightPadding() );
        double yscale = dataRect().height()/( height() - topPadding() - bottomPadding() );

        xc += ( oldx  - e->x() )*xscale;
        yc -= ( oldy - e->y() )*yscale; //Y data axis is reversed...

        if ( xc > -AUMAX && xc < AUMAX && yc > -AUMAX && yc < AUMAX ) {
            setLimits( xc - 0.5*dataRect().width(), xc + 0.5*dataRect().width(),
                       yc - 0.5*dataRect().height(), yc + 0.5*dataRect().height() );
            update();
            qApp->processEvents();
        }

        oldx = e->x();
        oldy = e->y();
    }
    else if( mouseButtonDown && pv->distanceButtonPressed() ){
        int dx = ( oldx - e->x() ) * dataRect().width() ;
        int dy = ( oldy - e->y() ) * dataRect().height() ;
        double distance =  sqrt( dx*dx + dy*dy ) / 1200;
        qDebug() << distance << "AU";
    }
}

void PVPlotWidget::mouseDoubleClickEvent( QMouseEvent *e ) {
    double xscale = dataRect().width()/( width() - leftPadding() - rightPadding() );
    double yscale = dataRect().height()/( height() - topPadding() - bottomPadding() );

    double xc = dataRect().x() + xscale*( e->x() - leftPadding() );
    double yc = dataRect().bottom() - yscale*( e->y() - topPadding() );

    if ( xc > -AUMAX && xc < AUMAX && yc > -AUMAX && yc < AUMAX ) {
        setLimits( xc - 0.5*dataRect().width(), xc + 0.5*dataRect().width(),
                   yc - 0.5*dataRect().height(), yc + 0.5*dataRect().height() );
        update();
    }

}

void PVPlotWidget::wheelEvent( QWheelEvent *e ) {
    updateFactor( e->modifiers() );
    if ( e->delta() > 0 ) slotZoomIn();
    else slotZoomOut();
}

void PVPlotWidget::slotZoomIn() {
    double size = dataRect().width();
    if ( size > 0.8 ) {
        setLimits( dataRect().x() + factor * 0.01 * size, dataRect().right() - factor * 0.01 * size, dataRect().y() + factor * 0.01 * size, dataRect().bottom() - factor * 0.01 * size );
        update();
    }
}

void PVPlotWidget::slotZoomOut() {
    double size = dataRect().width();
    if ( (size) < 100.0 ) {
        setLimits( dataRect().x() - factor * 0.01 * size, dataRect().right() + factor * 0.01 * size, dataRect().y() - factor * 0.01 * size, dataRect().bottom() + factor * 0.01 * size );
        update();
    }
}

void PVPlotWidget::updateFactor( const int modifier ) {
    factor = ( modifier & Qt::ControlModifier ) ? 1 : 25;
}

#include "pvplotwidget.moc"
