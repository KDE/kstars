/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksplanetbase.h"

#include "ksnumbers.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "Options.h"
#include "skymap.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "ksmoon.h"
#include "ksplanet.h"
#include "kssun.h"
#include "texturemanager.h"
#include "skycomponents/skymapcomposite.h"

QVector<QColor> KSPlanetBase::planetColor = QVector<QColor>() << QColor("slateblue") << //Mercury
        QColor("lightgreen") <<                     //Venus
        QColor("red") <<                            //Mars
        QColor("goldenrod") <<                      //Jupiter
        QColor("khaki") <<                          //Saturn
        QColor("lightseagreen") <<                  //Uranus
        QColor("skyblue") <<                        //Neptune
        QColor("grey") <<                           //Pluto
        QColor("yellow") <<                         //Sun
        QColor("white");                            //Moon

const SkyObject::UID KSPlanetBase::UID_SOL_BIGOBJ   = 0;
const SkyObject::UID KSPlanetBase::UID_SOL_ASTEROID = 1;
const SkyObject::UID KSPlanetBase::UID_SOL_COMET    = 2;

KSPlanetBase::KSPlanetBase(const QString &s, const QString &image_file, const QColor &c, double pSize)
    : TrailObject(2, 0.0, 0.0, 0.0, s)
{
    init(s, image_file, c, pSize);
}

void KSPlanetBase::init(const QString &s, const QString &image_file, const QColor &c, double pSize)
{
    m_image       = TextureManager::getImage(image_file);
    PositionAngle = 0.0;
    PhysicalSize  = pSize;
    m_Color       = c;
    setName(s);
    setLongName(s);
}

KSPlanetBase *KSPlanetBase::createPlanet(int n)
{
    switch (n)
    {
        case KSPlanetBase::MERCURY:
        case KSPlanetBase::VENUS:
        case KSPlanetBase::MARS:
        case KSPlanetBase::JUPITER:
        case KSPlanetBase::SATURN:
        case KSPlanetBase::URANUS:
        case KSPlanetBase::NEPTUNE:
            return new KSPlanet(n);
        /*case KSPlanetBase::PLUTO:
            return new KSPluto();
            break;*/
        case KSPlanetBase::SUN:
            return new KSSun();
        case KSPlanetBase::MOON:
            return new KSMoon();
    }
    return nullptr;
}

void KSPlanetBase::EquatorialToEcliptic(const CachingDms *Obliquity)
{
    findEcliptic(Obliquity, ep.longitude, ep.latitude);
}

void KSPlanetBase::EclipticToEquatorial(const CachingDms *Obliquity)
{
    setFromEcliptic(Obliquity, ep.longitude, ep.latitude);
}

void KSPlanetBase::updateCoords(const KSNumbers *num, bool includePlanets, const CachingDms *lat, const CachingDms *LST,
                                bool)
{
    KStarsData *kd = KStarsData::Instance();

    if (kd == nullptr || !includePlanets)
        return;

    kd->skyComposite()->earth()->findPosition(num); //since we don't pass lat & LST, localizeCoords will be skipped

    if (lat && LST)
    {
        findPosition(num, lat, LST, kd->skyComposite()->earth());
        // Don't add to the trail this time
        if (hasTrail())
            Trail.takeLast();
    }
    else
    {
        findGeocentricPosition(num, kd->skyComposite()->earth());
    }
}

void KSPlanetBase::findPosition(const KSNumbers *num, const CachingDms *lat, const CachingDms *LST,
                                const KSPlanetBase *Earth)
{

    lastPrecessJD = num->julianDay();

    findGeocentricPosition(num, Earth); //private function, reimplemented in each subclass
    findPhase();
    setAngularSize(findAngularSize()); //angular size in arcmin

    if (lat && LST)
        localizeCoords(num, lat, LST); //correct for figure-of-the-Earth

    if (hasTrail())
    {
        addToTrail(KStarsDateTime(num->getJD()).toString("yyyy.MM.dd hh:mm") +
                   i18nc("Universal time", "UT")); // TODO: Localize date/time format?
        if (Trail.size() > TrailObject::MaxTrail)
            clipTrail();
    }

    findMagnitude(num);

    if (type() == SkyObject::COMET)
    {
        // Compute tail size
        KSComet *me = static_cast<KSComet *>(this);
        double comaAngSize;
        // Convert the tail size in km to angular tail size (degrees)
        comaAngSize = asin(physicalSize() / Rearth / AU_KM) * 60.0 * 180.0 / dms::PI;
        // Find the apparent length as projected on the celestial sphere (the comet's tail points away from the sun)
        me->setComaAngSize(comaAngSize * fabs(sin(phase().radians())));
    }
}

bool KSPlanetBase::isMajorPlanet() const
{
    if (name() == i18n("Mercury") || name() == i18n("Venus") || name() == i18n("Mars") || name() == i18n("Jupiter") ||
            name() == i18n("Saturn") || name() == i18n("Uranus") || name() == i18n("Neptune"))
        return true;
    return false;
}

