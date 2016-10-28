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

bool Dome::abort()
{
    if (currentDome == NULL)
        return false;

    return currentDome->Abort();
}

bool Dome::isMoving()
{
    if (currentDome == NULL)
        return false;

    return currentDome->isMoving();
}

Dome::ParkingStatus Dome::getParkingStatus()
{
    if (currentDome == NULL || currentDome->canPark() == false)
        return PARKING_ERROR;

    ISwitchVectorProperty *parkSP = currentDome->getBaseDevice()->getSwitch("DOME_PARK");

    if (parkSP == NULL)
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


