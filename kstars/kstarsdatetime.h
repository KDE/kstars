/***************************************************************************
                          kstarsdatetime.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 05 May 2004
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSTARSDATETIME_H_
#define KSTARSDATETIME_H_

#define J2000 2451545.0 //Julian Date for noon on Jan 1, 2000 (epoch J2000)
#define B1950 2433282.4235  // Julian date for Jan 0.9235, 1950
#define SIDEREALSECOND 1.002737909 //number of sidereal seconds in one solar second

#include <QDateTime>

class dms;

/**@class KStarsDateTime
 *@short Extension of QDateTime for KStars
 *KStarsDateTime can represent the date/time as a Julian Day, using a long double, 
 *in which the fractional portion encodes the time of day to a precision of a less than a second.
 *Also adds Greenwich Sidereal Time and "epoch", which is just the date expressed as a floating
 *point number representing the year, with the fractional part representing the date and time
 *(with poor time resolution; typically the Epoch is only taken to the hundredths place, which is 
 *a few days).
 *@note Local time and Local sideral time are not handled here.  Because they depend on the 
 *geographic location, they are part of the GeoLocation class.
 *@sa GeoLocation::GSTtoLST()
 *@sa GeoLocation::UTtoLT()
 *@author Jason Harris
 *@version 1.0
 */

class KStarsDateTime : public QDateTime
{
public:
    /**
     *@short Default constructor  
     *Creates a date/time at J2000 (noon on Jan 1, 200)
     */
    KStarsDateTime();

    /**
     *@short Constructor  
     *Creates a date/time at the specified Julian Day.
     *@p jd The Julian Day
     */
    KStarsDateTime( long double djd );

    /**
     *@short Copy constructor
     *@p kdt The KStarsDateTime object to copy.
     */
    KStarsDateTime( const KStarsDateTime &kdt );

    /**
     *@short Copy constructor
     *@p qdt The QDateTime object to copy.
     */
    KStarsDateTime( const QDateTime &qdt );

    /**
     *@short Constructor
     *Create a KStarsDateTimne based on the specified Date and Time.
     *@p _d The QDate to assign
     *@p _t The QTime to assign
     */
    KStarsDateTime( const QDate &_d, const QTime &_t );

    /**
     *Assign the (long double) Julian Day value, which includes the time of day
     *encoded in the fractional portion.
     *@p jd the Julian Day value to assign.
     */
    void setDJD( long double jd );

    /**
     *Assign the Date according to a QDate object.
     *@p d the QDate to assign
     */
    void setDate( const QDate &d );

    /**
     *Assign the Time according to a QTime object.
     *@p t the QTime to assign
     */
    void setTime( const QTime &t );

    /**
     *@return a KStarsDateTime that is the given number of seconds later 
     *than this KStarsDateTime.  
     *@p s the number of seconds to add.  The number can be negative.
     */
    KStarsDateTime addSecs( double s ) const;

    /**
     *Modify the Date/Time by adding a number of days.  
     *@p nd the number of days to add.  The number can be negative.
     */
    inline KStarsDateTime addDays( int nd ) const { return KStarsDateTime( djd() + (long double)nd ); }

    inline bool operator == ( const KStarsDateTime &d ) const { return DJD == d.djd(); }
    inline bool operator != ( const KStarsDateTime &d ) const { return DJD != d.djd(); }
    inline bool operator  < ( const KStarsDateTime &d ) const { return DJD  < d.djd(); }
    inline bool operator <= ( const KStarsDateTime &d ) const { return DJD <= d.djd(); }
    inline bool operator  > ( const KStarsDateTime &d ) const { return DJD  > d.djd(); }
    inline bool operator >= ( const KStarsDateTime &d ) const { return DJD >= d.djd(); }

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
    static KStarsDateTime fromString( const QString &s );

    /**
     *@return the julian day as a long double, including the time as the fractional portion.
     */
    inline long double djd() const { return DJD; }

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
    QTime GSTtoUT( dms GST ) const; // FIXME: Shouldn't this be static?


    /**
     *@return the epoch value of the Date/Time.
     *@note the epoch is shorthand for the date, expressed as a floating-point year value.
     *@sa setFromEpoch()
     */
    inline double epoch() const { return ( double( date().year() )
                                        + double( date().dayOfYear() )/double( date().daysInYear() ) ); }

    /**
     *Set the Date/Time from an epoch value, represented as a double.
     *@p e the epoch value
     *@return true if date set successfully
     *@sa epoch()
     */
    bool setFromEpoch( double e );

    /**
     *Set the Date/Time from an epoch value, represented as a string.
     *@p e the epoch value
     *@return true if date set successfully
     *@sa epoch()
     */
    bool setFromEpoch( const QString &e );


private:
    /**
     *@return the Greenwich Sidereal Time at 0h UT on this object's Date
     *@note used internally by gst() and GSTtoUT()
     */
    dms GSTat0hUT() const;

    long double DJD;
};

#endif  //KSTARSDATETIME_H_

