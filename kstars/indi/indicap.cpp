/*  INDI DustCap
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include "indicap.h"
#include "clientmanager.h"

namespace ISD
{


void DustCap::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void DustCap::processNumber(INumberVectorProperty *nvp)
{

    DeviceDecorator::processNumber(nvp);
}

void DustCap::processSwitch(ISwitchVectorProperty *svp)
{
    DeviceDecorator::processSwitch(svp);

}

void DustCap::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}


bool DustCap::canPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");
    if (parkSP == NULL)
        return false;
    else
        return true;
}

bool DustCap::isParked()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");
    if (parkSP == NULL)
        return false;

    return (parkSP->s == IPS_OK && parkSP->sp[0].s == ISS_ON);
}

bool DustCap::Park()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");
    if (parkSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "PARK");
    if (parkSW == NULL)
        return false;

     IUResetSwitch(parkSP);
     parkSW->s = ISS_ON;
     clientManager->sendNewSwitch(parkSP);

     return true;
}

bool DustCap::UnPark()
{
    ISwitchVectorProperty *parkSP = baseDevice->getSwitch("CAP_PARK");
    if (parkSP == NULL)
        return false;

    ISwitch *parkSW = IUFindSwitch(parkSP, "UNPARK");
    if (parkSW == NULL)
        return false;

     IUResetSwitch(parkSP);
     parkSW->s = ISS_ON;
     clientManager->sendNewSwitch(parkSP);

     return true;
}

bool DustCap::hasLight()
{
    ISwitchVectorProperty *lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");
    if (lightSP == NULL)
        return false;
    else
        return true;
}

bool DustCap::isLightOn()
{
    ISwitchVectorProperty *lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");
    if (lightSP == NULL)
        return false;

    ISwitch *lightON  = IUFindSwitch(lightSP, "FLAT_LIGHT_ON");
    if (lightON == NULL)
        return false;

    return (lightON->s == ISS_ON);
}

bool DustCap::SetLightEnabled(bool enable)
{
    ISwitchVectorProperty *lightSP = baseDevice->getSwitch("FLAT_LIGHT_CONTROL");
    if (lightSP == NULL)
        return false;

    ISwitch *lightON  = IUFindSwitch(lightSP, "FLAT_LIGHT_ON");
    ISwitch *lightOFF = IUFindSwitch(lightSP, "FLAT_LIGHT_OFF");
    if (lightON == NULL || lightOFF == NULL)
        return false;

    IUResetSwitch(lightSP);

   if (enable)
       lightON->s  = ISS_ON;
   else
       lightOFF->s  = ISS_ON;

    clientManager->sendNewSwitch(lightSP);

    return true;
}

bool DustCap::SetBrightness(uint16_t val)
{
    INumberVectorProperty *lightIntensity = baseDevice->getNumber("FLAT_LIGHT_INTENSITY");
    if (lightIntensity)
    {
        lightIntensity->np[0].value = val;
        clientManager->sendNewNumber(lightIntensity);
        return true;
    }

    return false;
}

}
