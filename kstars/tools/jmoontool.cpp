/***************************************************************************
                          jmoontool.cpp  -  Display overhead view of the solar system
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
#include <qlabel.h>

#include <kdebug.h>
#include <klocale.h>

#include "jmoontool.h"
#include "jupitermoons.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksplanet.h"
#include "simclock.h"
#include "planetcatalog.h"
#include "dms.h"
#include "ksnumbers.h"

JMoonTool::JMoonTool(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Jupiter Moons Tool"), Close, Close, parent, name )
{
	ksw = (KStars*)parent;
	
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
	
	colJp = "White";
	colIo = "Red";
	colEu = "Yellow";
	colGn = "Orange";
	colCa = "YellowGreen";
	
	QLabel *labIo = new QLabel( "Io", page );
	QLabel *labEu = new QLabel( "Europa", page );
	QLabel *labGn = new QLabel( "Ganymede", page );
	QLabel *labCa = new QLabel( "Callisto", page );
	
	labIo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labEu->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labGn->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labCa->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labIo->setAlignment( AlignHCenter );
	labEu->setAlignment( AlignHCenter );
	labGn->setAlignment( AlignHCenter );
	labCa->setAlignment( AlignHCenter );

	labIo->setPaletteForegroundColor( colIo );
	labEu->setPaletteForegroundColor( colEu );
	labGn->setPaletteForegroundColor( colGn );
	labCa->setPaletteForegroundColor( colCa );
	labIo->setPaletteBackgroundColor( "Black" );
	labEu->setPaletteBackgroundColor( "Black" );
	labGn->setPaletteBackgroundColor( "Black" );
	labCa->setPaletteBackgroundColor( "Black" );
	
	QGridLayout *glay = new QGridLayout( 2, 2, 0 );
	glay->addWidget( labIo, 0, 0 );
	glay->addWidget( labEu, 1, 0 );
	glay->addWidget( labGn, 0, 1 );
	glay->addWidget( labCa, 1, 1 );
	
	pw = new KStarsPlotWidget( 0.0, 1.0, 0.0, 1.0, page );
	pw->setShowGrid( false );
	pw->setYAxisType0( KStarsPlotWidget::TIME );
	pw->setLimits( -12.0, 12.0, -240.0, 240.0 );
	pw->setXAxisLabel( i18n( "offset from Jupiter (arcmin)" ) );
	pw->setYAxisLabel( i18n( "time since now (days)" ) );
	vlay->addLayout( glay );
	vlay->addWidget( pw );
	resize( 250, 500 );

	initPlotObjects();
	update();
}

JMoonTool::~JMoonTool()
{
}

void JMoonTool::initPlotObjects() {
	KPlotObject *orbit[4];
	KPlotObject *jpath;
	long double jd0 = ksw->data()->ut().djd();
	KSSun *ksun = (KSSun*)ksw->data()->PCat->findByName( "Sun" );
	KSPlanet *jup = (KSPlanet*)ksw->data()->PCat->findByName( "Jupiter" );
	JupiterMoons jm;
	
	if ( pw->objectCount() ) pw->clearObjectList();
	
	orbit[0] = new KPlotObject( "io", colIo, KPlotObject::CURVE, 1, KPlotObject::SOLID );
	orbit[1] = new KPlotObject( "europa", colEu, KPlotObject::CURVE, 1, KPlotObject::SOLID );
	orbit[2] = new KPlotObject( "ganymede", colGn, KPlotObject::CURVE, 1, KPlotObject::SOLID );
	orbit[3] = new KPlotObject( "callisto", colCa, KPlotObject::CURVE, 1, KPlotObject::SOLID );
	jpath    = new KPlotObject( "jupiter", colJp, KPlotObject::CURVE, 1, KPlotObject::SOLID );
	
	double dy = 0.01*pw->dataHeight();
	
	//t is the offset from jd0, in hours.
	for ( double t=pw->y(); t<=pw->y2(); t+=dy ) {
		KSNumbers num( jd0 + t/24.0 );
		jm.findPosition( &num, jup, ksun );
		
		//jm.x(i) tells the offset from Jupiter, in units of Jupiter's angular radius.
		//multiply by 0.5*jup->angSize() to get arcminutes
		for ( unsigned int i=0; i<4; ++i ) 
			orbit[i]->addPoint( new DPoint( 0.5*jup->angSize()*jm.x(i), t ) );
		
		jpath->addPoint( new DPoint( 0.0, t ) );
	}

	for ( unsigned int i=0; i<4; ++i ) 
		pw->addObject( orbit[i] );

	pw->addObject( jpath );
}

void JMoonTool::keyPressEvent( QKeyEvent *e ) {
	switch ( e->key() ) {
		case Key_BracketRight:
		{
			double dy = 0.02*pw->dataHeight();
			pw->setLimits( pw->x(), pw->x2(), pw->y()+dy, pw->y2()+dy );
			initPlotObjects();
			pw->update();
			break;
		}
		case Key_BracketLeft:
		{
			double dy = 0.02*pw->dataHeight();
			pw->setLimits( pw->x(), pw->x2(), pw->y()-dy, pw->y2()-dy );
			initPlotObjects();
			pw->update();
			break;
		}
		case Key_Plus:
		case Key_Equal:
		{
			if ( pw->dataHeight() > 48.0 ) { 
				double dy = 0.45*pw->dataHeight();
				double y0 = pw->y() + 0.5*pw->dataHeight();
				pw->setLimits( pw->x(), pw->x2(), y0-dy, y0+dy );
				initPlotObjects();
				pw->update();
			}
			break;
		}
		case Key_Minus:
		case Key_Underscore:
		{
			if ( pw->dataHeight() < 960.0 ) {
				double dy = 0.55*pw->dataHeight();
				double y0 = pw->y() + 0.5*pw->dataHeight();
				pw->setLimits( pw->x(), pw->x2(), y0-dy, y0+dy );
				initPlotObjects();
				pw->update();
			}
			break;
		}
		case Key_Escape:
		{
			close();
			break;
		}
		
		default: { e->ignore(); break; }
	}
}

#include "jmoontool.moc"
