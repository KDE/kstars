/***************************************************************************
                          skycalendar.h  -  description
                             -------------------
    begin                : Wed Jul 16 2008
    copyright            : (C) 2008 by Jason Harris
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

#pragma once

#include <QDialog>
#include <QMutex>

#include "ui_skycalendar.h"

class GeoLocation;

class SkyCalendarUI : public QFrame, public Ui::SkyCalendar
{
    Q_OBJECT

  public:
    explicit SkyCalendarUI(QWidget *p = nullptr);
};

/**
 * @class SkyCalendar
 *
 * Draws Rise/Set/Transit curves for major solar system planets for any calendar year.
 */
class SkyCalendar : public QDialog
{
    Q_OBJECT

    friend class CalendarWidget;

  public:
    explicit SkyCalendar(QWidget *parent = nullptr);
    ~SkyCalendar();

    int year();
    GeoLocation *get_geo();

  public slots:
    void slotFillCalendar();
    void slotPrint();
    void slotLocation();
    void slotCalculating();

  private:
    void addPlanetEvents(int nPlanet);
    void drawEventLabel(float x1, float y1, float x2, float y2, QString LabelText);

    SkyCalendarUI *scUI { nullptr };
    GeoLocation *geo { nullptr };
    QMutex calculationMutex;
    QString plotButtonText;
    bool calculating { false };
};
