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
	*@author Jason Harris
	*@version 1.0
	*/

class KStarsDateTime : public ExtDateTime
{
public:
	KStarsDateTime();
	KStarsDateTime( long int jd );
	KStarsDateTime( const KStarsDateTime &kdt );
	KStarsDateTime( const ExtDate &_d, const QTime &_t );
	KStarsDateTime( double djd );
	KStarsDateTime( long double djd );
	
	void setDJD( long double jd );
	void setDate( const ExtDate &d );
	void setTime( const QTime &t ); 
	
	KStarsDateTime addSecs( long double s ) const { return KStarsDateTime( djd() + s/86400. ); }
	KStarsDateTime addDays( int nd ) const { return KStarsDateTime( djd() + (long double)nd ); }

	bool operator == ( const KStarsDateTime &d ) const { return DJD == d.djd(); }
	bool operator != ( const KStarsDateTime &d ) const { return DJD != d.djd(); }
	bool operator  < ( const KStarsDateTime &d ) const { return DJD  < d.djd(); }
	bool operator <= ( const KStarsDateTime &d ) const { return DJD <= d.djd(); }
	bool operator  > ( const KStarsDateTime &d ) const { return DJD  > d.djd(); }
	bool operator >= ( const KStarsDateTime &d ) const { return DJD >= d.djd(); }
	
	/**@return the date and time according to the CPU clock (note that this is not
		*necessarily UT)
		*/
	static KStarsDateTime currentDateTime();
	
/**@return the julian day as a long double, including the time as the fractional portion.
	*/
	long double djd() const { return DJD; }

/**@return the fraction of the Julian Day corresponding to the current time.
	*
	*Because the integer Julian Day value jd() is referenced to Noon on the current date,
	*jdFrac() ranges between values of -0.5 and +0.5 for the previous and next midnights,
	*respectively.
	*/
	double jdFrac() const { return ((time().hour()-12) + (time().minute() 
			+ (time().second() + time().msec()/1000.)/60.)/60.)/24.; }
	long double JDat0hUT() const { return int( djd() - 0.5 ) + 0.5; }
	
	dms gst() const;
	QTime GSTtoUT( dms GST ) const;
	
	double epoch() const { return ( double( date().year() ) 
			+ double( date().dayOfYear()/date().daysInYear() ) ); }
	void setFromEpoch( double e );
private:
	dms GSTat0hUT() const;
	long double DJD;
};

#endif  //KSTARSDATETIME_H

