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
#include <kdebug.h>
#include <klocale.h>
#include "kstars.h"
#include "planetcatalog.h"
#include "planetviewer.h"
#include "dms.h"

PlanetViewer::PlanetViewer(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Solar System Viewer"), Close, Close, parent )
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	pw = new KStarsPlotWidget( -46.0, 46.0, -46.0, 46.0, page );
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

	for ( unsigned int i=0; i<9; ++i ) {
		orbit[i] = new KPlotObject( "", "white", KPlotObject::CURVE, 1, KPlotObject::SOLID );

		for ( double t=0.0; t<=360.0; t+=5.0 ) {
			dms d( t );
			double sd, cd;
			d.SinCos( sd, cd );
			orbit[i]->addPoint( new DPoint( rad[i]*cd, rad[i]*sd ) );
		}
		pw->addObject( orbit[i] );
	}

	//**Planets**//
	KPlotObject *sun;
	KPlotObject *planets;
	PlanetCatalog *PC = ((KStars*)parent())->data()->PC;
	dms elong[9];

	elong[0] = dms( PC->findByName( "Mercury" )->ecLong()->Degrees() );
	elong[1] = dms( PC->findByName( "Venus"   )->ecLong()->Degrees() );
	elong[2] = dms( PC->findByName( "Earth"   )->ecLong()->Degrees() );
	elong[3] = dms( PC->findByName( "Mars"    )->ecLong()->Degrees() );
	elong[4] = dms( PC->findByName( "Jupiter" )->ecLong()->Degrees() );
	elong[5] = dms( PC->findByName( "Saturn"  )->ecLong()->Degrees() );
	elong[6] = dms( PC->findByName( "Uranus"  )->ecLong()->Degrees() );
	elong[7] = dms( PC->findByName( "Neptune" )->ecLong()->Degrees() );
	elong[8] = dms( PC->findByName( "Pluto"   )->ecLong()->Degrees() );

	sun = new KPlotObject( "Sun", "yellow", KPlotObject::POINTS, 12, KPlotObject::SOLID );
	sun->addPoint( new DPoint( 0.0, 0.0 ) );
	
	planets = new KPlotObject( "Planets", "cyan2", KPlotObject::POINTS, 6, KPlotObject::CIRCLE );
	for ( unsigned int i=0; i<9; ++i ) {
		double s, c;
		elong[i].SinCos( s, c );
		planets->addPoint( new DPoint( rad[i]*c, rad[i]*s ) );
	}

	pw->addObject( sun );
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
		pw->setLimits( 0.95*pw->x(), 0.95*pw->x2(), 0.95*pw->y(), 0.95*pw->y2() );
		pw->update();
	}
}

void PlanetViewer::slotZoomOut() {
	if ( pw->x2() < 50.0 ) {
		pw->setLimits( 1.05*pw->x(), 1.05*pw->x2(), 1.05*pw->y(), 1.05*pw->y2() );
		pw->update();
	}
}

#include "planetviewer.moc"
