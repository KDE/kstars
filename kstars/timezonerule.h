/***************************************************************************
                          timezonerule.h  -  description
                             -------------------
    begin                : Tue Apr 2 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TIMEZONERULE_H
#define TIMEZONERULE_H

/**@class TimeZoneRule 
	*This class provides the information needed to determine whether Daylight
	*Savings Time (DST; a.k.a. "Summer Time") is currently active at a given 
	*location.  There are (at least) 25 different "rules" which govern DST 
	*around the world; a string identifying the appropriate rule is attachded 
	*to each city in Cities.dat.
	*
	*The rules themselves are stored in the TZrulebook.dat file, which is read
	*on startup; each line in the file creates a TimeZoneRule object.
	*
	*TimeZoneRule consists of QStrings identifying the months and days on which
	*DST starts and ends, QTime objects identifying the time at which the
	*changes occur, and a double indicating the size of the offset in hours
	*(probably always 1.00).
	*
	*Month names should be the English three-letter abbreviation, uncapitalized.
	*Day names are either an integer indicating the calendar date (i.e., "15" is
	*the fifteenth of the month), or a number paired with a three-letter
	*abbreviation for a weekday.  This indicates the Nth weekday of the month
	*(i.e., "2sun" is the second Sunday of the Month).  Finally, the weekday
	*string on its own indicates the last weekday of the month (i.e.,
	*"mon" is the last Monday of the month).
	*
	*The isDSTActive(KStarsDateTime) function returns true if DST is active for the
	*DateTime given as an argument.
	*
	*The nextDSTChange(KStarsDateTime) function returns the KStarsDateTime of the moment
	*at which the next DST change will occur for the current location.
	*@author Jason Harris
	*@version 1.0
	*/

#include <qstring.h>
#include "kstarsdatetime.h"

class TimeZoneRule {
public: 
	/**Default Constructor. Makes the "empty" time zone rule (i.e., no DST correction)*/
	TimeZoneRule();

	/**Constructor. Create a TZ rule according to the arguments.
		*@p smonth the three-letter code for the month in which DST starts
		*@p sday a string encoding the day on which DST starts (see the class description)
		*@p stime the time at which DST starts
		*@p rmonth the three-letter code for the month in which DST reverts
		*@p rday a string encoding the day on which DST reverts (see the class description)
		*@p rtime the time at which DST reverts
		*@p offset the offset between normal time and DS time (always 1.00?)
		*/
	TimeZoneRule( const QString &smonth, const QString &sday, const QTime &stime, 
		const QString &rmonth, const QString &rday, const QTime &rtime, const double &offset=1.00 );

	/**Destructor. (empty)*/
	~TimeZoneRule();

	/**Determine whether DST is in effect for the given DateTime, according to this rule 
		*@p date the date/time to test for DST
		*/
	bool isDSTActive( const KStarsDateTime &date );

	/**@return TRUE if the rule is the "empty" TZ rule. */
	bool isEmptyRule() { return ( HourOffset == 0.0 ); }

	/** Toggle DST on/off.  The @p activate argument should probably be isDSTActive() 
		*@param activate if TRUE, then set DST active; otherwise, deactivate DST
		*/
	void setDST( bool activate=true );

	/**@return the current Timezone offset, compared to the timezone's Standard Time.
		*This is typically 0.0 if DST is inactive, and 1.0 if DST is active. */
	double deltaTZ() const { return dTZ; }

/** Recalculate next dst change and if DST is active by a given local time with 
	*timezone offset and time direction.
	*@param ltime the time to be tested
	*@param time_runs_forward time direction
	*@param TZoffset offset of timezone in some fictional unit
	*@param automaticDSTchange is automatic DST change?
	*
	* @todo Check dox for TZoffset
	*/
	void reset_with_ltime( KStarsDateTime &ltime, const double TZoffset, const bool time_runs_forward,
												const bool automaticDSTchange = false );

	/**@return computed value for next DST change in universal time.
		*/
	KStarsDateTime nextDSTChange() { return next_change_utc; }

	/**@return computed value for next DST change in local time.
		*/
	KStarsDateTime nextDSTChange_LTime() { return next_change_ltime; }

	/**@return TRUE if this rule is the same as the argument.
		*@p r the rule to check for equivalence
		*/
	bool equals( TimeZoneRule *r );

	int StartMonth, RevertMonth;

private:

	/**@return the KStarsDateTime of the moment when the next DST change will occur in local time
		*This is useful because DST change times are saved in local times*/
	void nextDSTChange_LTime( const KStarsDateTime &date );

	/**@return the KStarsDateTime of the moment when the last DST change occurred in local time
		*This is useful because DST change times are saved in local times
		*We need this in case time is running backwards. */
	void previousDSTChange_LTime( const KStarsDateTime &date );

	/**calculate the next DST change in universal time for current location */
	void nextDSTChange( const KStarsDateTime &local_date, const double TZoffset );

	/**calculate the previous DST change in universal time for current location */
	void previousDSTChange( const KStarsDateTime &local_date, const double TZoffset );

	/**Interpret the string as a month of the year; 
		*@return the month integer (1=jan ... 12=dec) 
		*/
	int initMonth( const QString &m );

	/**Interpret the day string as a week ID and a day-of-week ID.  The day-of-week
		*is an integer between 1 (sunday) and 7 (saturday); the week integer can
		*be 1-3 (1st/2nd/third weekday of the month), or 5 (last weekday of the month) 
		*@p day the day integer is returned by reference through this value
		*@p week the week integer is returned by reference through this value
		*@return TRUE if the day string was successfully parsed 
		*/
	bool initDay( const QString &d, int &day, int &week );

	/**Find the calendar date on which DST starts for the calendar year
		*of the given KStarsDateTime.
		*@p d the date containing the year to be tested
		*@return the calendar date, an integer between 1 and 31. */
	int findStartDay( const KStarsDateTime &d );

	/**Find the calendar date on which DST ends for the calendar year
		*of the given KStarsDateTime.
		*@p d the date containing the year to be tested
		*@return the calendar date, an integer between 1 and 31. */
	int findRevertDay( const KStarsDateTime &d );

	int StartDay, RevertDay;
	int StartWeek, RevertWeek;
	QTime StartTime, RevertTime;
	KStarsDateTime next_change_utc, next_change_ltime;
	double dTZ, HourOffset;
	
};

#endif
