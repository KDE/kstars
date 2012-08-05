/***************************************************************************
                          obsconditions.h  -  K Desktop Planetarium
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

#ifndef OBS_CONDITIONS_H
#define OBS_CONDITIONS_H

#include "kstarsdata.h"

class ObsConditions
{
public:
    enum Equipment { Telescope = 0, Binoculars, Both, None };
    enum EquipmentType { Reflector = 0, Refractor };
    ObsConditions(int bortle, double aperture, Equipment equip, EquipmentType eqType);
    ~ObsConditions();
    inline void setEquipment(Equipment equip) { m_Equip = equip; }
    inline void setEquipmentType(EquipmentType eqType) { m_EqType = eqType; }
    void setLimMagnitude();
    double getOptimumMAG();
    double getTrueMagLim();
    bool isVisible(GeoLocation *geo, dms *lst, SkyObject *so);

private:
    int m_BortleClass;
    Equipment m_Equip;
    EquipmentType m_EqType;
    double m_Aperture;
    double m_tParam;
    double m_LM;
};

#endif
