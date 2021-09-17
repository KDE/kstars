/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksasteroid.h"

#include "dms.h"
#include "ksnumbers.h"
#include "Options.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif

#include <qdebug.h>

#include <typeinfo>

KSAsteroid::KSAsteroid(int _catN, const QString &s, const QString &imfile, long double _JD, double _a, double _e,
                       dms _i, dms _w, dms _Node, dms _M, double _H, double _G)
    : KSPlanetBase(s, imfile), catN(_catN), JD(_JD), a(_a), e(_e), i(_i), w(_w), M(_M), N(_Node), H(_H), G(_G)
{
    setType(SkyObject::ASTEROID);
    setMag(H);
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
    // TODO: (Valentin) TOP LEVEL CONTROL OF CALCULATION FOR ALL OBJECTS
    if(!toCalculate()){
        return false;
    }

    //determine the mean anomaly for the desired date.  This is the mean anomaly for the
    //ephemeis epoch, plus the number of days between the desired date and ephemeris epoch,
    //times the asteroid's mean daily motion (360/P):

    // All elements are in the heliocentric ecliptic J2000 reference frame.
    // Mean anomaly is supplied at the Epoch (which is JD here)

    dms m = dms(double(M.Degrees() + (lastPrecessJD - JD) * 360.0 / P)).reduce();
    double sinm, cosm;
    m.SinCos(sinm, cosm);

    //compute eccentric anomaly:
    double E = m.Degrees() + e * 180.0 / dms::PI * sinm * (1.0 + e * cosm);

    if (e > 0.005) //need more accurate approximation, iterate...
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

    // Assert that the solution of the Kepler equation E = M + e sin E is accurate to about 0.1 arcsecond
    //Q_ASSERT( fabs( E - ( m.Degrees() + ( e * 180.0 / dms::PI ) * sin( E * dms::DegToRad ) ) ) < 0.10/3600.0 );

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
    helEcPos.longitude.reduceToRange(dms::ZERO_TO_2PI);
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
    double rr = sqrt(xh * xh + yh * yh);
    ELatRad   = atan2(zh, rr);

    ep.longitude.setRadians(ELongRad);
    ep.longitude.reduceToRange(dms::ZERO_TO_2PI);
    ep.latitude.setRadians(ELatRad);
    if (Earth)
        setRearth(Earth);

    EclipticToEquatorial(num->obliquity());

    // JM 2017-09-10: The calculations above produce J2000 RESULTS
    // So we have to precess as well
    setRA0(ra());
    setDec0(dec());
    apparentCoord(J2000, lastPrecessJD);
    //nutate(num);
    //aberrate(num);

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

bool KSAsteroid::toCalculate()
{
    // Filter by magnitude, but draw focused asteroids anyway :)
    return ((mag() < Options::magLimitAsteroid())|| (std::isnan(mag()) != 0) ||
#ifdef KSTARS_LITE
            SkyMapLite::Instance()->focusObject() == this
#else
            SkyMap::Instance()->focusObject() == this
#endif
            );
}

QDataStream &operator<<(QDataStream &out, const KSAsteroid &asteroid)
{
    out << asteroid.Name << asteroid.OrbitClass << asteroid.Dimensions << asteroid.OrbitID
        << asteroid.catN << static_cast<double>(asteroid.JD) << asteroid.a
        << asteroid.e << asteroid.i << asteroid.w << asteroid.N
        << asteroid.M << asteroid.H << asteroid.G << asteroid.q
        << asteroid.NEO << asteroid.Diameter
        << asteroid.Albedo << asteroid.RotationPeriod
        << asteroid.Period << asteroid.EarthMOID;
    return out;
}

QDataStream &operator>>(QDataStream &in, KSAsteroid *&asteroid)
{
    QString name, orbit_id, orbit_class, dimensions;
    double q, a, e, H, G, earth_moid;
    dms i, w, N, M;
    double JD;
    float diameter, albedo, rot_period, period;
    bool neo;
    int catN;

    in >> name;
    in >> orbit_class;
    in >> dimensions;
    in >> orbit_id;

    in >> catN >> JD >> a >> e >> i >> w >> N >> M >> H >> G >> q >> neo >> diameter
            >> albedo >> rot_period >> period >> earth_moid;

    asteroid = new KSAsteroid(catN, name, QString(), JD, a, e, i, w, N, M, H, G);
    asteroid->setPerihelion(q);
    asteroid->setOrbitID(orbit_id);
    asteroid->setNEO(neo);
    asteroid->setDiameter(diameter);
    asteroid->setDimensions(dimensions);
    asteroid->setAlbedo(albedo);
    asteroid->setRotationPeriod(rot_period);
    asteroid->setPeriod(period);
    asteroid->setEarthMOID(earth_moid);
    asteroid->setOrbitClass(orbit_class);
    asteroid->setPhysicalSize(diameter);

    return in;
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
