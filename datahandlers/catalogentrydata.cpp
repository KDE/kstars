/***************************************************************************
                  CatalogEntryData.h  -  K Desktop Planetarium
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


#include "catalogentrydata.h"
#include "../kstars/nan.h"

CatalogEntryData::CatalogEntryData() {
  catalog_name = "";
  ID = -1;
  long_name = "Nil";
  ra = NaN::d;
  dec = NaN::d;
  type = -1;
  magnitude = NaN::f;
  position_angle = 0;
  major_axis = NaN::f;
  minor_axis = NaN::f;
  flux = NaN::f;
};
