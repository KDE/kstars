/***************************************************************************
                          skycalendar.cpp  -  description
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

#include "skycalendar.h"

#include <kdebug.h>
#include <KPlotObject>
#include <KPushButton>

#include "calendarwidget.h"
#include "geolocation.h"
#include "kstars.h"
#include "kstarsdatetime.h"
#include "kstarsdata.h"
#include "ksplanet.h"

SkyCalendarUI::SkyCalendarUI( QWidget *parent )
    : QFrame( parent )
{
    setupUi( this );
}

SkyCalendar::SkyCalendar( KStars *parent )
    : KDialog( (QWidget*)parent ), ks(parent)
{
    scUI = new SkyCalendarUI( this );
    setMainWidget( scUI );
    
    geo = ks->geo();
    
    setCaption( i18n( "Sky Calendar" ) );
    setButtons( KDialog::User1 | KDialog::Close );
    setModal( false );

    scUI->CalendarView->setLimits( -9.0, 9.0, 0.0, 366.0 );
    scUI->CalendarView->setShowGrid( true ); 
    scUI->Year->setValue( ks->data()->lt().date().year() );

    connect( scUI->CreateButton, SIGNAL(clicked()), this, SLOT(slotFillCalendar()) );
}

SkyCalendar::~SkyCalendar() {
}

int SkyCalendar::year()  { return scUI->Year->value(); }

void SkyCalendar::slotFillCalendar() {
    addPlanetEvents( KSPlanetBase::MERCURY );
    addPlanetEvents( KSPlanetBase::VENUS );
    addPlanetEvents( KSPlanetBase::MARS );
    addPlanetEvents( KSPlanetBase::JUPITER );
    addPlanetEvents( KSPlanetBase::SATURN );
    addPlanetEvents( KSPlanetBase::URANUS );
    addPlanetEvents( KSPlanetBase::NEPTUNE );
    addPlanetEvents( KSPlanetBase::PLUTO );
    
    update();
}

void SkyCalendar::addPlanetEvents( int nPlanet ) {
    KSPlanet *ksp = (KSPlanet*)KSPlanetBase::createPlanet( nPlanet );
    int y = scUI->Year->value();
    KStarsDateTime kdt( QDate( y, 1, 1 ), QTime( 12, 0, 0 ) );

    QVector<QPointF> vRise, vSet, vTransit;
    int iweek = 0;
    while ( kdt.date().year() == y ) {
        float dy = float( kdt.date().daysInYear() - kdt.date().dayOfYear() );
        ksp->recomputeCoords( kdt, geo );

        //Compute rise/set/transit times.  If they occur before noon, 
        //recompute for the following day
        QTime rtime = ksp->riseSetTime( kdt, geo, true, true );//rise time, exact
        if ( rtime.secsTo( QTime( 12, 0, 0 ) ) > 0 ) {
            rtime = ksp->riseSetTime( kdt.addDays( 1 ), geo, true, true );
        }
        QTime stime = ksp->riseSetTime( kdt, geo, false, true );//set time, exact
        if ( stime.secsTo( QTime( 12, 0, 0 ) ) > 0 ) {
            stime = ksp->riseSetTime( kdt.addDays( 1 ), geo, false, true );
        }
        QTime ttime = ksp->transitTime( kdt, geo );
        if ( ttime.secsTo( QTime( 12, 0, 0 ) ) > 0 ) {
            ttime = ksp->transitTime( kdt.addDays( 1 ), geo );
        }

        //convert time to decimal hours since midnight
        float rt = rtime.secsTo(QTime())*-24.0/86400.0;
        if ( rt > 12.0 ) { rt -= 24.0; }
        float st = stime.secsTo(QTime())*-24.0/86400.0;
        if ( st > 12.0 ) { st -= 24.0; }
        float tt = ttime.secsTo(QTime())*-24.0/86400.0;
        if ( tt > 12.0 ) { tt -= 24.0; }
        
        vRise << QPointF( rt, dy );
        vSet << QPointF( st, dy );
        vTransit << QPointF( tt, dy );
        ++iweek;
        
        kdt = kdt.addDays( 7 );
    }

    //DEBUG
    if ( nPlanet == KSPlanetBase::MERCURY ) {
        for ( int i=0; i<vRise.size(); ++i ) {
            kDebug() << vRise.at(i).x() << "," << vRise.at(i).y() << "  " << vSet.at(i).x() << "," << vSet.at(i).y() << endl;
        }
    }
    
    //Now, find continuous segments in each QVector and add each segment 
    //as a separate KPlotObject
    KPlotObject *oRise = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
    KPlotObject *oSet = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
    KPlotObject *oTransit = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
    for ( int i=0; i<vRise.size(); ++i ) {
        if ( i > 0 && fabs(vRise.at(i).x() - vRise.at(i-1).x()) > 6.0 ) { 
            scUI->CalendarView->addPlotObject( oRise );
            oRise = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
            update();
        }
        if ( i > 0 && fabs(vSet.at(i).x() - vSet.at(i-1).x()) > 6.0 ) {
            scUI->CalendarView->addPlotObject( oSet );
            oSet = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
            update();
        }
        if ( i > 0 && fabs(vTransit.at(i).x() - vTransit.at(i-1).x()) > 6.0 ) {
            scUI->CalendarView->addPlotObject( oTransit );
            oTransit = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
            update();
        }
        
        oRise->addPoint( vRise.at(i) );
        oSet->addPoint( vSet.at(i) );
        oTransit->addPoint( vTransit.at(i) );
    }
    
    scUI->CalendarView->addPlotObject( oRise );
    scUI->CalendarView->addPlotObject( oSet );
    scUI->CalendarView->addPlotObject( oTransit );
    update();
}

void SkyCalendar::slotPrint() {
}

#include "skycalendar.moc"
