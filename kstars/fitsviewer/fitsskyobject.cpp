/***************************************************************************
                          fitsskyobject.cpp  -  FITS Image
                             -------------------
    begin                : Tue Apr 07 2020
    copyright            : (C) 2004 by Jasem Mutlaq, (C) 2020 by Eric Dejouhanet
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

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
