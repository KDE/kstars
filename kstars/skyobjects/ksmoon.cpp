/*
    SPDX-FileCopyrightText: 2001 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksmoon.h"

#include "ksnumbers.h"
#include "ksutils.h"
#include "kssun.h"
#include "kstarsdata.h"
#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/solarsystemcomposite.h"
#include "texturemanager.h"

#include <QFile>
#include <QTextStream>

#include <cstdlib>
#include <cmath>
#if defined(_MSC_VER)
#include <float.h>
#endif

#include <typeinfo>

using namespace std;

namespace
{
// Convert degrees to radians and put it into [0,2*pi] range
double degToRad(double x)
{
    return dms::DegToRad * KSUtils::reduceAngle(x, 0.0, 360.0);
}

/*
 * Data used to calculate moon magnitude.
 *
 * Formula and data were obtained from SkyChart v3.x Beta
 *
 */
// intensities in Table 1 of M. Minnaert (1961),
// Phase  Frac.            Phase  Frac.            Phase  Frac.
// angle  ill.   Mag       angle  ill.   Mag       angle  ill.   Mag
//  0    1.00  -12.7        60   0.75  -11.0       120   0.25  -8.7
// 10    0.99  -12.4        70   0.67  -10.8       130   0.18  -8.2
// 20    0.97  -12.1        80   0.59  -10.4       140   0.12  -7.6
// 30    0.93  -11.8        90   0.50  -10.0       150   0.07  -6.7
// 40    0.88  -11.5       100   0.41   -9.6       160   0.03  -3.4
// 50    0.82  -11.2       110   0.33   -9.2
static const double MagArray[19] = { -12.7, -12.4, -12.1, -11.8, -11.5, -11.2, -11.0, -10.8, -10.4, -10.0,
                                     -9.6,  -9.2,  -8.7,  -8.2,  -7.6,  -6.7,  -3.4,  0,     0
                                   };
}

KSMoon::KSMoon() : KSPlanetBase(i18n("Moon"), QString(), QColor("white"), 3474.8 /*diameter in km*/)
{
    instance_count++;
    //Reset object type
    setType(SkyObject::MOON);
}

KSMoon::KSMoon(const KSMoon &o) : KSPlanetBase(o)
{
    instance_count++;
}

KSMoon *KSMoon::clone() const
{
    Q_ASSERT(typeid(this) == typeid(static_cast<const KSMoon *>(this))); // Ensure we are not slicing a derived class
    return new KSMoon(*this);
}

KSMoon::~KSMoon()
{
    instance_count--;
    if (instance_count <= 0)
    {
        LRData.clear();
        BData.clear();
        data_loaded = false;
    }
}

bool KSMoon::data_loaded   = false;
int KSMoon::instance_count = 0;
QList<KSMoon::MoonLRData> KSMoon::LRData;
QList<KSMoon::MoonBData> KSMoon::BData;

bool KSMoon::loadData()
{
    if (data_loaded)
        return true;

    QStringList fields;
    QFile f;

    if (KSUtils::openDataFile(f, "moonLR.dat"))
    {
        QTextStream stream(&f);
        while (!stream.atEnd())
        {
            fields = stream.readLine().split(' ', QString::SkipEmptyParts);

            if (fields.size() == 6)
            {
                LRData.append(MoonLRData());
                LRData.last().nd  = fields[0].toInt();
                LRData.last().nm  = fields[1].toInt();
                LRData.last().nm1 = fields[2].toInt();
                LRData.last().nf  = fields[3].toInt();
                LRData.last().Li  = fields[4].toDouble();
                LRData.last().Ri  = fields[5].toDouble();
            }
        }
        f.close();
    }
    else
        return false;

    if (KSUtils::openDataFile(f, "moonB.dat"))
    {
        QTextStream stream(&f);
        while (!stream.atEnd())
        {
            fields = stream.readLine().split(' ', QString::SkipEmptyParts);

            if (fields.size() == 5)
            {
                BData.append(MoonBData());
                BData.last().nd  = fields[0].toInt();
                BData.last().nm  = fields[1].toInt();
                BData.last().nm1 = fields[2].toInt();
                BData.last().nf  = fields[3].toInt();
                BData.last().Bi  = fields[4].toDouble();
            }
        }
        f.close();
    }

    data_loaded = true;
    return true;
}

