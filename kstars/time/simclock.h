/*
    SPDX-FileCopyrightText: 2002 Mark Hollomon <mhh@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdatetime.h"

#include <QTime>
#include <QElapsedTimer>
#include <QTimer>
#include <qtdbusglobal.h>

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
        explicit SimClock(QObject *parent = nullptr, const KStarsDateTime &when = KStarsDateTime::currentDateTimeUtc());

        /** @return const reference to the current simulation Universal Time. */
        const KStarsDateTime &utc() const
        {
            return m_UTC;
        }

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
        double scale() const
        {
            return m_Scale;
        }

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
        bool isManualMode() const
        {
            return m_ManualMode;
        }

        /**Sets Manual Mode on/off according to the bool argument. */
        void setManualMode(bool on = true);

    public Q_SLOTS:
#ifndef KSTARS_LITE
        /** DBUS function to stop the SimClock. */
        Q_SCRIPTABLE Q_NOREPLY void stop();

        /** DBUS function to start the SimClock. */
        Q_SCRIPTABLE Q_NOREPLY void start();

        /** DBUS function to set the time of the SimClock. */
        Q_SCRIPTABLE Q_NOREPLY void setUTC(const KStarsDateTime &newtime);

        /** DBUS function to set the current time of the SimClock. */
        Q_SCRIPTABLE Q_NOREPLY void setNow()
        {
            setUTC(KStarsDateTime::currentDateTimeUtc());
        }

        /** DBUS function to set scale of simclock. */
        /**
         * @brief setClockScale Set simulation clock scale per second.
         * @param scale Scale per second. 1 means normal scale, each 1 simulation
         * second equals 1 real second. Value less than one *slows* the simulation clock,
         * while values over 1 speeds it up. A scale of 0.5 makes the simulation clock ticks
         * once every 2 actual seconds (or half-ticks per one actual second),
         * while a value of 60 make the simulation clock ticks 60 simulation seconds (1 simulation minute)
         * for every 1 actual second.
         */
        Q_SCRIPTABLE Q_NOREPLY void setClockScale(double scale);
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
             * year, instead of adding 31 million seconds.
             * set backward to true to reverse sign of Scale value
        */

        void manualTick(bool force = false, bool backward = false);

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
        long double m_JulianMark { 0 };
        KStarsDateTime m_UTC;
        QTimer m_InternalTimer;
        double m_Scale { 1 };
        QElapsedTimer m_SystemMark;
        int m_LastElapsed { 0 };
        bool m_ManualMode { false };
        bool m_ManualActive { false };

        // used to generate names for dcop interfaces
        //static int idgen;
        // how often to update
        static int TimerInterval;

        // Disallow copying
        SimClock(const SimClock &);
        SimClock &operator=(const SimClock &);
};
