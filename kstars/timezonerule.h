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

/**TimeZoneRule provides the information needed to determine whether Daylight
	*Savings Time (DST) is currently active at a given location.  There are 25
	*different "rules" which govern DST around the world; a string identifying
	*the appropriate rule is attachded to each city in Cities.dat.
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
	*The isDSTActive(QDateTime) function returns true if DST is active for the
	*DateTime given as an argument.
	*
	*The nextDSTChange(QDateTime) function returns the QDateTime of the moment
	*at which the next DST change will occur for the current location.
	*@author Jason Harris
	*@version 0.9
	*/

#include <qdatetime.h>
#include <qstring.h>

class TimeZoneRule {
public: 
/**Default Constructor. Makes the "empty" time zone rule (i.e., no DST correction)*/
	TimeZoneRule();

/**Constructor. Make a TZ rule according to arguments.*/
	TimeZoneRule( QString smonth, QString sday, QTime stime, QString rmonth, QString rday, QTime rtime, double offset=1.00 );

/**Destructor. (empty)*/
	~TimeZoneRule();

/**Determine whether DST is in effect for the given DateTime, according to this rule */
	bool isDSTActive( QDateTime date );

/**@returns true if the rule is the "empty" TZ rule. */
	bool isEmptyRule( void ) { if ( HourOffset ) return false; else return true; }

/**@returns the QDateTime of the moment when the next DST change will occur */
	QDateTime nextDSTChange( QDateTime date );

/**@returns the QDateTime of the moment when the last DST change occurred.
	*We need this in case time is running backwards. */
	QDateTime previousDSTChange( QDateTime date );

/**Toggle DST on/off.  The "activate" argument should always be isDSTActive() */
	void setDST( bool activate=true );

/**@returns the current Timezone offset, compared to the timezone's Standard Time.
	*This is typically 0.0 if DST is inactive, and 1.0 if DST is active. */
	double deltaTZ() const { return dTZ; }

	bool equals( TimeZoneRule *r );

	int StartMonth, RevertMonth;

private:
/**Interpret the string as a month of the year; return the month integer
	*(1=jan; 12=dec) */
	int initMonth( QString m );

/**Interpret the day string as a week ID and a day-of-week ID.  The day-of-week
	*is an integer between 1 (sunday) and 7 (saturday); the week integer can
	*be 1-3 (1st/2nd/third weekday of the month), or 5 (last weekday of the month) */
	bool initDay( QString d, int &day, int &week );

/**Find the calendar date on which DST starts for the calendar year
	*of the given QDateTime.
	*@returns the calendar date, an integer between 1 and 31. */
	int findStartDay( QDateTime d );

/**Find the calendar date on which DST ends for the calendar year
	*of the given QDateTime.
	*@returns the calendar date, an integer between 1 and 31. */
	int findRevertDay( QDateTime d );

	int StartDay, RevertDay;
	int StartWeek, RevertWeek;
	QTime StartTime, RevertTime;
	double dTZ, HourOffset;
};

#endif
