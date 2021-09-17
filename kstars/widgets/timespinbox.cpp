/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timespinbox.h"

#include "kstars_debug.h"

#include <KLocalizedString>

#include <QDebug>
#include <QLineEdit>

#include <cmath>

#define SECS_PER_DAY  86400.
#define SIDEREAL_YEAR 31558149.77

//Time steps:
//0-9:   0 sec, 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 30 sec
//10-14: 1 minute, 2, 5, 10, 15, 30 min
//15-19: 1 hour, 2, 4, 6, 12 hours
//20-23: 1 day, 2, 3, 5 days
//24-26: 1 week, 2, 3 weeks
//27-32: 1 month, 2, 3, 4, 6, 9 months
//33-41: 1 year, 2, 3, 4, 5, 10, 25, 50, 100 years

TimeSpinBox::TimeSpinBox(QWidget *parent, bool _daysonly) : QSpinBox(parent)
{
    setDaysOnly(_daysonly);

    setMinimum(-41);
    setMaximum(41);
    setSingleStep(1);

    setButtonSymbols(QSpinBox::PlusMinus);
    lineEdit()->setReadOnly(true);
    setValue(4); //1 second (real time)

    //Set width:
    QFontMetrics fm(font());
    int extra = width() - lineEdit()->width();
    uint wmax = 0;
    for (int i = 0; i < maximum(); ++i)
    {
        #if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
        uint w = fm.horizontalAdvance('-' + TimeString[i]);
        #else
        uint w = fm.width('-' + TimeString[i]);
        #endif

        if (w > wmax)
            wmax = w;
    }
    setFixedWidth(wmax + extra);

    connect(this, SIGNAL(valueChanged(int)), this, SLOT(reportChange()));
    //	updateDisplay();
}

