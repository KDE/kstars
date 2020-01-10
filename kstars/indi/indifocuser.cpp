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

bool Focuser::stop()
{
    ISwitchVectorProperty *focusStop = baseDevice->getSwitch("FOCUS_ABORT_MOTION");

    if (focusStop == nullptr)
        return false;

    focusStop->sp[0].s = ISS_ON;
    clientManager->sendNewSwitch(focusStop);

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
    INumberVectorProperty *focusProp;

    if(canManualFocusDriveMove())
    {
        focusProp = baseDevice->getNumber("manualfocusdrive");

        FocusDirection dir;
        getFocusDirection(&dir);
        if (dir == FOCUS_INWARD)
            steps = -abs(steps);
        else if (dir == FOCUS_OUTWARD)
            steps = abs(steps);

        //manualfocusdrive needs different steps value ​​at every turn
        if (steps == getLastManualFocusDriveValue())
            steps += 1;

        //Nikon Z6 fails if step is -1, 0, 1
        if (deviation == NIKONZ6)
        {
            if (abs(steps) < 2)
                steps = 2;
        }
    }
    else
    {
        focusProp = baseDevice->getNumber("REL_FOCUS_POSITION");
    }

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

bool Focuser::canManualFocusDriveMove()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("manualfocusdrive");

    if (focusProp == nullptr)
        return false;
    else
        return true;
}

double Focuser::getLastManualFocusDriveValue()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("manualfocusdrive");

    if (focusProp == nullptr)
        return 0;

    return (double)focusProp->np[0].value;
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

bool Focuser::hasBacklash()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_BACKLASH_STEPS");
    return (focusProp != nullptr);
}

bool Focuser::setBacklash(int32_t steps)
{
    ISwitchVectorProperty *focusToggle = baseDevice->getSwitch("FOCUS_BACKLASH_TOGGLE");
    if (!focusToggle)
        return false;

    // Make sure focus compensation is enabled.
    if (steps != 0 && focusToggle->sp[0].s != ISS_ON)
    {
        IUResetSwitch(focusToggle);
        focusToggle->sp[0].s = ISS_ON;
        focusToggle->sp[1].s = ISS_OFF;
        clientManager->sendNewSwitch(focusToggle);
    }

    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_BACKLASH_STEPS");
    if (!focusProp)
        return false;

    focusProp->np[0].value = steps;
    clientManager->sendNewNumber(focusProp);

    // If steps = 0, disable compensation
    if (steps == 0 && focusToggle->sp[0].s == ISS_ON)
    {
        IUResetSwitch(focusToggle);
        focusToggle->sp[0].s = ISS_OFF;
        focusToggle->sp[1].s = ISS_ON;
        clientManager->sendNewSwitch(focusToggle);
    }
    return true;
}

int32_t Focuser::getBacklash()
{
    INumberVectorProperty *focusProp = baseDevice->getNumber("FOCUS_BACKLASH_STEPS");
    if (!focusProp)
        return -1;

    return focusProp->np[0].value;
}

bool Focuser::hasDeviation()
{
    if (!strcmp(getDeviceName(), "Nikon DSLR Z6"))
    {
        deviation = NIKONZ6;
        return true;
    }
    return false;
}

}
