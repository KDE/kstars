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
	PlotObject *po = new PlotObject( "Mercury", "white", PlotObject::CURVE, 1 );
	po->setParam( PlotObject::SOLID );

	for ( double t=0.0; t<=360.0; t+=20.0 ) {
		dms d( t );
		double sd, cd;
		d.SinCos( sd, cd );
		po->addPoint( new DPoint( 10.0*cd, 10.0*sd ) );
	}

	pw->addObject( po );

	vlay->addWidget( pw );
	resize( 500, 500 );
}

PlanetViewer::~PlanetViewer()
{
}

void PlanetViewer::paintEvent( QPaintEvent *e ) {
	pw->update();
}

#include "planetviewer.moc"
