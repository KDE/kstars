/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dms.h"

#include <QLocale>

#include <QRegExp>

#ifdef COUNT_DMS_SINCOS_CALLS
long unsigned dms::dms_constructor_calls         = 0;
long unsigned dms::dms_with_sincos_called        = 0;
long unsigned dms::trig_function_calls           = 0;
long unsigned dms::redundant_trig_function_calls = 0;
double dms::seconds_in_trig                      = 0;
#endif

void dms::setD(const int &d, const int &m, const int &s, const int &ms)
{
    D = (double)abs(d) + ((double)m + ((double)s + (double)ms / 1000.) / 60.) / 60.;
    if (d < 0)
    {
        D = -1.0 * D;
    }
#ifdef COUNT_DMS_SINCOS_CALLS
    m_cosDirty = m_sinDirty = true;
#endif
}

void dms::setH(const int &h, const int &m, const int &s, const int &ms)
{
    D = 15.0 * ((double)abs(h) + ((double)m + ((double)s + (double)ms / 1000.) / 60.) / 60.);
    if (h < 0)
    {
        D = -1.0 * D;
    }
#ifdef COUNT_DMS_SINCOS_CALLS
    m_cosDirty = m_sinDirty = true;
#endif
}

bool dms::setFromString(const QString &str, bool isDeg)
{
    int d(0), m(0);
    double s(0.0);
    bool checkValue(false), badEntry(false), negative(false);
    QString entry = str.trimmed();
    entry.remove(QRegExp("[hdms'\"°]"));

    //Account for localized decimal-point settings
    //QString::toDouble() requires that the decimal symbol is "."
    entry.replace(QLocale().decimalPoint(), ".");

    //empty entry returns false
    if (entry.isEmpty())
    {
        dms::setD(NaN::d);
        return false;
    }

    //try parsing a simple integer
    d = entry.toInt(&checkValue);
    if (checkValue)
    {
        if (isDeg)
            dms::setD(d, 0, 0);
        else
            dms::setH(d, 0, 0);
        return true;
    }

    //try parsing a simple double
    double x = entry.toDouble(&checkValue);
    if (checkValue)
    {
        if (isDeg)
            dms::setD(x);
        else
            dms::setH(x);
        return true;
    }

    //try parsing multiple fields.
    QStringList fields;

    //check for colon-delimiters or space-delimiters
    if (entry.contains(':'))
        fields = entry.split(':', QString::SkipEmptyParts);
    else
        fields = entry.split(' ', QString::SkipEmptyParts);

    //anything with one field is invalid!
    if (fields.count() == 1)
    {
        dms::setD(NaN::d);
        return false;
    }

    //If two fields we will add a third one, and then parse with
    //the 3-field code block. If field[1] is an int, add a third field equal to "0".
    //If field[1] is a double, convert it to integer arcmin, and convert
    //the remainder to integer arcsec
    //If field[1] is neither int nor double, return false.
    if (fields.count() == 2)
    {
        m = fields[1].toInt(&checkValue);
        if (checkValue)
            fields.append(QString("0"));
        else
        {
            double mx = fields[1].toDouble(&checkValue);
            if (checkValue)
            {
                fields[1] = QString::number(int(mx));
                fields.append(QString::number(int(60.0 * (mx - int(mx)))));
            }
            else
            {
                dms::setD(NaN::d);
                return false;
            }
        }
    }

    //Now have (at least) three fields ( h/d m s );
    //we can ignore anything after 3rd field
    if (fields.count() >= 3)
    {
        //See if first two fields parse as integers, and third field as a double

        d = fields[0].toInt(&checkValue);
        if (!checkValue)
            badEntry = true;
        m = fields[1].toInt(&checkValue);
        if (!checkValue)
            badEntry = true;
        s = fields[2].toDouble(&checkValue);
        if (!checkValue)
            badEntry = true;

        //Special case: If first field is "-0", store the negative sign.
        //(otherwise it gets dropped)
        if (fields[0].at(0) == '-' && d == 0)
            negative = true;
    }

    if (!badEntry)
    {
        double D = (double)abs(d) + (double)abs(m) / 60. + (double)fabs(s) / 3600.;

        if (negative || d < 0 || m < 0 || s < 0)
        {
            D = -1.0 * D;
        }

        if (isDeg)
        {
            dms::setD(D);
        }
        else
        {
            dms::setH(D);
        }
    }
    else
    {
        dms::setD(NaN::d);
        return false;
    }

    return true;
}

