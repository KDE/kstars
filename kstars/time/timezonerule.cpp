/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timezonerule.h"
#include "kstars_debug.h"

#include <KLocalizedString>

#include <QString>

TimeZoneRule::TimeZoneRule()
{
    setEmpty();
}

TimeZoneRule::TimeZoneRule(const QString &smonth, const QString &sday, const QTime &stime, const QString &rmonth,
                           const QString &rday, const QTime &rtime, const double &dh)
{
    dTZ = 0.0;
    if (smonth != "0")
    {
        StartMonth  = initMonth(smonth);
        RevertMonth = initMonth(rmonth);

        if (StartMonth && RevertMonth && initDay(sday, StartDay, StartWeek) && initDay(rday, RevertDay, RevertWeek) &&
            stime.isValid() && rtime.isValid())
        {
            StartTime  = stime;
            RevertTime = rtime;
            HourOffset = dh;
        }
        else
        {
            qCWarning(KSTARS) << i18n("Error parsing TimeZoneRule, setting to empty rule.");
            setEmpty();
        }
    }
    else //Empty rule
    {
        setEmpty();
    }
}

void TimeZoneRule::setEmpty()
{
    StartMonth  = 0;
    RevertMonth = 0;
    StartDay    = 0;
    RevertDay   = 0;
    StartWeek   = -1;
    RevertWeek  = -1;
    StartTime   = QTime();
    RevertTime  = QTime();
    HourOffset  = 0.0;
    dTZ         = 0.0;
}

void TimeZoneRule::setDST(bool activate)
{
    if (activate)
    {
        qCDebug(KSTARS) << "Daylight Saving Time active";
        dTZ = HourOffset;
    }
    else
    {
        qCDebug(KSTARS) << "Daylight Saving Time inactive";
        dTZ = 0.0;
    }
}

int TimeZoneRule::initMonth(const QString &mn)
{
    //Check whether the argument is a three-letter English month code.
    QString ml = mn.toLower();
    if (ml == "jan")
        return 1;
    else if (ml == "feb")
        return 2;
    else if (ml == "mar")
        return 3;
    else if (ml == "apr")
        return 4;
    else if (ml == "may")
        return 5;
    else if (ml == "jun")
        return 6;
    else if (ml == "jul")
        return 7;
    else if (ml == "aug")
        return 8;
    else if (ml == "sep")
        return 9;
    else if (ml == "oct")
        return 10;
    else if (ml == "nov")
        return 11;
    else if (ml == "dec")
        return 12;

    qCWarning(KSTARS) << i18n("Could not parse %1 as a valid month code.", mn);
    return 0;
}

bool TimeZoneRule::initDay(const QString &dy, int &Day, int &Week)
{
    //Three possible ways to express a day.
    //1. simple integer; the calendar date...set Week=0 to indicate that Date is not the day of the week
    bool ok;
    int day = dy.toInt(&ok);
    if (ok)
    {
        Day  = day;
        Week = 0;
        return true;
    }

    QString dl = dy.toLower();
    //2. 3-letter day of week string, indicating the last of that day of the month
    //   ...set Week to 5 to indicate the last weekday of the month
    if (dl == "mon")
    {
        Day  = 1;
        Week = 5;
        return true;
    }
    else if (dl == "tue")
    {
        Day  = 2;
        Week = 5;
        return true;
    }
    else if (dl == "wed")
    {
        Day  = 3;
        Week = 5;
        return true;
    }
    else if (dl == "thu")
    {
        Day  = 4;
        Week = 5;
        return true;
    }
    else if (dl == "fri")
    {
        Day  = 5;
        Week = 5;
        return true;
    }
    else if (dl == "sat")
    {
        Day  = 6;
        Week = 5;
        return true;
    }
    else if (dl == "sun")
    {
        Day  = 7;
        Week = 5;
        return true;
    }

    //3. 1,2 or 3 followed by 3-letter day of week string; this indicates
    //   the (1st/2nd/3rd) weekday of the month.
    int wn = dl.leftRef(1).toInt();
    if (wn > 0 && wn < 4)
    {
        QString dm = dl.mid(1, dl.length()).toLower();
        if (dm == "mon")
        {
            Day  = 1;
            Week = wn;
            return true;
        }
        else if (dm == "tue")
        {
            Day  = 2;
            Week = wn;
            return true;
        }
        else if (dm == "wed")
        {
            Day  = 3;
            Week = wn;
            return true;
        }
        else if (dm == "thu")
        {
            Day  = 4;
            Week = wn;
            return true;
        }
        else if (dm == "fri")
        {
            Day  = 5;
            Week = wn;
            return true;
        }
        else if (dm == "sat")
        {
            Day  = 6;
            Week = wn;
            return true;
        }
        else if (dm == "sun")
        {
            Day  = 7;
            Week = wn;
            return true;
        }
    }

    qCWarning(KSTARS) << i18n("Could not parse %1 as a valid day code.", dy);
    return false;
}

