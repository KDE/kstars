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

#define XPADDING 20
#define YPADDING 20
#define BIGTICKSIZE 10
#define SMALLTICKSIZE 4
#define TICKOFFSET 0


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

void CalendarWidget::drawAxes( QPainter *p ) {
    p->setPen( foregroundColor() );
    p->setBrush( Qt::NoBrush );

    //Draw bounding box for the plot
    p->drawRect( pixRect() );

    //set small font for axis labels
    QFont f = p->font();
    int s = f.pointSize();
    f.setPointSize( s - 2 );
    p->setFont( f );

    //Top/Bottom axis tickmarks and time labels
    for ( float xx=-8.0; xx<= 8.0; xx += 2.0 ) {
				int h = int(xx);
				if ( h < 0 ) h += 24;
				QString sTime = KGlobal::locale()->formatTime( QTime( h, 0, 0 ) );

        QPointF pTick = mapToWidget( QPointF( xx, dataRect().y() ) );
        p->drawLine( pTick, QPointF( pTick.x(), pTick.y() - BIGTICKSIZE ) );
				QRectF r( pTick.x() - BIGTICKSIZE, pTick.y() + 0.5*BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
				p->drawText( r, Qt::AlignCenter | Qt::TextDontClip, sTime );

        pTick = QPointF( pTick.x(), 0.0 );
        p->drawLine( pTick, QPointF( pTick.x(), pTick.y() + BIGTICKSIZE ) );
				r.moveTop( -2.0*BIGTICKSIZE );
				p->drawText( r, Qt::AlignCenter | Qt::TextDontClip, sTime );
    }

		
    //Month dividers
    SkyCalendar *skycal = (SkyCalendar*)topLevelWidget();
    int y = skycal->year();
    for ( int imonth=2; imonth <= 12; ++imonth ) {
        QDate dt( y, imonth, 1 );
        float doy = float( dt.daysInYear() - dt.dayOfYear() );
        QPointF pdoy = mapToWidget( QPointF( dataRect().x(), doy ) );
        p->drawLine( pdoy, QPointF( pixRect().right(), pdoy.y() ) );
    }
  
}

#include "calendarwidget.moc"
