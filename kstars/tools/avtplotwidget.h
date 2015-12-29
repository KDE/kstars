/***************************************************************************
                          avtplotwidget.h  -  description
                             -------------------
    begin                : Sat Nov 10 2007
    copyright            : (C) 2007 by Jason Harris
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

#ifndef AVTPLOTWIDGET_H_
#define AVTPLOTWIDGET_H_

#include <QPoint>

#include <kplotwidget.h>

/** @class AVTPlotWidget
    *@short An extension of the KPlotWidget for the AltVsTime tool.
    *The biggest difference is that in addition to the plot objects, it
    *draws the "ground" below Alt=0 and draws the sky light blue for day
    *times, and black for night times.  The transition between day and
    *night is drawn with a gradient, and the position follows the actual
    *sunrise/sunset times of the given date/location.
    *Also, this plot widget provides two time axes (local time along the
    *bottom, and local sideral time along the top).
    *Finally, it provides user interaction: on mouse click, it draws
    *crosshairs at the mouse position with labels for the time and altitude.
    *@version 1.0
    *@author Jason Harris
    */
class AVTPlotWidget : public KPlotWidget
{
    Q_OBJECT
public:
    /**Constructor
        */
    explicit AVTPlotWidget( QWidget *parent=0 );

    /**Set the fractional positions of the Sunrise and Sunset positions,
        *in units where last midnight was 0.0, and next midnight is 1.0.
        *i.e., if Sunrise is at 06:00, then we set it as 0.25 in this
        *function.  Likewise, if Sunset is at 18:00, then we set it as
        *0.75 in this function.
        *@param sr the fractional position of Sunrise
        *@param ss the fractional position of Sunset
        */
    void setSunRiseSetTimes( double sr, double ss );

    void setDawnDuskTimes( double da, double du );

    void setMinMaxSunAlt( double min, double max );

protected:
    /**Handle mouse move events.  If the mouse button is down,
        *draw crosshair lines centered at the cursor position.  This
        *allows the user to pinpoint specific position sin the plot.
        */
    void mouseMoveEvent( QMouseEvent *e );

    /**
        *Simply calls mouseMoveEvent().
        */
    void mousePressEvent( QMouseEvent *e );

    /**
        *Reset the MousePoint to a null value, to erase the crosshairs
        */
    void mouseDoubleClickEvent( QMouseEvent *e );

    /**Redraw the plot.
        */
    void paintEvent( QPaintEvent *e );

private:
    double SunRise, SunSet, Dawn, Dusk, SunMinAlt, SunMaxAlt;
    QPoint MousePoint;
};

#endif
