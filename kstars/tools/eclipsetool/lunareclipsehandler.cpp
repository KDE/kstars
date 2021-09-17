/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lunareclipsehandler.h"
#include "skymapcomposite.h"
#include "solarsystemcomposite.h"
#include "dms.h"

LunarEclipseHandler::LunarEclipseHandler(QObject * parent) : EclipseHandler(parent),
    m_sun(), m_moon(), m_shadow(&m_moon, &m_sun, &m_Earth)
{
}

EclipseHandler::EclipseVector LunarEclipseHandler::computeEclipses(long double startJD, long double endJD)
{
    m_mode = CLOSEST_APPROACH;

    const long double SEARCH_INTERVAL = 5.l; // Days

    QVector<EclipseEvent_s> eclipses;
    QVector<long double> fullMoons = getFullMoons(startJD, endJD);

    int total = fullMoons.length();
    if (total == 0)
        return eclipses;

    float step = 1 / total;
    float progress = 0;

    connect(this, &ApproachSolver::solverMadeProgress, this, [ &, this] (int dProgress)
    {
        float tmpProgress = roundf(progress + step * dProgress);
        if (tmpProgress > progress)
        {
            progress = tmpProgress;
            emit signalProgress(static_cast<int>(progress));
        }
    });

    for(auto date : fullMoons)
    {
        findClosestApproach(date, date + SEARCH_INTERVAL, [&eclipses, this] (long double JD, dms)
        {
            EclipseEvent::ECLIPSE_TYPE type;
            updatePositions(JD);

            KSEarthShadow::ECLIPSE_TYPE extended_type = m_shadow.getEclipseType();
            switch (extended_type)
            {
                case KSEarthShadow::FULL_PENUMBRA:
                case KSEarthShadow::FULL_UMBRA:
                    type = EclipseEvent::FULL;
                    break;
                case KSEarthShadow::NONE:
                    return;
                default:
                    type = EclipseEvent::PARTIAL;
                    break;
            }

            EclipseEvent_s event = std::make_shared<LunarEclipseEvent>(JD, *getGeoLocation(), type, extended_type);
            emit signalEventFound(event);
            eclipses.append(event);
        });

        progress++;
        emit signalProgress(static_cast<int>(roundf(100 * (progress / total))));
    }

    emit signalProgress(100);
    emit signalComputationFinished();
    return eclipses;
}

// FIXME: (Valentin) This doesn't work for now. We need another method.
LunarEclipseDetails LunarEclipseHandler::findEclipseDetails(LunarEclipseEvent *event)
{
    Q_UNUSED(event);
    //    const long double INTERVAL = 1.l;

    //    const long double JD = event->getJD();
    //    const long double start = JD - INTERVAL;
    //    const long double stop = JD + INTERVAL;

    LunarEclipseDetails details;
    //    details.available = true;
    //    details.eclipseTimes.insert(JD, LunarEclipseDetails::CLOSEST_APPROACH);

    //    auto type = event->getDetailedType();
    //    auto findBoth = [&](LunarEclipseDetails::EVENT ev1 /* first (temporal) */, LunarEclipseDetails::EVENT ev2) {
    //        QMap<long double, dms> tmpApproaches;

    //        QPair<long double, dms> out;
    //        findPrecise(&out, JD, 0.001, -1);
    //        details.eclipseTimes.insert(out.first, ev1);

    //        findPrecise(&out, JD, 0.001, 1);
    //        details.eclipseTimes.insert(out.first, ev2);
    //    };

    //    // waterfall method...

    //    if(type == KSEarthShadow::NONE) {
    //        details.available = false;
    //       return details;
    //    }

    //    if(type == KSEarthShadow::FULL_UMBRA) {
    //        m_mode = UMBRA_IMMERSION;
    //        findBoth(LunarEclipseDetails::BEGIN_FULL_PENUMRA, LunarEclipseDetails::END_FULL_PENUMRA);

    //        m_mode = UMBRA_CONTACT;
    //        findBoth(LunarEclipseDetails::BEGIN_UMBRA_CONTACT, LunarEclipseDetails::END_UMBRA_CONTACT);
    //    }

    ////    if(type == KSEarthShadow::FULL_PENUMBRA || type == KSEarthShadow::FULL_UMBRA) {

    ////        m_mode = UMR
    ////    };
    return details;

}

