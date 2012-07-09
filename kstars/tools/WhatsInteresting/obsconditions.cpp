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

ObsConditions::ObsConditions(int bortle, Equipment eq, EquipmentType t)
{
    bortleClass = bortle;
    equip = eq;
    type = t;
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

float ObsConditions::getTrueMagLim()
{
    return 4.0; //dummy
}
