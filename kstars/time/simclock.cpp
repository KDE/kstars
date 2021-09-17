/*
    SPDX-FileCopyrightText: 2002 Mark Hollomon <mhh@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "simclock.h"

#ifndef KSTARS_LITE
#include "kstars.h"
#include "simclockadaptor.h"
#endif

#include <kstars_debug.h>

int SimClock::TimerInterval = 100; //msec

SimClock::SimClock(QObject *parent, const KStarsDateTime &when) : QObject(parent), tmr(this)
{
#ifndef KSTARS_LITE
    new SimClockAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/SimClock", this);
#endif
    if (!when.isValid())
        tmr.stop();
    setUTC(when);
    julianmark = UTC.djd();

    QObject::connect(&tmr, SIGNAL(timeout()), this, SLOT(tick()));
}

void SimClock::tick()
{
    if (!ManualMode) //only tick if ManualMode is false
    {
        long mselapsed = sysmark.elapsed();
        if (mselapsed < lastelapsed)
        {
            // The sysmark timer has wrapped after 24 hours back to 0 ms.
            // Reset sysmark and julianmark
            julianmark = UTC.djd();
            sysmark.start();
            lastelapsed = 0;
        }
        else
        {
            lastelapsed = mselapsed;
        }

        long double scaledsec = static_cast<long double>(mselapsed) * static_cast<long double>(Scale) / 1000.0;
        UTC.setDJD(julianmark + scaledsec / (24. * 3600.));

        // 		qDebug() << "tick() : JD = " << QLocale().toString( UTC.djd(), 7 ) <<
        // 			" mselapsed = " << mselapsed << " scale = " << Scale <<
        // 			"  scaledsec = " << double(scaledsec);

        emit timeAdvanced();
    }
}

void SimClock::setManualMode(bool on)
{
    if (on)
    {
        //Turn on manual ticking.
        ManualActive = tmr.isActive();
        tmr.stop();
    }
    else
    {
        //Turn off manual ticking.  If the Manual clock was active, start the timer.
        if (isActive())
        {
            sysmark.start();
            julianmark  = UTC.djd();
            lastelapsed = 0;
            tmr.start(TimerInterval);
        }
    }
    ManualMode = on;
}

void SimClock::manualTick(bool force, bool backward)
{
    if (force || (ManualMode && ManualActive))
    {
        //The single shot timer is needed because otherwise the animation is happening so frequently
        //that the kstars interface becomes too unresponsive.
        //QTimer::singleShot(1, [this,backward] { setUTC(UTC.addSecs(static_cast<long double>Scale * (backward ? -1 : 1))); });
        setUTC(UTC.addSecs(static_cast<long double>(Scale) * (backward ? -1 : 1)));
    }
    else if (!ManualMode)
        tick();
}

bool SimClock::isActive()
{
    if (ManualMode)
        return ManualActive;
    else
        return tmr.isActive();
}

void SimClock::stop()
{
    if (ManualMode && ManualActive)
    {
        ManualActive = false;
        emit clockToggled(true);
    }

    if (!ManualMode && tmr.isActive())
    {
        qCDebug(KSTARS) << "Stopping the timer";
        tmr.stop();
        emit clockToggled(true);
    }
}

void SimClock::start()
{
    if (ManualMode && !ManualActive)
    {
        ManualActive = true;
        sysmark.start();
        julianmark  = UTC.djd();
        lastelapsed = 0;
        emit clockToggled(false);
        //emit timeChanged() in order to restart calls to updateTime()
        emit timeChanged();
    }
    else if (!ManualMode && !tmr.isActive())
    {
        qCDebug(KSTARS) << "Starting the timer";
        sysmark.start();
        julianmark  = UTC.djd();
        lastelapsed = 0;
        tmr.start(TimerInterval);
        emit clockToggled(false);
    }
}

void SimClock::setUTC(const KStarsDateTime &newtime)
{
    //DEBUG
    //qDebug() << newtime.toString();
    //qDebug() << "is dateTime valid? " << newtime.isValid();

    if (newtime.isValid())
    {
        UTC = newtime;
        if (tmr.isActive())
        {
            julianmark = UTC.djd();
            sysmark.start();
            lastelapsed = 0;
        }

        // N.B. Too much log spam when in manual mode
        //qCInfo(KSTARS) << QString("Setting clock:  UTC: %1  JD: %2").arg(UTC.toString(), QLocale().toString((double)UTC.djd(), 'f', 2));
        emit timeChanged();
    }
    else
    {
        qCWarning(KSTARS) << "Cannot set SimClock:  Invalid Date/Time.";
    }
}

void SimClock::setClockScale(float s)
{
    if (Scale != s)
    {
        qCInfo(KSTARS) << "New clock scale: " << s << " sec";
        emit scaleChanged(s);
        Scale = s;
        if (tmr.isActive())
        {
            julianmark = UTC.djd();
            sysmark.start();
            lastelapsed = 0;
        }
    }
}
