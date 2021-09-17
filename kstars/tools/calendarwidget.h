/*
    SPDX-FileCopyrightText: 2008 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CALENDARWIDGET_H_
#define CALENDARWIDGET_H_

#include <QDate>
#include <kplotwidget.h>

/** @class CalendarWidget
 *@short An extension of the KPlotWidget for the SkyCalendar tool.
 */
class CalendarWidget : public KPlotWidget
{
    Q_OBJECT
  public:
    explicit CalendarWidget(QWidget *parent = nullptr);
    void setHorizon();
    inline float getRiseTime(int i) { return riseTimeList.at(i); }
    inline float getSetTime(int i) { return setTimeList.at(i); }

  protected:
    void paintEvent(QPaintEvent *e) override;

  private:
    void drawHorizon(QPainter *p);
    void drawAxes(QPainter *p) override;

    QList<QDate> dateList;
    QList<float> riseTimeList;
    QList<float> setTimeList;

    float minSTime;
    float maxRTime;

    QPolygonF polySunRise;
    QPolygonF polySunSet;
};

#endif
