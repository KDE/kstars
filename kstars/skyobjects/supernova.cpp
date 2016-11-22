/*  Supernova Object
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "supernova.h"

#include <typeinfo>
#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif

Supernova::Supernova(const QString &sName, dms ra, dms dec, const QString& type, const QString& hostGalaxy,
                   const QString& date, float sRedShift, float sMag)
    : SkyObject(SkyObject::SUPERNOVA,ra, dec, sMag, sName),
      type(type),
      hostGalaxy(hostGalaxy),
      date(date),
      redShift(sRedShift)
{

}

Supernova* Supernova::clone() const
{
    Q_ASSERT( typeid( this ) == typeid( static_cast<const Supernova *>( this ) ) ); // Ensure we are not slicing a derived class
    return new Supernova(*this);
}

void Supernova::initPopupMenu(KSPopupMenu* pmenu)
{
#ifdef KSTARS_LITE
    Q_UNUSED(pmenu)
#else
    pmenu->createSupernovaMenu(this);
#endif
}
