/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>

#define J2000          2451545.0    //Julian Date for noon on Jan 1, 2000 (epoch J2000)
#define B1950          2433282.4235 // Julian date for Jan 0.9235, 1950
#define SIDEREALSECOND 1.002737909  //number of sidereal seconds in one solar second

class dms;

/** @class KStarsDateTime
 *@short Extension of QDateTime for KStars
 *KStarsDateTime can represent the date/time as a Julian Day, using a long double,
 *in which the fractional portion encodes the time of day to a precision of a less than a second.
 *Also adds Greenwich Sidereal Time and "epoch", which is just the date expressed as a floating
 *point number representing the year, with the fractional part representing the date and time
 *(with poor time resolution; typically the Epoch is only taken to the hundredths place, which is
 *a few days).
 *@note Local time and Local sideral time are not handled here.  Because they depend on the
 *geographic location, they are part of the GeoLocation class.
 *@note The default timespec is UTC unless the passed value has different timespec value.
 *@sa GeoLocation::GSTtoLST()
 *@sa GeoLocation::UTtoLT()
 *@author Jason Harris
 *@author Jasem Mutlaq
 *@version 1.1
 */

class KStarsDateTime : public QDateTime
{
    public:
        /**
             *@short Default constructor
             *Creates a date/time at J2000 (noon on Jan 1, 200)
             *@note This sets the timespec to UTC.
             */
        KStarsDateTime();

        /**
             *@short Constructor
             *Creates a date/time at the specified Julian Day.
             *@p jd The Julian Day
             *@note This sets the timespec to UTC.
             */
        explicit KStarsDateTime(long double djd);

        /**
             *@short Copy constructor
             *@p kdt The KStarsDateTime object to copy.
             *@note The timespec is copied from kdt.
             */
        /** @{ */
        KStarsDateTime(const KStarsDateTime &kdt);
        KStarsDateTime& operator=(const KStarsDateTime &kdt) noexcept;
        /** @} */

        /**
             *@short Copy constructor
             *@p qdt The QDateTime object to copy.
             *@note The timespec is copied from qdt.
             */
        explicit KStarsDateTime(const QDateTime &qdt);

        /**
             *@short Constructor
             *Create a KStarsDateTimne based on the specified Date and Time.
             *@p _d The QDate to assign
             *@p _t The QTime to assign
             *@p timespec The desired timespec, UTC by default.
             */
        KStarsDateTime(const QDate &_d, const QTime &_t, Qt::TimeSpec timeSpec = Qt::UTC);

        /**
             *Assign the static_cast<long double> Julian Day value, which includes the time of day
             *encoded in the fractional portion.
             *@p jd the Julian Day value to assign.
             */
        void setDJD(long double jd);

        /**
             *Assign the Date according to a QDate object.
             *@p d the QDate to assign
             */
        void setDate(const QDate &d);

        /**
             *Assign the Time according to a QTime object.
             *@p t the QTime to assign
             *@note timespec is NOT changed even if the passed QTime has a different timespec than current.
             */
        void setTime(const QTime &t);

        /**
             *@return a KStarsDateTime that is the given number of seconds later
             *than this KStarsDateTime.
             *@p s the number of seconds to add.  The number can be negative.
             */
        KStarsDateTime addSecs(double s) const;

        /**
             *Modify the Date/Time by adding a number of days.
             *@p nd the number of days to add.  The number can be negative.
             */
        inline KStarsDateTime addDays(int nd) const
        {
            return KStarsDateTime(djd() + static_cast<long double>(nd));
        }

        inline bool operator==(const KStarsDateTime &d) const
        {
            return DJD == d.djd();
        }
        inline bool operator!=(const KStarsDateTime &d) const
        {
            return DJD != d.djd();
        }
        inline bool operator<(const KStarsDateTime &d) const
        {
            return DJD < d.djd();
        }
        inline bool operator<=(const KStarsDateTime &d) const
        {
            return DJD <= d.djd();
        }
        inline bool operator>(const KStarsDateTime &d) const
        {
            return DJD > d.djd();
        }
        inline bool operator>=(const KStarsDateTime &d) const
        {
            return DJD >= d.djd();
        }

