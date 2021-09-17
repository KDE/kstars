/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarsh.simha@kdemail.net>
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "approachsolver.h"
#include <kstars_debug.h>

ApproachSolver::ApproachSolver(QObject *parent) : QObject(parent)
{
    m_geoPlace = KStarsData::Instance()->geo();
    m_Earth = KSPlanet(i18n("Earth"), QString(), QColor("white"), 12756.28 /*diameter in km*/);
}

void ApproachSolver::setGeoLocation(GeoLocation *geo)
{
    if (geo != nullptr)
        m_geoPlace = geo;
    else
        m_geoPlace = KStarsData::Instance()->geo();
}

// FIXME: We need a better algo for finding approaches!
QMap<long double, dms> ApproachSolver::findClosestApproach(long double startJD,
        long double stopJD, std::function<void (long double, dms)> const &callback)
{
    QMap<long double, dms> Separations;
    QPair<long double, dms> extremum;
    dms Dist;
    dms prevDist;

    double step, step0;
    int Sign, prevSign;

    //  qCDebug(KSTARS) << "Entered KSConjunct::findClosestApproach() with startJD = " << (double)startJD;
    //  qCDebug(KSTARS) << "Initial Positional Information: \n";
    //  qCDebug(KSTARS) << m_object1->name() << ": RA = " << m_object1->ra() -> toHMSString() << "; Dec = " << m_object1->dec() -> toDMSString() << "\n";
    //  qCDebug(KSTARS) << m_object2->name() << ": RA = " << m_object2->ra() -> toHMSString() << "; Dec = " << m_object2->dec() -> toDMSString() << "\n";
    prevSign = 0;

    step0 = findInitialStep(startJD, stopJD);
    step = step0;
    //	qCDebug(KSTARS) << "Initial Separation between " << m_object1->name() << " and " << m_object2->name() << " = " << (prevDist.toDMSString());

    long double jd = startJD;
    prevDist       = updateAndFindDistance(jd);
    jd += step;
    while (jd <= stopJD)
    {
        int progress = int(100.0 * (jd - startJD) / (stopJD - startJD));
        emit solverMadeProgress(progress);

        Dist = updateAndFindDistance(jd);
        Sign = sgn(Dist - prevDist);
        //	qCDebug(KSTARS) << "Dist = " << Dist.toDMSString() << "; prevDist = " << prevDist.toDMSString() << "; Difference = " << (Dist.Degrees() - prevDist.Degrees()) << "; Step = " << step;

        //How close are we to a conjunction, and how fast are we approaching one?
        double factor = fabs((Dist.Degrees() - prevDist.Degrees()) / Dist.Degrees());
        if (factor > 10.0) //let's go faster!
        {
            step = step0 * factor / 10.0;
        }
        else //slow down, we're getting close!
        {
            step = step0;
        }

        if (Sign != prevSign && prevSign == -1) //all right, we may have just passed a conjunction
        {
            if (step > step0) //mini-loop to back up and make sure we're close enough
            {
                //            qCDebug(KSTARS) << "Entering slow loop: ";
                jd -= step;
                step = step0;
                Sign = prevSign;
                while (jd <= stopJD)
                {
                    Dist = updateAndFindDistance(jd);
                    Sign = sgn(Dist - prevDist);
                    //	qCDebug(KSTARS) << "Dist=" << Dist.toDMSString() << "; prevDist=" << prevDist.toDMSString() << "; Diff=" << (Dist.Degrees() - prevDist.Degrees()) << "djd=" << (int)(jd - startJD);
                    if (Sign != prevSign)
                        break;

                    prevDist = Dist;
                    prevSign = Sign;
                    jd += step;
                }
            }

            //	qCDebug(KSTARS) << "Sign = " << Sign << " and " << "prevSign = " << prevSign << ": Entering findPrecise()\n";
            if (findPrecise(&extremum, jd, step, Sign))
            {
                if (extremum.second.radians() < getMaxSeparation())
                {
                    Separations.insert(extremum.first, extremum.second);

                    if(callback)
                        callback(extremum.first, extremum.second);
                }
            }
        }

        prevDist = Dist;
        prevSign = Sign;
        jd += step;
    }

    return Separations;
}

bool ApproachSolver::findPrecise(QPair<long double, dms> *out, long double jd,
                                 double step, int prevSign)
{
    dms prevDist;
    int Sign;
    dms Dist;

    if (out == nullptr)
    {
        qCDebug(KSTARS) << "ERROR: Argument out to KSConjunct::findPrecise(...) was nullptr!";
        return false;
    }

    prevDist = updateAndFindDistance(jd);

    step     = -step / 2.0;
    prevSign = -prevSign;

    while (true)
    {
        jd += step;
        Dist = updateAndFindDistance(jd);
        //    qCDebug(KSTARS) << "Dist=" << Dist.toDMSString() << "; prevDist=" << prevDist.toDMSString() << "; Diff=" << (Dist.Degrees() - prevDist.Degrees()) << "step=" << step;

        if (fabs(step) < 1.0 / (24.0 * 60.0))
        {
            out->first  = jd - step / 2.0;
            out->second = updateAndFindDistance(jd - step / 2.0);
            if (out->second.radians() < updateAndFindDistance(jd - 5.0).radians())
                return true;
            else
                return false;
        }
        Sign = sgn(Dist - prevDist);
        if (Sign != prevSign)
        {
            step = -step / 2.0;
            Sign = -Sign;
        }
        prevSign = Sign;
        prevDist = Dist;
    }
}

dms ApproachSolver::findSkyPointDistance(SkyPoint * obj1, SkyPoint * obj2)
{
    dms dist;
    dist.setRadians(obj1->angularDistanceTo(obj2).radians());
    return dist;
}

int ApproachSolver::sgn(dms a)
{
    // Auxiliary function used by the KSConjunct::findClosestApproach(...)
    // method and the KSConjunct::findPrecise(...) method

    return ((a.radians() > 0) ? 1 : ((a.radians() < 0) ? -1 : 0));
}

