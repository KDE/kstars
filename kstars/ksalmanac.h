/***************************************************************************
                          ksalmanac.h  -  description

                             -------------------
    begin                : Friday May 8, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "kstarsdatetime.h"

/**
 *@class KSAlmanac
 *
 *A class that implements methods to find sun rise, sun set, twilight
 *begin / end times, moon rise and moon set times.
 *
 *@short Implement methods to find important times in a day
 *@author Prakash Mohan
 *@version 1.0
 */

class GeoLocation;

// TODO: Fix the documentation for these methods

class KSAlmanac
{
  public:
    // TODO: Add documentation
    KSAlmanac();

    /**
         *@short Set the date for computations to the given date.
         *@param newdt The new date to set as a KStarsDateTime
         *@note The time must be midnight (fixme: check this)
         */
    void setDate(const KStarsDateTime *newdt);

    /**
         *@short Set the location for computations to the given location
         *@param geo_ The location to set for computations
         */
    void setLocation(const GeoLocation *geo_);

    /**
         *All the functions returns the fraction of the day
         *as their return value
         */
    inline double getSunRise() { return SunRise; }
    inline double getSunSet() { return SunSet; }
    inline double getMoonRise() { return MoonRise; }
    inline double getMoonSet() { return MoonSet; }
    inline double getDuskAstronomicalTwilight() { return DuskAstronomicalTwilight; }
    inline double getDawnAstronomicalTwilight() { return DawnAstronomicalTwilight; }

    /**
         *These functions return the max and min altitude of the sun during the course of the day in degrees
         */
    inline double getSunMaxAlt() { return SunMaxAlt; }
    inline double getSunMinAlt() { return SunMinAlt; }

    /**
         *@return the moon phase in degrees at the given date/time. Ranges is [0, 180]
         */
    inline double getMoonPhase() { return MoonPhase; }

    /**
         *@return get the moon illuminated fraction at the given date/time. Range is [0.,1.]
         */
    inline double getMoonIllum() { return m_Moon.illum(); }

    inline QTime sunRise() { return SunRiseT; }
    inline QTime sunSet() { return SunSetT; }
    inline QTime moonRise() { return MoonRiseT; }
    inline QTime moonSet() { return MoonSetT; }
    // TODO: Implement:
    //    inline QTime duskAstronomicalTwilight() { return DuskAstronomicalTwilightT; }
    //    inline QTime dawnAstronomicalTwilight() { return DawnAstronomicalTwilightT; }

    /**
         *@short Convert the zenithal distance of the sun to fraction of the day
         *@param z Zenithal angular distance
         *@return Time as a fraction of the day, at which the zenithal distance is attained by the sun
         *@note This is accurate only for zenithal angles close to sunset. TODO: Make this more accurate
         */
    double sunZenithAngleToTime(double z);

  private:
    void update();

    /**
          * This function computes the rise and set time for the given SkyObject. This is done in order to
          * have a common function for the computation of the Sun and Moon rise and set times.
          */
    void RiseSetTime(SkyObject *o, double *riseTime, double *setTime, QTime *RiseTime, QTime *SetTime);

    /**
         * Computes astronomical twilight for dawn and dusk
         * @note Code duplication -- copied from Alt vs Time
         */
    void findDawnDusk();

    /**
         * Computes the moon phase at the given date/time
         */
    void findMoonPhase();

    /**
         * FIXME: More code duplication!
         * findAltitude should really be part of KSEngine. Copying from ObservingList.
         * returns in degrees
         */
    double findAltitude(const SkyPoint *p, double hour);

    KSSun m_Sun;
    KSMoon m_Moon;
    KStarsDateTime dt;

    const GeoLocation *geo { nullptr };
    double SunRise { 0 };
    double SunSet { 0 };
    double MoonRise { 0 };
    double MoonSet { 0 };
    double DuskAstronomicalTwilight { 0 };
    double DawnAstronomicalTwilight { 0 };
    double SunMinAlt { 0 };
    double SunMaxAlt { 0 };
    double MoonPhase { 0 };
    QTime SunRiseT, SunSetT, MoonRiseT, MoonSetT, DuskAstronomicalTwilightT, DawnAstronomicalTwilightT;
};
