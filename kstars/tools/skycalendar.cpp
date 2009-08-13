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

#include <QDesktopWidget>
#include <kdebug.h>
#include <KPlotObject>
#include <KPushButton>

#include "calendarwidget.h"
#include "geolocation.h"
#include "dialogs/locationdialog.h"
#include "kstars.h"
#include "kstarsdatetime.h"
#include "kstarsdata.h"
#include "skyobjects/ksplanet.h"

SkyCalendarUI::SkyCalendarUI( QWidget *parent )
    : QFrame( parent )
{
    setupUi( this );
}

SkyCalendar::SkyCalendar( QWidget *parent )
    : KDialog( parent )
{
    scUI = new SkyCalendarUI( this );
    setMainWidget( scUI );
    
    geo = KStars::Instance()->data()->geo();

    setCaption( i18n( "Sky Calendar" ) );
    setButtons( KDialog::User1 | KDialog::Close );
    setModal( false );

    //Adjust minimum size for small screens:
    if ( QApplication::desktop()->availableGeometry().height() <= scUI->CalendarView->height() ) {
        scUI->CalendarView->setMinimumSize( 400, 600 );
    }
    
    scUI->CalendarView->setLimits( -9.0, 9.0, 0.0, 366.0 );
    scUI->CalendarView->setShowGrid( false ); 
    scUI->Year->setValue( KStarsData::Instance()->lt().date().year() );

    scUI->LocationButton->setText( geo->fullName() );
    setButtonGuiItem( KDialog::User1, KGuiItem( i18n("&Print..."), QString(), i18n("Print the Sky Calendar") ) );

    connect( scUI->CreateButton, SIGNAL(clicked()), this, SLOT(slotFillCalendar()) );
    connect( scUI->LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()) );
    connect( this, SIGNAL( user1Clicked() ), this, SLOT( slotPrint() ) );
}

SkyCalendar::~SkyCalendar() {
}

int SkyCalendar::year()  { return scUI->Year->value(); }

void SkyCalendar::slotFillCalendar() {
    scUI->CalendarView->resetPlot();
    scUI->CalendarView->setLimits( -9.0, 9.0, 0.0, 366.0 );
    
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

// FIXME: For the time being, adjust with dirty, cluttering labels that don't align to the line
/*
void SkyCalendar::drawEventLabel( float x1, float y1, float x2, float y2, QString LabelText ) {
    QFont origFont = p->font();
    p->setFont( QFont( "Bitstream Vera", 10 ) );

    int textFlags = Qt::AlignCenter; // TODO: See if Qt::SingleLine flag works better
    QFontMetricsF fm( p->font(), p->device() );

    QRectF LabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, LabelText );
    QPointF LabelPoint = scUI->CalendarView->mapToWidget( QPointF( x, y ) );
        
    float LabelAngle = atan2( y2 - y1, x2 - x1 )/dms::DegToRad;
        
    p->save();
    p->translate( LabelPoint );
    p->rotate( LabelAngle );
    p->drawText( LabelRect, textFlags, LabelText );
    p->restore();

    p->setFont( origFont );
}
*/

void SkyCalendar::addPlanetEvents( int nPlanet ) {
    KSPlanetBase *ksp = KStarsData::Instance()->skyComposite()->planet( nPlanet );
    int y = scUI->Year->value();
    KStarsDateTime kdt( QDate( y, 1, 1 ), QTime( 12, 0, 0 ) );
    QColor pColor = KSPlanetBase::planetColor[nPlanet];

    QVector<QPointF> vRise, vSet, vTransit;
    int iweek = 0;
    while ( kdt.date().year() == y ) {
        float dy = float( kdt.date().daysInYear() - kdt.date().dayOfYear() );

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

    //Now, find continuous segments in each QVector and add each segment 
    //as a separate KPlotObject

    // Flags to indicate whether the set / rise / transit labels should be drawn
    bool setLabel, riseLabel, transitLabel;

    KPlotObject *oRise = new KPlotObject( pColor, KPlotObject::Lines, 2.0 );
    KPlotObject *oSet = new KPlotObject( pColor, KPlotObject::Lines, 2.0 );
    KPlotObject *oTransit = new KPlotObject( pColor, KPlotObject::Lines, 2.0 );

    setLabel = riseLabel = transitLabel = false;

    for ( int i=0; i<vRise.size(); ++i ) {
        if ( i > 0 && fabs(vRise.at(i).x() - vRise.at(i-1).x()) > 6.0 ) { 
            scUI->CalendarView->addPlotObject( oRise );
            oRise = new KPlotObject( pColor, KPlotObject::Lines, 2.0 );
            scUI->CalendarView->update();
        }
        if ( i > 0 && fabs(vSet.at(i).x() - vSet.at(i-1).x()) > 6.0 ) {
            scUI->CalendarView->addPlotObject( oSet );
            oSet = new KPlotObject( pColor, KPlotObject::Lines, 2.0 );
            scUI->CalendarView->update();
        }
        if ( i > 0 && fabs(vTransit.at(i).x() - vTransit.at(i-1).x()) > 6.0 ) {
            scUI->CalendarView->addPlotObject( oTransit );
            oTransit = new KPlotObject( pColor, KPlotObject::Lines, 2.0 );
            scUI->CalendarView->update();
        }
        
        if( i > 0 ) {
            // Draw a label when a line crosses the Y-Axis
            if( vRise.at( i - 1 ).x() * vRise.at( i ).x() <= 0 )
                riseLabel = true;
            if( vSet.at( i - 1 ).x() * vSet.at( i ).x() <= 0 )
                setLabel = true;
            if( vTransit.at( i - 1 ).x() * vTransit.at( i ).x() <= 0 )
                transitLabel = true;
            
            // Draw a label when a line reaches a maximum / minimum
            if( i < vRise.size() - 1 ) {
                if( ( vRise.at( i - 1 ).x() - vRise.at( i ).x() ) * ( vRise.at( i ).x() - vRise.at( i + 1 ).x() ) < 0 )
                    riseLabel = true;
                if( ( vSet.at( i - 1 ).x() - vSet.at( i ).x() ) * ( vSet.at( i ).x() - vSet.at( i + 1 ).x() ) < 0 )
                    setLabel = true;
                if( ( vTransit.at( i - 1 ).x() - vTransit.at( i ).x() ) * ( vTransit.at( i ).x() - vTransit.at( i + 1 ).x() ) < 0 )
                    transitLabel = true;
            }                
        }
        oRise->addPoint( vRise.at(i), riseLabel ? i18nc( "A planet rises from the horizon", "%1 rises", ksp->name() ) : QString() );
        oSet->addPoint( vSet.at(i), setLabel ? i18nc( "A planet sets from the horizon", "%1 sets", ksp->name() ) : QString() );
        oTransit->addPoint( vTransit.at(i), transitLabel ? i18nc( "A planet transits across the meridian", "%1 transits", ksp->name() ) : QString() );
        setLabel = riseLabel = transitLabel = false;
    }
    
    scUI->CalendarView->addPlotObject( oRise );
    scUI->CalendarView->addPlotObject( oSet );
    scUI->CalendarView->addPlotObject( oTransit );
    scUI->CalendarView->update();
}

void SkyCalendar::slotPrint() {
}

void SkyCalendar::slotLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( KStars::Instance() );
    if ( ld->exec() == QDialog::Accepted ) {
        GeoLocation *newGeo = ld->selectedCity();
        if ( newGeo ) {
            geo = newGeo;
            scUI->LocationButton->setText( geo->fullName() );
        }
    }
    delete ld;
}

#include "skycalendar.moc"
