/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksplanet.h"

#include "ksnumbers.h"
#include "ksutils.h"
#include "ksfilereader.h"

#include <cmath>
#include <typeinfo>

#include "kstars_debug.h"

KSPlanet::OrbitDataManager KSPlanet::odm;

KSPlanet::OrbitDataManager::OrbitDataManager()
{
    //EMPTY
}

bool KSPlanet::OrbitDataManager::readOrbitData(const QString &fname, QVector<OrbitData> *vector)
{
    QFile f;

    if (KSUtils::openDataFile(f, fname))
    {
        KSFileReader fileReader(f); // close file is included
        QStringList fields;
        while (fileReader.hasMoreLines())
        {
            fields = fileReader.readLine().split(' ', QString::SkipEmptyParts);

            if (fields.size() == 3)
            {
                double A = fields[0].toDouble();
                double B = fields[1].toDouble();
                double C = fields[2].toDouble();
                vector->append(OrbitData(A, B, C));
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool KSPlanet::OrbitDataManager::loadData(KSPlanet::OrbitDataColl &odc, const QString &n)
{
    QString fname, snum;
    QFile f;
    int nCount = 0;
    QString nl = n.toLower();

    if (hash.contains(nl))
    {
        odc = hash[nl];
        return true; //orbit data already loaded
    }

    //Create a new OrbitDataColl
    OrbitDataColl ret;

    //Ecliptic Longitude
    for (int i = 0; i < 6; ++i)
    {
        snum.setNum(i);
        fname = nl + ".L" + snum + ".vsop";
        if (readOrbitData(fname, &ret.Lon[i]))
            nCount++;
    }

    if (nCount == 0)
        return false;

    //Ecliptic Latitude
    for (int i = 0; i < 6; ++i)
    {
        snum.setNum(i);
        fname = nl + ".B" + snum + ".vsop";
        if (readOrbitData(fname, &ret.Lat[i]))
            nCount++;
    }

    if (nCount == 0)
        return false;

    //Heliocentric Distance
    for (int i = 0; i < 6; ++i)
    {
        snum.setNum(i);
        fname = nl + ".R" + snum + ".vsop";
        if (readOrbitData(fname, &ret.Dst[i]))
            nCount++;
    }

    if (nCount == 0)
        return false;

    hash[nl] = ret;
    odc      = hash[nl];

    return true;
}

KSPlanet::KSPlanet(const QString &s, const QString &imfile, const QColor &c, double pSize)
    : KSPlanetBase(s, imfile, c, pSize)
{
}

KSPlanet::KSPlanet(int n) : KSPlanetBase()
{
    switch (n)
    {
        case MERCURY:
            KSPlanetBase::init(i18n("Mercury"), "mercury", KSPlanetBase::planetColor[KSPlanetBase::MERCURY], 4879.4);
            break;
        case VENUS:
            KSPlanetBase::init(i18n("Venus"), "venus", KSPlanetBase::planetColor[KSPlanetBase::VENUS], 12103.6);
            break;
        case MARS:
            KSPlanetBase::init(i18n("Mars"), "mars", KSPlanetBase::planetColor[KSPlanetBase::MARS], 6792.4);
            break;
        case JUPITER:
            KSPlanetBase::init(i18n("Jupiter"), "jupiter", KSPlanetBase::planetColor[KSPlanetBase::JUPITER], 142984.);
            break;
        case SATURN:
            KSPlanetBase::init(i18n("Saturn"), "saturn", KSPlanetBase::planetColor[KSPlanetBase::SATURN], 120536.);
            break;
        case URANUS:
            KSPlanetBase::init(i18n("Uranus"), "uranus", KSPlanetBase::planetColor[KSPlanetBase::URANUS], 51118.);
            break;
        case NEPTUNE:
            KSPlanetBase::init(i18n("Neptune"), "neptune", KSPlanetBase::planetColor[KSPlanetBase::NEPTUNE], 49572.);
            break;
        default:
            qDebug() << "Error: Illegal identifier in KSPlanet constructor: " << n;
            break;
    }
}

KSPlanet *KSPlanet::clone() const
{
    Q_ASSERT(typeid(this) == typeid(static_cast<const KSPlanet *>(this))); // Ensure we are not slicing a derived class
    return new KSPlanet(*this);
}

// TODO: Get rid of this dirty hack post KDE 4.2 release
QString KSPlanet::untranslatedName() const
{
    if (name() == i18n("Mercury"))
        return "Mercury";
    else if (name() == i18n("Venus"))
        return "Venus";
    else if (name() == i18n("Mars"))
        return "Mars";
    else if (name() == i18n("Jupiter"))
        return "Jupiter";
    else if (name() == i18n("Saturn"))
        return "Saturn";
    else if (name() == i18n("Uranus"))
        return "Uranus";
    else if (name() == i18n("Neptune"))
        return "Neptune";
    else if (name() == i18n("Earth"))
        return "Earth";
    else if (name() == i18n("Earth Shadow"))
        return "Earth Shadow";
    else if (name() == i18n("Moon"))
        return "Moon";
    else if (name() == i18n("Sun"))
        return "Sun";
    else
        return name();
}

//we don't need the reference to the ODC, so just give it a junk variable
bool KSPlanet::loadData()
{
    OrbitDataColl odc;
    return odm.loadData(odc, untranslatedName());
}

void KSPlanet::calcEcliptic(double Tau, EclipticPosition &epret) const
{
    double sum[6];
    OrbitDataColl odc;
    double Tpow[6];

    Tpow[0] = 1.0;
    for (int i = 1; i < 6; ++i)
    {
        Tpow[i] = Tpow[i - 1] * Tau;
    }

    if (!odm.loadData(odc, untranslatedName()))
    {
        epret.longitude = dms(0.0);
        epret.latitude  = dms(0.0);
        epret.radius    = 0.0;
        qCWarning(KSTARS) << "Could not get data for name:" << name() << "(" << untranslatedName() << ")";
        return;
    }

    //Ecliptic Longitude
    for (int i = 0; i < 6; ++i)
    {
        sum[i] = 0.0;
        for (int j = 0; j < odc.Lon[i].size(); ++j)
        {
            sum[i] += odc.Lon[i][j].A * cos(odc.Lon[i][j].B + odc.Lon[i][j].C * Tau);
            /*
            qDebug() << "sum[" << i <<"] =" << sum[i] <<
                " A = " << odc.Lon[i][j].A << " B = " << odc.Lon[i][j].B <<
                " C = " << odc.Lon[i][j].C;
                */
        }
        sum[i] *= Tpow[i];
        //qDebug() << name() << " : sum[" << i << "] = " << sum[i];
    }

    epret.longitude.setRadians(sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5]);
    epret.longitude.setD(epret.longitude.reduce().Degrees());

    //Compute Ecliptic Latitude
    for (uint i = 0; i < 6; ++i)
    {
        sum[i] = 0.0;
        for (int j = 0; j < odc.Lat[i].size(); ++j)
        {
            sum[i] += odc.Lat[i][j].A * cos(odc.Lat[i][j].B + odc.Lat[i][j].C * Tau);
        }
        sum[i] *= Tpow[i];
    }

    epret.latitude.setRadians(sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5]);

    //Compute Heliocentric Distance
    for (uint i = 0; i < 6; ++i)
    {
        sum[i] = 0.0;
        for (int j = 0; j < odc.Dst[i].size(); ++j)
        {
            sum[i] += odc.Dst[i][j].A * cos(odc.Dst[i][j].B + odc.Dst[i][j].C * Tau);
        }
        sum[i] *= Tpow[i];
    }

    epret.radius = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5];

    /*
    qDebug() << name() << " pre: Lat = " << epret.latitude.toDMSString() << " Long = " <<
        epret.longitude.toDMSString() << " Dist = " << epret.radius;
    */
}

bool KSPlanet::findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth)
{
    if (Earth != nullptr)
    {
        double sinL, sinL0, sinB, sinB0;
        double cosL, cosL0, cosB, cosB0;
        double x = 0.0, y = 0.0, z = 0.0;

        double olddst = -1000;
        double dst    = 0;

        EclipticPosition trialpos;

        double jm = num->julianMillenia();

        Earth->ecLong().SinCos(sinL0, cosL0);
        Earth->ecLat().SinCos(sinB0, cosB0);

        double eX = Earth->rsun() * cosB0 * cosL0;
        double eY = Earth->rsun() * cosB0 * sinL0;
        double eZ = Earth->rsun() * sinB0;

        bool once = true;
        while (fabs(dst - olddst) > .001)
        {
            calcEcliptic(jm, trialpos);

            // We store the heliocentric ecliptic coordinates the first time they are computed.
            if (once)
            {
                helEcPos = trialpos;
                once     = false;
            }

            olddst = dst;

            trialpos.longitude.SinCos(sinL, cosL);
            trialpos.latitude.SinCos(sinB, cosB);

            x = trialpos.radius * cosB * cosL - eX;
            y = trialpos.radius * cosB * sinL - eY;
            z = trialpos.radius * sinB - eZ;

            //distance from Earth
            dst = sqrt(x * x + y * y + z * z);

            //The light-travel time delay, in millenia
            //0.0057755183 is the inverse speed of light,
            //in days/AU
            double delay = (.0057755183 * dst) / 365250.0;

            jm = num->julianMillenia() - delay;
        }

        ep.longitude.setRadians(atan2(y, x));
        ep.longitude.reduce();
        ep.latitude.setRadians(atan2(z, sqrt(x * x + y * y)));
        setRsun(trialpos.radius);
        setRearth(dst);

        EclipticToEquatorial(num->obliquity());

        setRA0(ra());
        setDec0(dec());
        apparentCoord(J2000, lastPrecessJD);

        //nutate(num);
        //aberrate(num);
    }
    else
    {
        calcEcliptic(num->julianMillenia(), ep);
        helEcPos = ep;
    }

    //determine the position angle
    findPA(num);

    return true;
}

