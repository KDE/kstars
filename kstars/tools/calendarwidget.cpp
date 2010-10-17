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
#include "skyobjects/kssun.h"
#include "skycalendar.h"
#include "ksalmanac.h"

#define XPADDING 20
#define YPADDING 20
#define BIGTICKSIZE 10
#define SMALLTICKSIZE 4
#define TICKOFFSET 0


CalendarWidget::CalendarWidget( QWidget *parent ) 
    : KPlotWidget( parent ) 
{
    setAntialiasing( true );
    setTopPadding( 40 );
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
    KSSun thesun;
    KStarsData *data = KStarsData::Instance();
    // FIXME: OMG!!!
    SkyCalendar *skycal = (SkyCalendar*)topLevelWidget();
    int y = skycal->year();
    KStarsDateTime kdt( QDate( y, 1, 1 ), QTime( 12, 0, 0 ) );
    
    QPolygonF polySunRise;
    QPolygonF polySunSet;
    //Add points along curved edge of horizon polygons
    int imonth = -1;
    float rTime, sTime;

    while ( y == kdt.date().year() ) {
        rTime = thesun.riseSetTime( kdt.djd() + 1.0, data->geo(), true, true ).secsTo(QTime())*-24.0/86400.0;
        sTime = thesun.riseSetTime( kdt.djd(),       data->geo(), false, true  ).secsTo(QTime())*-24.0/86400.0 - 24.0;

        // FIXME why do the above two give different values ? ( Difference < 1 min though )
        if ( kdt.date().month() != imonth ) {
            riseTimeList.append( rTime );
            setTimeList.append( sTime );
            imonth = kdt.date().month();
        }
        
        float t = kdt.date().daysInYear() - kdt.date().dayOfYear();
        polySunRise << mapToWidget( QPointF( rTime, t ) );
        polySunSet  << mapToWidget( QPointF( sTime, t ) );
        
        kdt = kdt.addDays( 7 );
    }

    //Add last rise/set times to the list
    riseTimeList.append( rTime );
    setTimeList.append( sTime );
    
    //Finish polygons by adding pixRect corners
    polySunRise << mapToWidget( QPointF( rTime,              dataRect().top() ) )
                << mapToWidget( QPointF( dataRect().right(), dataRect().top() ) )
                << mapToWidget( QPointF( dataRect().right(), dataRect().bottom() ) )
                << mapToWidget( QPointF( riseTimeList[0],    dataRect().bottom() ) );
    polySunSet << mapToWidget( QPointF( sTime,             dataRect().top() ) )
               << mapToWidget( QPointF( dataRect().left(), pixRect().top() ) )
               << mapToWidget( QPointF( dataRect().left(), pixRect().bottom() ) )
               << mapToWidget( QPointF( setTimeList[0],    dataRect().bottom() ) );

    p->setPen( Qt::darkGreen );
    p->setBrush( Qt::darkGreen );
    p->drawPolygon( polySunRise );
    p->drawPolygon( polySunSet );
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
    QColor c = p->pen().color();
    c.setAlpha( 100 );
    p->setPen( c );
    SkyCalendar *skycal = (SkyCalendar*)topLevelWidget();
    int y = skycal->year();
    for ( int imonth=2; imonth <= 12; ++imonth ) {
        QDate dt( y, imonth, 1 );
        float doy = float( dt.daysInYear() - dt.dayOfYear() );
        QPointF pdoy = mapToWidget( QPointF( dataRect().x(), doy ) );
        p->drawLine( pdoy, QPointF( pixRect().right(), pdoy.y() ) );
    }
    c.setAlpha( 255 );
    p->setPen( c );
    
    //Draw month labels along each horizon curve
    QFont origFont = p->font();
    p->setFont( QFont( "Monospace", origFont.pointSize() + 5 ) );
    int textFlags = Qt::TextSingleLine | Qt::AlignCenter;
    QFontMetricsF fm( p->font(), p->device() );
    for ( int imonth=1; imonth <= 12; ++ imonth ) {
        QRectF riseLabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, QDate::shortMonthName( imonth ) );
        QRectF setLabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, QDate::shortMonthName( imonth ) );
        
        QDate dt( y, imonth, 15 );
        float doy = float( dt.daysInYear() - dt.dayOfYear() );
        float xRiseLabel = 0.5*( riseTimeList[imonth-1] + riseTimeList[imonth] ) + 0.6;
        float xSetLabel = 0.5*( setTimeList[imonth-1] + setTimeList[imonth] ) - 0.6;
        QPointF pRiseLabel = mapToWidget( QPointF( xRiseLabel, doy ) );
        QPointF pSetLabel = mapToWidget( QPointF( xSetLabel, doy ) );
        
        //Determine rotation angle for month labels
        QDate dt1( y, imonth, 1 );
        float doy1 = float( dt1.daysInYear() - dt1.dayOfYear() );
        QDate dt2( y, imonth, dt1.daysInMonth() );
        float doy2 = float( dt2.daysInYear() - dt2.dayOfYear() );
        QPointF p1 = mapToWidget( QPointF( riseTimeList[imonth-1], doy1 ) );
        QPointF p2 = mapToWidget( QPointF( riseTimeList[imonth], doy2 ) );
        float rAngle = atan2( p2.y() - p1.y(), p2.x() - p1.x() )/dms::DegToRad;
        
        p1 = mapToWidget( QPointF( setTimeList[imonth-1], doy1 ) );
        p2 = mapToWidget( QPointF( setTimeList[imonth], doy2 ) );
        float sAngle = atan2( p2.y() - p1.y(), p2.x() - p1.x() )/dms::DegToRad;
                
        p->save();
        p->translate( pRiseLabel );
        p->rotate( rAngle );
        p->drawText( riseLabelRect, textFlags, QDate::shortMonthName( imonth ) );
        p->restore();
        
        p->save();
        p->translate( pSetLabel );
        p->rotate( sAngle );
        p->drawText( setLabelRect, textFlags, QDate::shortMonthName( imonth ) );
        p->restore();
    }        
    p->setFont( origFont );
}

#include "calendarwidget.moc"
