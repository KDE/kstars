/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "dome.h"

#include "domeadaptor.h"
#include "ekos/manager.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"
#include "kstars.h"

#include <basedevice.h>

namespace Ekos
{
Dome::Dome()
{
    new DomeAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Dome", this);

    currentDome = nullptr;
}

void Dome::setDome(ISD::GDInterface *newDome)
{
    if (newDome == currentDome)
        return;

    currentDome = static_cast<ISD::Dome *>(newDome);

    currentDome->disconnect(this);

    connect(currentDome, &ISD::Dome::newStatus, this, &Dome::newStatus);
    connect(currentDome, &ISD::Dome::newParkStatus, this, &Dome::newParkStatus);
    connect(currentDome, &ISD::Dome::newParkStatus, [&](ISD::ParkStatus status) {m_ParkStatus = status;});
    connect(currentDome, &ISD::Dome::newShutterStatus, this, &Dome::newShutterStatus);
    connect(currentDome, &ISD::Dome::newShutterStatus, [&](ISD::Dome::ShutterStatus status) {m_ShutterStatus = status;});
    connect(currentDome, &ISD::Dome::azimuthPositionChanged, this, &Dome::azimuthPositionChanged);
    connect(currentDome, &ISD::Dome::ready, this, &Dome::ready);
    connect(currentDome, &ISD::Dome::Disconnected, this, &Dome::disconnected);
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

bool Dome::canAbsoluteMove()
{
    if (currentDome)
        return currentDome->canAbsMove();

    return false;
}

double Dome::azimuthPosition()
{
    if (currentDome)
        return currentDome->azimuthPosition();
    return -1;
}

void Dome::setAzimuthPosition(double position)
{
    if (currentDome)
        currentDome->setAzimuthPosition(position);
}

bool Dome::hasShutter()
{
    if (currentDome)
        return currentDome->hasShutter();
    // no dome, no shutter
    return false;
}

bool Dome::controlShutter(bool open)
{

    if (currentDome)
        return currentDome->ControlShutter(open);
    // no dome, no shutter control
    return false;
}

#if 0
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
#endif
}
