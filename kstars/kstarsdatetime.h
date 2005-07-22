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

#ifndef KSTARSDATETIME_H
#define KSTARSDATETIME_H

#define J2000 2451545.0 //Julian Date for noon on Jan 1, 2000 (epoch J2000)
                       //defined here because this file is included in every other class.
#define B1950 2433282.4235  // Julian date for Jan 0.9235, 1950
#define SIDEREALSECOND 1.002737909 //number of sidereal seconds in one solar second

#include "libkdeedu/extdate/extdatetime.h"

class dms;

/**@class KStarsDateTime
	*@short Extension of ExtDateTime for KStars
	*Instead of an integer Julian Day, KStarsDateTime uses a long double Julian Day, 
	*in which the fractional portion encodes the time of day to a precision of a less than a second.
	*Also adds Greenwich Sidereal Time.
	*@note Local time and Local sideral time are not handled here.  Because they depend on the 
	*geographic location, they are part of the GeoLocation class.
	*@sa GeoLocation::GSTtoLST()
	*@sa GeoLocation::UTtoLT()
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsDateTime : public ExtDateTime
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
	*@note this is overloaded from ExtDateTime.  It does not allow for assigning the 
	*time of day, because the jd argument is an integer
	*/
	KStarsDateTime( long int jd );

/**
	*@short Constructor  
	*Creates a date/time at the specified Julian Day.
	*@p jd The Julian Day
	*/
	KStarsDateTime( double djd );

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
	*@p kdt The ExtDateTime object to copy.
	*/
	KStarsDateTime( const ExtDateTime &kdt );

/**
	*@short Constructor
	*Create a KStarsDateTimne based on the specified Date and Time.
	*@p _d The ExtDate to assign
	*@p _t The QTime to assign
	*/
	KStarsDateTime( const ExtDate &_d, const QTime &_t );

/**
	*Assign the (long double) Julian Day value, which includes the time of day
	*encoded in the fractional portion.
	*@p jd the Julian Day value to assign.
	*/
	void setDJD( long double jd );

/**
	*Assign the Date according to an ExtDate object.
	*@p d the ExtDate to assign
	*/
	void setDate( const ExtDate &d );

/**
	*Assign the Time according to a QTime object.
	*@p t the QTime to assign
	*/
	void setTime( const QTime &t ); 
	
/**
	*Modify the Date/Time by adding a number of seconds.  
	*@p s the number of seconds to add.  The number can be negative.
	*/
	KStarsDateTime addSecs( long double s ) const { return KStarsDateTime( djd() + s/86400. ); }
	
/**
	*Modify the Date/Time by adding a number of days.  
	*@p nd the number of days to add.  The number can be negative.
	*/
	KStarsDateTime addDays( int nd ) const { return KStarsDateTime( djd() + (long double)nd ); }

	bool operator == ( const KStarsDateTime &d ) const { return DJD == d.djd(); }
	bool operator != ( const KStarsDateTime &d ) const { return DJD != d.djd(); }
	bool operator  < ( const KStarsDateTime &d ) const { return DJD  < d.djd(); }
	bool operator <= ( const KStarsDateTime &d ) const { return DJD <= d.djd(); }
	bool operator  > ( const KStarsDateTime &d ) const { return DJD  > d.djd(); }
	bool operator >= ( const KStarsDateTime &d ) const { return DJD >= d.djd(); }
	
/**
	*@return the date and time according to the CPU clock (note that this is not
	*necessarily UT)
	*/
	static KStarsDateTime currentDateTime();
	
/**
	*@return the julian day as a long double, including the time as the fractional portion.
	*/
	long double djd() const { return DJD; }

/**
	*@return the fraction of the Julian Day corresponding to the current time.
	*Because the integer Julian Day value jd() is referenced to Noon on the current date,
	*jdFrac() ranges between values of -0.5 and +0.5 for the previous and next midnights,
	*respectively.
	*/
	double jdFrac() const { return ((time().hour()-12) + (time().minute() 
			+ (time().second() + time().msec()/1000.)/60.)/60.)/24.; }

/**
	*@return the Julian Day value for the current date, but at 0h UT.  
	*@note the returned value is always an integer value + 0.5.
	*/
	long double JDat0hUT() const { return int( djd() - 0.5 ) + 0.5; }
	
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
	QTime GSTtoUT( dms GST ) const;
	

/**
	*@return the epoch value of the Date/Time.
	*@note the epoch is shorthand for the date, expressed as a floating-point year value.
	*@sa setFromEpoch()
	*/
	double epoch() const { return ( double( date().year() ) 
			+ double( date().dayOfYear() )/double( date().daysInYear() ) ); }

/**
	*Set the Date/Time from an epoch value.
	*@p e the epoch value
	*@sa epoch()
	*/
	void setFromEpoch( double e );

private:
/**
	*@return the Greenwich Sidereal Time at 0h UT on this object's Date
	*@note used internally by gst() and GSTtoUT()
	*/
	dms GSTat0hUT() const;

	long double DJD;
};

#endif  //KSTARSDATETIME_H

