/***************************************************************************
                          kstarsplotwidget.h - A widget for dat plotting in KStars
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

#ifndef _KSTARSPLOTWIDGET_H_
#define _KSTARSPLOTWIDGET_H_

#include <qwidget.h>
#include <libkdeedu/kdeeduplot/kplotwidget.h>

#define BIGTICKSIZE 10
#define SMALLTICKSIZE 4
#define XPADDING 20
#define YPADDING 20

class QColor;
class QPixmap;

/**@class KStarsPlotWidget
	*@short A plotting widget for KStars tools, derived from the more generic class KPlotWidget.
	*@author Jason Harris
	*@version 1.0
	*Widget for drawing plots.  Features added to KPlotWidget: secondary Axis labels, data types
	*/

class KStarsPlotWidget : public KPlotWidget {
	Q_OBJECT
public:
/**Constructor
	*/
	KStarsPlotWidget( double x1=0.0, double x2=1.0, double y1=0.0, double y2=1.0, QWidget *parent=0, const char* name=0 );

/**Destructor (empty)
	*/
	~KStarsPlotWidget() {}

/**@enum AXIS_TYPE
	*Specifies the data type of the axis.
	*/
	enum AXIS_TYPE { DOUBLE=0, TIME=1, ANGLE=2, UNKNOWN_TYPE };

/**@short Determine the placement of major and minor tickmarks, based on the current Limit settings
	*/
	void updateTickmarks();
	void setLimits( double xb1, double xb2, double yb1, double yb2 );
	void setSecondaryLimits( double xb1, double xb2, double yb1, double yb2 );
	void checkLimits();

	double xb() const { return DataRect2.x(); }
	double xb2() const { return DataRect2.x2(); }
	double yb() const { return DataRect2.y(); }
	double yb2() const { return DataRect2.y2(); }
	double dataWidth2() const { return DataRect2.width(); }
	double dataHeight2() const { return DataRect2.height(); }

	double xScale() const { return XScaleFactor; }
	void setXScale( double s ) { XScaleFactor = s; }
	double yScale() const { return YScaleFactor; }
	void setYScale( double s ) { YScaleFactor = s; }

	/**@return the data type of the x-axis (DOUBLE, TIME or ANGLE)
		*/
	AXIS_TYPE xAxisType() const { return XAxisType; }
	AXIS_TYPE xAxisType0() const { return XAxisType_0; }
	/**@return the data type of the y-axis (DOUBLE, TIME or ANGLE)
		*/
	AXIS_TYPE yAxisType() const { return YAxisType; }
	AXIS_TYPE yAxisType0() const { return YAxisType_0; }

	/**@short set the data type of the x-axis
		*@param xtype the new data type (DOUBLE, TIME, or ANGLE)
		*/
	void setXAxisType( AXIS_TYPE xtype ) { XAxisType = xtype; }
	void setXAxisType0( AXIS_TYPE xtype ) { XAxisType_0 = xtype; }
	/**@short set the data type of the y-axis
		*@param ytype the new data type (DOUBLE, TIME, or ANGLE)
		*/
	void setYAxisType( AXIS_TYPE ytype ) { YAxisType = ytype; }
	void setYAxisType0( AXIS_TYPE ytype ) { YAxisType_0 = ytype; }

	/**@short set the secondary X-axis label
		*@param xlabel a short string describing the data plotted on the x-axis.
		*Set the label to an empty string to omit the axis label.
		*/
	virtual void setXAxisLabel2( QString xlabel ) { XAxisLabel2 = xlabel; }
	/**@short set the secondary Y-axis label
		*@param ylabel a short string describing the data plotted on the y-axis.
		*Set the label to an empty string to omit the axis label.
		*/
	virtual void setYAxisLabel2( QString ylabel ) { YAxisLabel2 = ylabel; }

	/**@returns the number of pixels to the right of the plot area.
		*Padding values are set to -1 by default; if unchanged, this function will try to guess
		*a good value, based on whether ticklabels and/or axis labels are to be drawn.
		*/
	int rightPadding()  const;
	/**@returns the number of pixels above the plot area.
		*Padding values are set to -1 by default; if unchanged, this function will try to guess
		*a good value, based on whether ticklabels and/or axis labels are to be drawn.
		*/
	int topPadding()    const;

protected:
	void drawBox( QPainter *p );

	double dXtick2, dYtick2;
	int nmajX2, nminX2, nmajY2, nminY2;
	AXIS_TYPE XAxisType, YAxisType, XAxisType_0, YAxisType_0;
	double XScaleFactor, YScaleFactor;
	DRect DataRect2;

	QString XAxisLabel2, YAxisLabel2;
};

#endif