void KSPlanetBase::localizeCoords(const KSNumbers *num, const CachingDms *lat, const CachingDms *LST)
{
    //convert geocentric coordinates to local apparent coordinates (topocentric coordinates)
    dms HA, HA2; //Hour Angle, before and after correction
    double rsinp, rcosp, u, sinHA, cosHA, sinDec, cosDec, D;
    double cosHA2;
    double r = Rearth * AU_KM; //distance from Earth, in km
    u        = atan(0.996647 * tan(lat->radians()));
    rsinp    = 0.996647 * sin(u);
    rcosp    = cos(u);
    HA.setD(LST->Degrees() - ra().Degrees());
    HA.SinCos(sinHA, cosHA);
    dec().SinCos(sinDec, cosDec);

    D = atan2(rcosp * sinHA, r * cosDec / 6378.14 - rcosp * cosHA);
    dms temp;
    temp.setRadians(ra().radians() - D);
    setRA(temp);

    HA2.setD(LST->Degrees() - ra().Degrees());
    cosHA2 = cos(HA2.radians());

    //temp.setRadians( atan2( cosHA2*( r*sinDec/6378.14 - rsinp ), r*cosDec*cosHA/6378.14 - rcosp ) );
    // The atan2() version above makes the planets move crazy in the htm branch -jbb
    temp.setRadians(atan(cosHA2 * (r * sinDec / 6378.14 - rsinp) / (r * cosDec * cosHA / 6378.14 - rcosp)));

    setDec(temp);

    //Make sure Dec is between -90 and +90
    if (dec().Degrees() > 90.0)
    {
        setDec(180.0 - dec().Degrees());
        setRA(ra().Hours() + 12.0);
        ra().reduce();
    }
    if (dec().Degrees() < -90.0)
    {
        setDec(180.0 + dec().Degrees());
        setRA(ra().Hours() + 12.0);
        ra().reduce();
    }

    EquatorialToEcliptic(num->obliquity());
}

void KSPlanetBase::setRearth(const KSPlanetBase *Earth)
{
    double sinL, sinB, sinL0, sinB0;
    double cosL, cosB, cosL0, cosB0;
    double x, y, z;

    //The Moon's Rearth is set in its findGeocentricPosition()...
    if (name() == i18n("Moon"))
    {
        return;
    }

    if (name() == i18n("Earth"))
    {
        Rearth = 0.0;
        return;
    }

    if (!Earth)
    {
        qDebug() << "KSPlanetBase::setRearth():  Error: Need an Earth pointer.  (" << name() << ")";
        Rearth = 1.0;
        return;
    }

    Earth->ecLong().SinCos(sinL0, cosL0);
    Earth->ecLat().SinCos(sinB0, cosB0);
    double eX = Earth->rsun() * cosB0 * cosL0;
    double eY = Earth->rsun() * cosB0 * sinL0;
    double eZ = Earth->rsun() * sinB0;

    helEcLong().SinCos(sinL, cosL);
    helEcLat().SinCos(sinB, cosB);
    x = rsun() * cosB * cosL - eX;
    y = rsun() * cosB * sinL - eY;
    z = rsun() * sinB - eZ;

    Rearth = sqrt(x * x + y * y + z * z);

    //Set angular size, in arcmin
    AngularSize = asin(PhysicalSize / Rearth / AU_KM) * 60. * 180. / dms::PI;
}

void KSPlanetBase::findPA(const KSNumbers *num)
{
    //Determine position angle of planet (assuming that it is aligned with
    //the Ecliptic, which is only roughly correct).
    //Displace a point along +Ecliptic Latitude by 1 degree
    SkyPoint test;
    dms newELat(ecLat().Degrees() + 1.0);
    test.setFromEcliptic(num->obliquity(), ecLong(), newELat);
    double dx = ra().Degrees() - test.ra().Degrees();
    double dy = test.dec().Degrees() - dec().Degrees();
    double pa;
    if (dy)
    {
        pa = atan2(dx, dy) * 180.0 / dms::PI;
    }
    else
    {
        pa = dx < 0 ? 90.0 : -90.0;
    }
    setPA(pa);
}

double KSPlanetBase::labelOffset() const
{
    double size = angSize() * dms::PI * Options::zoomFactor() / 10800.0;

    //Determine minimum size for offset
    double minsize = 4.;
    if (type() == SkyObject::ASTEROID || type() == SkyObject::COMET)
        minsize = 2.;
    if (name() == i18n("Sun") || name() == i18n("Moon"))
        minsize = 8.;
    if (size < minsize)
        size = minsize;

    //Inflate offset for Saturn
    if (name() == i18n("Saturn"))
        size = int(2.5 * size);

    return 0.5 * size + 4.;
}

void KSPlanetBase::findPhase()
{
    if (2 * rsun()*rearth() == 0)
    {
        Phase = std::numeric_limits<double>::quiet_NaN();
        return;
    }
    /* Compute the phase of the planet in degrees */
    double earthSun = KStarsData::Instance()->skyComposite()->earth()->rsun();
    double cosPhase = (rsun() * rsun() + rearth() * rearth() - earthSun * earthSun) / (2 * rsun() * rearth());

    Phase           = acos(cosPhase) * 180.0 / dms::PI;
    /* More elegant way of doing it, but requires the Sun.
       TODO: Switch to this if and when we make KSSun a singleton */
    //    Phase = ecLong()->Degrees() - Sun->ecLong()->Degrees();
}
