/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdatetime.h"

#include <QTime>

class QString;

/** @class TimeZoneRule
	*This class provides the information needed to determine whether Daylight
	*Savings Time (DST; a.k.a. "Summer Time") is currently active at a given
	*location.  There are (at least) 25 different "rules" which govern DST
	*around the world; a string identifying the appropriate rule is attached
    *to each city in citydb.sqlite.
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

class TimeZoneRule
{
  public:
    /** Default Constructor. Makes the "empty" time zone rule (i.e., no DST correction)*/
    TimeZoneRule();

    /** Constructor. Create a TZ rule according to the arguments.
        	*@param smonth the three-letter code for the month in which DST starts
        	*@param sday a string encoding the day on which DST starts (see the class description)
        	*@param stime the time at which DST starts
        	*@param rmonth the three-letter code for the month in which DST reverts
        	*@param rday a string encoding the day on which DST reverts (see the class description)
        	*@param rtime the time at which DST reverts
        	*@param offset the offset between normal time and DS time (always 1.00?)
        	*/
    TimeZoneRule(const QString &smonth, const QString &sday, const QTime &stime, const QString &rmonth,
                 const QString &rday, const QTime &rtime, const double &offset = 1.00);

    /** Determine whether DST is in effect for the given DateTime, according to this rule
        	*@param date the date/time to test for DST
        	*/
    bool isDSTActive(const KStarsDateTime &date);

    /** @return true if the rule is the "empty" TZ rule. */
    bool isEmptyRule() const { return (HourOffset == 0.0); }

    /** Toggle DST on/off.  The @p activate argument should probably be isDSTActive()
        	*@param activate if true, then set DST active; otherwise, deactivate DST
        	*/
    void setDST(bool activate = true);

    /** @return the current Timezone offset, compared to the timezone's Standard Time.
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
    void reset_with_ltime(KStarsDateTime &ltime, const double TZoffset, const bool time_runs_forward,
                          const bool automaticDSTchange = false);

    /** @return computed value for next DST change in universal time. */
    KStarsDateTime nextDSTChange() const { return next_change_utc; }

    /** @return computed value for next DST change in local time. */
    KStarsDateTime nextDSTChange_LTime() const { return next_change_ltime; }

    /** @return true if this rule is the same as the argument.
         * @param r the rule to check for equivalence
         */
    bool equals(TimeZoneRule *r);

  private:
    /** @return the KStarsDateTime of the moment when the next DST change will occur in local time
        	*This is useful because DST change times are saved in local times*/
    void nextDSTChange_LTime(const KStarsDateTime &date);

    /** @return the KStarsDateTime of the moment when the last DST change occurred in local time
        	*This is useful because DST change times are saved in local times
        	*We need this in case time is running backwards. */
    void previousDSTChange_LTime(const KStarsDateTime &date);

    /** calculate the next DST change in universal time for current location */
    void nextDSTChange(const KStarsDateTime &local_date, const double TZoffset);

    /** calculate the previous DST change in universal time for current location */
    void previousDSTChange(const KStarsDateTime &local_date, const double TZoffset);

    /** Interpret the string as a month of the year;
        	*@return the month integer (1=jan ... 12=dec)
        	*/
    int initMonth(const QString &m);

    /** Set up empty time zone rule */
    void setEmpty();

    /** Interpret the day string as a week ID and a day-of-week ID.  The day-of-week
        	*is an integer between 1 (sunday) and 7 (saturday); the week integer can
        	*be 1-3 (1st/2nd/third weekday of the month), or 5 (last weekday of the month)
        	*@param day the day integer is returned by reference through this value
        	*@param week the week integer is returned by reference through this value
        	*@return true if the day string was successfully parsed
        	*/
    bool initDay(const QString &d, int &day, int &week);

    /** Find the calendar date on which DST starts for the calendar year
        	*of the given KStarsDateTime.
        	*@param d the date containing the year to be tested
        	*@return the calendar date, an integer between 1 and 31. */
    int findStartDay(const KStarsDateTime &d);

    /** Find the calendar date on which DST ends for the calendar year
        	*of the given KStarsDateTime.
        	*@param d the date containing the year to be tested
        	*@return the calendar date, an integer between 1 and 31. */
    int findRevertDay(const KStarsDateTime &d);

    int StartDay { 0 };
    int RevertDay { 0 };
    int StartMonth { 0 };
    int RevertMonth { 0 };
    int StartWeek { 0 };
    int RevertWeek { 0 };
    QTime StartTime, RevertTime;
    KStarsDateTime next_change_utc;
    KStarsDateTime next_change_ltime;
    double dTZ { 0 };
    double HourOffset { 0 };
};
