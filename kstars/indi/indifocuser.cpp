/*  INDI Focuser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <libindi/basedevice.h>
#include "indifocuser.h"
#include "clientmanager.h"

namespace ISD
{


void Focuser::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Focuser::processNumber(INumberVectorProperty *nvp)
{

    DeviceDecorator::processNumber(nvp);
}

void Focuser::processSwitch(ISwitchVectorProperty *svp)
{
    DeviceDecorator::processSwitch(svp);

}

void Focuser::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

bool Focuser::focusIn()
{
    ISwitchVectorProperty *focusProp = baseDevice->getSwitch("FOCUS_MOTION");
    if (focusProp == NULL)
        return false;


    ISwitch *inFocus = IUFindSwitch(focusProp, "FOCUS_INWARD");
    if (inFocus == NULL)
        return false;

    if (inFocus->s == ISS_ON)
        return true;

    IUResetSwitch(focusProp);
    inFocus->s = ISS_ON;

    clientManager->sendNewSwitch(focusProp);

    return true;

}

bool Focuser::focusOut()
{
    ISwitchVectorProperty *focusProp = baseDevice->getSwitch("FOCUS_MOTION");
    if (focusProp == NULL)
        return false;


    ISwitch *outFocus = IUFindSwitch(focusProp, "FOCUS_OUTWARD");
    if (outFocus == NULL)
        return false;

    if (outFocus->s == ISS_ON)
        return true;

    IUResetSwitch(focusProp);
    outFocus->s = ISS_ON;

    clientManager->sendNewSwitch(focusProp);

    return true;
}

bool Focuser::moveFocuser(int secs)
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_TIMER");
    if (focusProp == NULL)
        return false;

    focusProp->np[0].value = secs;

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::absMoveFocuser(int steps)
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_POSITION");
    if (focusProp == NULL)
        return false;

    focusProp->np[0].value = steps;

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::canAbsMove()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_POSITION");
    if (focusProp == NULL)
        return false;
    else
        return true;
}

}
