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

#include "ksutils.h"
#include "kstars.h"
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

#define DX  3
#define DY 30

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

	// xmin, xmax = Minimum and maximum X values = 480   24x60/3   (3 minutes/pix)
	// ymin, ymax = Minimum and maximum Y values = 360            (0.5 degree/pix)
	xmin = 0, xmax = 480, ymin = 0, ymax = 360;

	xwidth = xmax-xmin;
	ywidth = ymax-ymin;

	// Value of the interval tick for x and y

	xticksep = (int)(xwidth/xnticks);
	xmticksep = (int)(xwidth/xmticks);
	yticksep = (int)(ywidth/ynticks);
	ymticksep = (int)(ywidth/ymticks);

	yticksize =   ywidth/50;
	xticksize =   xwidth/50;
	xmticksize =  xwidth /25;
	xMticksize =  xwidth /15;
}

void eltsCanvas::paintEvent( QPaintEvent * ) {
	QPainter paint;
	
	erase();
	paint.begin(this);
	
	drawGrid( &paint );
	drawCurves( &paint );

	paint.end();
}

void eltsCanvas::drawGrid( QPainter * pcanvas ) {
	
	int lst_minutes = UtMinutes();
	pcanvas->setPen( white );
	pcanvas->setFont( topLevelWidget()->font() );
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
		
		int h = int(xt*24/xmax);
		QString s;
		if ( h<10 ) {
			s = QString("0%1:00").arg(h);
		} else {
			s = QString("%1:00").arg(h);
		}
		
		pcanvas->setPen( "white" );
//		pcanvas->drawText(xt-2*xticksize-2, ymax+xmticksize, s);
		pcanvas->drawLine(xt,ymax, xt,ymax-xmticksize);
		pcanvas->drawText(xt-2*xticksize+2, ymax+xmticksize, s);
	}
	QString xLowLabel = i18n("Local Time");
	pcanvas->drawText((xmax-xmin)/2-3*xticksize,ymax+xMticksize,xLowLabel);

	// Y grid and labels:
	for (iy=1;iy<ymticks;iy++) {
		yt = ymin+ymticksep*iy;
		pcanvas->setPen( "#555555" );
		pcanvas->drawLine(xmin,yt, xmax,yt);
		pcanvas->setPen( "white" );
		int d = int((ymax - yt)/2)-90;
		int dx = 5*xticksize;
//		int dx = 4*xticksize;
//		if ( d == 0 ) dx = 2*xticksize;
//		if ( d <  0 ) dx = 4*xticksize;
//		pcanvas->drawText(xmin-dx, yt+xmticksize, QString("%1").arg(d) );
		pcanvas->drawText(xmin-dx, yt+xticksize-5, QString("%1").arg(d,8) );
	}
	QString yLeftLabel = i18n("Altitude");
	pcanvas->save();
	pcanvas->rotate(-90);
	pcanvas->drawText((ymin-ymax)/2-3*yticksize,xmin-xwidth/20,yLeftLabel);
	pcanvas->restore();

	// X minor ticks for the lower axis:
	
	for (ix=1;ix<xnticks;ix++) {
		xt = xmin+xticksep*ix;
//		pcanvas->drawLine(xt,ymin, xt,ymin+xticksize);
		pcanvas->drawLine(xt,ymax, xt,ymax-xticksize);
	}
	// X minor ticks for the upper axis
	for (int i = 0; i<2 ; i++) {
		pcanvas->save();
		pcanvas->translate(i*xmax-lst_minutes/3, 0);
		int xtranslation = i*xmax-lst_minutes/3;
		for (ix=1;ix<xnticks;ix++) {
			xt = xmin+xticksep*ix;
			pcanvas->setPen( "white" );
			if (xt+xtranslation < xmax && xt+xtranslation > xmin)
				pcanvas->drawLine(xt,ymin, xt,ymin+xticksize);
                }
                
                // Major ticks and labels for the upper axis

		for (ix=0;ix<xmticks;ix++) {
			xt = xmin+xmticksep*ix;
			
			int h = int(xt*24/xmax);
			QString s;
			if ( h<10 ) {
				s = QString("0%1:00").arg(h);
			} else {
				s = QString("%1:00").arg(h);
			}
			pcanvas->setPen( "white" );
			if (xt+xtranslation < xmax && xt+xtranslation > xmin) {
//			If we want larger ticks each four hours uncomment the following
//			line:
				pcanvas->drawLine(xt,ymin, xt,ymin+xmticksize);
				pcanvas->drawText(xt-2*xticksize+2, ymin-xmticksize/2, s);
			}
                }

                pcanvas->restore();
        }
	QString xUpperLabel = i18n("Local Sidereal Time");
	pcanvas->drawText((xmax-xmin)/2-4*xticksize,ymin-xwidth/20,xUpperLabel);
	
	// Y minor ticks:
	for (iy=1;iy<ynticks;iy++) {
		yt = ymin+yticksep*iy;
		pcanvas->drawLine(xmin,yt, xmin+yticksize,yt);
		pcanvas->drawLine(xmax,yt, xmax-yticksize,yt);
	}

	pcanvas->drawRect(xmin,ymin,xwidth+1,ywidth+1); //need +1 to avoid tick sticking out beyond box
}

void eltsCanvas::drawCurves(QPainter * pcanvas) {
	elts *el = (elts *)topLevelWidget();
	int utm = 0, elevation = 0, utmPrev = 0, elevationPrev = 0;
	
	pcanvas->setPen( red );

	for ( SkyPoint *p = el->pList.first(); p; p = el->pList.next() ) {
		utmPrev = 0;
		elevationPrev = getEl(p, utmPrev);
		pcanvas->moveTo( utmPrev, elevationPrev );
		
		for (int i=1;i<=24*4;i++) {
			utm = i*15;
			elevation = getEl(p, utm);
			//if(elevationPrev >= 0 && elevation >= 0) 
				pcanvas->drawLine(utmPrev/DX,(5400-elevationPrev)/DY, utm/DX,(5400-elevation)/DY);
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
}

//Returns elevation above horizon, in minutes of arc.
int eltsCanvas::getEl (SkyPoint *sp, int ut) {
	elts *el = (elts *)topLevelWidget();

	//SkyPoint sp = el->appCoords();
	//dms lat;

	dms lat = el->getLatitude();

	dms LSTh = DateTimetoLST (el->getQDate(), ut , el->getLongitude() );
	sp->EquatorialToHorizontal( &LSTh, &lat );

	int elm = sp->alt()->degree()*60 + sp->alt()->arcmin();
	return elm;
}

int eltsCanvas::UtMinutes(void) {

	elts *el = (elts *)topLevelWidget();
	dms longit;

	longit = el->getLongitude();

	QTime lst_at_ut0 = KSUtils::UTtoLST( el->getQDate(), &longit);
	return lst_at_ut0.hour()*60+lst_at_ut0.minute();
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

dms eltsCanvas::QTimeToDMS(QTime qtime) {

	dms tt;
	tt.setH(qtime.hour(), qtime.minute(), qtime.second());
	tt.reduce();

	return tt;
}

int eltsCanvas::Interpol(int x1,int x2,int y1,int y2) {

	return (x1 - y1*(x2-x1)/(y2-y1));

}
