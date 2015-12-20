/*  INDI Light Box
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include "indilightbox.h"
#include "clientmanager.h"

namespace ISD
{

bool LightBox::hasLight()
{
    return true;
}

bool LightBox::canPark()
{
    return false;
}

}
