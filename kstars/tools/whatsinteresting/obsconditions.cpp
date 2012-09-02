/***************************************************************************
                          obsconditions.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/10/07
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "obsconditions.h"
#include "math.h"
#include "kdebug.h"

ObsConditions::ObsConditions(int bortle, double aperture, Equipment equip, TelescopeType telType):
    m_BortleClass(bortle), m_Aperture(aperture), m_Equip(equip), m_TelType(telType)
{
    // 't' parameter
    switch (m_TelType)
    {
        case Reflector:
            m_tParam = 0.7;
            break;
        case Refractor:
            m_tParam = 0.9;
            break;
    }
    setLimMagnitude();
}

ObsConditions::~ObsConditions() {}

void ObsConditions::setLimMagnitude()
{
    switch (m_BortleClass)
    {
        case 1:
            m_LM = 7.8;       //Excellent dark-sky site
            break;
        case 2:
            m_LM = 7.3;       //Typical truly dark site
            break;
        case 3:
            m_LM = 6.8;       //Rural sky
            break;
        case 4:
            m_LM = 6.3;
            break;
        case 5:
            m_LM = 5.8;
            break;
        case 6:
            m_LM = 5.3;
            break;
        case 7:
            m_LM = 4.8;
            break;
        case 8:
            m_LM = 4.3;
            break;
        case 9:
            m_LM = 3.8;
            break;
        default:
            m_LM = 4.0;
            break;
    }
}

double ObsConditions::getOptimumMAG()
{
    double power = (2.81 + 2.814 * m_LM - 0.3694 * pow(m_LM, 2)) / 5;
    return 0.1333 * m_Aperture * sqrt(m_tParam ) * pow(power, 10);
}

double ObsConditions::getTrueMagLim()
{
//     kDebug()<< (4.12 + 2.5 * log10( pow(aperture,2)*t ));
//     return 4.12 + 2.5 * log10( pow(aperture,2)*t ); //Taking optimum magnification into consideration

    /**
     * This is a more traditional formula which does not take the
     * 't' parameter into account. It also does not take into account
     * the magnification being used. The formula used is:
     *
     * TLM_trad = LM + 5*log10(aperture/7.5)
     *
     * The calculation is just based on the calculation of the
     * telescope's aperture to eye's pupil surface ratio.
     */

    return m_LM + 5 * log10(m_Aperture / 7.5);
}

bool ObsConditions::isVisible(GeoLocation *geo, dms *lst, SkyObject *so)
{
    KStarsDateTime ut = geo->LTtoUT(KStarsDateTime(KDateTime::currentLocalDateTime()));
    SkyPoint sp = so->recomputeCoords(ut, geo);

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(lst, geo->lat());

    return (sp.alt().Degrees() > 6.0 && so->mag() < getTrueMagLim());
}
