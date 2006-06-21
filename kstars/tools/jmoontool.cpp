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

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLayout>
#include <QFrame>
#include <QKeyEvent>

#include <kdebug.h>
#include <klocale.h>

#include "jmoontool.h"
#include "jupitermoons.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksplanet.h"
#include "simclock.h"
#include "dms.h"
#include "ksnumbers.h"
#include "libkdeedu/kdeeduplot/kplotobject.h"
#include "libkdeedu/kdeeduplot/kplotaxis.h"

JMoonTool::JMoonTool(QWidget *parent)
	: KDialog( parent )
{
	ksw = (KStars*)parent;
	QFrame *page = new QFrame(this);
	setMainWidget( page );
        setCaption( i18n("Jupiter Moons Tool") );
        setButtons( KDialog::Close );

        QVBoxLayout *vlay = new QVBoxLayout( page );
	vlay->setMargin( 0 );
	vlay->setSpacing( 0 );

	colJp = QColor(Qt::white);
	colIo = QColor(Qt::red);
	colEu = QColor(Qt::yellow);
	colGn = QColor(Qt::cyan);
	colCa = QColor(Qt::green);

	QLabel *labIo = new QLabel( "Io", page );
	QLabel *labEu = new QLabel( "Europa", page );
	QLabel *labGn = new QLabel( "Ganymede", page );
	QLabel *labCa = new QLabel( "Callisto", page );

	labIo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labEu->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labGn->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labCa->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	labIo->setAlignment( Qt::AlignHCenter );
	labEu->setAlignment( Qt::AlignHCenter );
	labGn->setAlignment( Qt::AlignHCenter );
	labCa->setAlignment( Qt::AlignHCenter );

        QPalette p = palette();
        p.setColor( QPalette::Window, Qt::black );
        p.setColor( QPalette::Text, colIo );
	labIo->setPalette( p );
        p.setColor( QPalette::Text, colEu );
	labEu->setPalette( p );
        p.setColor( QPalette::Text, colGn );
	labGn->setPalette( p );
        p.setColor( QPalette::Text, colCa );
	labCa->setPalette( p );
	labIo->setAutoFillBackground( true );
	labEu->setAutoFillBackground( true );
	labGn->setAutoFillBackground( true );
	labCa->setAutoFillBackground( true );

	QGridLayout *glay = new QGridLayout();
	glay->addWidget( labIo, 0, 0 );
	glay->addWidget( labEu, 1, 0 );
	glay->addWidget( labGn, 0, 1 );
	glay->addWidget( labCa, 1, 1 );

	pw = new KStarsPlotWidget( 0.0, 1.0, 0.0, 1.0, page );
	pw->setShowGrid( false );
	pw->setYAxisType0( KStarsPlotWidget::TIME );
	pw->setLimits( -12.0, 12.0, -240.0, 240.0 );
	pw->axis(KPlotWidget::BottomAxis)->setLabel( i18n( "offset from Jupiter (arcmin)" ) );
	pw->axis(KPlotWidget::LeftAxis)->setLabel( i18n( "time since now (days)" ) );
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
	KSSun *ksun = (KSSun*)ksw->data()->skyComposite()->findByName( "Sun" );
	KSPlanet *jup = (KSPlanet*)ksw->data()->skyComposite()->findByName( "Jupiter" );
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
			orbit[i]->addPoint( new QPointF( 0.5*jup->angSize()*jm.x(i), t ) );

		jpath->addPoint( new QPointF( 0.0, t ) );
	}

	for ( unsigned int i=0; i<4; ++i )
		pw->addObject( orbit[i] );

	pw->addObject( jpath );
}

void JMoonTool::keyPressEvent( QKeyEvent *e ) {
	switch ( e->key() ) {
		case Qt::Key_BracketRight:
		{
			double dy = 0.02*pw->dataHeight();
			pw->setLimits( pw->x(), pw->x2(), pw->y()+dy, pw->y2()+dy );
			initPlotObjects();
			pw->update();
			break;
		}
		case Qt::Key_BracketLeft:
		{
			double dy = 0.02*pw->dataHeight();
			pw->setLimits( pw->x(), pw->x2(), pw->y()-dy, pw->y2()-dy );
			initPlotObjects();
			pw->update();
			break;
		}
		case Qt::Key_Plus:
		case Qt::Key_Equal:
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
		case Qt::Key_Minus:
		case Qt::Key_Underscore:
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
		case Qt::Key_Escape:
		{
			close();
			break;
		}

		default: { e->ignore(); break; }
	}
}

#include "jmoontool.moc"
