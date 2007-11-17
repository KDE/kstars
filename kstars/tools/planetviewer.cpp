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

#include <stdlib.h> //needed for abs() on some platforms

#include <qfile.h>
#include <qlayout.h>
#include <kdebug.h>
#include <klocale.h>
#include <kglobal.h>
#include <kiconloader.h>

#include "planetviewer.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "ksplanetbase.h"
#include "dms.h"
#include "timestepbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

#define AUMAX 48

PlanetViewer::PlanetViewer(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Solar System Viewer"), Close, Close, parent, name ), PCat( ((KStars*)parent)->data() ), scale(1.0), isClockRunning(false), tmr(this)
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	pw = new PlanetViewerUI( page );
	pw->map->setLimits( -48.0, 48.0, -48.0, 48.0 );
	pw->map->setXAxisLabel( i18n( "axis label for x-coordinate of solar system viewer.  AU means astronomical unit.", "X-position (AU)" ) );
	pw->map->setYAxisLabel( i18n( "axis label for y-coordinate of solar system viewer.  AU means astronomical unit.", "Y-position (AU)" ) );
	
	pw->timeStep->setDaysOnly( true );
	pw->timeStep->tsbox()->setValue( 1 ); //start with 1-day timestep

	pw->RunButton->setPixmap( KGlobal::iconLoader()->loadIcon( "1rightarrow", KIcon::Toolbar ) );
	pw->dateBox->setDate( ((KStars*)parent)->data()->lt().date() );
	
	vlay->addWidget( pw );
	resize( 500, 500 );
	pw->map->QWidget::setFocus(); //give keyboard focus to the plot widget for key and mouse events
	
	pName[0] = "Mercury"; pColor[0] = "SlateBlue1";
	pName[1] = "Venus";   pColor[1] = "LightGreen";
	pName[2] = "Earth";   pColor[2] = "Blue";
	pName[3] = "Mars";    pColor[3] = "Red";
	pName[4] = "Jupiter"; pColor[4] = "Goldenrod";
	pName[5] = "Saturn";  pColor[5] = "LightYellow2";
	pName[6] = "Uranus";  pColor[6] = "LightSeaGreen";
	pName[7] = "Neptune"; pColor[7] = "SkyBlue";
	pName[8] = "Pluto";   pColor[8] = "gray";

	setCenterPlanet("");
	
	PCat.initialize();
	ut = ((KStars*)parent)->data()->ut();
	KSNumbers num( ut.djd() );
	PCat.findPosition( &num, 0, 0 ); //NULL args: don't need geocent. coords.
	
	for ( uint i=0; i<9; ++i ) 
		LastUpdate[i] = int( ut.date().jd() );

	//The planets' update intervals are 0.25% of one period:
	UpdateInterval[0] = 0;
	UpdateInterval[1] = 0;
	UpdateInterval[2] = 0;
	UpdateInterval[3] = 1;
	UpdateInterval[4] = 5;
	UpdateInterval[5] = 13;
	UpdateInterval[6] = 38;
	UpdateInterval[7] = 75;
	UpdateInterval[8] = 113;

	QTimer::singleShot( 0, this, SLOT( initPlotObjects() ) );

	connect( &tmr, SIGNAL( timeout() ), SLOT( tick() ) );
	connect( pw->timeStep, SIGNAL( scaleChanged(float) ), SLOT( setTimeScale(float) ) );
	connect( pw->RunButton, SIGNAL( clicked() ), SLOT( slotRunClock() ) );
	connect( pw->dateBox, SIGNAL( valueChanged( const ExtDate & ) ), SLOT( slotChangeDate( const ExtDate & ) ) );
	connect( pw->TodayButton, SIGNAL( clicked() ), SLOT( slotToday() ) );
}

PlanetViewer::~PlanetViewer()
{
}

void PlanetViewer::tick() {
	//Update the time/date
	ut.setDJD( ut.djd() + scale*0.1 );
	pw->dateBox->setDate( ut.date() );
	
	updatePlanets();
}

void PlanetViewer::setTimeScale(float f) {
	scale = f/86400.; //convert seconds to days
}

void PlanetViewer::slotRunClock() {
	isClockRunning = !isClockRunning;
	
	if ( isClockRunning ) {
		pw->RunButton->setPixmap( KGlobal::iconLoader()->loadIcon( "player_pause", KIcon::Toolbar ) );
		tmr.start( 100 );
//		pw->dateBox->setEnabled( false );
	} else {
		pw->RunButton->setPixmap( KGlobal::iconLoader()->loadIcon( "1rightarrow", KIcon::Toolbar ) );
		tmr.stop();
//		pw->dateBox->setEnabled( true );
	}
}

void PlanetViewer::slotChangeDate( const ExtDate & ) {
	if ( pw->dateBox->date().isValid() ) {
		ut.setDate( pw->dateBox->date() ); 
		updatePlanets();
	}
}

void PlanetViewer::updatePlanets() {
	KSNumbers num( ut.djd() );
	bool changed(false);
	
	//Check each planet to see if it needs to be updated
	for ( unsigned int i=0; i<9; ++i ) {
		if ( abs( int(ut.date().jd()) - LastUpdate[i] ) > UpdateInterval[i] ) {
			KSPlanetBase *p = PCat.findByName( pName[i] );
			p->findPosition( &num );
			
			double s, c, s2, c2;
			p->helEcLong()->SinCos( s, c );
			p->helEcLat()->SinCos( s2, c2 );
			planet[i]->point(0)->setX( p->rsun()*c*c2 );
			planet[i]->point(0)->setY( p->rsun()*s*c2 );
			planetLabel[i]->point(0)->setX( p->rsun()*c*c2 );
			planetLabel[i]->point(0)->setY( p->rsun()*s*c2 );
			
			if ( centerPlanet() == pName[i] ) {
				double xc = (pw->map->x2() + pw->map->x())*0.5;
				double yc = (pw->map->y2() + pw->map->y())*0.5;
				double dx = planet[i]->point(0)->x() - xc;
				double dy = planet[i]->point(0)->y() - yc;
				pw->map->setLimits( pw->map->x() + dx, pw->map->x2() + dx, 
						pw->map->y() + dy, pw->map->y2() + dy );
			}
			
			LastUpdate[i] = int(ut.date().jd());
			changed = true;
		}
	}
	
	if ( changed ) pw->map->update();
}

