/***************************************************************************
                          plotobject.cpp - A list of points to be plotted
                             -------------------
    begin                : Sun 18 May 2003
    copyright            : (C) 2003 by Jason Harris
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

#include <qcolor.h>
#include <qpainter.h>
#include <qrect.h>
#include <qstring.h>
#include <kdebug.h>
#include <klocale.h>
#include "plotobject.h"

PlotObject::PlotObject() {
	PlotObject( "", "white", 0 );
}

PlotObject::PlotObject( const QString &n, const QString &c, int t, int s, int p ) {
	//We use the set functions because they may include data validation
	setName( n );
	setColor( c );
	setType( t );
	setSize( s );
	setParam( p );

	pList.setAutoDelete( TRUE );
}

PlotObject::~PlotObject()
{
}

void PlotObject::removePoint( unsigned int index ) {
	if ( index > pList.count() - 1 ) {
		kdWarning() << i18n( "Ignoring attempt to remove non-existent plot object" ) << endl;
		return;
	}

	pList.remove( index );
}