int dms::arcmin(void) const
{
    if (std::isnan(D))
        return 0;

    int am = int(60.0 * (fabs(D) - abs(degree())));
    if (D < 0.0 && D > -1.0) //angle less than zero, but greater than -1.0
    {
        am = -1 * am; //make minute negative
    }
    return am; // Warning: Will return 0 if the value is NaN
}

int dms::arcsec(void) const
{
    if (std::isnan(D))
        return 0;

    int as = int(60.0 * (60.0 * (fabs(D) - abs(degree())) - abs(arcmin())));
    //If the angle is slightly less than 0.0, give ArcSec a neg. sgn.
    if (degree() == 0 && arcmin() == 0 && D < 0.0)
    {
        as = -1 * as;
    }
    return as; // Warning: Will return 0 if the value is NaN
}

int dms::marcsec(void) const
{
    if (std::isnan(D))
        return 0;

    int as = int(1000.0 * (60.0 * (60.0 * (fabs(D) - abs(degree())) - abs(arcmin())) - abs(arcsec())));
    //If the angle is slightly less than 0.0, give ArcSec a neg. sgn.
    if (degree() == 0 && arcmin() == 0 && arcsec() == 0 && D < 0.0)
    {
        as = -1 * as;
    }
    return as; // Warning: Will return 0 if the value is NaN
}

int dms::minute(void) const
{
    int hm = int(60.0 * (fabs(Hours()) - abs(hour())));
    if (Hours() < 0.0 && Hours() > -1.0) //angle less than zero, but greater than -1.0
    {
        hm = -1 * hm; //make minute negative
    }
    return hm; // Warning: Will return 0 if the value is NaN
}

int dms::second(void) const
{
    int hs = int(60.0 * (60.0 * (fabs(Hours()) - abs(hour())) - abs(minute())));
    if (hour() == 0 && minute() == 0 && Hours() < 0.0)
    {
        hs = -1 * hs;
    }
    return hs; // Warning: Will return 0 if the value is NaN
}

int dms::msecond(void) const
{
    int hs = int(1000.0 * (60.0 * (60.0 * (fabs(Hours()) - abs(hour())) - abs(minute())) - abs(second())));
    if (hour() == 0 && minute() == 0 && second() == 0 && Hours() < 0.0)
    {
        hs = -1 * hs;
    }
    return hs; // Warning: Will return 0 if the value is NaN
}

const dms dms::reduce(void) const
{
    if (std::isnan(D))
        return dms(0);

    return dms(D - 360.0 * floor(D / 360.0));
}

const dms dms::deltaAngle(dms angle) const
{
    double angleDiff = D - angle.Degrees();

    // Put in the range of [-360,360]
    while (angleDiff > 360)
        angleDiff -= 360;
    while (angleDiff < -360)
        angleDiff += 360;
    // angleDiff in the range [180,360]
    if (angleDiff > 180)
        return dms(360 - angleDiff);
    // angleDiff in the range [-360,-180]
    else if (angleDiff < -180)
        return dms(-(360 + angleDiff));
    // angleDiff in the range [-180,180]
    else
        return dms(angleDiff);
}

