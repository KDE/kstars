/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#include "fitsskyobject.h"

FITSSkyObject::FITSSkyObject(SkyObject /*const*/ * object, int xPos, int yPos) : QObject()
{
    skyObjectStored = object;
    xLoc            = xPos;
    yLoc            = yPos;
}

SkyObject /*const*/ * FITSSkyObject::skyObject()
{
    return skyObjectStored;
}

int FITSSkyObject::x() const
{
    return xLoc;
}

int FITSSkyObject::y() const
{
    return yLoc;
}

void FITSSkyObject::setX(int xPos)
{
    xLoc = xPos;
}

void FITSSkyObject::setY(int yPos)
{
    yLoc = yPos;
}
