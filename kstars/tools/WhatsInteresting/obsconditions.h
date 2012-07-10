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

class ObsConditions
{
public:
    enum Equipment {Telescope = 0, Binoculars, Both, None };
    enum EquipmentType { Reflector = 0, Refractor };
    ObsConditions(int bortleClass, Equipment eq, double aperture, EquipmentType tp);
    ~ObsConditions();
    void setEquipment(Equipment eq);
    void setEquipmentType(EquipmentType t);
    void setLimMagnitude( int bortleClass);
    double getOptimumMAG();
    double getTrueMagLim();

private:
    int bortleClass;
    Equipment equip;
    EquipmentType type;
    double aperture;
    double t;
    double LM;
};

#endif