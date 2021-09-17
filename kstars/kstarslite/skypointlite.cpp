/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "skypointlite.h"
#include "skyobject.h"

SkyPointLite::SkyPointLite() : point(nullptr)
{
}

void SkyPointLite::setPoint(SkyPoint *p)
{
    point = p;
}
