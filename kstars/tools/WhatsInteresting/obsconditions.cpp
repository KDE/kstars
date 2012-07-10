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

ObsConditions::ObsConditions(int bortle, Equipment eq, double ap, EquipmentType tp)
{
    bortleClass = bortle;
    equip = eq;
    aperture = ap;

    // 't' parameter
    switch ( type = tp )
    {
        case Reflector:
            t = 0.7;
            break;
        case Refractor:
            t = 0.9;
            break;
        default:
            break;
    }
}

ObsConditions::~ObsConditions() {}

void ObsConditions::setEquipment(Equipment eq)
{
    equip = eq;
}

void ObsConditions::setEquipmentType(EquipmentType t)
{
    type = t;
}

void ObsConditions::setLimMagnitude(int bortleClass)
{
    switch ( bortleClass )
    {
        case 1:
            LM = 7.8;       //Excellent dark-sky site
            break;
        case 2:
            LM = 7.3;       //Typical truly dark site
            break;
        case 3:
            LM = 6.8;       //Rural sky
            break;
        case 4:
            LM = 6.3;
            break;
        case 5:
            LM = 5.8;
            break;
        case 6:
            LM = 5.3;
            break;
        case 7:
            LM = 4.8;
            break;
        case 8:
            LM = 4.3;
            break;
        case 9:
            LM = 3.8;
            break;
        default:
            LM = 4.0;
            break;
    }
}


double ObsConditions::getOptimumMAG()
{
    double power = ( 2.81 + 2.814 * LM - 0.3694 * pow( LM , 2 ) ) / 5;
    return 0.1333 * aperture * sqrt(t) * pow( power , 10 );
}


double ObsConditions::getTrueMagLim()
{
    kDebug()<< (4.12 + 2.5 * log10( pow(aperture,2)*t ));
    return 4.12 + 2.5 * log10( pow(aperture,2)*t ); //Taking optimum magnification into consideration
}
