/***************************************************************************
                          timezonerule.cpp  -  description
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

#include <kdebug.h>
#include <klocale.h>

#include "timezonerule.h"

TimeZoneRule::TimeZoneRule() {
	//Build the empty TimeZoneRule.
	StartMonth = 0;
	RevertMonth = 0;
	StartDay = 0;
	RevertDay = 0;
	StartWeek = -1;
	RevertWeek = -1;
	StartTime = QTime();
	RevertTime = QTime();
	HourOffset = 0.0;
}

TimeZoneRule::TimeZoneRule( QString smonth, QString sday, QTime stime,
			QString rmonth, QString rday, QTime rtime, double dh ) {

	if ( smonth != "0" ) {
		StartMonth = initMonth( smonth );
		RevertMonth = initMonth( rmonth );

		if ( StartMonth && RevertMonth && initDay( sday, StartDay, StartWeek ) &&
					initDay( rday, RevertDay, RevertWeek ) && stime.isValid() && rtime.isValid() ) {
			StartTime = stime;
			RevertTime = rtime;
			HourOffset = dh;
		} else {
			kdWarning() << i18n( "Error parsing TimeZoneRule, setting to empty rule." ) << endl;
			StartMonth = 0;
			RevertMonth = 0;
			StartDay = 0;
			RevertDay = 0;
			StartWeek = -1;
			RevertWeek = -1;
			StartTime = QTime();
			RevertTime = QTime();
			HourOffset = 0.0;
		}
	} else { //Empty rule
		StartMonth = 0;
		RevertMonth = 0;
		StartDay = 0;
		RevertDay = 0;
		StartWeek = -1;
		RevertWeek = -1;
		StartTime = QTime();
		RevertTime = QTime();
		HourOffset = 0.0;
	}
}

TimeZoneRule::~TimeZoneRule() {
}

void TimeZoneRule::setDST( bool activate ) {
	if ( activate ) {
		kdDebug() << i18n( "Daylight Saving Time active" ) << endl;
		dTZ = HourOffset;
	} else {
		kdDebug() << i18n( "Daylight Saving Time inactive" ) << endl;
		dTZ = 0.0;
	}
}

int TimeZoneRule::initMonth( QString mn ) {
//Check whether the argument is a three-letter English month code.
	QString ml = mn.lower();
	if ( ml == "jan" ) return 1;
	else if ( ml == "feb" ) return 2;
	else if ( ml == "mar" ) return 3;
	else if ( ml == "apr" ) return 4;
	else if ( ml == "may" ) return 5;
	else if ( ml == "jun" ) return 6;
	else if ( ml == "jul" ) return 7;
	else if ( ml == "aug" ) return 8;
	else if ( ml == "sep" ) return 9;
	else if ( ml == "oct" ) return 10;
	else if ( ml == "nov" ) return 11;
	else if ( ml == "dec" ) return 12;

	kdWarning() << i18n( "Could not parse " ) << mn << i18n( " as a valid month code." ) << endl;
	return false;
}

bool TimeZoneRule::initDay( QString dy, int &Day, int &Week ) {
//Three possible ways to express a day.
//1. simple integer; the calendar date...set Week=0 to indicate that Date is not the day of the week
	bool ok;
	int day = dy.toInt( &ok );
	if ( ok ) {
		Day = day;
		Week = 0;
		return true;
	}

	QString dl = dy.lower();
//2. 3-letter day of week string, indicating the last of that day of the month
//   ...set Week to 5 to indicate the last weekday of the month
	if ( dl == "mon" ) { Day = 1; Week = 5; return true; }
	else if ( dl == "tue" ) { Day = 2; Week = 5; return true; }
	else if ( dl == "wed" ) { Day = 3; Week = 5; return true; }
	else if ( dl == "thu" ) { Day = 4; Week = 5; return true; }
	else if ( dl == "fri" ) { Day = 5; Week = 5; return true; }
	else if ( dl == "sat" ) { Day = 6; Week = 5; return true; }
	else if ( dl == "sun" ) { Day = 7; Week = 5; return true; }

//3. 1,2 or 3 followed by 3-letter day of week string; this indicates
//   the (1st/2nd/3rd) weekday of the month.
	int wn = dl.left(1).toInt();
	if ( wn >0 && wn <4 ) {
		QString dm = dl.mid( 1, dl.length() ).lower();
		if ( dm == "mon" ) { Day = 1; Week = wn; return true; }
		else if ( dm == "tue" ) { Day = 2; Week = wn; return true; }
		else if ( dm == "wed" ) { Day = 3; Week = wn; return true; }
		else if ( dm == "thu" ) { Day = 4; Week = wn; return true; }
		else if ( dm == "fri" ) { Day = 5; Week = wn; return true; }
		else if ( dm == "sat" ) { Day = 6; Week = wn; return true; }
		else if ( dm == "sun" ) { Day = 7; Week = wn; return true; }
	}

	kdWarning() << i18n( "Could not parse " ) << dy << i18n( " as a valid day code." ) << endl;
	return false;
}

int TimeZoneRule::findStartDay( QDateTime d ) {
//Determine the calendar date of StartDay for the month and year of the given date.
	QDate test;

//TimeZoneRule is empty, return -1
	if ( isEmptyRule() ) return -1;

//If StartWeek=0, just return the integer.
	if ( StartWeek==0 ) return StartDay;

//Since StartWeek was not zero, StartDay is the day of the week, not the calendar date
	else if ( StartWeek==5 ) { //count back from end of month until StartDay is found.
		for ( test = QDate( d.date().year(), d.date().month(), d.date().daysInMonth() );
					test.day() > 21; test = test.addDays( -1 ) )
			if ( test.dayOfWeek() == StartDay ) break;
	} else { //Count forward from day 1, 8 or 15 (depending on StartWeek) until correct day of week is found
		for ( test = QDate( d.date().year(), d.date().month(), (StartWeek-1)*7 + 1 );
					test.day() < 7*StartWeek; test = test.addDays( 1 ) )
			if ( test.dayOfWeek() == StartDay ) break;
	}
	return test.day();
}

int TimeZoneRule::findRevertDay( QDateTime d ) {
//Determine the calendar date of RevertDay for the month and year of the given date.
	QDate test;

//TimeZoneRule is empty, return -1
	if ( isEmptyRule() ) return -1;

//If RevertWeek=0, just return the integer.
	if ( RevertWeek==0 ) return RevertDay;

//Since RevertWeek was not zero, RevertDay is the day of the week, not the calendar date
	else if ( RevertWeek==5 ) { //count back from end of month until RevertDay is found.
		for ( test = QDate( d.date().year(), d.date().month(), d.date().daysInMonth() );
					test.day() > 21; test = test.addDays( -1 ) )
			if ( test.dayOfWeek() == RevertDay ) break;
	} else { //Count forward from day 1, 8 or 15 (depending on RevertWeek) until correct day of week is found
		for ( test = QDate( d.date().year(), d.date().month(), (RevertWeek-1)*7 + 1 );
					test.day() < 7*RevertWeek; test = test.addDays( 1 ) )
			if ( test.dayOfWeek() == StartDay ) break;
	}
	return test.day();
}

bool TimeZoneRule::isDSTActive( QDateTime date ) {
//The empty rule always returns false
	if ( HourOffset == 0.0 ) return false;

//First, check whether the month is outside the DST interval.  Note that
//the interval chack is different if StartMonth > RevertMonth (indicating that
//the DST interval includes the end of the year).
	int month = date.date().month();
	if ( StartMonth < RevertMonth )
		if ( month < StartMonth || month > RevertMonth ) return false;
	else
		if ( month < StartMonth && month > RevertMonth ) return false;

//OK, if the month is equal to StartMonth or Revert Month, we have more
//detailed checking to do...
	int day = date.date().day();

//Subtlety: we need to know if DST is presently in use.  Imagine the time is
//30 minutes before RevertTime...it's ambiguous whether we are actually approaching
//RevertTime, or whether we've reverted already, and are re-living the hour...
	QTime rtime = RevertTime;
	if ( !deltaTZ() ) rtime = RevertTime.addSecs( int( -1*3600 ) );

	if ( month == StartMonth ) {
		int sday = findStartDay( date );
		if ( day < sday ) return false;
		else if ( day==sday && date.time() < StartTime ) return false;
	} else if ( month == RevertMonth ) {
		int rday = findRevertDay( date );
		if ( day > rday ) return false;
		else if ( day==rday && date.time() > rtime ) return false;
	}

	//passed all tests, so we must be in DST.
	return true;
}

QDateTime TimeZoneRule::nextDSTChange( QDateTime date ) {
	QDateTime result;

//return a very remote date if the rule is the empty rule.
	if ( isEmptyRule() ) return QDateTime( QDate( 8000, 1, 1 ), QTime() );

	if ( isDSTActive( date ) ) {
		//Next change is reverting back to standard time.
		result = QDateTime( QDate( date.date().year(), RevertMonth, 1 ), RevertTime );
		result = QDateTime( QDate( date.date().year(), RevertMonth, findRevertDay( result ) ), RevertTime );

		//It's possible that the revert date occurs in the next calendar year.
		//this is the case if RevertMonth is less than the date's month.
		//if ( RevertMonth < date.date().month() ) result = result.addYears(1); 
		if ( RevertMonth < date.date().month() ) result = QDateTime( QDate(result.date().year()+1, result.date().month(), result.date().day()), RevertTime);
	} else if ( StartMonth ) {
		//Next change is starting DST.
		result = QDateTime( QDate( date.date().year(), StartMonth, 1 ), StartTime );
		result = QDateTime( QDate( date.date().year(), StartMonth, findStartDay( result ) ), StartTime );

		//It's possible that the start date occurs in the next calendar year.
		//this is the case if StartMonth is less than the date's month.
	//	if ( StartMonth < date.date().month() ) result = result.addYears(1);
		if ( StartMonth < date.date().month() ) result = QDateTime( QDate(result.date().year()+1, result.date().month(), result.date().day() ), StartTime);
	}

	kdDebug() << i18n( "Next Daylight Savings Time change: " ) << result.toString() << endl;
	return result;
}

QDateTime TimeZoneRule::previousDSTChange( QDateTime date ) {
	QDateTime result;

	//return a very remote date if the rule is the empty rule
	if ( isEmptyRule() ) return QDateTime( QDate( 1783, 1, 1 ), QTime() );

	if ( isDSTActive( date ) ) {
		//Last change was starting DST.
		result = QDateTime( QDate( date.date().year(), StartMonth, 1 ), StartTime );
		result = QDateTime( QDate( date.date().year(), StartMonth, findStartDay( result ) ), StartTime );

		//It's possible that the start date occured in the previous calendar year.
		//this is the case if StartMonth is greater than the date's month.
	//	if ( StartMonth > date.date().month() ) result = result.addYears(-1);
		if ( StartMonth > date.date().month() ) result = QDateTime( QDate( result.date().year()-1, result.date().month(), result.date().day()), StartTime);
	} else if ( StartMonth ) {
		//Last change was reverting to standard time.
		result = QDateTime( QDate( date.date().year(), RevertMonth, 1 ), RevertTime );
		result = QDateTime( QDate( date.date().year(), RevertMonth, findRevertDay( result ) ), RevertTime );

		//It's possible that the revert date occurred in the previous calendar year.
		//this is the case if RevertMonth is greater than the date's month.
		//if ( RevertMonth > date.date().month() ) result = result.addYears(-1);
		if ( RevertMonth > date.date().month() ) result = QDateTime( QDate( result.date().year()-1, result.date().month(), result.date().day() ), RevertTime);
	}

	kdDebug() << i18n( "Previous Daylight Savings Time change: " ) << result.toString() << endl;
	return result;
}
