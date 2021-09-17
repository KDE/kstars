/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "noprecessindex.h"
#include "Options.h"
#include "skyobjects/skypoint.h"
#include "kstarsdata.h"
#include "linelist.h"

NoPrecessIndex::NoPrecessIndex(SkyComposite *parent, const QString &name) : LineListIndex(parent, name)
{
}

// Don't precess the points, just account for the Earth's rotation
void NoPrecessIndex::JITupdate(LineList *lineList)
{
    KStarsData *data   = KStarsData::Instance();
    lineList->updateID = data->updateID();
    SkyList *points    = lineList->points();

    for (const auto &point : *points)
    {
        point->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}
