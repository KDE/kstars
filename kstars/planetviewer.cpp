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

PlanetViewer::PlanetViewer(QWidget *parent, const char *name)
 : KDialogBase( KDialogBase::Plain, i18n("Solar System Viewer"), Ok, Ok, parent )
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );

	pw = new PlotWidget( -46.0, 46.0, -46.0, 46.0, page );
	pw->setFixedSize( 500, 500 );

	vlay->addWidget( pw );
}

PlanetViewer::~PlanetViewer()
{
}

void PlanetViewer::paintEvent( QPaintEvent *e ) {
	pw->update();
}

#include "planetviewer.moc"
