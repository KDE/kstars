/***************************************************************************
                          eltscanvas.h  -  description
                             -------------------
    begin                : mon dic 23 2002
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

#ifndef ELTSCANVAS_H
#define ELTSCANVAS_H

#include <qwidget.h>
#include <qpainter.h>

class QDateTime;
class dms;
class QPainter;
class KStars;


/**
  *@author Pablo de Vicente
  */

class eltsCanvas : public QWidget  {
	Q_OBJECT
public: 
	eltsCanvas(QWidget *parent=0, const char *name=0);
	~eltsCanvas();

	void drawAxis(void);
	
private:
	void initVars(void);
	void paintEvent( QPaintEvent * );
	int UtMinutes(void);
	void drawGrid( QPainter * pcanvas );
	dms DateTimetoLST (QDateTime date, int ut, dms longitude);
	dms QTimeToDMS(QTime qtime);
	int getEl (int ut);
	void drawSource(QPainter * pcanvas);
	int Interpol(int x1,int x2,int y1,int y2);

	// xnticks, ynticks = number of minor ticks of X and Y axis
	// xmticks, ymticks = number of major ticks of X and Y axis
	int xnticks, xmticks, ynticks, ymticks;

	// xmin, xmax = Minimum and maximum X values = 0 24x60 (mnutes)
	// ymin, ymax = Minimum and maximum Y values = 0 90x60  (arcmin)
	int xmin, xmax, ymin, ymax;
	
	int xwidth, ywidth;
	
	// Value of the interval tick for x and y

	int xticksep, xmticksep, yticksep, ymticksep;
	int yticksize, xticksize, xmticksize;
	KStars *ks;

};


#endif
