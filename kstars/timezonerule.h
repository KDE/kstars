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

/**
	*TimeZoneRule provides the information needed to determine whether Daylight
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
  */

#include <qdatetime.h>
#include <qstring.h>

class TimeZoneRule {
public: 
	TimeZoneRule(); //For the "empty" rule (no daylight savings time)
	TimeZoneRule( QString smonth, QString sday, QTime stime, QString rmonth, QString rday, QTime rtime, double offset=1.00 );
	~TimeZoneRule();
	bool isDSTActive( QDateTime date );
	QDateTime nextDSTChange( QDateTime date );
	QDateTime previousDSTChange( QDateTime date );
	void setDST( bool activate=true );
	double deltaTZ() const { return dTZ; }

private:
	int initMonth( QString m );
	bool initDay( QString d, int &day, int &week );
	int findStartDay( QDateTime d );
	int findRevertDay( QDateTime d );

	int StartMonth, RevertMonth;
	int StartDay, RevertDay;
	int StartWeek, RevertWeek;
	QTime StartTime, RevertTime;
	double dTZ, HourOffset;
};

#endif
