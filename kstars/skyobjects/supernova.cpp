/***************************************************************************
                          supernova.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sunday, 19th June, 2011
    copyright            : (C) 2011 by Samikshan Bairagya
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

#include "supernova.h"
#include "kspopupmenu.h"

Supernova::Supernova(dms ra, dms dec, const QString& date ,float m, const QString& serialNo,
                     const QString& type, const QString& hostGalaxy, const QString& offset,
                     const QString& discoverer)
                    : SkyObject(SkyObject::SUPERNOVA,ra, dec, m, serialNo),
                      serialNumber(serialNo),
                      type(type),
                      hostGalaxy(hostGalaxy),
                      offset(offset),
                      discoverers(discoverer),
                      date(date),
                      RA(ra),
                      Dec(dec),
                      Magnitude(m)
{}


void Supernova::initPopupMenu(KSPopupMenu* pmenu)
{
    pmenu->createSupernovaMenu(this);
}
