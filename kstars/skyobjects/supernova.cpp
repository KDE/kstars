/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "supernova.h"

#include <typeinfo>
#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif

Supernova::Supernova(const QString &sName, dms ra, dms dec, const QString &type, const QString &hostGalaxy,
                     const QString &date, float sRedShift, float sMag)
    : SkyObject(SkyObject::SUPERNOVA, ra, dec, sMag, sName), type(type), hostGalaxy(hostGalaxy), date(date),
      redShift(sRedShift)
{
}

Supernova *Supernova::clone() const
{
    Q_ASSERT(typeid(this) == typeid(static_cast<const Supernova *>(this))); // Ensure we are not slicing a derived class
    return new Supernova(*this);
}

void Supernova::initPopupMenu(KSPopupMenu *pmenu)
{
#ifdef KSTARS_LITE
    Q_UNUSED(pmenu)
#else
    pmenu->createSupernovaMenu(this);
#endif
}
