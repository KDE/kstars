/*  Ekos DustCap Interface
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "dustcap.h"
#include "ekos/ekosmanager.h"
#include "kstars.h"
#include "dustcapadaptor.h"

#include <basedevice.h>


namespace Ekos
{

DustCap::DustCap()
{
    new DustCapAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/DustCap",  this);

    currentDustCap = nullptr;
}

DustCap::~DustCap()
{

}

void DustCap::setDustCap(ISD::GDInterface * newDustCap)
{
    currentDustCap = static_cast<ISD::DustCap *>(newDustCap);
}

DustCap::ParkingStatus DustCap::getParkingStatus()
{
    if (currentDustCap == nullptr)
        return PARKING_ERROR;

    ISwitchVectorProperty * parkSP = currentDustCap->getBaseDevice()->getSwitch("CAP_PARK");

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

bool DustCap::park()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->Park();
}

bool DustCap::unpark()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->UnPark();
}

bool DustCap::canPark()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->canPark();
}

bool DustCap::hasLight()
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->hasLight();
}

bool DustCap::setLightEnabled(bool enable)
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->SetLightEnabled(enable);
}

bool DustCap::setBrightness(uint16_t val)
{
    if (currentDustCap == nullptr)
        return false;

    return currentDustCap->SetBrightness(val);
}

}


