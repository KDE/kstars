/***************************************************************************
                         CatalogData.h  -  K Desktop Planetarium
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


#ifndef CATALOGDATA_H
#define CATALOGDATA_H

#include <QString>

/**
 * @brief Add the catalog with given details into the database
 *
 * catalog_name = Name to be given to the catalog
 * prefix = Prefix of the catalog
 * color = Color of the drawn objects
 * epoch = Epoch of the catalog
 * fluxfreq = Flux Frequency of the catalog
 * fluxunit = Flux Unit of the catalog
 * author = Author of the catalog. Defaults to "KStars Community".
 * license = License for the catalog. Defaults to "None".
 **/


class CatalogData {
 public:
  CatalogData();
  QString catalog_name;
  QString prefix;
  QString color;
  float epoch;
  QString fluxfreq;
  QString fluxunit;
  QString author;
  QString license;
};

#endif // CATALOGDATA_H
