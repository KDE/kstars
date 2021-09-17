/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

class KSAlmanac
{
  public:
    /**
         *@brief KSAlmanac constructor initializing an almanac for the current KStarsData::Instance geolocation and time.
         */
    KSAlmanac();

    /**
         *@brief KSAlmanac constructor initializing an almanac for an arbitrary geolocation and time.
         *@param midnight is the midnight date and time to consider as beginning of the day at the "geo" location.
         *@param geo is the GeoLocation to use for this almanac, defaulting to the KStarsData::Instance geolocation.
         *@note if the timespec of midnight is local time, its UTC value at the geolocation "geo" will be used instead.
         */
    KSAlmanac(const KStarsDateTime &midnight, const GeoLocation *geo = nullptr);

    /**
         *@short Get/set the date for computations to the given date.
         *@param utc_midnight and local_midnight are the midnight date and time to consider as beginning of the day at the geo_ location, either UTC or local.
         *@note The time must be the local time midnight of the day the almanac is required for, so that
         *      resulting ephemerides are calculated around that time.
         *@note These functions are not merged into a single timespec-aware one for backwards compatilibity.
         */
    /** @{ */
    void setDate(const KStarsDateTime &utc_midnight);
    void setDateFromLT(const KStarsDateTime &local_midnight) { setDate(geo->LTtoUT(local_midnight)); }
    KStarsDateTime getDate() const { return dt; }
    /** @} */

    /**
         *@short Set the location for computations to the given location
         *@param geo_ The location to set for computations
         */
    void setLocation(const GeoLocation *geo_);

    /**
         *All the functions returns the fraction of the day given by getDate()
         *as their return value
         */
    inline double getSunRise() const { return SunRise; }
    inline double getSunSet() const { return SunSet; }
    inline double getMoonRise() const { return MoonRise; }
    inline double getMoonSet() const { return MoonSet; }
    inline double getDuskAstronomicalTwilight() const { return DuskAstronomicalTwilight; }
    inline double getDawnAstronomicalTwilight() const { return DawnAstronomicalTwilight; }

    /**
         *These functions return the max and min altitude of the sun during the course of the day in degrees
         */
    inline double getSunMaxAlt() const { return SunMaxAlt; }
    inline double getSunMinAlt() const { return SunMinAlt; }

    /**
         *@return the moon phase in degrees at the given date/time. Ranges is [0, 180]
         */
    inline double getMoonPhase() const { return MoonPhase; }

    /**
         *@return get the moon illuminated fraction at the given date/time. Range is [0.,1.]
         */
    inline double getMoonIllum() const { return m_Moon.illum(); }

    inline QTime sunRise() const { return SunRiseT; }
    inline QTime sunSet() const { return SunSetT; }
    inline QTime moonRise() const { return MoonRiseT; }
    inline QTime moonSet() const { return MoonSetT; }

    /**
         *@short Convert the zenithal distance of the sun to fraction of the day
         *@param z Zenithal angular distance
         *@return Time as a fraction of the day, at which the zenithal distance is attained by the sun
         *@note This is accurate only for zenithal angles close to sunset. TODO: Make this more accurate
         */
    double sunZenithAngleToTime(double z) const;

  private:
    void update();

    /**
          * This function computes the rise and set time for the given SkyObject. This is done in order to
          * have a common function for the computation of the Sun and Moon rise and set times.
          */
    void RiseSetTime(SkyObject *o, double *riseTime, double *setTime, QTime *RiseTime, QTime *SetTime);

    /**
         * Compute the dawn and dusk times in a [-12,+12] hours around the day midnight of this KSAlmanac, if any, as well as min and max altitude.
         * - If the day midnight of this KSAlmanac is during astronomical night time, dusk will be before dawn.
         * - If the day midnight of this KSAlmanac is during twilight or day time, dawn will be before dusk.
         * - If there is no astronomical night time, dawn and dusk will be set to the time of minimal altitude of the Sun.
         * - If there is no twilight or day time, dawn and dusk will be set to the time of minimal altitude of the Sun.
         */
    void findDawnDusk(double altitude = -18.0);

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
