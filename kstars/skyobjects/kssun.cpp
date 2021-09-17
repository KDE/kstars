/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kssun.h"

#include <typeinfo>

#include <cmath>

#include "ksnumbers.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"

KSSun::KSSun() : KSPlanet(i18n("Sun"), "sun", Qt::yellow, 1392000. /*diameter in km*/)
{
    setMag(-26.73);
}

KSSun *KSSun::clone() const
{
    Q_ASSERT(typeid(this) == typeid(static_cast<const KSSun *>(this))); // Ensure we are not slicing a derived class
    return new KSSun(*this);
}

bool KSSun::loadData()
{
    OrbitDataColl odc;
    return (odm.loadData(odc, "earth") != 0);
}

// We don't need to do anything here
void KSSun::findMagnitude(const KSNumbers *)
{
}

bool KSSun::findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth)
{
    if (Earth)
    {
        //
        // For the precision we need, the earth's orbit is circular.
        // So don't bother to iterate like KSPlanet does. Just subtract
        // The current delay and recompute (once).
        //

        //The light-travel time delay, in millenia
        //0.0057755183 is the inverse speed of light, in days/AU
        double delay = (.0057755183 * Earth->rsun()) / 365250.0;
        //
        // MHH 2002-02-04 I don't like this. But it avoids code duplication.
        // Maybe we can find a better way.
        //
        const KSPlanet *pEarth = static_cast<const KSPlanet *>(Earth);
        EclipticPosition trialpos;
        pEarth->calcEcliptic(num->julianMillenia() - delay, trialpos);

        setEcLong((trialpos.longitude + dms(180.0)).reduce());
        setEcLat(-trialpos.latitude);

        setRearth(Earth->rsun());
    }
    else
    {
        double sum[6];
        dms EarthLong, EarthLat; //heliocentric coords of Earth
        OrbitDataColl odc;
        double T = num->julianMillenia(); //Julian millenia since J2000
        double Tpow[6];

        Tpow[0] = 1.0;
        for (int i = 1; i < 6; ++i)
        {
            Tpow[i] = Tpow[i - 1] * T;
        }
        //First, find heliocentric coordinates

        if (!odm.loadData(odc, "earth"))
            return false;

        //Ecliptic Longitude
        for (int i = 0; i < 6; ++i)
        {
            sum[i] = 0.0;
            for (int j = 0; j < odc.Lon[i].size(); ++j)
            {
                sum[i] += odc.Lon[i][j].A * cos(odc.Lon[i][j].B + odc.Lon[i][j].C * T);
            }
            sum[i] *= Tpow[i];
            //qDebug() << name() << " : sum[" << i << "] = " << sum[i];
        }

        EarthLong.setRadians(sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5]);
        EarthLong = EarthLong.reduce();

        //Compute Ecliptic Latitude
        for (int i = 0; i < 6; ++i)
        {
            sum[i] = 0.0;
            for (int j = 0; j < odc.Lat[i].size(); ++j)
            {
                sum[i] += odc.Lat[i][j].A * cos(odc.Lat[i][j].B + odc.Lat[i][j].C * T);
            }
            sum[i] *= Tpow[i];
        }

        EarthLat.setRadians(sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5]);

        //Compute Heliocentric Distance
        for (int i = 0; i < 6; ++i)
        {
            sum[i] = 0.0;
            for (int j = 0; j < odc.Dst[i].size(); ++j)
            {
                sum[i] += odc.Dst[i][j].A * cos(odc.Dst[i][j].B + odc.Dst[i][j].C * T);
            }
            sum[i] *= Tpow[i];
        }

        ep.radius = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5];
        setRearth(ep.radius);

        setEcLong((EarthLong + dms(180.0)).reduce());
        setEcLat(-EarthLat);
    }

    //Finally, convert Ecliptic coords to Ra, Dec.  Ecliptic latitude is zero, by definition
    EclipticToEquatorial(num->obliquity());

    nutate(num);

    // Store in RA0 and Dec0, the unaberrated coordinates
    setRA0(ra());
    setDec0(dec());

    precess(num);

    aberrate(num);

    // We obtain the apparent geocentric ecliptic coordinates. That is, after
    // nutation and aberration have been applied.
    EquatorialToEcliptic(num->obliquity());

    //Determine the position angle
    findPA(num);

    return true;
}

SkyObject::UID KSSun::getUID() const
{
    return solarsysUID(UID_SOL_BIGOBJ) | 0;
}
