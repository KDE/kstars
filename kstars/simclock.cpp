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

#include <kdebug.h>
#include <klocale.h>

#include "simclock.h"
#include "kstars.h"

int SimClock::idgen = 1;

int SimClock::TimerInterval = 100; //msec

SimClock::SimClock(QObject *parent, const KStarsDateTime &when) :
		DCOPObject("clock#" + QCString().setNum(idgen++)),
		QObject(parent),
		tmr(this)
{
	if (! when.isValid() ) tmr.stop();
	setUTC(when);
	julianmark = UTC.djd();
	
	Scale = 1.0;
	ManualMode = false;
	ManualActive = false;
	
	QObject::connect(&tmr, SIGNAL(timeout()), this, SLOT(tick()));
}

SimClock::SimClock (const SimClock &old) :
		DCOPObject("clock#" + QCString().setNum(idgen++)),
		QObject(old.parent()),
		SimClockInterface(),
		tmr(this)
{
	UTC = old.UTC;
	julianmark = old.julianmark;
	
	Scale = old.Scale;
	ManualMode = old.ManualMode;
	ManualActive = old.ManualActive;
	
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

// 		kdDebug() << "tick() : JD = " << KGlobal::locale()->formatNumber( UTC.djd(), 7 ) <<
// 			" mselapsed = " << mselapsed << " scale = " << Scale <<
// 			"  scaledsec = " << double(scaledsec) << endl;

		emit timeAdvanced();
	}
}

void SimClock::setManualMode( bool on ) {
	if ( on ) {
		//Turn on manual ticking.  
		//If the timer was active, stop the timer and set ManualActive=true.  
		//Otherwise, set ManualActive=false.
		//Finally, set ManualMode=true.
		if ( tmr.isActive() ) {
			tmr.stop();
			ManualActive = true;
		} else {
			ManualActive = false;
		}
		ManualMode = true;
	} else {
		//Turn off manual ticking.  If the Manual clock was active, start the timer.
		//Then set ManualMode=false.
		if ( isActive() ) {
			sysmark.start();
			julianmark = UTC.djd();
			lastelapsed = 0;
			tmr.start(TimerInterval);
		}
		ManualMode = false;
	}
}

void SimClock::manualTick( bool force ) {
	if ( force || (ManualMode && ManualActive) ) {
		setUTC( UTC.addSecs( (long double)Scale ) );
	} else if ( ! ManualMode ) tick();
}

bool SimClock::isActive() {
	if ( ManualMode ) return ManualActive;
	else return tmr.isActive();
}

// The SimClockInterface
void SimClock::stop() {
	if ( ManualMode && ManualActive ) {
		ManualActive = false;
		emit clockStopped();
	}

	if (!ManualMode && tmr.isActive()) {
		kdDebug() << i18n( "Stopping the timer" ) << endl;
		tmr.stop();
		emit clockStopped();
	}
}

void SimClock::start() {
	if ( ManualMode && !ManualActive ) {
		ManualActive = true;
		sysmark.start();
		julianmark = UTC.djd();
		lastelapsed = 0;
		emit clockStarted();
		//emit timeChanged() in order to restart calls to updateTime()
		emit timeChanged();
	}

	if (! ManualMode && ! tmr.isActive()) {
		kdDebug() << i18n( "Starting the timer" ) << endl;
		sysmark.start();
		julianmark = UTC.djd();
		lastelapsed = 0;
		tmr.start(TimerInterval);
		emit clockStarted();
	}
}

void SimClock::setUTC(const KStarsDateTime &newtime) {
	if ( newtime.isValid() ) {
		UTC = newtime;
		if (tmr.isActive()) {
			julianmark = UTC.djd();
			sysmark.start();
			lastelapsed = 0;
		}
		
		kdDebug() << i18n( "Setting clock:  UTC: %1  JD: %2" )
				.arg( UTC.toString() ).arg( KGlobal::locale()->formatNumber( UTC.djd() ) ) << endl;
		emit timeChanged();
	} else {
		kdDebug() << i18n( "Cannot set SimClock:  Invalid Date/Time." ) << endl;
	}
}

void SimClock::setScale(float s) {
	if (Scale != s ) {
		kdDebug() << i18n( "New clock scale: %1 sec" ).arg( s ) << endl;
		Scale = s;
		if (tmr.isActive()) {
			julianmark = UTC.djd();
			sysmark.start();
			lastelapsed = 0;
		}
		emit scaleChanged(s);
	}
}

//DCOP function to set clock scale
void SimClock::setClockScale(float s) {
	setScale(s);
}

#include "simclock.moc"
