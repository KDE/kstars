/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
