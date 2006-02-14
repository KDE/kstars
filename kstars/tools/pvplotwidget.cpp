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

#include <math.h> //for sqrt()

#include <kapplication.h>

#include "pvplotwidget.h"
#include "planetviewer.h"
#include "libkdeedu/kdeeduplot/kplotobject.h"

PVPlotWidget::PVPlotWidget( double x1, double x2, double y1, double y2, QWidget *par ) :
			KStarsPlotWidget( x1, x2, y1, y2, par ), 
			mouseButtonDown(false), oldx(0), oldy(0) {
	setFocusPolicy( Qt::StrongFocus );
	setMouseTracking (true);
	pv = (PlanetViewer*)topLevelWidget();
}

PVPlotWidget::PVPlotWidget( QWidget *parent ) :
			KStarsPlotWidget( 0.0, 1.0, 0.0, 1.0, parent ), 
			mouseButtonDown(false), oldx(0), oldy(0) {
	setFocusPolicy( Qt::StrongFocus );
	setMouseTracking (true);
	pv = (PlanetViewer*)topLevelWidget();
}

PVPlotWidget::~ PVPlotWidget() {}
 
void PVPlotWidget::keyPressEvent( QKeyEvent *e ) {
	double xc = (x2() + x())*0.5;
	double yc = (y2() + y())*0.5;
	double xstep = 0.01*(x2() - x());
	double ystep = 0.01*(y2() - y());
	double dx = 0.5*dataWidth();
	double dy = 0.5*dataHeight();
	
	switch ( e->key() ) {
		case Qt::Key_Left:
			if ( xc - xstep > -AUMAX ) {
				setLimits( x() - xstep, x2() - xstep, y(), y2() );
				pv->setCenterPlanet(QString());
				update();
			}
			break;
		
		case Qt::Key_Right:
			if ( xc + xstep < AUMAX ) { 
				setLimits( x() + xstep, x2() + xstep, y(), y2() );
				pv->setCenterPlanet(QString());
				update();
			}
			break;
		
		case Qt::Key_Down:
			if ( yc - ystep > -AUMAX ) {
				setLimits( x(), x2(), y() - ystep, y2() - ystep );
				pv->setCenterPlanet(QString());
				update();
			}
			break;
		
		case Qt::Key_Up:
			if ( yc + ystep < AUMAX ) {
				setLimits( x(), x2(), y() + ystep, y2() + ystep );
				pv->setCenterPlanet(QString());
				update();
			}
			break;
		
		case Qt::Key_Plus:
		case Qt::Key_Equal:
			slotZoomIn();
			break;
		
		case Qt::Key_Minus:
		case Qt::Key_Underscore:
			slotZoomOut();
			break;
		
		case Qt::Key_0: //Sun
			setLimits( -dx, dx, -dy, dy );
			pv->setCenterPlanet( "Sun" );
			update();
			break;
		
		case Qt::Key_1: //Mercury
		{
			QPointF *p = object(10)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Mercury" );
			update();
			break;
		}
		
		case Qt::Key_2: //Venus
		{
			QPointF *p = object(12)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Venus" );
			update();
			break;
		}
		
		case Qt::Key_3: //Earth
		{
			QPointF *p = object(14)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Earth" );
			update();
			break;
		}
		
		case Qt::Key_4: //Mars
		{
			QPointF *p = object(16)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Mars" );
			update();
			break;
		}
		
		case Qt::Key_5: //Jupiter
		{
			QPointF *p = object(18)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Jupiter" );
			update();
			break;
		}
		
		case Qt::Key_6: //Saturn
		{
			QPointF *p = object(20)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Saturn" );
			update();
			break;
		}
		
		case Qt::Key_7: //Uranus
		{
			QPointF *p = object(22)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Uranus" );
			update();
			break;
		}
		
		case Qt::Key_8: //Neptune
		{
			QPointF *p = object(24)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Neptune" );
			update();
			break;
		}
		
		case Qt::Key_9: //Pluto
		{
			QPointF *p = object(26)->point(0);
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
}

void PVPlotWidget::mouseReleaseEvent( QMouseEvent * ) {
	mouseButtonDown = false;
	update();
}

void PVPlotWidget::mouseMoveEvent( QMouseEvent *e ) {
	if ( mouseButtonDown ) {
		//Determine how far we've moved
		double xc = (x2() + x())*0.5;
		double yc = (y2() + y())*0.5;
		double xscale = dataWidth()/( width() - leftPadding() - rightPadding() );
		double yscale = dataHeight()/( height() - topPadding() - bottomPadding() );
		
		xc += ( oldx  - e->x() )*xscale;
		yc -= ( oldy - e->y() )*yscale; //Y data axis is reversed...
		
		if ( xc > -AUMAX && xc < AUMAX && yc > -AUMAX && yc < AUMAX ) {
			setLimits( xc - 0.5*dataWidth(), xc + 0.5*dataWidth(), 
					yc - 0.5*dataHeight(), yc + 0.5*dataHeight() );
			update();
			kapp->processEvents();
		}
		
		oldx = e->x();
		oldy = e->y();
	}
}

void PVPlotWidget::mouseDoubleClickEvent( QMouseEvent *e ) {
	double xscale = dataWidth()/( width() - leftPadding() - rightPadding() );
	double yscale = dataHeight()/( height() - topPadding() - bottomPadding() );
	
	double xc = x() + xscale*( e->x() - leftPadding() );
	double yc = y2() - yscale*( e->y() - topPadding() );

	if ( xc > -AUMAX && xc < AUMAX && yc > -AUMAX && yc < AUMAX ) {
		setLimits( xc - 0.5*dataWidth(), xc + 0.5*dataWidth(), 
				yc - 0.5*dataHeight(), yc + 0.5*dataHeight() );
		update();
	}

	pv->setCenterPlanet( QString() );
	for ( unsigned int i=0; i<9; ++i ) {
		double dx = ( pv->planetObject(i)->point(0)->x() - xc )/xscale;
		if ( dx < 4.0 ) {
			double dy = ( pv->planetObject(i)->point(0)->y() - yc )/yscale;
			if ( sqrt( dx*dx + dy*dy ) < 4.0 ) {
				pv->setCenterPlanet( pv->planetName(i) );
			}
		}
	}
}

void PVPlotWidget::wheelEvent( QWheelEvent *e ) {
	if ( e->delta() > 0 ) slotZoomIn();
	else slotZoomOut();
}

void PVPlotWidget::slotZoomIn() {
	double size( x2() - x() );
	if ( size > 0.8 ) {
		setLimits( x() + 0.02*size, x2() - 0.02*size, y() + 0.02*size, y2() - 0.02*size );
		update();
	}
}

void PVPlotWidget::slotZoomOut() {
	double size( x2() - x() );
	if ( (x2() - x()) < 100.0 ) {
		setLimits( x() - 0.02*size, x2() + 0.02*size, y() - 0.02*size, y2() + 0.02*size );
		update();
	}
}

#include "pvplotwidget.moc"