        /**
             *@return the date and time according to the CPU clock
             */
        static KStarsDateTime currentDateTime();

        /**
             *@return the UTC date and time according to the CPU clock
             */
        static KStarsDateTime currentDateTimeUtc();

        /**
             *@return a KStarsDateTime object parsed from the given string.
             *@note This function is format-agnostic; it will try several formats
             *when parsing the string.
             *@param s the string expressing the date/time to be parsed.
             */
        static KStarsDateTime fromString(const QString &s);

        /**
             *@return the julian day as a long double, including the time as the fractional portion.
             */
        inline long double djd() const
        {
            return DJD;
        }

        /**
             *@return The Greenwich Sidereal Time
             *The Greenwich sidereal time is the Right Ascension coordinate that is currently transiting
             *the Prime Meridian at the Royal Observatory in Greenwich, UK (longitude=0.0)
             */
        dms gst() const;

        /**
             *Convert a given Greenwich Sidereal Time to Universal Time (=Greenwich Mean Time).
             *@p GST the Greenwich Sidereal Time to convert to Universal Time.
             */
        QTime GSTtoUT(dms GST) const; // FIXME: Shouldn't this be static?

        /**
             *@enum EpochType description options
             *@note After 1976, the IAU standard for epochs is Julian Years.
             */
        enum EpochType
        {
            JULIAN,    /**< Julian epoch (see http://scienceworld.wolfram.com/astronomy/JulianEpoch.html) */
            BESSELIAN, /**< Besselian epoch (see http://scienceworld.wolfram.com/astronomy/BesselianEpoch.html) */
        };

        /**
             *@return the (Julian) epoch value of the Date/Time.
             *@short This is (approximately) the year expressed as a floating-point value
             *@sa setFromEpoch()
             *@note The definition of Julian Epoch used here comes from http://scienceworld.wolfram.com/astronomy/JulianEpoch.html
             */
        inline double epoch() const
        {
            return 2000.0 + (djd() - J2000) / 365.25;
        }

        /**
             *Set the Date/Time from an epoch value, represented as a double.
             *@p e the epoch value
             *@sa epoch()
             */
        bool setFromEpoch(double e, EpochType type);

        /**
             *Set the Date/Time from an epoch value, represented as a string.
             *@p e the epoch value
             *@return true if date set successfully
             *@sa epoch()
             */
        bool setFromEpoch(const QString &e);

        /**
             *Set the Date/Time from an epoch value, represented as a double.
             *@p e the epoch value
             *@note This method assumes that the epoch 1950.0 is Besselian, otherwise assumes that the epoch is a Julian epoch. This is provided for backward compatibility, and because custom catalogs may still use 1950.0 to mean B1950.0 despite the IAU standard for epochs being Julian.
             *@sa epoch()
             */
        void setFromEpoch(double e);

        /**
             *@short Takes in an epoch and returns a Julian Date
             *@return the Julian Date (date with fraction)
             *@param epoch A floating-point year value specifying the Epoch
             *@param type JULIAN or BESSELIAN depending on what convention the epoch is specified in
             */
        static long double epochToJd(double epoch, EpochType type = JULIAN);

        /**
             *@short Takes in a Julian Date and returns the corresponding epoch year in the given system
             *@return the epoch as a floating-point year value
             *@param jd Julian date
             *@param type Epoch system (KStarsDateTime::JULIAN or KStarsDateTime::BESSELIAN)
             */
        static double jdToEpoch(long double jd, EpochType type = JULIAN);

        /**
             *@short Takes in a string and returns a Julian epoch
             */
        static double stringToEpoch(const QString &eName, bool &ok);

        /**
             * The following values were obtained from Eric Weisstein's world of science:
             * http://scienceworld.wolfram.com/astronomy/BesselianEpoch.html
             */
        constexpr static const double B1900        = 2415020.31352; // Julian date of B1900 epoch
        constexpr static const double JD_PER_BYEAR = 365.242198781; // Julian days in a Besselian year
    private:
        /**
             *@return the Greenwich Sidereal Time at 0h UT on this object's Date
             *@note used internally by gst() and GSTtoUT()
             */
        dms GSTat0hUT() const;

        long double DJD { 0 };
};
