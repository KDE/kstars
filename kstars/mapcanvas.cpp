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
#include <qcolor.h>
#include "mapcanvas.h"
#include "locationdialog.h"
#include "kstars.h"

MapCanvas::MapCanvas(QWidget *parent, const char *name ) : QWidget(parent,name) {
	BGColor = "#33A";
	setBackgroundColor( QColor( BGColor ) );
	setBackgroundMode( QWidget::NoBackground );
	Canvas = new QPixmap();
	bgImage = new QPixmap();
	LocationDialog *ld = (LocationDialog *)topLevelWidget();
	KStars *ks = (KStars *)ld->parent();
	QString bgFile = ks->GetData()->stdDirs->findResource( "data", "kstars/geomap.png" );
	bgImage->load( bgFile, "PNG" );
}

MapCanvas::~MapCanvas(){
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
  KStars *ks = (KStars *)ld->parent();

	//Determine Lat/Long corresponding to event press
	int lng = ( e->x() - origin.x() );
	int lat  = ( origin.y() - e->y() );

	//find all cities within 3 degrees of event; list them in GeoBox
	ld->GeoBox->clear();
	for ( unsigned int i=0; i<ks->GetData()->geoList.count(); ++i ) {
		if ( ( abs(	lng - int( ks->GetData()->geoList.at(i)->lng().getD() ) ) < 3 ) &&
				 ( abs( lat - int( ks->GetData()->geoList.at(i)->lat().getD() ) ) < 3 ) ) {
	    QString sc( ks->GetData()->geoList.at(i)->translatedName() );
			sc.append( ", " );
			if ( !ks->GetData()->geoList.at(i)->province().isEmpty() ) {
	      sc.append( ks->GetData()->geoList.at(i)->translatedProvince() );
	      sc.append( ", " );
			}
      sc.append( ks->GetData()->geoList.at(i)->translatedCountry() );

      ld->GeoBox->insertItem( sc );
      ld->GeoID[ld->GeoBox->count() - 1] = i;
		}
	}
}

void MapCanvas::paintEvent( QPaintEvent *e ) {
	QPainter pcanvas;
	LocationDialog *ld = (LocationDialog *)topLevelWidget();
  KStars *ks = (KStars *)ld->parent();

	//prepare the canvas
	pcanvas.begin( Canvas );
//	pcanvas.fillRect( 0, 0, width(), height(), QBrush( QColor( BGColor ) ) );
	pcanvas.drawPixmap( 0, 0, *bgImage );
	pcanvas.setBrush( white );
	pcanvas.setPen( white );

	//Draw cities
	QPoint o;

	for ( unsigned int i=0; i < ks->GetData()->geoList.count(); ++i ) {
		o.setX( int( ks->GetData()->geoList.at(i)->lng().getD() + origin.x() ) );
		o.setY( height() - int( ks->GetData()->geoList.at(i)->lat().getD() + origin.y() ) );

		if ( o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
			pcanvas.drawPoint( o.x(), o.y() );
		}
	}

	if (ld->newCity <ks->GetData()->geoList.count()) {
		o.setX( int( ks->GetData()->geoList.at(ld->newCity)->lng().getD() + origin.x() ) );
		o.setY( height() - int( ks->GetData()->geoList.at(ld->newCity)->lat().getD() + origin.y() ) );

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
