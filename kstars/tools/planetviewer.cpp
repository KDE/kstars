/***************************************************************************
                          planetviewer.cpp  -  Display overhead view of the solar system
                             -------------------
    begin                : Sun May 25 2003
    copyright            : (C) 2003 by Jason Harris
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

#include <qlayout.h>
#include <qtimer.h>
#include <kdebug.h>
#include <klocale.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "ksplanetbase.h"
#include "planetcatalog.h"
#include "planetviewer.h"
#include "dms.h"

PlanetViewer::PlanetViewer(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Solar System Viewer"), Close, Close, parent, name )
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	pw = new PVPlotWidget( -48.0, 48.0, -48.0, 48.0, page );
	pw->setXAxisLabel( i18n( "axis label for x-coordinate of solar system viewer.  AU means astronomical unit.", 
		"X-position (AU)" ) );
	pw->setYAxisLabel( i18n( "axis label for y-coordinate of solar system viewer.  AU means astronomical unit.", 
		"Y-position (AU)" ) );
	vlay->addWidget( pw );
	resize( 500, 500 );
	pw->QWidget::setFocus(); //give keyboard focus to the plot widget for key and mouse events
	
	QTimer::singleShot( 0, this, SLOT( initPlotObjects() ) );
}

PlanetViewer::~PlanetViewer()
{
}

void PlanetViewer::paintEvent( QPaintEvent* ) {
	pw->update();
}

void PlanetViewer::initPlotObjects() {
	//**Orbits**//
	KPlotObject *orbit[9];

	//mean orbital radii, in AU
	double rad[9];
	rad[0] = 0.4;
	rad[1] = 0.7;
	rad[2] = 1.0;
	rad[3] = 1.5;
	rad[4] = 5.2;
	rad[5] = 9.5;
	rad[6] = 19.2;
	rad[7] = 30.1;
	rad[8] = 39.5;

	// Planets
	PlanetCatalog *PCat = ((KStars*)parent())->data()->PCat;
	KPlotObject *ksun;
	KPlotObject *planet[9];
	KPlotObject *planetLabel[9];
	QString pName[9], pColor[9];
	
	pName[0] = "Mercury"; pColor[0] = "SlateBlue1";
	pName[1] = "Venus";   pColor[1] = "LightGreen";
	pName[2] = "Earth";   pColor[2] = "Blue";
	pName[3] = "Mars";    pColor[3] = "Red";
	pName[4] = "Jupiter"; pColor[4] = "Goldenrod";
	pName[5] = "Saturn";  pColor[5] = "LightYellow2";
	pName[6] = "Uranus";  pColor[6] = "LightSeaGreen";
	pName[7] = "Neptune"; pColor[7] = "SkyBlue";
	pName[8] = "Pluto";   pColor[8] = "gray";

	ksun = new KPlotObject( "Sun", "yellow", KPlotObject::POINTS, 12, KPlotObject::CIRCLE );
	ksun->addPoint( new DPoint( 0.0, 0.0 ) );
	pw->addObject( ksun );
	
	//Construct the orbit curves.  For each planet, divide one orbital period 
	//into 100 steps, and compute the planet's position at each step.
	for ( unsigned int i=0; i<9; ++i ) {
		orbit[i] = new KPlotObject( "", "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );
		
		double Period = pow(rad[i], 1.5)*1.1*86400.*365.25; //estimate orbital period in seconds
		KSNumbers *num = new KSNumbers( KStarsDateTime::currentDateTime().djd() );
		KSPlanetBase *p = PCat->findByName( pName[i] );
		
		for ( double t=-Period; t <= 0.0; t += 0.01*Period ) {
			num->updateValues( KStarsDateTime::currentDateTime().addSecs( t ).djd() );
			
			p->updateCoords( num );
			double sLg, cLg, sLt, cLt;
			p->helEcLong()->SinCos( sLg, cLg );
			p->helEcLat()->SinCos( sLt, cLt );
			
			orbit[i]->addPoint( new DPoint( p->rsun()*cLg*cLt, p->rsun()*sLg*cLt ) );
		}
		
		//reset original position
		num->updateValues( ((KStars*)parent())->data()->ut().djd() );
		p->updateCoords( num );
		delete num;
		
		pw->addObject( orbit[i] );
	}
	
	for ( unsigned int i=0; i<9; ++i ) {
		planet[i] = new KPlotObject( pName[i], pColor[i], KPlotObject::POINTS, 6, KPlotObject::CIRCLE );
		planetLabel[i] = new KPlotObject( pName[i], pColor[i], KPlotObject::LABEL );
		
		double s, c;
		KSPlanetBase *p = PCat->findByName( pName[i] );
		p->helEcLong()->SinCos( s, c );
		
		planet[i]->addPoint( new DPoint( p->rsun()*c, p->rsun()*s ) );
		planetLabel[i]->addPoint( new DPoint( p->rsun()*c, p->rsun()*s ) );
		pw->addObject( planet[i] );
		pw->addObject( planetLabel[i] );
	}
}

void PlanetViewer::keyPressEvent( QKeyEvent *e ) {
	switch ( e->key() ) {
		case Key_Escape:
			close();
			break;
		default:
			e->ignore();
			break;
	}
}

PVPlotWidget::PVPlotWidget( double x1, double x2, double y1, double y2, QWidget *parent, const char *name ) :
		KStarsPlotWidget( x1, x2, y1, y2, parent, name ) {
	setFocusPolicy( QWidget::StrongFocus );
}

PVPlotWidget::~ PVPlotWidget() {}
 
void PVPlotWidget::keyPressEvent( QKeyEvent *e ) {
	double xc = (x2() + x())*0.5;
	double yc = (y2() + y())*0.5;
	double xstep = 0.01*(x2() - x());
	double ystep = 0.01*(y2() - y());
	
	switch ( e->key() ) {
		case Key_Left:
			if ( xc - xstep > -48.0 ) {
				setLimits( x() - xstep, x2() - xstep, y(), y2() );
				update();
			}
			break;
		
		case Key_Right:
			if ( xc + xstep < 48.0 ) { 
				setLimits( x() + xstep, x2() + xstep, y(), y2() );
				update();
			}
			break;
		
		case Key_Down:
			if ( yc - ystep > -48.0 ) {
				setLimits( x(), x2(), y() - ystep, y2() - ystep );
				update();
			}
			break;
		
		case Key_Up:
			if ( yc + ystep < 48.0 ) {
				setLimits( x(), x2(), y() + ystep, y2() + ystep );
				update();
			}
			break;
		
		case Key_Plus:
		case Key_Equal:
			slotZoomIn();
			break;
		
		case Key_Minus:
		case Key_Underscore:
			slotZoomOut();
			break;
		
		default:
			e->ignore();
			break;
	}
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

#include "planetviewer.moc"
