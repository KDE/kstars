/***************************************************************************
                         CatalogData.cpp  -  K Desktop Planetarium
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


#include "catalogdata.h"

CatalogData::CatalogData() {
  catalog_name = "Dafault Catalog";
  prefix = "Default Prefix";
  color = "#FF000000";
  epoch = 2000.0;
  fluxfreq = "Nil";
  fluxunit = "";
  author = "KStars";
  license = "None";
}
