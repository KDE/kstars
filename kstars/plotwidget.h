/***************************************************************************
                          plotwidget.h - A widget for plotting in KStars
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

#ifndef _PLOTWIDGET_H_
#define _PLOTWIDGET_H_

#include <qwidget.h>
#include "plotobject.h"

#define BIGTICKSIZE 10
#define SMALLTICKSIZE 4
#define XPADDING 40
#define YPADDING 40

class QColor;

/**@class PlotWidget
	*@short Genric plotting widget for KStars tools.
	*@author Jason Harris
	*@version 1.0
	*Widget for drawing plots.  Includes adjustable axes with tickmarks and labels, and a
	*list of PlotObjects to be drawn.
  */

class PlotWidget : public QWidget {
	Q_OBJECT
public:
/**Constructor
	*/
	PlotWidget( double x1=0.0, double x2=1.0, double y1=0.0, double y2=1.0, QWidget *parent=0, const char* name=0 );

/**Destructor (empty)
	*/
	~PlotWidget() {}

/**@short Determine the placement of major and minor tickmarks, based on the current Limit settings
	*/
	void initTickmarks();
	void setLimits( double xa1, double xa2, double ya1, double ya2 );
	void setSecondaryLimits( double xb1, double xb2, double yb1, double yb2 );

	double x1() const { return XA1; }
	double x2() const { return XA2; }
	double y1() const { return YA1; }
	double y2() const { return YA2; }

	double xb1() const { return XB1; }
	double xb2() const { return XB2; }
	double yb1() const { return YB1; }
	double yb2() const { return YB2; }

	void addObject( PlotObject *o ) { ObjectList.append( o ); }
	void clearObjectList() { ObjectList.clear(); update(); }

	QColor bgColor() const { return cBackground; }
	void setBGColor( const QColor &bg ) { cBackground = bg; setBackgroundColor( bg ); }
	QColor fgColor() const { return cForeground; }
	void setFGColor( const QColor &fg ) { cForeground = fg; }
	QColor gridColor() const { return cGrid; }
	void setGridColor( const QColor &gc ) { cGrid = gc; }

private:
	void paintEvent( QPaintEvent *e );
	void resizeEvent( QResizeEvent *e );
/**@short draws all of the objects onto the widget.  Internal use only; one should simply call update()
	*to draw the widget with axes and all objects.
	*/
	void drawObjects( QPainter *p );
	void drawBox( QPainter *p, bool showAxes=true, bool showTickMarks=true, bool showTickLabels=true, bool showGrid=false );

	double dmod( double a, double b );

	int nmajX1, nmajY1, nminX1, nminY1;
	int nmajX2, nmajY2, nminX2, nminY2;
	double dXtick1, dYtick1, dXtick2, dYtick2;

	int dXS, XS1, XS2, dYS, YS1, YS2;
	double dXA, XA1, XA2, dYA, YA1, YA2;
	double dXB, XB1, XB2, dYB, YB1, YB2;
	QPtrList<PlotObject> ObjectList;
	QColor cBackground, cForeground, cGrid;
};

#endif
