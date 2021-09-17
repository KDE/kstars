/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <kstar@bas.org.in>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksconjunct.h"

#include "ksnumbers.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/ksplanetbase.h"

#include <cmath>

KSConjunct::KSConjunct() : ApproachSolver ()
{
    connect(this, &ApproachSolver::solverMadeProgress, this, &KSConjunct::madeProgress);
}

dms KSConjunct::findDistance()
{
    dms dist = findSkyPointDistance(m_object1.get(), m_object2.get());
    if (m_opposition)
    {
        dist.setD(180 - dist.Degrees());
    }

    return dist;
}

void KSConjunct::updatePositions(long double jd)
{
    KStarsDateTime t(jd);
    KSNumbers num(jd);

    m_Earth.findPosition(&num);
    CachingDms LST(getGeoLocation()->GSTtoLST(t.gst()));

    KSPlanetBase *p = dynamic_cast<KSPlanetBase*>(m_object1.get());
    if (p)
        p->findPosition(&num, getGeoLocation()->lat(), &LST, &m_Earth);
    else
        m_object1->updateCoordsNow(&num);

    m_object2->findPosition(&num, getGeoLocation()->lat(), &LST, &m_Earth);
}

double KSConjunct::findInitialStep(long double startJD, long double stopJD)
{

    double step0 =
        double(stopJD - startJD) / 4.0; // I'm an idiot for having done this without having the lines that follow -- asimha

    // TODO: Work out a solid footing on which one can decide step0. -- asimha
    if (step0 > 24.8 * 365.25) // Sample pluto's orbit (248.09 years) at least 10 times.
        step0 = 24.8 * 365.25;

    // FIXME: This can be done better, but for now, I'm doing it the dumb way -- asimha
    if (m_object1->name() == i18n("Neptune") || m_object2->name() == i18n("Neptune") || m_object1->name() == i18n("Uranus") ||
            m_object2->name() == i18n("Uranus"))
        if (step0 > 3652.5)
            step0 = 3652.5;
    if (m_object1->name() == i18n("Jupiter") || m_object2->name() == i18n("Jupiter") || m_object1->name() == i18n("Saturn") ||
            m_object2->name() == i18n("Saturn"))
        if (step0 > 365.25)
            step0 = 365;
    if (m_object1->name() == i18n("Mars") || m_object2->name() == i18n("Mars"))
        if (step0 > 10.0)
            step0 = 10.0;
    if (m_object1->name() == i18n("Venus") || m_object1->name() == i18n("Mercury") || m_object2->name() == i18n("Mercury") ||
            m_object2->name() == i18n("Venus"))
        if (step0 > 5.0)
            step0 = 5.0;
    if (m_object1->name() == i18n("Moon") || m_object2->name() == i18n("Moon"))
        if (step0 > 0.25)
            step0 = 0.25;

    return step0;
}