void KSPlanet::findMagnitude(const KSNumbers *num)
{
    double cosDec, sinDec;
    dec().SinCos(cosDec, sinDec);

    /* Computation of the visual magnitude (V band) of the planet.
    * Algorithm provided by Pere Planesas (Observatorio Astronomico Nacional)
    * It has some similarity to J. Meeus algorithm in Astronomical Algorithms, Chapter 40.
    * */

    // Initialized to the faintest magnitude observable with the HST
    float magnitude = 30;

    double param = 5 * log10(rsun() * rearth());
    double phase = this->phase().Degrees();
    double f1    = phase / 100.;

    if (name() == i18n("Mercury"))
    {
        if (phase > 150.)
            f1 = 1.5;
        magnitude = -0.36 + param + 3.8 * f1 - 2.73 * f1 * f1 + 2 * f1 * f1 * f1;
    }
    else if (name() == i18n("Venus"))
    {
        magnitude = -4.29 + param + 0.09 * f1 + 2.39 * f1 * f1 - 0.65 * f1 * f1 * f1;
    }
    else if (name() == i18n("Mars"))
    {
        magnitude = -1.52 + param + 0.016 * phase;
    }
    else if (name() == i18n("Jupiter"))
    {
        magnitude = -9.25 + param + 0.005 * phase;
    }
    else if (name() == i18n("Saturn"))
    {
        double T     = num->julianCenturies();
        double a0    = (40.66 - 4.695 * T) * dms::PI / 180.;
        double d0    = (83.52 + 0.403 * T) * dms::PI / 180.;
        double sinx  = -cos(d0) * cosDec * cos(a0 - ra().radians());
        sinx         = fabs(sinx - sin(d0) * sinDec);
        double rings = -2.6 * sinx + 1.25 * sinx * sinx;
        magnitude    = -8.88 + param + 0.044 * phase + rings;
    }
    else if (name() == i18n("Uranus"))
    {
        magnitude = -7.19 + param + 0.0028 * phase;
    }
    else if (name() == i18n("Neptune"))
    {
        magnitude = -6.87 + param;
    }
    setMag(magnitude);
}

SkyObject::UID KSPlanet::getUID() const
{
    SkyObject::UID n;
    if (name() == i18n("Mercury"))
    {
        n = 1;
    }
    else if (name() == i18n("Venus"))
    {
        n = 2;
    }
    else if (name() == i18n("Earth"))
    {
        n = 3;
    }
    else if (name() == i18n("Mars"))
    {
        n = 4;
    }
    else if (name() == i18n("Jupiter"))
    {
        n = 5;
    }
    else if (name() == i18n("Saturn"))
    {
        n = 6;
    }
    else if (name() == i18n("Uranus"))
    {
        n = 7;
    }
    else if (name() == i18n("Neptune"))
    {
        n = 8;
    }
    else
    {
        return SkyObject::invalidUID;
    }
    return solarsysUID(UID_SOL_BIGOBJ) | n;
}
