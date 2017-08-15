/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "dome.h"
#include "ekos/ekosmanager.h"
#include "kstars.h"
#include "domeadaptor.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"

#include <basedevice.h>

namespace Ekos
{
Dome::Dome()
{
    new DomeAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Dome", this);

    currentDome = nullptr;
}

Dome::~Dome()
{
}

void Dome::setDome(ISD::GDInterface *newDome)
{
    currentDome = static_cast<ISD::Dome *>(newDome);
}

void Dome::setTelescope(ISD::GDInterface *newTelescope)
{
    if (currentDome == nullptr)
        return;

    ITextVectorProperty *activeDevices = currentDome->getBaseDevice()->getText("ACTIVE_DEVICES");
    if (activeDevices)
    {
        IText *activeTelescope = IUFindText(activeDevices, "ACTIVE_TELESCOPE");
        if (activeTelescope)
        {
            IUSaveText(activeTelescope, newTelescope->getDeviceName());
            currentDome->getDriverInfo()->getClientManager()->sendNewText(activeDevices);
        }
    }
}

bool Dome::canPark()
{
    if (currentDome == nullptr)
        return false;

    return currentDome->canPark();
}

bool Dome::park()
{
    if (currentDome == nullptr || currentDome->canPark() == false)
        return false;

    return currentDome->Park();
}

bool Dome::unpark()
{
    if (currentDome == nullptr || currentDome->canPark() == false)
        return false;

    return currentDome->UnPark();
}

bool Dome::abort()
{
    if (currentDome == nullptr)
        return false;

    return currentDome->Abort();
}

bool Dome::isMoving()
{
    if (currentDome == nullptr)
        return false;

    return currentDome->isMoving();
}

Dome::ParkingStatus Dome::getParkingStatus()
{
    if (currentDome == nullptr || currentDome->canPark() == false)
        return PARKING_ERROR;

    ISwitchVectorProperty *parkSP = currentDome->getBaseDevice()->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return PARKING_ERROR;

    switch (parkSP->s)
    {
        case IPS_IDLE:
            return PARKING_IDLE;

        case IPS_OK:
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_OK;
            else
                return UNPARKING_OK;
            break;

        case IPS_BUSY:
            if (parkSP->sp[0].s == ISS_ON)
                return PARKING_BUSY;
            else
                return UNPARKING_BUSY;

        case IPS_ALERT:
            return PARKING_ERROR;
    }

    return PARKING_ERROR;
}
}
