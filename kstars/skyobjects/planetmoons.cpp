/*
    SPDX-FileCopyrightText: Vipul Kumar Singh <vipulkrsingh@gmail.com>
    SPDX-FileCopyrightText: Médéric Boquien <mboquien@free.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "planetmoons.h"

#include "ksnumbers.h"
#include "ksplanetbase.h"
#include "kssun.h"
#include "trailobject.h"

#include <QDebug>

PlanetMoons::~PlanetMoons()
{
    qDeleteAll(Moon);
}

QString PlanetMoons::name(int id) const
{
    return Moon[id]->translatedName();
}

void PlanetMoons::EquatorialToHorizontal(const dms *LST, const dms *lat)
{
    int nmoons = nMoons();

    for (int i = 0; i < nmoons; ++i)
        moon(i)->EquatorialToHorizontal(LST, lat);
}
