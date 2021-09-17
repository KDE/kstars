/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "obsconditions.h"

#include <QDebug>

#include <cmath>

ObsConditions::ObsConditions(int bortle, double aperture, Equipment equip, TelescopeType telType)
    : m_BortleClass(bortle), m_Equip(equip), m_TelType(telType), m_Aperture(aperture)
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
        case Invalid:
            m_tParam = -1; //invalid value
            break;
    }
    setLimMagnitude();

    qDebug() << "Aperture value being used:" << m_Aperture;
}

QMap<int, double> ObsConditions::setLMMap()
{
    QMap<int, double> LMMap;
    LMMap.insert(1, 7.8);
    LMMap.insert(2, 7.3);
    LMMap.insert(3, 6.8);
    LMMap.insert(4, 6.3);
    LMMap.insert(5, 5.8);
    LMMap.insert(6, 5.3);
    LMMap.insert(7, 4.8);
    LMMap.insert(8, 4.3);
    LMMap.insert(9, 3.8);

    return LMMap;
}

const QMap<int, double> ObsConditions::m_LMMap = setLMMap();

void ObsConditions::setLimMagnitude()
{
    m_LM = m_LMMap[m_BortleClass];
}

double ObsConditions::getOptimumMAG()
{
    double power = (2.81 + 2.814 * m_LM - 0.3694 * pow(m_LM, 2)) / 5;
    return 0.1333 * m_Aperture * sqrt(m_tParam) * pow(power, 10);
}

double ObsConditions::getTrueMagLim()
{
    //     qDebug()<< (4.12 + 2.5 * log10( pow(aperture,2)*t ));
    //     return 4.12 + 2.5 * log10( pow(aperture,2)*t ); //Taking optimum magnification into consideration

    ///If there is no equipment available then return limiting magnitude for naked-eye
    if (m_Equip == None || m_Aperture == -1)
        return m_LM;

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
    if (so->type() == SkyObject::SATELLITE)
    {
        return so->alt().Degrees() > 6.0;
    }
    KStarsDateTime ut = geo->LTtoUT(KStarsDateTime(QDateTime::currentDateTime().toLocalTime()));
    SkyPoint sp       = so->recomputeCoords(ut, geo);

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(lst, geo->lat());

    return (sp.alt().Degrees() > 6.0 && so->mag() < getTrueMagLim());
}

void ObsConditions::setObsConditions(int bortle, double aperture, ObsConditions::Equipment equip,
                                     ObsConditions::TelescopeType telType)
{
    m_BortleClass = bortle;
    setLimMagnitude();
    m_Aperture = aperture;
    m_Equip    = equip;
    m_TelType  = telType;

    qDebug() << "Aperture value being used:" << m_Aperture;
}
