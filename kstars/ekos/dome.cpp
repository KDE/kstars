/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "dome.h"
#include "ekosmanager.h"
#include "kstars.h"
#include "domeadaptor.h"

#include <basedevice.h>


namespace Ekos
{

Dome::Dome()
{
    new DomeAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Dome",  this);

    currentDome = NULL;
}

Dome::~Dome()
{

}

void Dome::setDome(ISD::GDInterface *newDome)
{
    currentDome = static_cast<ISD::Dome *>(newDome);
}

bool Dome::canPark()
{
    if (currentDome == NULL)
        return false;

    return currentDome->canPark();
}

bool Dome::park()
{
    if (currentDome == NULL || currentDome->canPark() == false)
        return false;

    return currentDome->Park();
}

bool Dome::unpark()
{
    if (currentDome == NULL || currentDome->canPark() == false)
        return false;

    return currentDome->UnPark();
}

bool Dome::isParked()
{
    if (currentDome == NULL || currentDome->canPark() == false)
        return false;

    return currentDome->isParked();
}

bool Dome::abort()
{
    if (currentDome == NULL)
        return false;

    return currentDome->Abort();
}

IPState  Dome::getDomeState()
{
    if (currentDome == NULL)
        return IPS_ALERT;

    return currentDome->getBaseDevice()->getPropertyState("DOME_PARK");
}


}


