/***************************************************************************
                  CatalogEntryData.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/06/10
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include <QString>

/**
 * @brief Class to store details of a Catalog Entry
 *
 * catalog_name = Name of the Catalog (must exist prior to execution)
 * ID = The ID number from the catalog. eg. for M 31, ID is 31
 * long_name = long name (if any) of the object
 * ra = Right Ascension of the object (in HH:MM:SS format)
 * dec = Declination of the object (in +/-DD:MM:SS format)
 * type = type of the object (from skyqpainter::drawDeepSkySymbol())
 * 0: general star (not to be used for custom catalogs)
 * 1: Catalog star
 * 2: planet
 * 3: Open Cluster
 * 4: Globular Cluster
 * 5: Gaseous Nebula
 * 6: Planetary Nebula
 * 7: Supernova remnant
 * 8: Galaxy
 * 13: Asterism
 * 14: Galaxy cluster
 * 15: Dark Nebula
 * 16: Quasar
 * 17: Multiple Star
 * 18: Radio Source
 * 19: Satellite
 * 20: Supernova
 * 255: TYPE_UNKNOWN
 *
 * magnitude = Apparent Magnitude of the object
 * position_angle = Position Angle of the object
 * major_axis = Major Axis Length (arcmin)
 * minor_axis = Minor Axis Length (arcmin)
 * flux = Flux for the object
 **/

class CatalogEntryData
{
  public:
    CatalogEntryData();

    QString catalog_name;
    int ID;
    QString long_name;
    double ra;
    double dec;
    int type;
    float magnitude;
    int position_angle;
    float major_axis;
    float minor_axis;
    float flux;
};
