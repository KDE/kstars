/***************************************************************************
                ksearthshadow.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Aug 24 2018
    copyright            : (C) 2018 by Valentin Boettcher
    email                : valentin@boettcher.cf
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ksearthshadow.h"
#include "kssun.h"
#include "ksmoon.h"

KSEarthShadow::KSEarthShadow(const KSMoon *moon, const KSSun *sun, const KSPlanet * earth)
    : KSPlanetBase ("Earth Shadow"), m_sun { sun }, m_moon { moon }, m_earth {earth}
{
}

bool KSEarthShadow::shouldUpdate()
{
    return (m_moon->illum() > 0.8);
}

bool KSEarthShadow::isInEclipse()
{
    double dist = angularDistanceTo(m_moon).Degrees() * 60;
    return (dist - m_moon->angSize() / 2) <= m_penumbra_ang;
}

KSEarthShadow::ECLIPSE_TYPE KSEarthShadow::getEclipseType()
{
    double half_a_moon = m_moon->angSize() / 2;
    double dist = angularDistanceTo(m_moon).Degrees() * 60; // arcminutes
    if (dist <= (m_penumbra_ang - half_a_moon)) {
        if(dist <= (m_umbra_ang - half_a_moon))
            return FULL_UMBRA;
        else
            return FULL_PENUMBRA;
    } else if ((dist - half_a_moon) <= m_penumbra_ang) {
       return PARTIAL;
    }

    return NONE;
}

bool KSEarthShadow::findGeocentricPosition(const KSNumbers *, const KSPlanetBase *)
{
    updateCoords();
    return true; //TODO: not nice!
}


void KSEarthShadow::updateCoords(const KSNumbers *, bool, const CachingDms *, const CachingDms *, bool )
{
    updateCoords();
}

//TODO: Abort if Null
void KSEarthShadow::updateCoords()
{
    // flip the sun around to get the 'shadow coordinates'
    dms t_ra(m_sun->ra().Degrees() + 180);
    t_ra.reduceToRange(dms::ZERO_TO_2PI);
    dms t_dec(-1*(m_sun->dec().Degrees()));

    set(t_ra, t_dec);
    Rearth = m_moon->rearth();
}


//NOTE: This can easily be generalized to any three bodies.
void KSEarthShadow::calculateShadowRadius()
{
    double d_sun = m_sun->rearth() * AU_KM;
    double d_moon = m_moon->rearth() * AU_KM;
    double r_sun = m_sun->physicalSize() / 2;
    double r_earth = m_earth->physicalSize() / 2;

    double umbraRad = 1.01 * r_earth + (r_earth-r_sun)/d_sun * d_moon;
    double penumbraRad = 1.01 * r_earth+(r_sun+r_earth)/d_sun*d_moon;

    m_umbra_ang = asin(umbraRad / d_moon) * 60. * 180. / dms::PI;
    m_penumbra_ang = asin(penumbraRad / d_moon) * 60. * 180. / dms::PI;

    return;
}
