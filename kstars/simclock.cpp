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
#include "ksutils.h"

int SimClock::idgen = 1;

int SimClock::TimerInterval = 100; //msec

SimClock::SimClock(QObject *parent, const QDateTime &when) :
	QObject(parent),
	DCOPObject("clock#" + QCString().setNum(idgen++)),
	tmr(this)
{
	if (! when.isValid() ) tmr.stop();
	Scale = 1.0;
	setUTC(when);
	julianmark = julian;
	QObject::connect(&tmr, SIGNAL(timeout()), this, SLOT(tick()));
	ManualMode = false;
	ManualActive = false;
}

SimClock::SimClock (const SimClock &old) :
	QObject(old.parent()),
	DCOPObject("clock#" + QCString().setNum(idgen++)),
	tmr(this)
{
	QObject::connect(&tmr, SIGNAL(timeout()), this, SLOT(tick()));
	julian = old.julian;
	utcvalid = false;
	Scale = old.Scale;
	ManualMode = old.ManualMode;
	ManualActive = old.ManualActive;
}

void SimClock::tick() {
	if ( ! ManualMode ) { //only tick if ManualMode is false
		long mselapsed = sysmark.elapsed();
		if (mselapsed < lastelapsed) {
			//
			// The clock has been running more than 24 hours "real" time.
			// Reset our julian base by scale number of days.
			//
			julianmark += 1.0 * Scale;
		}
		lastelapsed = mselapsed;
		long double scaledsec = (long double)mselapsed * (long double)Scale / 1000.0;
		
		julian = julianmark + (scaledsec / (24 * 3600));

//	kdDebug() << "tick() : julianmark = " << QString("%1").arg( julianmark, 10, 'f', 2) <<
//		" julian = " << QString("%1").arg( julian, 10, 'f', 2) <<
//		" mselapsed = " << mselapsed << " scale = " << Scale << endl;
		utcvalid = false;
		emit timeAdvanced();
	}
}

void SimClock::setManualMode( bool on ) {
	if ( on ) {
		//Turn on manual ticking.  If the timer was active, stop the timer and
		//set ManualActive=true.  Otherwise, set ManualActive=false.
		//Then set ManualMode=true.
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
			julianmark = julian;
			lastelapsed = 0;
			tmr.start(TimerInterval);
		}
		ManualMode = false;
	}
}

void SimClock::manualTick() {
	if ( ManualMode && ManualActive ) {

		setUTC( UTC().addSecs( int( Scale ) ) );
		julian += Scale / ( 24.*3600. );
	} else if ( ! ManualMode ) tick();
}

long double SimClock::JD() {
	return julian;
}

QDateTime SimClock::UTC() {
	if (! utcvalid) {
		utc = KSUtils::JDtoDateTime(julian);
		utcvalid = true;
	}

	return utc;
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
		julianmark = julian;
		lastelapsed = 0;
		emit clockStarted();
		//emit timeChanged() in order to restart calls to updateTime()
		emit timeChanged();
	}

	if (! ManualMode && ! tmr.isActive()) {
		kdDebug() << i18n( "Starting the timer" ) << endl;
		sysmark.start();
		julianmark = julian;
		lastelapsed = 0;
		tmr.start(TimerInterval);
		emit clockStarted();
	}
}

void SimClock::setUTC(const QDateTime &newtime) {
	utc = newtime;
	utcvalid = true;
	julian = KSUtils::UTtoJulian(utc);
	if (tmr.isActive()) {
		julianmark = julian;
		sysmark.start();
		lastelapsed = 0;
	}
	kdDebug() << i18n( "Setting clock UTC = " ) << utc.toString() <<
		i18n( " julian day = " ) << QString("%1").arg( julian, 10, 'f', 2) << endl;
	emit timeChanged();

}

void SimClock::setScale(float s) {
	if (Scale != s ) {
		kdDebug() << "setScale: setting scale = " << s << endl;
		Scale = s;
		if (tmr.isActive()) {
			julianmark = julian;
			sysmark.start();
			lastelapsed = 0;
		}
		emit scaleChanged(s);
	} else {
		kdDebug() << "setScale: scale and parm equal " << s << endl;
	}
}

#include "simclock.moc"
