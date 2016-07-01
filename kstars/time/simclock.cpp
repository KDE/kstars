/***************************************************************************
                          simclock.cpp  -  description
                             -------------------
    begin                : Mon Feb 18 2002
    copyright          : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "simclock.h"

#include <QDebug>
#include <KLocalizedString>

#include "kstars.h"
#include "simclockadaptor.h"


int SimClock::TimerInterval = 100; //msec

SimClock::SimClock(QObject *parent, const KStarsDateTime &when) :
        QObject(parent),
        tmr(this)
{
    new SimClockAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/SimClock",  this);

    if (! when.isValid() )
        tmr.stop();
    setUTC(when);
    julianmark = UTC.djd();

    Scale = 1.0;
    ManualMode = false;
    ManualActive = false;

    QObject::connect(&tmr, SIGNAL(timeout()), this, SLOT(tick()));
}

void SimClock::tick() {
    if ( ! ManualMode ) { //only tick if ManualMode is false
        long mselapsed = sysmark.elapsed();
        if (mselapsed < lastelapsed) {
            // The sysmark timer has wrapped after 24 hours back to 0 ms.
            // Reset sysmark and julianmark
            julianmark = UTC.djd();
            sysmark.start();
            lastelapsed = 0;
        } else {
            lastelapsed = mselapsed;
        }

        long double scaledsec = (long double)mselapsed * (long double)Scale / 1000.0;
        UTC.setDJD( julianmark + scaledsec / (24. * 3600.) );

        // 		qDebug() << "tick() : JD = " << QLocale().toString( UTC.djd(), 7 ) <<
        // 			" mselapsed = " << mselapsed << " scale = " << Scale <<
        // 			"  scaledsec = " << double(scaledsec) << endl;

        emit timeAdvanced();
    }
}

void SimClock::setManualMode( bool on ) {
    if ( on ) {
        //Turn on manual ticking.
        ManualActive = tmr.isActive();
        tmr.stop();
    } else {
        //Turn off manual ticking.  If the Manual clock was active, start the timer.
        if ( isActive() ) {
            sysmark.start();
            julianmark = UTC.djd();
            lastelapsed = 0;
            tmr.start(TimerInterval);
        }
    }
    ManualMode = on;
}

void SimClock::manualTick( bool force ) {
    if ( force || (ManualMode && ManualActive) ) {
        setUTC( UTC.addSecs( (long double)Scale ) );
    } else if ( ! ManualMode )
        tick();
}

bool SimClock::isActive() {
    if ( ManualMode )
        return ManualActive;
    else
        return tmr.isActive();
}

void SimClock::stop() {
    if ( ManualMode && ManualActive ) {
        ManualActive = false;
        emit clockToggled(true);
    }

    if (!ManualMode && tmr.isActive()) {
        qDebug() << "Stopping the timer";
        tmr.stop();
        emit clockToggled(true);
    }
}

void SimClock::start() {
    if ( ManualMode && !ManualActive ) {
        ManualActive = true;
        sysmark.start();
        julianmark = UTC.djd();
        lastelapsed = 0;
        emit clockToggled( false );
        //emit timeChanged() in order to restart calls to updateTime()
        emit timeChanged();
    } else if ( !ManualMode && !tmr.isActive()) {
        qDebug() << "Starting the timer";
        sysmark.start();
        julianmark = UTC.djd();
        lastelapsed = 0;
        tmr.start(TimerInterval);
        emit clockToggled( false );
    }
}

void SimClock::setUTC(const KStarsDateTime &newtime) {
    //DEBUG
    //qDebug() << newtime.toString();
    //qDebug() << "is dateTime valid? " << newtime.isValid();

    if ( newtime.isValid() ) {
        UTC = newtime;
        if (tmr.isActive()) {
            julianmark = UTC.djd();
            sysmark.start();
            lastelapsed = 0;
        }

        qDebug() << QString("Setting clock:  UTC: %1  JD: %2").arg(UTC.toString()).arg(QLocale().toString( (double) UTC.djd(), 'f' , 2 ) );
        emit timeChanged();
    } else {
        qDebug() << "Cannot set SimClock:  Invalid Date/Time.";
    }
}

void SimClock::setClockScale(float s) {
    if (Scale != s ) {
        qDebug() << "New clock scale: " << s << " sec";
        Scale = s;
        if (tmr.isActive()) {
            julianmark = UTC.djd();
            sysmark.start();
            lastelapsed = 0;
        }
        emit scaleChanged(s);
    }
}