int TimeZoneRule::findStartDay(const KStarsDateTime &d)
{
    // Determine the calendar date of StartDay for the month and year of the given date.
    QDate test;

    // TimeZoneRule is empty, return -1
    if (isEmptyRule())
        return -1;

    // If StartWeek=0, just return the integer.
    if (StartWeek == 0)
        return StartDay;

    // Since StartWeek was not zero, StartDay is the day of the week, not the calendar date
    else if (StartWeek == 5) // count back from end of month until StartDay is found.
    {
        for (test = QDate(d.date().year(), d.date().month(), d.date().daysInMonth()); test.day() > 21;
             test = test.addDays(-1))
            if (test.dayOfWeek() == StartDay)
                break;
    }
    else // Count forward from day 1, 8 or 15 (depending on StartWeek) until correct day of week is found
    {
        for (test = QDate(d.date().year(), d.date().month(), (StartWeek - 1) * 7 + 1); test.day() < 7 * StartWeek;
             test = test.addDays(1))
            if (test.dayOfWeek() == StartDay)
                break;
    }
    return test.day();
}

int TimeZoneRule::findRevertDay(const KStarsDateTime &d)
{
    // Determine the calendar date of RevertDay for the month and year of the given date.
    QDate test;

    // TimeZoneRule is empty, return -1
    if (isEmptyRule())
        return -1;

    // If RevertWeek=0, just return the integer.
    if (RevertWeek == 0)
        return RevertDay;

    // Since RevertWeek was not zero, RevertDay is the day of the week, not the calendar date
    else if (RevertWeek == 5) //count back from end of month until RevertDay is found.
    {
        for (test = QDate(d.date().year(), d.date().month(), d.date().daysInMonth()); test.day() > 21;
             test = test.addDays(-1))
            if (test.dayOfWeek() == RevertDay)
                break;
    }
    else //Count forward from day 1, 8 or 15 (depending on RevertWeek) until correct day of week is found
    {
        for (test = QDate(d.date().year(), d.date().month(), (RevertWeek - 1) * 7 + 1); test.day() < 7 * RevertWeek;
             test = test.addDays(1))
            if (test.dayOfWeek() == StartDay)
                break;
    }
    return test.day();
}

bool TimeZoneRule::isDSTActive(const KStarsDateTime &date)
{
    // The empty rule always returns false
    if (isEmptyRule())
        return false;

    // First, check whether the month is outside the DST interval.  Note that
    // the interval check is different if StartMonth > RevertMonth (indicating that
    // the DST interval includes the end of the year).
    int month = date.date().month();

    if (StartMonth < RevertMonth)
    {
        if (month < StartMonth || month > RevertMonth)
            return false;
    }
    else
    {
        if (month < StartMonth && month > RevertMonth)
            return false;
    }

    // OK, if the month is equal to StartMonth or Revert Month, we have more
    // detailed checking to do...
    int day = date.date().day();

    if (month == StartMonth)
    {
        int sday = findStartDay(date);
        if (day < sday)
            return false;
        if (day == sday && date.time() < StartTime)
            return false;
    }
    else if (month == RevertMonth)
    {
        int rday = findRevertDay(date);
        if (day > rday)
            return false;
        if (day == rday && date.time() > RevertTime)
            return false;
    }

    // passed all tests, so we must be in DST.
    return true;
}