const QString dms::toDMSString(const bool forceSign, const bool machineReadable, const bool highPrecision) const
{
    QString dummy;
    char pm(' ');
    QChar zero('0');
    int dd, dm, ds;

    if (machineReadable || !highPrecision)
    // minimize the mean angle representation error of DMS format
    // set LSD transition in the middle of +- half precision range
    {
        double half_precision = 1.0 / 7200.0;
	if (Degrees() < 0.0)
            half_precision = -half_precision;
	dms angle(Degrees() + half_precision);
        dd = abs(angle.degree());
        dm = abs(angle.arcmin());
        ds = abs(angle.arcsec());
    }
    else
    {
        dd = abs(degree());
        dm = abs(arcmin());
        ds = abs(arcsec());
    }

    if (Degrees() < 0.0)
        pm = '-';
    else if (forceSign && Degrees() > 0.0)
        pm = '+';

    if (machineReadable)
        return QString("%1%2:%3:%4").arg(pm)
                .arg(dd, 2, 10, QChar('0'))
                .arg(dm, 2, 10, QChar('0'))
                .arg(ds, 2, 10, QChar('0'));

    if (highPrecision)
    {
        double sec = arcsec() + marcsec() / 1000.;
        return QString("%1%2° %3\' %L4\"").arg(pm)
                                         .arg(dd, 2, 10, zero)
                                         .arg(dm, 2, 10, zero)
                                         .arg(sec, 2,'f', 2, zero);
    }

    return QString("%1%2° %3\' %4\"").arg(pm)
                                     .arg(dd, 2, 10, zero)
                                     .arg(dm, 2, 10, zero)
                                     .arg(ds, 2, 10, zero);

#if 0
    if (!machineReadable && dd < 10)
    {
        if (highPrecision)
        {
            double sec = arcsec() + marcsec() / 1000.;
            return dummy.sprintf("%c%1d%c %02d\' %05.2f\"", pm, dd, 176, dm, sec);
        }

        return dummy.sprintf("%c%1d%c %02d\' %02d\"", pm, dd, 176, dm, ds);
    }

    if (!machineReadable && dd < 100)
    {
        if (highPrecision)
        {
            double sec = arcsec() + marcsec() / 1000.;
            return dummy.sprintf("%c%2d%c %02d\' %05.2f\"", pm, dd, 176, dm, sec);
        }

        return dummy.sprintf("%c%2d%c %02d\' %02d\"", pm, dd, 176, dm, ds);
    }
    if (machineReadable && dd < 100)
        return dummy.sprintf("%c%02d:%02d:%02d", pm, dd, dm, ds);

    if (!machineReadable)
    {
        if (highPrecision)
        {
            double sec = arcsec() + marcsec() / 1000.;
            return dummy.sprintf("%c%3d%c %02d\' %05.2f\"", pm, dd, 176, dm, sec);
        }
        else
            return dummy.sprintf("%c%3d%c %02d\' %02d\"", pm, dd, 176, dm, ds);
    }
    else
        return dummy.sprintf("%c%03d:%02d:%02d", pm, dd, dm, ds);
#endif
}

const QString dms::toHMSString(const bool machineReadable, const bool highPrecision) const
{
    QChar zero('0');
    dms angle;
    int hh, hm, hs;

    if (machineReadable || !highPrecision)
    // minimize the mean angle representation error of HMS format
    // set LSD transition in the middle of +- half precision range
    {
        double half_precision = 15.0 / 7200.0;
	angle.setD(Degrees() + half_precision);
        hh = angle.hour();
        hm = angle.minute();
        hs = angle.second();
    }

    if (machineReadable)
        return QString("%1:%2:%3").arg(hh, 2, 10, zero)
                                  .arg(hm, 2, 10, zero)
                                  .arg(hs, 2, 10, zero);

    if (highPrecision)
    {
        double sec = second() + msecond() / 1000.;
        return QString("%1h %2m %L3s").arg(hour(), 2, 10, zero)
                                     .arg(minute(), 2, 10, zero)
                                     .arg(sec, 2, 'f', 2, zero);
    }

    return QString("%1h %2m %3s").arg(hh, 2, 10, zero)
                                 .arg(hm, 2, 10, zero)
                                 .arg(hs, 2, 10, zero);

#if 0
    QString dummy;
    if (!machineReadable)
    {
        if (highPrecision)
        {
            double sec = second() + msecond() / 1000.;
            return dummy.sprintf("%02dh %02dm %05.2f", hour(), minute(), sec);
        }
        else
            return dummy.sprintf("%02dh %02dm %02ds", hh, hm, hs);
    }
    else
        return dummy.sprintf("%02d:%02d:%02d", hh, hm, hs);
#endif
}

dms dms::fromString(const QString &st, bool deg)
{
    dms result;
    result.setFromString(st, deg);
    return result;
    //bool ok( false );

    //ok = result.setFromString( st, deg );

    //	if ( ok )
    //return result;
    //	else {
    //		kDebug() << i18n( "Could Not Set Angle from string: " ) << st;
    //		return result;
    //	}
}

void dms::reduceToRange(enum dms::AngleRanges range)
{
    if (std::isnan(D))
        return;

    switch (range)
    {
        case MINUSPI_TO_PI:
            D -= 360. * floor((D + 180.) / 360.);
            break;
        case ZERO_TO_2PI:
            D -= 360. * floor(D / 360.);
    }
}

QDataStream &operator<<(QDataStream &out, const dms &d)
{
    out << d.D;
    return out;
}

QDataStream &operator>>(QDataStream &in, dms &d){
   double D;
   in >> D;
   d = dms(D);
   return in;
}