LunarEclipseHandler::~LunarEclipseHandler()
{

}

void LunarEclipseHandler::updatePositions(long double jd)
{
    KStarsDateTime t(jd);
    KSNumbers num(jd);
    CachingDms LST(getGeoLocation()->GSTtoLST(t.gst()));
    const CachingDms * LAT = getGeoLocation()->lat();

    m_Earth.findPosition(&num);
    m_sun.findPosition(&num, LAT, &LST, &m_Earth);
    m_moon.findPosition(&num, LAT, &LST, &m_Earth);
    m_shadow.findPosition(&num, LAT, &LST, &m_Earth);
}

dms LunarEclipseHandler::findDistance()
{
    dms moon_rad = dms(m_moon.angSize() / 120);
    dms pen_rad = dms(m_shadow.getPenumbraAngSize() / 60);
    dms um_rad = dms(m_shadow.getUmbraAngSize() / 60);

    dms dist = findSkyPointDistance(&m_shadow, &m_moon);
    switch (m_mode)
    {
        case CLOSEST_APPROACH:
            return dist;
        case PENUMBRA_CONTACT:
            return dist - (moon_rad + pen_rad);
        case PUNUMBRA_IMMERSION:
            return dist + moon_rad - pen_rad;
        case UMBRA_CONTACT:
            return dist - (moon_rad + um_rad);
        case UMBRA_IMMERSION:
            return dist + moon_rad - um_rad;
    }

    return dms();
}

double LunarEclipseHandler::getMaxSeparation()
{
    const double SEP_QUALITY = 0.1;

    // we use the penumbra as measure :)
    if(m_mode == CLOSEST_APPROACH)
        return (m_shadow.getPenumbraAngSize() + m_moon.angSize()) / 60;
    else
        return SEP_QUALITY;
}

QVector<long double> LunarEclipseHandler::getFullMoons(long double startJD, long double endJD)
{
    const long double NEXT_STEP = 0.5l;
    const long double INTERVAL = 26.5l;
    long double &currentJD = startJD;

    QVector<long double> fullMoons;
    while(currentJD <= endJD)
    {
        KStarsDateTime t(currentJD);
        KSNumbers num(currentJD);
        CachingDms LST = getGeoLocation()->GSTtoLST(t.gst());

        m_sun.updateCoords(&num, true, getGeoLocation()->lat(), &LST, true);
        m_moon.updateCoords(&num, true, getGeoLocation()->lat(), &LST, true);
        m_moon.findPhase(&m_sun);

        if(m_moon.illum() > 0.9)
        {
            fullMoons.append(currentJD);
            currentJD += INTERVAL;
            continue;
        }

        currentJD += NEXT_STEP;
    }

    return fullMoons;
}

LunarEclipseEvent::LunarEclipseEvent(long double jd, GeoLocation geoPlace, EclipseEvent::ECLIPSE_TYPE type, KSEarthShadow::ECLIPSE_TYPE detailed_type)
    : EclipseEvent (jd, geoPlace, type), m_detailedType { detailed_type }
{
    m_details.available = false;
}

QString LunarEclipseEvent::getExtraInfo()
{
    switch(m_detailedType)
    {
        case KSEarthShadow::FULL_UMBRA:
            return "Full Umbral";
        case KSEarthShadow::FULL_PENUMBRA:
            return "Full Penumbral";
        case KSEarthShadow::PARTIAL:
        case KSEarthShadow::NONE:
            return "";
    }
    return "";
}

SkyObject *LunarEclipseEvent::getEclipsingObjectFromSkyComposite()
{
    return KStarsData::Instance()->skyComposite()->solarSystemComposite()->moon();
}

void LunarEclipseEvent::slotShowDetails()
{
    if(!m_details.available)
    {
        LunarEclipseHandler handler;
        GeoLocation loc = getGeolocation();
        handler.setGeoLocation(&loc);
        handler.findEclipseDetails(this);
    }
}
