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

	xnticks = 24, ynticks = 9;
	xmticks = 6, ymticks = 5;

	// xmin, xmax = Minimum and maximum X values = 0 24x60 (mnutes)
	// ymin, ymax = Minimum and maximum Y values = 0 90x60  (arcmin)
	xmin = 0, xmax = 1440, ymin = 0, ymax = 5400;

	xwidth = xmax-xmin;
	ywidth = ymax-ymin;

	// Value of the interval tick for x and y

	xticksep = (int)(xwidth/xnticks);
	xmticksep = (int)(xwidth/xmticks);
	yticksep = (int)(ywidth/ynticks);
	ymticksep = 20*60;

	yticksize =   ywidth/200;
	xticksize =   xwidth/20;
	xmticksize =  xwidth /10;
}

void eltsCanvas::paintEvent( QPaintEvent * ) {
	elts *el = (elts *)topLevelWidget();

	QPainter paint;
	paint.begin(this);
	paint.setPen( white );
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

	pcanvas->setWindow(-100,-100,1600,1600);
	// pcanvas->setWindow(xmin-xwidth/10,ymin-ywidth/10,
	//		  xwidth*1.1,ywidth*1.1);
	pcanvas->translate(0,1350);
	pcanvas->scale(1,-0.25);
	//pcanvas->drawRect(0,0,1440,5400);
	pcanvas->drawRect(xmin,ymin,xwidth,ywidth);

	int xt = 0, ix = 1;
	int yt = 0, iy = 1;
	
	// Lower X minor ticks:
	for (ix=1;ix<xnticks;ix++) {
		xt = xmin+xticksep*ix;
		pcanvas->drawLine(xt,ymin, xt,ymin+xticksize);
	}
	
	// Major ticks and labels
	for (ix=1;ix<xmticks;ix++) {
		xt = xmin+xmticksep*ix;
		pcanvas->drawLine(xt,ymin, xt,ymax);
		pcanvas->drawText(xt,ymin+xmticksize,"60");
		pcanvas->drawText(xt,ymin+xmticksize,QString("%1").arg( int(xt/60)));
	}

	// Upper X ticks and labels:
	// Minor ticks

	for (int i = 0; i<2 ; i++) {
		pcanvas->save();
		pcanvas->translate(i*1440-lst_minutes, 0);
		for (ix=1;ix<xnticks;ix++) {
			xt = xmin+xticksep*ix;
//			if( xt > xmin && xt < xmax )
				pcanvas->drawLine(xt,ymax, xt,ymax-xticksize);
		}
		
		// Major ticks and labels

		for (ix=0;ix<xmticks;ix++) {
			xt = xmin+xmticksep*ix;
//			if( xt > xmin && xt < xmax ) {
				pcanvas->drawLine(xt,ymax, xt,ymax-xmticksize);
				pcanvas->drawText(xt,ymax+ xmticksize,
						QString("%1").arg( int(xt/60)));
//			}
		}

		pcanvas->restore();
	}

	// Right Y ticks and labels:
	for (iy=1;iy<ynticks;iy++) {
		yt = ymin+yticksep*iy;
		pcanvas->drawLine(xmin,yt, xmin+yticksize,yt);
	}

	// Right Y grid and labels:
	for (iy=1;iy<ymticks;iy++) {
		yt = ymin+ymticksep*iy;
		pcanvas->drawLine(xmin,yt, xmax,yt);
		pcanvas->drawText(xmin-xmticksize,yt, QString("%1").arg( int(yt/60)));
	}

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
		if(elevationPrev >= 0 && elevation >= 0) 
			pcanvas->drawLine(utmPrev,elevationPrev, utm,elevation);		else if (elevationPrev <0 && elevation > 0) 
			pcanvas->drawLine(Interpol(utmPrev,utm,
				elevationPrev,elevation),0,utm,elevation);
		else if (elevationPrev >0 && elevation < 0)
			pcanvas->drawLine(utmPrev,elevationPrev,
				Interpol(utmPrev,utm,elevationPrev,elevation),0);
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
