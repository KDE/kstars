/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pointlistcomponent.h"

#include "kstarsdata.h"
#include "skyobjects/skypoint.h"

PointListComponent::PointListComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

void PointListComponent::update(KSNumbers *num)
{
    if (!selected())
        return;

    KStarsData *data = KStarsData::Instance();

    for (auto &p : pointList())
    {
        if (num)
            p->updateCoords(num);

        p->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}
