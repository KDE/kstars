/***************************************************************************
                          planetviewer.h  -  Display overhead view of the solar system
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
#include <kdebug.h>
#include <klocale.h>
#include "planetviewer.h"
#include "dms.h"

PlanetViewer::PlanetViewer(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Solar System Viewer"), Ok, Ok, parent )
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	pw = new PlotWidget( -46.0, 46.0, -46.0, 46.0, page );
	vlay->addWidget( pw );
	resize( 500, 500 );

	initPlotObjects();
}

PlanetViewer::~PlanetViewer()
{
}

void PlanetViewer::paintEvent( QPaintEvent *e ) {
	pw->update();
}

void PlanetViewer::initPlotObjects() {
	//**Orbits**//
	PlotObject *orbit[9];

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

	for ( unsigned int i=0; i<9; ++i ) {
		orbit[i] = new PlotObject( "", "white", PlotObject::CURVE, 1, PlotObject::SOLID );

		for ( double t=0.0; t<=360.0; t+=5.0 ) {
			dms d( t );
			double sd, cd;
			d.SinCos( sd, cd );
			orbit[i]->addPoint( new DPoint( rad[i]*cd, rad[i]*sd ) );
		}
		pw->addObject( orbit[i] );
	}

	//**Planets**//
	PlotObject *planets;
	dms elong[9];

	elong[0] = dms(   0.0 );
	elong[1] = dms(  30.0 );
	elong[2] = dms(  60.0 );
	elong[3] = dms(  90.0 );
	elong[4] = dms( 120.0 );
	elong[5] = dms( 150.0 );
	elong[6] = dms( 180.0 );
	elong[7] = dms( 210.0 );
	elong[8] = dms( 240.0 );

	planets = new PlotObject( "Planets", "cyan2", PlotObject::POINTS, 8, PlotObject::CIRCLE );
	for ( unsigned int i=0; i<9; ++i ) {
		double s, c;
		elong[i].SinCos( s, c );
		planets->addPoint( new DPoint( rad[i]*c, rad[i]*s ) );
	}

	pw->addObject( planets );
}

void PlanetViewer::keyPressEvent( QKeyEvent *e ) {
	switch ( e->key() ) {
		case Key_Plus:
		case Key_Equal:
			slotZoomIn();
			break;
		case Key_Minus:
		case Key_Underscore:
			slotZoomOut();
			break;
	}
}

void PlanetViewer::slotZoomIn() {
	if ( pw->x2() > 0.4 ) {
		pw->setLimits( 0.9*pw->x1(), 0.9*pw->x2(), 0.9*pw->y1(), 0.9*pw->y2() );
		pw->update();
	}
}

void PlanetViewer::slotZoomOut() {
	if ( pw->x2() < 50.0 ) {
		pw->setLimits( 1.1*pw->x1(), 1.1*pw->x2(), 1.1*pw->y1(), 1.1*pw->y2() );
		pw->update();
	}
}

#include "planetviewer.moc"
