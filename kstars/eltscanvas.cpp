/***************************************************************************
                          eltscanvas.cpp  -  description
                             -------------------
    begin                : lun dic 23 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "elts.h"
#include "eltscanvas.h"

#include "skypoint.h"
#include "geolocation.h"
#include <qvariant.h>
#include <dmsbox.h>
#include <qdatetimeedit.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>
#include "ksutils.h"
#include "kstars.h"

eltsCanvas::eltsCanvas(QWidget *parent, const char *name) : QWidget(parent,name) {
	elts *el = (elts *)topLevelWidget();
	KStars *ks = (KStars *)el->parent();

	setBackgroundColor( QColor( ks->options()->colorScheme()->colorNamed( "SkyColor" ) ) );
	//setBackgroundMode( QWidget::NoBackground );
	initVars();
}

eltsCanvas::~eltsCanvas(){

}

void eltsCanvas::initVars(void) {

	xnticks = 24, ynticks = 18;
	xmticks = 6, ymticks = 6;

	// xmin, xmax = Minimum and maximum X values = 360   24x60/4   (4 minutes/pix)
	// ymin, ymax = Minimum and maximum Y values = 180            (1 degree/pix)
	xmin = 0, xmax = 360, ymin = 0, ymax = 180;

	xwidth = xmax-xmin;
	ywidth = ymax-ymin;

	// Value of the interval tick for x and y

	xticksep = (int)(xwidth/xnticks);
	xmticksep = (int)(xwidth/xmticks);
	yticksep = (int)(ywidth/ynticks);
	ymticksep = (int)(ywidth/ymticks);

	yticksize =   ywidth/50;
	xticksize =   xwidth/50;
	xmticksize =  xwidth /20;
}

void eltsCanvas::paintEvent( QPaintEvent * ) {
	elts *el = (elts *)topLevelWidget();

	QPainter paint;
	paint.begin(this);
	paint.setPen( white );
	paint.setFont( el->font() );
	drawGrid( &paint );
	paint.setPen( red );
	if (el->newsource)
		drawSource(&paint);

	paint.end();
}

int eltsCanvas::UtMinutes(void) {

	elts *el = (elts *)topLevelWidget();
	dms longit;

	longit = el->getLongitude();

	QTime lst_at_ut0 = KSUtils::UTtoLST( el->getQDate(), &longit);
	return lst_at_ut0.hour()*60+lst_at_ut0.minute();
}

void eltsCanvas::drawGrid( QPainter * pcanvas ) {

	int lst_minutes = UtMinutes();
	pcanvas->translate(40, 40);
	
	int xt = 0, ix = 1;
	int yt = 0, iy = 1;
	
	//Ground indicator
	pcanvas->setPen( "#002200" );
	pcanvas->fillRect( xmin, ymin+ywidth/2, xmax, ywidth/2, QBrush( "#002200" ) );
	
	// X grid and labels
	for (ix=1;ix<xmticks;ix++) {
		xt = xmin+xmticksep*ix;
		pcanvas->setPen( "#555555" );
		pcanvas->drawLine(xt,ymin, xt,ymax);
		
		int h = int(xt/15);
		QString s;
		if ( h<10 ) {
			s = QString("0%1:00").arg(h);
		} else {
			s = QString("%1:00").arg(h);
		}
		
		pcanvas->setPen( "white" );
		pcanvas->drawText(xt-2*xticksize-2, ymax+xmticksize, s);
	}

	// Y grid and labels:
	for (iy=1;iy<ymticks;iy++) {
		yt = ymin+ymticksep*iy;
		pcanvas->setPen( "#555555" );
		pcanvas->drawLine(xmin,yt, xmax,yt);
		pcanvas->setPen( "white" );
		int d = int(ymax - yt)-90;
		int dx = 3*xticksize;
		if ( d == 0 ) dx = 2*xticksize;
		if ( d <  0 ) dx = 4*xticksize;
		pcanvas->drawText(xmin-dx, yt+xticksize, QString("%1").arg(d) );
	}

	// X minor ticks:
	for (ix=1;ix<xnticks;ix++) {
		xt = xmin+xticksep*ix;
		pcanvas->drawLine(xt,ymin, xt,ymin+xticksize);
		pcanvas->drawLine(xt,ymax, xt,ymax-xticksize);
	}
	
	// Y minor ticks:
	for (iy=1;iy<ynticks;iy++) {
		yt = ymin+yticksep*iy;
		pcanvas->drawLine(xmin,yt, xmin+yticksize,yt);
		pcanvas->drawLine(xmax,yt, xmax-yticksize,yt);
	}

	pcanvas->drawRect(xmin,ymin,xwidth+1,ywidth+1); //need +1 to avoid tick sticking out beyond box
}

dms eltsCanvas::DateTimetoLST (QDateTime date, int ut, dms longitude)
{

	int uth = ut/60;
	int utm = ut%60;

	QDateTime utdt = QDateTime( date.date(), QTime(uth, utm, 0) );
	QTime lst = KSUtils::UTtoLST( utdt, &longitude);
	dms LSTh = QTimeToDMS (lst);

	return LSTh;
}

// Tomar esto de modCalcAzel::QTimeToDMS haciendolo static ?
//
dms eltsCanvas::QTimeToDMS(QTime qtime) {

	dms tt;
	tt.setH(qtime.hour(), qtime.minute(), qtime.second());
	tt.reduce();

	return tt;
}

int eltsCanvas::getEl (int ut) {
	elts *el = (elts *)topLevelWidget();

	SkyPoint sp = el->appCoords();
	//dms lat;

	dms lat = el->getLatitude();

	dms LSTh = DateTimetoLST (el->getQDate(), ut , el->getLongitude() );
	sp.EquatorialToHorizontal( &LSTh, &lat );

	int elm = sp.alt()->degree()*60 + sp.alt()->arcmin();
	return elm;
}

void eltsCanvas::drawSource(QPainter * pcanvas) {

	int utm = 0, elevation = 0, utmPrev = 0, elevationPrev = 0;

	elevationPrev = getEl(utmPrev);

	for (int i=1;i<=24*4;i++) {
		utm = i*15;
		elevation = getEl(utm);
		//if(elevationPrev >= 0 && elevation >= 0) 
			pcanvas->drawLine(utmPrev/4,(5400-elevationPrev)/60, utm/4,(5400-elevation)/60);
		//else if (elevationPrev <0 && elevation > 0) 
		//	pcanvas->drawLine(Interpol(utmPrev,utm,
		//		elevationPrev,elevation)/4,0,utm/4,(5400-elevation)/60);
		//else if (elevationPrev >0 && elevation < 0)
		//	pcanvas->drawLine(utmPrev/4,(5400-elevationPrev)/60,
		//		Interpol(utmPrev,utm,elevationPrev,elevation)/4,0);
		utmPrev=utm;
		elevationPrev=elevation;
	}

}

int eltsCanvas::Interpol(int x1,int x2,int y1,int y2) {

	return (x1 - y1*(x2-x1)/(y2-y1));

}

// Por hacer:
//  - Pasar UT al eje de abajo
//  - Slot para añadir fuentes nuevas sin borrar las viejas.
//  - Guardar las fuentes en una lista? 
