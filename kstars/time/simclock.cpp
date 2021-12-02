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

SimClock::SimClock(QObject *parent, const KStarsDateTime &when) : QObject(parent), m_InternalTimer(this)
{
#ifndef KSTARS_LITE
    new SimClockAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/SimClock", this);
#endif
    if (!when.isValid())
        m_InternalTimer.stop();
    setUTC(when);
    m_JulianMark = m_UTC.djd();

    QObject::connect(&m_InternalTimer, SIGNAL(timeout()), this, SLOT(tick()));
}

void SimClock::tick()
{
    if (!m_ManualMode) //only tick if ManualMode is false
    {
        long mselapsed = m_SystemMark.elapsed();
        if (mselapsed < m_LastElapsed)
        {
            // The sysmark timer has wrapped after 24 hours back to 0 ms.
            // Reset sysmark and julianmark
            m_JulianMark = m_UTC.djd();
            m_SystemMark.start();
            m_LastElapsed = 0;
        }
        else
        {
            m_LastElapsed = mselapsed;
        }

        long double scaledsec = static_cast<long double>(mselapsed) * static_cast<long double>(m_Scale) / 1000.0;
        m_UTC.setDJD(m_JulianMark + scaledsec / (24. * 3600.));

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
        m_ManualActive = m_InternalTimer.isActive();
        m_InternalTimer.stop();
    }
    else
    {
        //Turn off manual ticking.  If the Manual clock was active, start the timer.
        if (isActive())
        {
            m_SystemMark.start();
            m_JulianMark  = m_UTC.djd();
            m_LastElapsed = 0;
            m_InternalTimer.start(TimerInterval);
        }
    }
    m_ManualMode = on;
}

void SimClock::manualTick(bool force, bool backward)
{
    if (force || (m_ManualMode && m_ManualActive))
    {
        //The single shot timer is needed because otherwise the animation is happening so frequently
        //that the kstars interface becomes too unresponsive.
        //QTimer::singleShot(1, [this,backward] { setUTC(UTC.addSecs(static_cast<long double>Scale * (backward ? -1 : 1))); });
        setUTC(m_UTC.addSecs(static_cast<long double>(m_Scale) * (backward ? -1 : 1)));
    }
    else if (!m_ManualMode)
        tick();
}

bool SimClock::isActive()
{
    if (m_ManualMode)
        return m_ManualActive;
    else
        return m_InternalTimer.isActive();
}

void SimClock::stop()
{
    if (m_ManualMode && m_ManualActive)
    {
        m_ManualActive = false;
        emit clockToggled(true);
    }

    if (!m_ManualMode && m_InternalTimer.isActive())
    {
        qCDebug(KSTARS) << "Stopping the timer";
        m_InternalTimer.stop();
        emit clockToggled(true);
    }
}

void SimClock::start()
{
    if (m_ManualMode && !m_ManualActive)
    {
        m_ManualActive = true;
        m_SystemMark.start();
        m_JulianMark  = m_UTC.djd();
        m_LastElapsed = 0;
        emit clockToggled(false);
        //emit timeChanged() in order to restart calls to updateTime()
        emit timeChanged();
    }
    else if (!m_ManualMode && !m_InternalTimer.isActive())
    {
        qCDebug(KSTARS) << "Starting the timer";
        m_SystemMark.start();
        m_JulianMark  = m_UTC.djd();
        m_LastElapsed = 0;
        m_InternalTimer.start(TimerInterval);
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
        m_UTC = newtime;
        if (m_InternalTimer.isActive())
        {
            m_JulianMark = m_UTC.djd();
            m_SystemMark.start();
            m_LastElapsed = 0;
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

void SimClock::setClockScale(double scale)
{
    if (m_Scale != scale)
    {
        qCInfo(KSTARS) << "New clock scale: " << scale << " sec";
        emit scaleChanged(scale);
        m_Scale = scale;
        if (m_InternalTimer.isActive())
        {
            m_JulianMark = m_UTC.djd();
            m_SystemMark.start();
            m_LastElapsed = 0;
        }
    }
}
