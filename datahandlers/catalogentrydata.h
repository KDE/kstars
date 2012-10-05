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


#ifndef CATALOGENTRYDATA_H
#define CATALOGENTRYDATA_H

class CatalogEntryData {
 public:
  QString catalog_name;
  int ID;
  QString long_name;
  QString ra;
  QString dec;
  int type;
  float magnitude;
  int position_angle;
  float major_axis;
  float minor_axis;
  float flux;
};

#endif // CATALOGENTRYDATA_H