void TimeSpinBox::setDaysOnly(bool daysonly)
{
    DaysOnly = daysonly;

    int i        = 0;   //index for TimeScale values
    TimeScale[0] = 0.0; // 0.0 sec
    if (!daysOnly())
    {
        TimeScale[1]  = 0.1;     // 0.1 sec
        TimeScale[2]  = 0.25;    // 0.25 sec
        TimeScale[3]  = 0.5;     // 0.5 sec
        TimeScale[4]  = 1.0;     // 1 sec (real-time)
        TimeScale[5]  = 2.0;     // 2 sec
        TimeScale[6]  = 5.0;     // 5 sec
        TimeScale[7]  = 10.0;    // 10 sec
        TimeScale[8]  = 20.0;    // 20 sec
        TimeScale[9]  = 30.0;    // 30 sec
        TimeScale[10] = 60.0;    // 1 min
        TimeScale[11] = 120.0;   // 2 min
        TimeScale[12] = 300.0;   // 5 min
        TimeScale[13] = 600.0;   // 10 min
        TimeScale[14] = 900.0;   // 15 min
        TimeScale[15] = 1800.0;  // 30 min
        TimeScale[16] = 3600.0;  // 1 hr
        TimeScale[17] = 7200.0;  // 2 hr
        TimeScale[18] = 10800.0; // 3 hr
        TimeScale[19] = 21600.0; // 6 hr
        TimeScale[20] = 43200.0; // 12 hr
        i             = 20;
    }
    TimeScale[i + 1] = 86164.1;            // 23 hr 56 min
    TimeScale[i + 2] = SECS_PER_DAY;       // 1 day
    TimeScale[i + 3] = 2. * SECS_PER_DAY;  // 2 days
    TimeScale[i + 4] = 3. * SECS_PER_DAY;  // 3 days
    TimeScale[i + 5] = 5. * SECS_PER_DAY;  // 5 days
    TimeScale[i + 6] = 7. * SECS_PER_DAY;  // 1 week
    TimeScale[i + 7] = 14. * SECS_PER_DAY; //2 weeks
    TimeScale[i + 8] = 21. * SECS_PER_DAY; //3 weeks
    //Months aren't a simple measurement of time; I'll just use fractions of a year
    TimeScale[i + 9]  = SIDEREAL_YEAR / 12.0;  // 1 month
    TimeScale[i + 10] = SIDEREAL_YEAR / 6.0;   // 2 months
    TimeScale[i + 11] = 0.25 * SIDEREAL_YEAR;  // 3 months
    TimeScale[i + 12] = SIDEREAL_YEAR / 3.0;   // 4 months
    TimeScale[i + 13] = 0.5 * SIDEREAL_YEAR;   // 6 months
    TimeScale[i + 14] = 0.75 * SIDEREAL_YEAR;  // 9 months
    TimeScale[i + 15] = SIDEREAL_YEAR;         // 1 year
    TimeScale[i + 16] = 2.0 * SIDEREAL_YEAR;   // 2 years
    TimeScale[i + 17] = 3.0 * SIDEREAL_YEAR;   // 3 years
    TimeScale[i + 18] = 5.0 * SIDEREAL_YEAR;   // 5 years
    TimeScale[i + 19] = 10.0 * SIDEREAL_YEAR;  // 10 years
    TimeScale[i + 20] = 25.0 * SIDEREAL_YEAR;  // 25 years
    TimeScale[i + 21] = 50.0 * SIDEREAL_YEAR;  // 50 years
    TimeScale[i + 22] = 100.0 * SIDEREAL_YEAR; // 100 years

    TimeString.clear();
    if (!daysOnly())
    {
        TimeString.append("0 " + i18nc("seconds", "secs"));
        TimeString.append("0.1 " + i18nc("seconds", "secs"));
        TimeString.append("0.25 " + i18nc("seconds", "secs"));
        TimeString.append("0.5 " + i18nc("seconds", "secs"));
        TimeString.append("1 " + i18nc("second", "sec"));
        TimeString.append("2 " + i18nc("seconds", "secs"));
        TimeString.append("5 " + i18nc("seconds", "secs"));
        TimeString.append("10 " + i18nc("seconds", "secs"));
        TimeString.append("20 " + i18nc("seconds", "secs"));
        TimeString.append("30 " + i18nc("seconds", "secs"));
        TimeString.append("1 " + i18nc("minute", "min"));
        TimeString.append("2 " + i18nc("minutes", "mins"));
        TimeString.append("5 " + i18nc("minutes", "mins"));
        TimeString.append("10 " + i18nc("minutes", "mins"));
        TimeString.append("15 " + i18nc("minutes", "mins"));
        TimeString.append("30 " + i18nc("minutes", "mins"));
        TimeString.append("1 " + i18n("hour"));
        TimeString.append("2 " + i18nc("hours", "hrs"));
        TimeString.append("3 " + i18nc("hours", "hrs"));
        TimeString.append("6 " + i18nc("hours", "hrs"));
        TimeString.append("12 " + i18nc("hours", "hrs"));
    }
    else
    {
        TimeString.append("0 " + i18n("days"));
    }
    TimeString.append("1 " + i18nc("sidereal day", "sid day"));
    TimeString.append("1 " + i18n("day"));
    TimeString.append("2 " + i18n("days"));
    TimeString.append("3 " + i18n("days"));
    TimeString.append("5 " + i18n("days"));
    TimeString.append("1 " + i18n("week"));
    TimeString.append("2 " + i18nc("weeks", "wks"));
    TimeString.append("3 " + i18nc("weeks", "wks"));
    TimeString.append("1 " + i18n("month"));
    TimeString.append("2 " + i18nc("months", "mths"));
    TimeString.append("3 " + i18nc("months", "mths"));
    TimeString.append("4 " + i18nc("months", "mths"));
    TimeString.append("6 " + i18nc("months", "mths"));
    TimeString.append("9 " + i18nc("months", "mths"));
    TimeString.append("1 " + i18n("year"));
    TimeString.append("2 " + i18nc("years", "yrs"));
    TimeString.append("3 " + i18nc("years", "yrs"));
    TimeString.append("5 " + i18nc("years", "yrs"));
    TimeString.append("10 " + i18nc("years", "yrs"));
    TimeString.append("25 " + i18nc("years", "yrs"));
    TimeString.append("50 " + i18nc("years", "yrs"));
    TimeString.append("100 " + i18nc("years", "yrs"));

    if (!daysOnly())
    {
        setMinimum(-41);
        setMaximum(41);
    }
    else
    {
        setMinimum(-21);
        setMaximum(21);
    }
}

int TimeSpinBox::valueFromText(const QString &text) const
{
    for (int i = 0; i < TimeString.size(); i++)
    {
        if (text == TimeString[i])
        {
            return i;
        }
        if (text.mid(1, text.length()) == TimeString[i])
        {
            return -1 * i;
        }
    }

    return 0;
}

QString TimeSpinBox::textFromValue(int value) const
{
    QString result;
    int posval(abs(value));

    if (posval > TimeString.size() - 1)
        posval = 4;

    result = TimeString[posval];

    if (value < 0)
    {
        result = '-' + result;
    }
    return result;
}

void TimeSpinBox::changeScale(float x)
{
    //Pick the closest value
    int imin = 0;
    float dxlast(10000000000.0), dxmin(10000000000.0);

    for (unsigned int i = 0; i < 42; ++i)
    {
        float dx = fabs(TimeScale[i] - fabs(x));

        if (dx < dxmin)
        {
            imin  = i;
            dxmin = dx;
        }
        if (dx > dxlast)
            break; //we have passed the minimum dx

        dxlast = dx;
    }

    if (x < 0.0)
        imin *= -1;
    setValue(imin);
}

float TimeSpinBox::timeScale(void) const
{
    return value() > 0 ? TimeScale[value()] : -1. * TimeScale[abs(value())];
}

void TimeSpinBox::reportChange()
{
    qCDebug(KSTARS) << "Reporting new timestep value: " << timeScale();
    emit scaleChanged(timeScale());
}
