/***************************************************************************
                          ksasteroid.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksasteroid.h"

#include "dms.h"
#include "ksnumbers.h"

#include <typeinfo>

KSAsteroid::KSAsteroid(int _catN, const QString &s, const QString &imfile, long double _JD, double _a, double _e,
                       dms _i, dms _w, dms _Node, dms _M, double _H, double _G)
    : KSPlanetBase(s, imfile), catN(_catN), JD(_JD), a(_a), e(_e), i(_i), w(_w), M(_M), N(_Node), H(_H), G(_G)
{
    setType(SkyObject::ASTEROID);
    //Compute the orbital Period from Kepler's 3rd law:
    P = 365.2568984 * pow(a, 1.5); //period in days
}

KSAsteroid *KSAsteroid::clone() const
{
    Q_ASSERT(typeid(this) ==
             typeid(static_cast<const KSAsteroid *>(this))); // Ensure we are not slicing a derived class
    return new KSAsteroid(*this);
}

bool KSAsteroid::findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth)
{
    //determine the mean anomaly for the desired date.  This is the mean anomaly for the
    //ephemeis epoch, plus the number of days between the desired date and ephemeris epoch,
    //times the asteroid's mean daily motion (360/P):
    dms m = dms(double(M.Degrees() + (num->julianDay() - JD) * 360.0 / P)).reduce();
    double sinm, cosm;
    m.SinCos(sinm, cosm);

    //compute eccentric anomaly:
    double E = m.Degrees() + e * 180.0 / dms::PI * sinm * (1.0 + e * cosm);

    if (e > 0.05) //need more accurate approximation, iterate...
    {
        double E0;
        int iter(0);
        do
        {
            E0 = E;
            iter++;
            E = E0 -
                (E0 - e * 180.0 / dms::PI * sin(E0 * dms::DegToRad) - m.Degrees()) / (1 - e * cos(E0 * dms::DegToRad));
        } while (fabs(E - E0) > 0.001 && iter < 1000);
    }

    double sinE, cosE;
    dms E1(E);
    E1.SinCos(sinE, cosE);

    double xv = a * (cosE - e);
    double yv = a * sqrt(1.0 - e * e) * sinE;

    //v is the true anomaly; r is the distance from the Sun
    double v = atan2(yv, xv) / dms::DegToRad;
    double r = sqrt(xv * xv + yv * yv);

    //vw is the sum of the true anomaly and the argument of perihelion
    dms vw(v + w.Degrees());
    double sinN, cosN, sinvw, cosvw, sini, cosi;

    N.SinCos(sinN, cosN);
    vw.SinCos(sinvw, cosvw);
    i.SinCos(sini, cosi);

    //xh, yh, zh are the heliocentric cartesian coords with the ecliptic plane congruent with zh=0.
    double xh = r * (cosN * cosvw - sinN * sinvw * cosi);
    double yh = r * (sinN * cosvw + cosN * sinvw * cosi);
    double zh = r * (sinvw * sini);

    //the spherical ecliptic coordinates:
    double ELongRad = atan2(yh, xh);
    double ELatRad  = atan2(zh, r);

    helEcPos.longitude.setRadians(ELongRad);
    helEcPos.latitude.setRadians(ELatRad);
    setRsun(r);

    if (Earth)
    {
        //xe, ye, ze are the Earth's heliocentric cartesian coords
        double cosBe, sinBe, cosLe, sinLe;
        Earth->ecLong().SinCos(sinLe, cosLe);
        Earth->ecLat().SinCos(sinBe, cosBe);

        double xe = Earth->rsun() * cosBe * cosLe;
        double ye = Earth->rsun() * cosBe * sinLe;
        double ze = Earth->rsun() * sinBe;

        //convert to geocentric ecliptic coordinates by subtracting Earth's coords:
        xh -= xe;
        yh -= ye;
        zh -= ze;
    }

    //the spherical geocentricecliptic coordinates:
    ELongRad  = atan2(yh, xh);
    double rr = sqrt(xh * xh + yh * yh + zh * zh);
    ELatRad   = atan2(zh, rr);

    ep.longitude.setRadians(ELongRad);
    ep.latitude.setRadians(ELatRad);
    if (Earth)
        setRearth(Earth);

    EclipticToEquatorial(num->obliquity());
    nutate(num);
    aberrate(num);

    return true;
}

void KSAsteroid::findMagnitude(const KSNumbers *)
{
    double param     = 5 * log10(rsun() * rearth());
    double phase_rad = phase().radians();
    double phi1      = exp(-3.33 * pow(tan(phase_rad / 2), 0.63));
    double phi2      = exp(-1.87 * pow(tan(phase_rad / 2), 1.22));

    setMag(H + param - 2.5 * log((1 - G) * phi1 + G * phi2));
}

void KSAsteroid::setPerihelion(double perihelion)
{
    q = perihelion;
}

void KSAsteroid::setEarthMOID(double earth_moid)
{
    EarthMOID = earth_moid;
}

void KSAsteroid::setAlbedo(float albedo)
{
    Albedo = albedo;
}

void KSAsteroid::setDiameter(float diam)
{
    Diameter = diam;
}

void KSAsteroid::setDimensions(QString dim)
{
    Dimensions = dim;
}

void KSAsteroid::setNEO(bool neo)
{
    NEO = neo;
}

void KSAsteroid::setOrbitClass(QString orbit_class)
{
    OrbitClass = orbit_class;
}

void KSAsteroid::setOrbitID(QString orbit_id)
{
    OrbitID = orbit_id;
}

void KSAsteroid::setPeriod(float per)
{
    Period = per;
}

void KSAsteroid::setRotationPeriod(float rot_per)
{
    RotationPeriod = rot_per;
}

//Unused virtual function from KSPlanetBase
bool KSAsteroid::loadData()
{
    return false;
}

SkyObject::UID KSAsteroid::getUID() const
{
    return solarsysUID(UID_SOL_ASTEROID) | catN;
}
