/***************************************************************************
                          mapcanvas.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Apr 10 2001
    copyright            : (C) 2001 by Jason Harris
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

#include <stdlib.h>
#include <kstandarddirs.h>
#include <qpainter.h>
#include <qpixmap.h>

#include "mapcanvas.h"
#include "locationdialog.h"
#include "kstars.h"
#include "kstarsdata.h"

MapCanvas::MapCanvas(QWidget *parent, const char *name ) : QWidget(parent,name) {
	BGColor = "#33A";
	setBackgroundColor( QColor( BGColor ) );
	setBackgroundMode( QWidget::NoBackground );
	Canvas = new QPixmap();
	bgImage = new QPixmap();
	LocationDialog *ld = (LocationDialog *)topLevelWidget();
	KStars *ks = (KStars *)ld->parent();
	QString bgFile = ks->data()->stdDirs->findResource( "data", "kstars/geomap.png" );
	bgImage->load( bgFile, "PNG" );
}

MapCanvas::~MapCanvas(){
	delete bgImage;
	delete Canvas;
}

void MapCanvas::setGeometry( int x, int y, int w, int h ) {
	QWidget::setGeometry( x, y, w, h );
	Canvas->resize( w, h );
	origin.setX( w/2 );
	origin.setY( h/2 );
}

void MapCanvas::setGeometry( const QRect &r ) {
	QWidget::setGeometry( r );
	Canvas->resize( r.width(), r.height() );
	origin.setX( r.width()/2 );
	origin.setY( r.height()/2 );
}

void MapCanvas::mousePressEvent( QMouseEvent *e ) {
	LocationDialog *ld = (LocationDialog *)topLevelWidget();

	//Determine Lat/Long corresponding to event press
	int lng = ( e->x() - origin.x() );
	int lat  = ( origin.y() - e->y() );

	ld->findCitiesNear( lng, lat );
}

void MapCanvas::paintEvent( QPaintEvent * ) {
	QPainter pcanvas;
	LocationDialog *ld = (LocationDialog *)topLevelWidget();
  KStars *ks = (KStars *)ld->parent();

	//prepare the canvas
	pcanvas.begin( Canvas );
//	pcanvas.fillRect( 0, 0, width(), height(), QBrush( QColor( BGColor ) ) );
	pcanvas.drawPixmap( 0, 0, *bgImage );
//	pcanvas.setBrush( white );
	pcanvas.setPen( QPen( QColor( "SlateGrey" ) ) );

	//Draw cities
	QPoint o;

	for ( GeoLocation *g=ks->data()->geoList.first(); g; g = ks->data()->geoList.next() ) {
		o.setX( int( g->lng()->Degrees() + origin.x() ) );
		o.setY( height() - int( g->lat()->Degrees() + origin.y() ) );

		if ( o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
			pcanvas.drawPoint( o.x(), o.y() );
		}
	}

  //redraw the cities that appear in the filtered list, with a white pen
	//If the list has not been filtered, skip the redraw.
	if ( ld->filteredList()->count() ) {
		pcanvas.setPen( white );
		for ( GeoLocation *g=ld->filteredList()->first(); g; g = ld->filteredList()->next() ) {
			o.setX( int( g->lng()->Degrees() + origin.x() ) );
			o.setY( height() - int( g->lat()->Degrees() + origin.y() ) );

			if ( o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
				pcanvas.drawPoint( o.x(), o.y() );
			}
		}
	}

	GeoLocation *g = ld->selectedCity();
	if ( g ) {
		o.setX( int( g->lng()->Degrees() + origin.x() ) );
		o.setY( height() - int( g->lat()->Degrees() + origin.y() ) );

		pcanvas.setPen( red );
		pcanvas.setBrush( red );
		pcanvas.drawEllipse( o.x()-3, o.y()-3, 6, 6 );
		pcanvas.moveTo( o.x()-16, o.y() );
		pcanvas.lineTo( o.x()-8, o.y() );
		pcanvas.moveTo( o.x()+8, o.y() );
		pcanvas.lineTo( o.x()+16, o.y() );
		pcanvas.moveTo( o.x(), o.y()-16 );
		pcanvas.lineTo( o.x(), o.y()-8 );
		pcanvas.moveTo( o.x(), o.y()+8 );
		pcanvas.lineTo( o.x(), o.y()+16 );
		pcanvas.setPen( white );
		pcanvas.setBrush( white );
  }

	pcanvas.end();
	bitBlt( this, 0, 0, Canvas );
}
#include "mapcanvas.moc"
