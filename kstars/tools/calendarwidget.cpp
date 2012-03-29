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
    
    maxRTime = 12.0;
    minSTime = -12.0;
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

void CalendarWidget::setHorizon() {
    KSSun thesun;
    SkyCalendar *skycal = (SkyCalendar*)topLevelWidget();
    KStarsDateTime kdt( QDate( skycal->year(), 1, 1 ), QTime( 12, 0, 0 ) );
    
    maxRTime = 0.0;
    minSTime = 0.0;
    
    // Clear date, rise and set time lists
    dateList.clear();
    riseTimeList.clear();
    setTimeList.clear();
    
    float rTime, sTime;
    
    // Get rise and set time every 7 days for 1 year
    while ( skycal->year() == kdt.date().year() ) {
        QTime tmp_rTime, tmp_sTime;
        tmp_rTime = thesun.riseSetTime( kdt.djd() + 1.0, skycal->get_geo(), true, true );
        tmp_sTime = thesun.riseSetTime( kdt.djd(), skycal->get_geo(), false, true );

        /* riseSetTime seems buggy since it sometimes returns the same time for rise and set (01:00:00).
         * In this case, we just reset tmp_rTime and tmp_sTime so they will be considered invalid
         * in the folowing lines. */
        if ( tmp_rTime == tmp_sTime ) {
            tmp_rTime = QTime();
            tmp_sTime = QTime();
        }
        
        // If rise and set times are valid, the sun rise and set...
        if ( tmp_rTime.isValid() && tmp_sTime.isValid() ) {
            QTime midday( 12, 0, 0 );
            rTime = tmp_rTime.secsTo( midday ) * 24.0 / 86400.0;
            sTime = tmp_sTime.secsTo( midday ) * 24.0 / 86400.0;

            if ( tmp_rTime <= midday )
                rTime = 12.0 - rTime;
            else
                rTime = -12.0 - rTime;
            
            if ( tmp_sTime <= midday )
                sTime = 12.0 - sTime;
            else
                sTime = -12.0 - sTime;
        }
        /* else, the sun don't rise and/or don't set.
         * we look at the altitude of the sun at transit time, if it is above the horizon,
         * there is no night, else there is no day. */
        else {
            if ( thesun.transitAltitude( kdt.djd(), skycal->get_geo() ).degree() > 0 ) {
                rTime = -4.0;
                sTime =  4.0;
            } else {
                rTime =  12.0;
                sTime = -12.0;
            }
        }

        // Get max rise time and min set time
        if ( rTime > maxRTime )
            maxRTime = rTime;
        if ( sTime < minSTime )
            minSTime = sTime;
        
        // Keep the day, rise time and set time in lists
        dateList.append( kdt.date() );
        riseTimeList.append( rTime );
        setTimeList.append( sTime );

        // Next week
        kdt = kdt.addDays( 7 );
    }
    
    // Set widget limits
    maxRTime = ceil( maxRTime ) + 1.0;
    if ( (int) maxRTime % 2 != 0 )
        maxRTime++;
    if ( maxRTime > 12.0 )
        maxRTime = 12.0;
    minSTime = floor( minSTime ) - 1.0;
    if ( (int) minSTime % 2 != 0 )
        minSTime--;
    if ( minSTime < -12.0 )
        minSTime = -12.0;
    setLimits( minSTime, maxRTime, 0.0, 366.0 );
    setPixRect();
}

void CalendarWidget::drawHorizon( QPainter *p ) {
    polySunRise.clear();
    polySunSet.clear();
    
    for ( int date=0; date<dateList.size(); date++ ) {
        int day = dateList.at( date ).daysInYear() - dateList.at( date ).dayOfYear();
        polySunRise << mapToWidget( QPointF( riseTimeList.at( date ), day ) );
        polySunSet  << mapToWidget( QPointF( setTimeList.at( date ), day ) );
    }
    
    //Finish polygons by adding pixRect corners
    polySunRise << mapToWidget( QPointF( riseTimeList.last(),  dataRect().top() ) )
                << mapToWidget( QPointF( dataRect().right(),   dataRect().top() ) )
                << mapToWidget( QPointF( dataRect().right(),   dataRect().bottom() ) )
                << mapToWidget( QPointF( riseTimeList.first(), dataRect().bottom() ) );
    polySunSet << mapToWidget( QPointF( setTimeList.last(),  dataRect().top() ) )
               << mapToWidget( QPointF( dataRect().left(),   pixRect().top() ) )
               << mapToWidget( QPointF( dataRect().left(),   pixRect().bottom() ) )
               << mapToWidget( QPointF( setTimeList.first(), dataRect().bottom() ) );
    
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
    for ( float xx = minSTime; xx <= maxRTime; xx += 2.0 ) {
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
    
    for ( int date=0; date<dateList.size(); date++ ) {
        if ( dateList.at( date ).day() < 12 || dateList.at( date ).day() > 18 )
            continue;
        
        bool noNight = false;
        if ( riseTimeList.at( date ) < setTimeList.at( date ) )
            noNight = true;
        
        int imonth = dateList.at( date ).month();
        
        QString shortMonthName = QDate::shortMonthName( dateList.at( date ).month() );
        QRectF riseLabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, shortMonthName );
        QRectF setLabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, shortMonthName );
        
        QDate dt( y, imonth, 15 );
        float doy = float( dt.daysInYear() - dt.dayOfYear() );
        float xRiseLabel, xSetLabel;
        if ( noNight ) {
            xRiseLabel = 0.0;
            xSetLabel = 0.0;
        } else {
            xRiseLabel = riseTimeList.at( date ) + 0.6;
            xSetLabel = setTimeList.at( date )- 0.6;
        }
        QPointF pRiseLabel = mapToWidget( QPointF( xRiseLabel, doy ) );
        QPointF pSetLabel = mapToWidget( QPointF( xSetLabel, doy ) );
        
        //Determine rotation angle for month labels
        QDate dt1( y, imonth, 1 );
        float doy1 = float( dt1.daysInYear() - dt1.dayOfYear() );
        QDate dt2( y, imonth, dt1.daysInMonth() );
        float doy2 = float( dt2.daysInYear() - dt2.dayOfYear() );
        
        QPointF p1, p2;
        float rAngle, sAngle;
        if ( noNight ) {
            rAngle = 90.0;
        } else {
            p1 = mapToWidget( QPointF( riseTimeList.at( date-2 ), doy1 ) );
            p2 = mapToWidget( QPointF( riseTimeList.at( date+2 ), doy2 ) );
            rAngle = atan2( p2.y() - p1.y(), p2.x() - p1.x() )/dms::DegToRad;
            
            p1 = mapToWidget( QPointF( setTimeList.at( date-2 ), doy1 ) );
            p2 = mapToWidget( QPointF( setTimeList.at( date+2 ), doy2 ) );
            sAngle = atan2( p2.y() - p1.y(), p2.x() - p1.x() )/dms::DegToRad;
        }
        
        p->save();
        p->translate( pRiseLabel );
        p->rotate( rAngle );
        p->drawText( riseLabelRect, textFlags, shortMonthName );
        p->restore();
        
        if ( ! noNight ) {
            p->save();
            p->translate( pSetLabel );
            p->rotate( sAngle );
            p->drawText( setLabelRect, textFlags, shortMonthName );
            p->restore();
        }
    }
     
    p->setFont( origFont );
}

#include "calendarwidget.moc"
