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

void PlotObject::draw( QPainter &p ) {
	QRect r = p.window();
	int s = size();

	//Draw the object according to its type
	switch ( type() ) {
		case POINTS:
		{
			p.setPen( QColor( color() ) );
			p.setBrush( QColor( color() ) );
			for ( QPoint *o = pList.first(); o; o = pList.next() ) {
				if ( r.contains( *o ) ) {
					int x1 = o->x() - s/2;
					int y1 = o->y() - s/2;

					switch ( param() ) {
						case DOT:
							p.drawPoint( *o );
							break;
						case SQUARE:
							p.drawRect( x1, y1, s, s );
							break;
						case CIRCLE:
							p.drawEllipse( x1, y1, s, s );
							break;
						case LETTER:
						{
							QString letter = name().left(1);
							p.drawText( o->x(), o->y(), letter );
							break;
						}
						default:
							break;
					}
				}
			}
			break;
		}

		case CURVE:
		{
			p.setPen( QPen( QColor( color() ), size(), (Qt::PenStyle)param() ) );
			QPoint *o = pList.first();
			p.moveTo( *o );
			for ( o = pList.next(); o; o = pList.next() ) {
				p.lineTo( *o );
			}
			break;
		}

		case LABEL:
		{
			p.setPen( QColor( color() ) );
			QPoint *o = pList.first();
			p.drawText( *o, name() );
			break;
		}

		default:
			break;
	}
}
