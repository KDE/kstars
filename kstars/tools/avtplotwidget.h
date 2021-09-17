/*
    SPDX-FileCopyrightText: 2007 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kplotwidget.h>

#include <QPoint>

class GeoLocation;

/**
 * @class AVTPlotWidget
 * @short An extension of the KPlotWidget for the AltVsTime tool.
 * The biggest difference is that in addition to the plot objects, it draws the "ground" below
 * Alt=0 and draws the sky light blue for day times, and black for night times. The transition
 * between day and night is drawn with a gradient, and the position follows the actual
 * sunrise/sunset times of the given date/location. Also, this plot widget provides two
 * time axes (local time along the bottom, and local sideral time along the top). Finally, it
 * provides user interaction: on mouse click, it draws crosshairs at the mouse position with
 * labels for the time and altitude.
 *
 * @version 1.0
 * @author Jason Harris
 */
class AVTPlotWidget : public KPlotWidget
{
    Q_OBJECT
  public:
    explicit AVTPlotWidget(QWidget *parent = nullptr);

    /**
     * Set the fractional positions of the Sunrise and Sunset positions, in units where last
     * midnight was 0.0, and next midnight is 1.0. i.e., if Sunrise is at 06:00, then we set
     * it as 0.25 in this function.  Likewise, if Sunset is at 18:00, then we set it as
     * 0.75 in this function.
     * @param sr the fractional position of Sunrise
     * @param ss the fractional position of Sunset
     */
    void setSunRiseSetTimes(double sr, double ss);

    void setDawnDuskTimes(double da, double du);

    void setMinMaxSunAlt(double min, double max);

    /**
     * Set the fractional positions of moonrise and moon set in units
     * where last midnight was 0.0 and next midnight is 1.0
     */
    void setMoonRiseSetTimes(double mr, double ms);

    /**
     * @short Set the moon illumination
     * @param mi Moon illuminated fraction (0.0 to 1.0)
     * @note Used to determine the brightness of the gradient representing lunar skyglow
     */
    void setMoonIllum(double mi);

    /**
     * @short Set the GeoLocation
     * @param geo_ Used to convert and format the current time correctly
     * @warning Might be better to skip the entire shebang and include the KSAlmanac calls within AVTPlotWidget
     */
    inline void setGeoLocation(const GeoLocation *geo_) { geo = geo_; }

  protected:
    /**
     * Handle mouse move events. If the mouse button is down, draw crosshair lines
     * centered at the cursor position. This allows the user to pinpoint specific
     * position sin the plot.
     */
    void mouseMoveEvent(QMouseEvent *e) override;

    /** Simply calls mouseMoveEvent(). */
    void mousePressEvent(QMouseEvent *e) override;

    /** Reset the MousePoint to a null value, to erase the crosshairs */
    void mouseDoubleClickEvent(QMouseEvent *e) override;

    /** Redraw the plot. */
    void paintEvent(QPaintEvent *e) override;

  private:
    double SunRise { 0.25 };
    double SunSet { 0.75 };
    double Dawn { 0 };
    double Dusk { 0 };
    double SunMinAlt { 0 };
    double SunMaxAlt { 0 };
    double MoonRise { 0 };
    double MoonSet { 0 };
    double MoonIllum { 0 };
    QPoint MousePoint;
    const GeoLocation *geo { nullptr };
};