void PlanetViewer::slotToday() {
	KStars *ks = (KStars*)parent();
	pw->dateBox->setDate( ks->data()->lt().date() );
}

void PlanetViewer::paintEvent( QPaintEvent* ) {
	pw->map->update();
}

void PlanetViewer::initPlotObjects() {
	// Planets
	ksun = new KPlotObject( "Sun", "yellow", KPlotObject::POINTS, 12, KPlotObject::CIRCLE );
	ksun->addPoint( new DPoint( 0.0, 0.0 ) );
	pw->map->addObject( ksun );
	
	//Read in the orbit curves
	KPlotObject *orbit[9];
	for ( unsigned int i=0; i<9; ++i ) {
		orbit[i] = new KPlotObject( "", "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );
		
		QFile orbitFile;
		if ( KSUtils::openDataFile( orbitFile, pName[i].lower() + ".orbit" ) ) {
			QTextStream orbitStream( &orbitFile );
			double x, y, z;
			while ( !orbitStream.eof() ) {
				orbitStream >> x >> y >> z;
				orbit[i]->addPoint( new DPoint( x, y ) );
			}
		}
		
		pw->map->addObject( orbit[i] );
	}
	
	for ( unsigned int i=0; i<9; ++i ) {
		planet[i] = new KPlotObject( pName[i], pColor[i], KPlotObject::POINTS, 6, KPlotObject::CIRCLE );
		planetLabel[i] = new KPlotObject( i18n(pName[i].local8Bit()), pColor[i], KPlotObject::LABEL );
		
		double s, c;
		KSPlanetBase *p = PCat.findByName( pName[i] );
		p->helEcLong()->SinCos( s, c );
		
		planet[i]->addPoint( new DPoint( p->rsun()*c, p->rsun()*s ) );
		planetLabel[i]->addPoint( new DPoint( p->rsun()*c, p->rsun()*s ) );
		pw->map->addObject( planet[i] );
		pw->map->addObject( planetLabel[i] );
	}
	
	update();
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

PVPlotWidget::PVPlotWidget( double x1, double x2, double y1, double y2, QWidget *par, const char *name ) :
			KStarsPlotWidget( x1, x2, y1, y2, par, name ), 
			mouseButtonDown(false), oldx(0), oldy(0) {
	setFocusPolicy( QWidget::StrongFocus );
	setMouseTracking (true);
	pv = (PlanetViewer*)topLevelWidget();
}

PVPlotWidget::PVPlotWidget( QWidget *parent, const char *name ) :
			KStarsPlotWidget( 0.0, 1.0, 0.0, 1.0, parent, name ), 
			mouseButtonDown(false), oldx(0), oldy(0) {
	setFocusPolicy( QWidget::StrongFocus );
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
		case Key_Left:
			if ( xc - xstep > -AUMAX ) {
				setLimits( x() - xstep, x2() - xstep, y(), y2() );
				pv->setCenterPlanet("");
				update();
			}
			break;
		
		case Key_Right:
			if ( xc + xstep < AUMAX ) { 
				setLimits( x() + xstep, x2() + xstep, y(), y2() );
				pv->setCenterPlanet("");
				update();
			}
			break;
		
		case Key_Down:
			if ( yc - ystep > -AUMAX ) {
				setLimits( x(), x2(), y() - ystep, y2() - ystep );
				pv->setCenterPlanet("");
				update();
			}
			break;
		
		case Key_Up:
			if ( yc + ystep < AUMAX ) {
				setLimits( x(), x2(), y() + ystep, y2() + ystep );
				pv->setCenterPlanet("");
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
		
		case Key_0: //Sun
			setLimits( -dx, dx, -dy, dy );
			pv->setCenterPlanet( "Sun" );
			update();
			break;
		
		case Key_1: //Mercury
		{
			DPoint *p = object(10)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Mercury" );
			update();
			break;
		}
		
		case Key_2: //Venus
		{
			DPoint *p = object(12)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Venus" );
			update();
			break;
		}
		
		case Key_3: //Earth
		{
			DPoint *p = object(14)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Earth" );
			update();
			break;
		}
		
		case Key_4: //Mars
		{
			DPoint *p = object(16)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Mars" );
			update();
			break;
		}
		
		case Key_5: //Jupiter
		{
			DPoint *p = object(18)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Jupiter" );
			update();
			break;
		}
		
		case Key_6: //Saturn
		{
			DPoint *p = object(20)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Saturn" );
			update();
			break;
		}
		
		case Key_7: //Uranus
		{
			DPoint *p = object(22)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Uranus" );
			update();
			break;
		}
		
		case Key_8: //Neptune
		{
			DPoint *p = object(24)->point(0);
			setLimits( p->x() - dx, p->x() + dx, p->y() - dy, p->y() + dy );
			pv->setCenterPlanet( "Neptune" );
			update();
			break;
		}
		
		case Key_9: //Pluto
		{
			DPoint *p = object(26)->point(0);
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
			kapp->processEvents(20);
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

	pv->setCenterPlanet( "" );
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

#include "planetviewer.moc"
