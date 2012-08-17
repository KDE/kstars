/***************************************************************************
                          calendarwidget.h  -  description
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

#ifndef CALENDARWIDGET_H_
#define CALENDARWIDGET_H_

#include <QDate>
#include <kplotwidget.h>

/**@class CalendarWidget
 *@short An extension of the KPlotWidget for the SkyCalendar tool.
 */
class CalendarWidget : public KPlotWidget
{
    Q_OBJECT
    public:
        explicit CalendarWidget( QWidget *parent=0 );
        void setHorizon();
        inline float getRiseTime( int i ) { return riseTimeList.at( i ); }
        inline float getSetTime( int i ) { return setTimeList.at( i ); }
    
    protected:
        void paintEvent( QPaintEvent *e );
        
    private:
        void drawHorizon( QPainter *p );
        void drawAxes( QPainter *p );
        
        QList<QDate> dateList;
        QList<float> riseTimeList;
        QList<float> setTimeList;
        
        float minSTime;
        float maxRTime;
        
        QPolygonF polySunRise;
        QPolygonF polySunSet;
};

#endif
