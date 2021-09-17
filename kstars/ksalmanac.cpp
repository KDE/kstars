/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksalmanac.h"

#include "geolocation.h"
#include "ksnumbers.h"
#include "kstarsdata.h"

KSAlmanac::KSAlmanac()
{
    KStarsData *data = KStarsData::Instance();

    /*dt  = KStarsDateTime::currentDateTime();
    geo = data->geo();
    dt.setTime(QTime());
    dt  = geo->LTtoUT(dt);*/

    // Jasem 2015-08-24 Do NOT use KStarsDataTime for local time, it is only for UTC
    QDateTime midnight = QDateTime(data->lt().date(), QTime());
    geo                = data->geo();
    dt                 = geo->LTtoUT(KStarsDateTime(midnight));
    update();
}

KSAlmanac::KSAlmanac(const KStarsDateTime &midnight, const GeoLocation *g)
{
    geo = g ? g : KStarsData::Instance()->geo();

    dt = midnight.isValid() ?
                midnight.timeSpec() == Qt::LocalTime ?
                    geo->LTtoUT(midnight) :
                    midnight :
                KStarsData::Instance()->ut();

    update();
}

void KSAlmanac::update()
{
    RiseSetTime(&m_Sun, &SunRise, &SunSet, &SunRiseT, &SunSetT);
    RiseSetTime(&m_Moon, &MoonRise, &MoonSet, &MoonRiseT, &MoonSetT);
    //    qDebug() << "Sun rise: " << SunRiseT.toString() << " Sun set: " << SunSetT.toString() << " Moon rise: " << MoonRiseT.toString() << " Moon set: " << MoonSetT.toString();
    findDawnDusk();
    findMoonPhase();
}

void KSAlmanac::RiseSetTime(SkyObject *o, double *riseTime, double *setTime, QTime *RiseTime, QTime *SetTime)
{
    // Compute object rise and set times
    const KStarsDateTime today = dt;
    const GeoLocation *_geo    = geo;
    *RiseTime                  = o->riseSetTime(
        today, _geo,
        true); // FIXME: Should we add a day here so that we report future rise time? Not doing so produces the right results for the moon. Not sure about the sun.
    *SetTime  = o->riseSetTime(today, _geo, false);
    *riseTime = -1.0 * RiseTime->secsTo(QTime(0, 0, 0, 0)) / 86400.0;
    *setTime  = -1.0 * SetTime->secsTo(QTime(0, 0, 0, 0)) / 86400.0;

    // Check to see if the object is circumpolar
    // NOTE: Since we are working on a local copy of the Sun / Moon,
    //       we freely change the geolocation / time without setting
    //       them back.

    KSNumbers num(dt.djd());
    CachingDms LST = geo->GSTtoLST(dt.gst());
    o->updateCoords(&num, true, geo->lat(), &LST, true);
    if (o->checkCircumpolar(geo->lat()))
    {
        if (o->alt().Degrees() > 0.0)
        {
            //Circumpolar, signal it this way:
            *riseTime = 0.0;
            *setTime  = 1.0;
        }
        else
        {
            //never rises, signal it this way:
            *riseTime = 0.0;
            *setTime  = -1.0;
        }
    }
}

void KSAlmanac::findDawnDusk(double altitude)
{
    KStarsDateTime today = dt;
    KSNumbers num(today.djd());
    CachingDms LST = geo->GSTtoLST(today.gst());

    // Relocate our local Sun to this almanac time - local midnight
    m_Sun.updateCoords(&num, true, geo->lat(), &LST, true);

    // Granularity
    int const h_inc = 5;

    // Compute the altitude of the Sun twelve hours before this almanac time
    int const start_h = -1200, end_h = +1200;
    double last_alt = findAltitude(&m_Sun, start_h/100.0);

    int dawn = -1300, dusk = -1300, min_alt_time = -1300;
    double max_alt = -100.0, min_alt = +100.0;

    // Compute the dawn and dusk times in a [-12,+12] hours around the day midnight of this almanac as well as min and max altitude
    // See the header comment about dawn and dusk positions
    for (int h = start_h + h_inc; h <= end_h; h += h_inc)
    {
        // Compute the Sun's altitude in an increasing hour interval
        double const alt = findAltitude(&m_Sun, h/100.0);

        // Deduce whether the Sun is rising or setting
        bool const rising = alt - last_alt > 0;

        // Extend min/max altitude interval, push minimum time down
        if (max_alt < alt)
        {
            max_alt = alt;
        }
        else if (alt < min_alt)
        {
            min_alt = alt;
            min_alt_time = h;
        }

        // Dawn is when the Sun is rising and crosses an altitude of -18 degrees
        if (dawn < 0)
            if (rising)
                if (last_alt <= altitude && altitude <= alt)
                    dawn = h - h_inc * (alt == last_alt ? 0 : (alt - altitude)/(alt - last_alt));

        // Dusk is when the Sun is setting and crosses an altitude of -18 degrees
        if (dusk < 0)
            if (!rising)
                if (last_alt >= altitude && altitude >= alt)
                    dusk = h - h_inc * (alt == last_alt ? 0 : (alt - altitude)/(alt - last_alt));

        last_alt = alt;
    }

    // If the Sun did not cross the astronomical twilight altitude, use the minimal altitude
    if (dawn < start_h || dusk < start_h)
    {
        DawnAstronomicalTwilight = static_cast <double> (min_alt_time) / 2400.0;
        DuskAstronomicalTwilight = static_cast <double> (min_alt_time) / 2400.0;
    }
    // If the Sun did cross the astronomical twilight, use the computed time offsets
    else
    {
        DawnAstronomicalTwilight = static_cast <double> (dawn) / 2400.0;
        DuskAstronomicalTwilight = static_cast <double> (dusk) / 2400.0;
    }

    SunMaxAlt = max_alt;
    SunMinAlt = min_alt;
}

void KSAlmanac::findMoonPhase()
{
    const KStarsDateTime today = dt;
    KSNumbers num(today.djd());
    CachingDms LST = geo->GSTtoLST(today.gst());

    m_Sun.updateCoords(&num, true, geo->lat(), &LST, true); // We can abuse our own copy of the sun and/or moon
    m_Moon.updateCoords(&num, true, geo->lat(), &LST, true);
    m_Moon.findPhase(&m_Sun);
    MoonPhase = m_Moon.phase().Degrees();
}

void KSAlmanac::setDate(const KStarsDateTime &utc_midnight)
{
    dt = utc_midnight;
    update();
}

void KSAlmanac::setLocation(const GeoLocation *geo_)
{
    geo = geo_;
    update();
}

double KSAlmanac::sunZenithAngleToTime(double z) const
{
    // TODO: Correct for movement of the sun
    double HA       = acos((cos(z * dms::DegToRad) - m_Sun.dec().sin() * geo->lat()->sin()) /
                     (m_Sun.dec().cos() * geo->lat()->cos()));
    double HASunset = acos((-m_Sun.dec().sin() * geo->lat()->sin()) / (m_Sun.dec().cos() * geo->lat()->cos()));
    return SunSet + (HA - HASunset) / 24.0;
}

double KSAlmanac::findAltitude(const SkyPoint *p, double hour)
{
    return SkyPoint::findAltitude(p, dt, geo, hour).Degrees();
}
