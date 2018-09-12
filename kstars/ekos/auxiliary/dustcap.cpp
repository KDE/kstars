/*  Ekos DustCap Interface
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "dustcap.h"

#include "dustcapadaptor.h"
#include "ekos/manager.h"
#include "kstars.h"

#include <basedevice.h>

namespace Ekos
{
DustCap::DustCap()
{
    new DustCapAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/DustCap", this);
}

void DustCap::setDustCap(ISD::GDInterface *newDustCap)
{
    if (newDustCap == currentDustCap)
        return;

    currentDustCap = static_cast<ISD::DustCap *>(newDustCap);

    currentDustCap->disconnect(this);

    connect(currentDustCap, &ISD::GDInterface::switchUpdated, this, &DustCap::processSwitch);
}

void DustCap::processSwitch(ISwitchVectorProperty *svp)
{
    if (strcmp(svp->name, "CAP_PARK"))
        return;

    ISD::ParkStatus newStatus;

    switch (svp->s)
    {
    case IPS_IDLE:
        if (svp->sp[0].s == ISS_ON)
            newStatus = ISD::PARK_PARKED;
        else if (svp->sp[1].s == ISS_ON)
            newStatus = ISD::PARK_UNPARKED;
        else
            newStatus = ISD::PARK_UNKNOWN;
        break;

    case IPS_OK:
        if (svp->sp[0].s == ISS_ON)
            newStatus = ISD::PARK_PARKED;
        else
            newStatus = ISD::PARK_UNPARKED;
        break;

    case IPS_BUSY:
        if (svp->sp[0].s == ISS_ON)
            newStatus = ISD::PARK_PARKING;
        else
            newStatus = ISD::PARK_UNPARKING;
        break;

    case IPS_ALERT:
        newStatus = ISD::PARK_ERROR;
    }

    if (newStatus != m_ParkStatus)
    {
        m_ParkStatus = newStatus;
        emit newParkStatus(newStatus);
    }
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
