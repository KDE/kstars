/***************************************************************************
                          eltscanvas.h  -  Widget for drawing altitude vs. time curves
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
class SkyPoint;

/**@class eltsCanvas
	*@short The widget for displaying Altitude vs. Time curves
	*@author Pablo de Vicente
	*@version 1.0
	*eltsCanvas is the main display widget for the Altitude vs. Time tool.
	*The main draw routines are drawGrid(), which draws the X and Y axes and labels,
	*and drawCurves(), which draws the altitude curves of the objects stored in elts.
	*
	*This class will soon be generalized to a multipurpose plotting widget; all
	*elts-specific code will be moved to elts.  We will use the generalized plot
	*widget also in the Jupiter Moons plotter, and in the solar system viewer.
	*/

class eltsCanvas : public QWidget  {
	Q_OBJECT
public: 
/**Constructor
	*/
	eltsCanvas(QWidget *parent=0, const char *name=0);
	
/**Destructor (empty)
	*/
	~eltsCanvas();

private:
/**@short Initialize variables controlling the number and position of tickmarks.
	*/
	void initVars(void);
	
/**@Short draw the plot.
	*/
	void paintEvent( QPaintEvent * );
	
/**@short draw the X and Y axes, with tickmarks and labels.  Also draw gridlines.
	*@param pcanvas the QPainter on which to draw the axes and grid lines.
	*/
	void drawGrid( QPainter * pcanvas );
	
/**@short draw the altitude curves of objects stored in elts.
	*@param pcanvas the QPainter on which to draw the altitude curves.
	*@warning the elts-specific stuff should be moved there; this will be a generic curve-drawing routine.
	*/
	void drawCurves(QPainter * pcanvas);
	
/**@short determine the altitude of the given skypoint at the given time, expressed in arcminutes
	*@param p the skypoint whose altitude is sought
	*@param ut the Universal time for the altitude calculation (expressed in minutes)
	*@warning this function should move to elts
	*/
	int getAlt(SkyPoint *p, int ut);

/**@short determine the LST at UT=0h for the date stored in elts. 
	*@return The LST at UT=0h, expressed in minutes since 00:00 LST
	*@warning This function should move to elts 
	*/
	int LSTMinutes(void);
	
	// xnticks, ynticks = number of minor ticks of X and Y axis
	// xmticks, ymticks = number of major ticks of X and Y axis
	int xnticks, xmticks, ynticks, ymticks;

	// xmin, xmax = Minimum and maximum X values = 0 24x60 (mnutes)
	// ymin, ymax = Minimum and maximum Y values = 0 90x60  (arcmin)
	int xmin, xmax, ymin, ymax;
	
	int xwidth, ywidth;
	
	// Value of the interval tick for x and y

	int xticksep, xmticksep, yticksep, ymticksep;
	int yticksize, xticksize, xmticksize, xMticksize;
	KStars *ks;

};


#endif
