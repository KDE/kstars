/***************************************************************************
                          calendarwidget.cpp  -  description
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

#include "calendarwidget.h"

#include <QPainter>
#include <KPlotObject>
#include <kdebug.h>

#include "kstarsdata.h"
#include "kssun.h"
#include "skycalendar.h"

CalendarWidget::CalendarWidget( QWidget *parent ) 
    : KPlotWidget( parent ) 
{
    setAntialiasing( true );
}
    
void CalendarWidget::paintEvent( QPaintEvent *e ) {
    Q_UNUSED( e )
    
    QPainter p;

    p.begin( this );
    p.setRenderHint( QPainter::Antialiasing, antialiasing() );
    p.fillRect( rect(), backgroundColor() );
    p.translate( leftPadding(), topPadding() );

    setPixRect();
    p.setClipRect( pixRect() );
    p.setClipping( true );

    foreach ( KPlotObject *po, plotObjects() ) {
        po->draw( &p, this );
    }

    drawHorizon( &p );

    p.setClipping( false );
    drawAxes( &p );
    
}

void CalendarWidget::drawHorizon( QPainter *p ) {
    KSSun *sun = (KSSun*)KSPlanetBase::createPlanet( KSPlanetBase::SUN );
    KStarsData *data = KStarsData::Instance();
    SkyCalendar *skycal = (SkyCalendar*)topLevelWidget();
    int y = skycal->year();
    KStarsDateTime kdt( QDate( y, 1, 1 ), QTime( 12, 0, 0 ) );
    
    QPolygonF polySunRise;
    QPolygonF polySunSet;

    //Add points along curved edge of horizon polygons
    while ( y == kdt.date().year() ) {
        float t = float( kdt.date().daysInYear() - kdt.date().dayOfYear() );
        float rTime = sun->riseSetTime( kdt.djd() + 1.0, data->geo(), true, true ).secsTo(QTime())*-24.0/86400.0;
        float sTime = sun->riseSetTime( kdt.djd(), data->geo(), false, true  ).secsTo(QTime())*-24.0/86400.0 - 24.0;

        polySunRise << mapToWidget( QPointF( rTime, t ) );
        polySunSet << mapToWidget( QPointF(  sTime, t ) );
        
        kdt = kdt.addDays( 7 );
    }

    //Finish polygons by adding pixRect corners
    polySunRise << QPointF( pixRect().right(), pixRect().bottom() );
    polySunRise << QPointF( pixRect().right(), pixRect().top() );
    polySunSet << QPointF( pixRect().left(), pixRect().bottom() );
    polySunSet << QPointF( pixRect().left(), pixRect().top() );

    p->setPen( Qt::darkGreen );
    p->setBrush( Qt::darkGreen );
    p->drawPolygon( polySunRise );
    p->drawPolygon( polySunSet );
    
    delete sun;
}

#include "calendarwidget.moc"
