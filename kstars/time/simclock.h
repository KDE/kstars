/***************************************************************************
                          simclock.h  -  description
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

#ifndef KSTARS_SIMCLOCK_H__
#define KSTARS_SIMCLOCK_H__

#include <time.h>
#ifndef KSTARS_LITE
#include <QtDBus/QtDBus>
#endif
#include <QTimer>

#include "kstarsdatetime.h"

/** @class SimClock
	*@short kstars simulation clock
	*@author Mark Hollomon
	*@version 1.0
	*/

class SimClock : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.SimClock")
public:
    /**
     * Constructor
     * @param parent parent object for the clock
     * @param when the date/time to which the SimClock should be initialized in UTC
     */
    explicit SimClock(QObject *parent = 0, const KStarsDateTime &when = KStarsDateTime::currentDateTimeUtc() );

    /** @return const reference to the current simulation Universal Time. */
    const KStarsDateTime& utc() const { return UTC; }

    /** Whether the clock is active or not is a bit complicated by the
    	*introduction of "manual mode".  In manual mode, SimClock's internal timer
    	*is stopped, because the clock is ticked manually when the current update
    	*has finished.  So, if ManualMode is true, then isActive() checks
    	*whether ManualActive is true.  Otherwise, it checks whether the timer is
    	*running.
    	*@returns true if the Simulation clock is actively running.
    	*/
    Q_INVOKABLE bool isActive();

    /** @returns the current timestep setting */
    double scale() const { return Scale; }

    /** Manual Mode is a new (04/2002) addition to the SimClock.  It is
    	*intended to be activated for large timesteps, when we want each frame
    	*drawn to the screen to be precisely Scale seconds later than the
    	*previous frame.  (i.e., if the timescale is 1 year, then each successive
    	*frame should be 1 year later than the previous frame).  ManualMode
    	*accomplishes this by stopping the internal timer and allowing the clock
    	*to be advanced manually (the manualTick() slot is called at the end of each
    	*KStars::updateTime()).
    	*@returns whether Manual Mode is active.
    	*/
    bool isManualMode() const { return ManualMode; }

    /**Sets Manual Mode on/off according to the bool argument. */
    void setManualMode( bool on=true );

public Q_SLOTS:
    #ifndef KSTARS_LITE
    /** DBUS function to stop the SimClock. */
    Q_SCRIPTABLE Q_NOREPLY void stop();

    /** DBUS function to start the SimClock. */
    Q_SCRIPTABLE Q_NOREPLY void start();

    /** DBUS function to set the time of the SimClock. */
    Q_SCRIPTABLE Q_NOREPLY void setUTC(const KStarsDateTime &newtime);

    /** DBUS function to set scale of simclock. */
    Q_SCRIPTABLE Q_NOREPLY void setClockScale(float s);
    #else
    // Define non-DBUS versions of functions above for use within KStarsLite
    /** Function to stop the SimClock. */
    void stop();

    /** Function to start the SimClock. */
    void start();

    /** Function to set the time of the SimClock. */
    void setUTC(const KStarsDateTime &newtime);

    /** Function to set scale of simclock. */
    void setClockScale(float s);
    #endif

    /** Respond to the QTimer::timeout signal */
    void tick();

    /** Equivalent of tick() for manual mode.
     * If ManualActive is true, add Scale seconds to the SimClock time.
     * (we may want to modify this slightly...e.g., the number of seconds in a
     * year is not constant (leap years), so it is better to increment the
     * year, instead of adding 31 million seconds. */
    void manualTick( bool force=false );

signals:
    /** The time has changed (emitted by setUTC() ) */
    void timeChanged();

    /** The clock has ticked (emitted by tick() )*/
    void timeAdvanced();

    /** The timestep has changed*/
    void scaleChanged(float);

    /** This is an signal that is called on either clock start or
        clock stop with an appropriate boolean argument. Required so
        that we can bind it to KToggleAction::slotToggled(bool) */
    void clockToggled(bool);

private:
    long double julianmark;
    KStarsDateTime UTC;
    QTimer tmr;
    double Scale;
    QTime sysmark;
    int lastelapsed;
    bool ManualMode, ManualActive;

    // used to generate names for dcop interfaces
    //static int idgen;
    // how often to update
    static int TimerInterval;

    // Disallow copying
    SimClock(const SimClock&);
    SimClock& operator = (const SimClock&);
};

#endif
