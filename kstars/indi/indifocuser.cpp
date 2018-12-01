/*  INDI Focuser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include "indifocuser.h"
#include "clientmanager.h"

namespace ISD
{

void Focuser::registerProperty(INDI::Property *prop)
{
    if (!strcmp(prop->getName(), "FOCUS_MAX"))
    {
        INumberVectorProperty *nvp = prop->getNumber();
        m_maxPosition = nvp->np[0].value;
    }

    DeviceDecorator::registerProperty(prop);
}

void Focuser::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Focuser::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "FOCUS_MAX"))
    {
        m_maxPosition = nvp->np[0].value;
    }

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

    if (focusProp == nullptr)
        return false;

    ISwitch *inFocus = IUFindSwitch(focusProp, "FOCUS_INWARD");

    if (inFocus == nullptr)
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

    if (focusProp == nullptr)
        return false;

    ISwitch *outFocus = IUFindSwitch(focusProp, "FOCUS_OUTWARD");

    if (outFocus == nullptr)
        return false;

    if (outFocus->s == ISS_ON)
        return true;

    IUResetSwitch(focusProp);
    outFocus->s = ISS_ON;

    clientManager->sendNewSwitch(focusProp);

    return true;
}

bool Focuser::getFocusDirection(ISD::Focuser::FocusDirection *dir)
{
    ISwitchVectorProperty *focusProp = baseDevice->getSwitch("FOCUS_MOTION");

    if (focusProp == nullptr)
        return false;

    ISwitch *inFocus = IUFindSwitch(focusProp, "FOCUS_INWARD");

    if (inFocus == nullptr)
        return false;

    if (inFocus->s == ISS_ON)
        *dir = FOCUS_INWARD;
    else
        *dir = FOCUS_OUTWARD;

    return true;
}

bool Focuser::moveByTimer(int msecs)
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_TIMER");

    if (focusProp == nullptr)
        return false;

    focusProp->np[0].value = msecs;

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::moveAbs(int steps)
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("ABS_FOCUS_POSITION");

    if (focusProp == nullptr)
        return false;

    focusProp->np[0].value = steps;

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::canAbsMove()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("ABS_FOCUS_POSITION");

    if (focusProp == nullptr)
        return false;
    else
        return true;
}

bool Focuser::moveRel(int steps)
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("REL_FOCUS_POSITION");

    if (focusProp == nullptr)
        return false;

    focusProp->np[0].value = steps;

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::canRelMove()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("REL_FOCUS_POSITION");

    if (focusProp == nullptr)
        return false;
    else
        return true;
}

bool Focuser::canTimerMove()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_TIMER");

    if (focusProp == nullptr)
        return false;
    else
        return true;
}

bool Focuser::setmaxPosition(uint32_t steps)
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_MAX");

    if (focusProp == nullptr)
        return false;

    focusProp->np[0].value = steps;
    clientManager->sendNewNumber(focusProp);

    return true;
}

}
