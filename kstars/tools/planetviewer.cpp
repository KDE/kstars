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
#include "kstarsdata.h"
#include "ksplanetbase.h"
#include "planetcatalog.h"
#include "planetviewer.h"
#include "dms.h"

PlanetViewer::PlanetViewer(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Solar System Viewer"), Close, Close, parent, name )
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	pw = new KStarsPlotWidget( -46.0, 46.0, -46.0, 46.0, page );
	pw->setXAxisLabel( i18n( "axis label for x-coordinate of solar system viewer.  AU means astronomical unit.", 
		"X-position (AU)" ) );
	pw->setYAxisLabel( i18n( "axis label for y-coordinate of solar system viewer.  AU means astronomical unit.", 
		"Y-position (AU)" ) );
	vlay->addWidget( pw );
	resize( 500, 500 );

	initPlotObjects();
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
	PlanetCatalog *PCat = ((KStars*)parent())->data()->PCat;
	KPlotObject *ksun;
	KPlotObject *planet[9];
	KPlotObject *planetLabel[9];
	dms elong[9];
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
	
	for ( unsigned int i=0; i<9; ++i ) {
		elong[i] = dms( PCat->findByName( pName[i] )->helEcLong()->Degrees() );
		planet[i] = new KPlotObject( pName[i], pColor[i], KPlotObject::POINTS, 6, KPlotObject::CIRCLE );
		planetLabel[i] = new KPlotObject( pName[i], pColor[i], KPlotObject::LABEL );
		double s, c;
		elong[i].SinCos( s, c );
		planet[i]->addPoint( new DPoint( rad[i]*c, rad[i]*s ) );
		planetLabel[i]->addPoint( new DPoint( rad[i]*c, rad[i]*s ) );
		pw->addObject( planet[i] );
		pw->addObject( planetLabel[i] );
	}
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
		case Key_Escape:
			close();
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
