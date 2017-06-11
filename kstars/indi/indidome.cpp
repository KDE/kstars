/*  INDI Dome
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include "indidome.h"
#include "clientmanager.h"

namespace ISD
{
void Dome::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Dome::processNumber(INumberVectorProperty *nvp)
{
    DeviceDecorator::processNumber(nvp);
}

void Dome::processSwitch(ISwitchVectorProperty *svp)
{
    DeviceDecorator::processSwitch(svp);
}

void Dome::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

bool Dome::canPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    return (parkSW != nullptr);
}

bool Dome::isParked()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    if (parkSW == nullptr)
        return false;

    return ((parkSW->s == ISS_ON) && parkSP->s == IPS_OK);
}

bool Dome::Abort()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("TELESCOPE_ABORT_MOTION");

    if (motionSP == nullptr)
        return false;

    ISwitch *abortSW = IUFindSwitch(motionSP, "ABORT");

    if (abortSW == nullptr)
        return false;

    abortSW->s = ISS_ON;
    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Dome::Park()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");

    if (parkSW == nullptr)
        return false;

    IUResetSwitch(parkSP);
    parkSW->s = ISS_ON;
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Dome::UnPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("DOME_PARK");

    if (parkSP == nullptr)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "UNPARK");

    if (parkSW == nullptr)
        return false;

    IUResetSwitch(parkSP);
    parkSW->s = ISS_ON;
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Dome::isMoving()
{
    ISwitchVectorProperty *motionSP = baseDevice->getSwitch("DOME_MOTION");

    if (motionSP && motionSP->s == IPS_BUSY)
        return true;

    return false;
}
}