void TimeZoneRule::nextDSTChange_LTime(const KStarsDateTime &date)
{
    KStarsDateTime result;

    // return an invalid date if the rule is the empty rule.
    if (isEmptyRule())
        result = KStarsDateTime(QDateTime());

    else if (deltaTZ())
    {
        // Next change is reverting back to standard time.

        //y is the year for the next DST Revert date.  It's either the current year, or
        //the next year if the current month is already past RevertMonth
        int y = date.date().year();
        if (RevertMonth < date.date().month())
            ++y;

        result = KStarsDateTime(QDate(y, RevertMonth, 1), RevertTime);
        result = KStarsDateTime(QDate(y, RevertMonth, findRevertDay(result)), RevertTime);
    }
    else
    {
        // Next change is starting DST.

        //y is the year for the next DST Start date.  It's either the current year, or
        //the next year if the current month is already past StartMonth
        int y = date.date().year();
        if (StartMonth < date.date().month())
            ++y;

        result = KStarsDateTime(QDate(y, StartMonth, 1), StartTime);
        result = KStarsDateTime(QDate(y, StartMonth, findStartDay(result)), StartTime);
    }

    qCDebug(KSTARS) << "Next Daylight Savings Time change (Local Time): " << result.toString();
    next_change_ltime = result;
}

void TimeZoneRule::previousDSTChange_LTime(const KStarsDateTime &date)
{
    KStarsDateTime result;

    // return an invalid date if the rule is the empty rule
    if (isEmptyRule())
        next_change_ltime = KStarsDateTime(QDateTime());

    if (deltaTZ())
    {
        // Last change was starting DST.

        //y is the year for the previous DST Start date.  It's either the current year, or
        //the previous year if the current month is earlier than StartMonth
        int y = date.date().year();
        if (StartMonth > date.date().month())
            --y;

        result = KStarsDateTime(QDate(y, StartMonth, 1), StartTime);
        result = KStarsDateTime(QDate(y, StartMonth, findStartDay(result)), StartTime);
    }
    else if (StartMonth)
    {
        //Last change was reverting to standard time.

        //y is the year for the previous DST Start date.  It's either the current year, or
        //the previous year if the current month is earlier than StartMonth
        int y = date.date().year();
        if (RevertMonth > date.date().month())
            --y;

        result = KStarsDateTime(QDate(y, RevertMonth, 1), RevertTime);
        result = KStarsDateTime(QDate(y, RevertMonth, findRevertDay(result)), RevertTime);
    }

    qCDebug(KSTARS) << "Previous Daylight Savings Time change (Local Time): " << result.toString();
    next_change_ltime = result;
}

/**Convert current local DST change time in universal time */
void TimeZoneRule::nextDSTChange(const KStarsDateTime &local_date, const double TZoffset)
{
    // just decrement timezone offset and hour offset
    KStarsDateTime result = local_date.addSecs(int((TZoffset + deltaTZ()) * -3600));

    qCDebug(KSTARS) << "Next Daylight Savings Time change (UTC): " << result.toString();
    next_change_utc = result;
}

/**Convert current local DST change time in universal time */
void TimeZoneRule::previousDSTChange(const KStarsDateTime &local_date, const double TZoffset)
{
    // just decrement timezone offset
    KStarsDateTime result = local_date.addSecs(int(TZoffset * -3600));

    // if prev DST change is a revert change, so the revert time is in daylight saving time
    if (result.date().month() == RevertMonth)
        result = result.addSecs(int(HourOffset * -3600));

    qCDebug(KSTARS) << "Previous Daylight Savings Time change (UTC): " << result.toString();
    next_change_utc = result;
}