bool KSMoon::findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *)
{
    //Algorithms in this subroutine are taken from Chapter 45 of "Astronomical Algorithms"
    //by Jean Meeus (1991, Willmann-Bell, Inc. ISBN 0-943396-35-2.  https://www.willbell.com/math/mc1.htm)
    //updated to Jean Messus (1998, Willmann-Bell, http://www.naughter.com/aa.html )

    double T, L, D, M, M1, F, A1, A2, A3;
    double sumL, sumR, sumB;

    //Julian centuries since J2000
    T = num->julianCenturies();

    double Et = 1.0 - 0.002516 * T - 0.0000074 * T * T;

    //Moon's mean longitude
    L = degToRad(218.3164477 + 481267.88123421 * T - 0.0015786 * T * T + T * T * T / 538841.0 -
                 T * T * T * T / 65194000.0);
    //Moon's mean elongation
    D = degToRad(297.8501921 + 445267.1114034 * T - 0.0018819 * T * T + T * T * T / 545868.0 -
                 T * T * T * T / 113065000.0);
    //Sun's mean anomaly
    M = degToRad(357.5291092 + 35999.0502909 * T - 0.0001536 * T * T + T * T * T / 24490000.0);
    //Moon's mean anomaly
    M1 = degToRad(134.9633964 + 477198.8675055 * T + 0.0087414 * T * T + T * T * T / 69699.0 -
                  T * T * T * T / 14712000.0);
    //Moon's argument of latitude (angle from ascending node)
    F = degToRad(93.2720950 + 483202.0175233 * T - 0.0036539 * T * T - T * T * T / 3526000.0 +
                 T * T * T * T / 863310000.0);

    A1 = degToRad(119.75 + 131.849 * T);
    A2 = degToRad(53.09 + 479264.290 * T);
    A3 = degToRad(313.45 + 481266.484 * T);

    //Calculate the series expansions stored in moonLR.txt and moonB.txt.
    //
    sumL = 0.0;
    sumR = 0.0;

    if (!loadData())
        return false;

    for (const auto &mlrd : LRData)
    {
        double E = 1.0;

        if (mlrd.nm) //if M != 0, include changing eccentricity of Earth's orbit
        {
            E = Et;
            if (abs(mlrd.nm) == 2)
                E = E * E; //use E^2
        }
        sumL += E * mlrd.Li * sin(mlrd.nd * D + mlrd.nm * M + mlrd.nm1 * M1 + mlrd.nf * F);
        sumR += E * mlrd.Ri * cos(mlrd.nd * D + mlrd.nm * M + mlrd.nm1 * M1 + mlrd.nf * F);
    }

    sumB = 0.0;
    for (const auto &mbd : BData)
    {
        double E = 1.0;

        if (mbd.nm) //if M != 0, include changing eccentricity of Earth's orbit
        {
            E = Et;
            if (abs(mbd.nm) == 2)
                E = E * E; //use E^2
        }
        sumB += E * mbd.Bi * sin(mbd.nd * D + mbd.nm * M + mbd.nm1 * M1 + mbd.nf * F);
    }

    //Additive terms for sumL and sumB
    sumL += (3958.0 * sin(A1) + 1962.0 * sin(L - F) + 318.0 * sin(A2));
    sumB += (-2235.0 * sin(L) + 382.0 * sin(A3) + 175.0 * sin(A1 - F) + 175.0 * sin(A1 + F) + 127.0 * sin(L - M1) -
             115.0 * sin(L + M1));

    //Geocentric coordinates
    setEcLong(dms(sumL / 1000000.0 + L * 180.0 / dms::PI)); //convert radians to degrees
    setEcLat(dms(sumB / 1000000.0));
    Rearth = (385000.56 + sumR / 1000.0) / AU_KM; //distance from Earth, in AU

    EclipticToEquatorial(num->obliquity());

    //Determine position angle
    findPA(num);

    return true;
}

void KSMoon::findMagnitude(const KSNumbers *)
{
    // This block of code to compute Moon magnitude (and the
    // relevant data put into ksplanetbase.h) was taken from
    // SkyChart v3 Beta
    double phd = phase().Degrees();

    if (std::isnan(phd)) // Avoid nanny phases.
    {
        findPhase(nullptr);
        phd = phase().Degrees();
        if (std::isnan(phd))
            return;
    }
    int p = floor(phd);
    if (p > 180)
        p = p - 360;
    int i = p / 10;
    int k = p % 10;
    int j = (i + 1 > 18) ? 18 : i + 1;
    i     = 18 - abs(i);
    j     = 18 - abs(j);
    if (i >= 0 && j >= 0 && i <= 18 && j <= 18)
    {
        setMag(MagArray[i] + (MagArray[j] - MagArray[i]) * k / 10);
    }
}

void KSMoon::findPhase(const KSSun *Sun)
{
    if (Sun == nullptr)
    {
        if (defaultSun == nullptr)
            defaultSun = KStarsData::Instance()->skyComposite()->solarSystemComposite()->sun();
        Sun = defaultSun;
    }

    Q_ASSERT(Sun != nullptr);

    // This is an approximation justified by the small Earth-Moon distance in relation
    // to the great Earth-Sun distance
    Phase           = (ecLong() - Sun->ecLong()).Degrees(); // Phase is obviously in degrees
    double DegPhase = dms(Phase).reduce().Degrees();
    iPhase          = int(0.1 * DegPhase + 0.5) % 36; // iPhase must be in [0,36) range

    m_image = TextureManager::getImage(QString("moon%1").arg(iPhase, 2, 10, QChar('0')));
}

QString KSMoon::phaseName() const
{
    double f = illum();
    double p = abs(dms(Phase).reduce().Degrees());

    //First, handle the major phases
    if (f > 0.99)
        return i18nc("moon phase, 100 percent illuminated", "Full moon");
    if (f < 0.01)
        return i18nc("moon phase, 0 percent illuminated", "New moon");
    if (fabs(f - 0.50) < 0.06)
    {
        if (p < 180.0)
            return i18nc("moon phase, half-illuminated and growing", "First quarter");
        else
            return i18nc("moon phase, half-illuminated and shrinking", "Third quarter");
    }

    //Next, handle the more general cases
    if (p < 90.0)
        return i18nc("moon phase between new moon and 1st quarter", "Waxing crescent");
    else if (p < 180.0)
        return i18nc("moon phase between 1st quarter and full moon", "Waxing gibbous");
    else if (p < 270.0)
        return i18nc("moon phase between full moon and 3rd quarter", "Waning gibbous");
    else if (p < 360.0)
        return i18nc("moon phase between 3rd quarter and new moon", "Waning crescent");

    else
        return i18n("unknown");
}

void KSMoon::initPopupMenu(KSPopupMenu *pmenu)
{
#ifdef KSTARS_LITE
    Q_UNUSED(pmenu)
#else
    pmenu->createMoonMenu(this);
#endif
}

SkyObject::UID KSMoon::getUID() const
{
    return solarsysUID(UID_SOL_BIGOBJ) | 10;
}