void TimeZoneRule::reset_with_ltime(KStarsDateTime &ltime, const double TZoffset, const bool time_runs_forward,
                                    const bool automaticDSTchange)
{
    /**There are some problems using local time for getting next daylight saving change time.
    	1. The local time is the start time of DST change. So the local time doesn't exists and must
    		  corrected.
    	2. The local time is the revert time. So the local time exists twice.
    	3. Neither start time nor revert time. There is no problem.

    	Problem #1 is more complicated and we have to change the local time by reference.
    	Problem #2 we just have to reset status of DST.

    	automaticDSTchange should only set to true if DST status changed due to running automatically over
    	a DST change time. If local time will changed manually the automaticDSTchange should always set to
    	false, to hold current DST status if possible (just on start and revert time possible).
    	*/

    //don't need to do anything for empty rule
    if (isEmptyRule())
        return;

    // check if DST is active before resetting with new time
    bool wasDSTactive(false);

    if (deltaTZ() != 0.0)
    {
        wasDSTactive = true;
    }

    // check if current time is start time, this means if a DST change happened in last hour(s)
    bool active_with_houroffset = isDSTActive(ltime.addSecs(int(HourOffset * -3600)));
    bool active_normal          = isDSTActive(ltime);

    // store a valid local time
    KStarsDateTime ValidLTime = ltime;

    if (active_with_houroffset != active_normal && ValidLTime.date().month() == StartMonth)
    {
        // current time is the start time
        qCDebug(KSTARS) << "Current time = Starttime: invalid local time due to daylight saving time";

        // set a correct local time because the current time doesn't exists
        // if automatic DST change happened, new DST setting is the opposite of current setting
        if (automaticDSTchange)
        {
            // revert DST status
            setDST(!wasDSTactive);
            // new setting DST is inactive, so subtract hour offset to new time
            if (wasDSTactive)
            {
                // DST inactive
                ValidLTime = ltime.addSecs(int(HourOffset * -3600));
            }
            else
            {
                // DST active
                // add hour offset to new time
                ValidLTime = ltime.addSecs(int(HourOffset * 3600));
            }
        }
        else // if ( automaticDSTchange )
        {
            // no automatic DST change happened, so stay in current DST mode
            setDST(wasDSTactive);
            if (wasDSTactive)
            {
                // DST active
                // add hour offset to current time, because time doesn't exists
                ValidLTime = ltime.addSecs(int(HourOffset * 3600));
            }
            else
            {
                // DST inactive
                // subtrace hour offset to current time, because time doesn't exists
                ValidLTime = ltime.addSecs(int(HourOffset * -3600));
            }
        } // else { // if ( automaticDSTchange )
    }
    else // if ( active_with_houroffset != active_normal && ValidLTime.date().month() == StartMonth )
    {
        // If current time was not start time, so check if current time is revert time
        // this means if a DST change happened in next hour(s)
        active_with_houroffset = isDSTActive(ltime.addSecs(int(HourOffset * 3600)));
        if (active_with_houroffset != active_normal && RevertMonth == ValidLTime.date().month())
        {
            // current time is the revert time
            qCDebug(KSTARS) << "Current time = Reverttime";

            // we don't kneed to change the local time, because local time always exists, but
            // some times exists twice, so we have to reset DST status
            if (automaticDSTchange)
            {
                // revert DST status
                setDST(!wasDSTactive);
            }
            else
            {
                // no automatic DST change so stay in current DST mode
                setDST(wasDSTactive);
            }
        }
        else
        {
            //Current time was neither starttime nor reverttime, so use normal calculated DST status
            setDST(active_normal);
        }
    } // if ( active_with_houroffset != active_normal && ValidLTime.date().month() == StartMonth )

    //	qDebug() << "Using Valid Local Time = " << ValidLTime.toString();

    if (time_runs_forward)
    {
        // get next DST change time in local time
        nextDSTChange_LTime(ValidLTime);
        nextDSTChange(next_change_ltime, TZoffset);
    }
    else
    {
        // get previous DST change time in local time
        previousDSTChange_LTime(ValidLTime);
        previousDSTChange(next_change_ltime, TZoffset);
    }
    ltime = ValidLTime;
}

bool TimeZoneRule::equals(TimeZoneRule *r)
{
    if (StartDay == r->StartDay && RevertDay == r->RevertDay && StartWeek == r->StartWeek &&
        RevertWeek == r->RevertWeek && StartMonth == r->StartMonth && RevertMonth == r->RevertMonth &&
        StartTime == r->StartTime && RevertTime == r->RevertTime && isEmptyRule() == r->isEmptyRule())
        return true;
    else
        return false;
}
