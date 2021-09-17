/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include "indifocuser.h"
#include "clientmanager.h"

namespace ISD
{

void Focuser::registerProperty(INDI::Property prop)
{
    if (!prop.getRegistered())
        return;

    if (prop.isNameMatch("FOCUS_MAX"))
    {
        auto nvp = prop.getNumber();
        m_maxPosition = nvp->at(0)->getValue();
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
    auto focusProp = baseDevice->getSwitch("FOCUS_MOTION");

    if (!focusProp)
        return false;

    auto inFocus = focusProp->findWidgetByName("FOCUS_INWARD");

    if (!inFocus)
        return false;

    if (inFocus->getState() == ISS_ON)
        return true;

    focusProp->reset();
    inFocus->setState(ISS_ON);

    clientManager->sendNewSwitch(focusProp);

    return true;
}

bool Focuser::stop()
{
    auto focusStop = baseDevice->getSwitch("FOCUS_ABORT_MOTION");

    if (!focusStop)
        return false;

    focusStop->at(0)->setState(ISS_ON);
    clientManager->sendNewSwitch(focusStop);

    return true;
}

bool Focuser::focusOut()
{
    auto focusProp = baseDevice->getSwitch("FOCUS_MOTION");

    if (!focusProp)
        return false;

    auto outFocus = focusProp->findWidgetByName("FOCUS_OUTWARD");

    if (!outFocus)
        return false;

    if (outFocus->getState() == ISS_ON)
        return true;

    focusProp->reset();
    outFocus->setState(ISS_ON);

    clientManager->sendNewSwitch(focusProp);

    return true;
}

bool Focuser::getFocusDirection(ISD::Focuser::FocusDirection *dir)
{
    auto focusProp = baseDevice->getSwitch("FOCUS_MOTION");

    if (!focusProp)
        return false;

    auto inFocus = focusProp->findWidgetByName("FOCUS_INWARD");

    if (!inFocus)
        return false;

    if (inFocus->getState() == ISS_ON)
        *dir = FOCUS_INWARD;
    else
        *dir = FOCUS_OUTWARD;

    return true;
}

bool Focuser::moveByTimer(int msecs)
{
    auto focusProp = baseDevice->getNumber("FOCUS_TIMER");

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(msecs);

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::moveAbs(int steps)
{
    auto focusProp = baseDevice->getNumber("ABS_FOCUS_POSITION");

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::canAbsMove()
{
    auto focusProp = baseDevice->getNumber("ABS_FOCUS_POSITION");

    if (!focusProp)
        return false;
    else
        return true;
}

bool Focuser::moveRel(int steps)
{
    INDI::PropertyView<INumber> *focusProp;

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

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);

    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::canRelMove()
{
    auto focusProp = baseDevice->getNumber("REL_FOCUS_POSITION");

    if (!focusProp)
        return false;
    else
        return true;
}

bool Focuser::canManualFocusDriveMove()
{
    auto focusProp = baseDevice->getNumber("manualfocusdrive");

    if (!focusProp)
        return false;
    else
        return true;
}

double Focuser::getLastManualFocusDriveValue()
{
    auto focusProp = baseDevice->getNumber("manualfocusdrive");

    if (!focusProp)
        return 0;

    return (double)focusProp->at(0)->getValue();
}


bool Focuser::canTimerMove()
{
    auto focusProp = baseDevice->getNumber("FOCUS_TIMER");

    if (!focusProp)
        return false;
    else
        return true;
}

bool Focuser::setmaxPosition(uint32_t steps)
{
    auto focusProp = baseDevice->getNumber("FOCUS_MAX");

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);
    clientManager->sendNewNumber(focusProp);

    return true;
}

bool Focuser::hasBacklash()
{
    auto focusProp = baseDevice->getNumber("FOCUS_BACKLASH_STEPS");
    return (focusProp != nullptr);
}

bool Focuser::setBacklash(int32_t steps)
{
    auto focusToggle = baseDevice->getSwitch("FOCUS_BACKLASH_TOGGLE");
    if (!focusToggle)
        return false;

    // Make sure focus compensation is enabled.
    if (steps != 0 && focusToggle->at(0)->getState() != ISS_ON)
    {
        focusToggle->reset();
        focusToggle->at(0)->setState(ISS_ON);
        focusToggle->at(1)->setState(ISS_OFF);
        clientManager->sendNewSwitch(focusToggle);
    }

    auto focusProp = baseDevice->getNumber("FOCUS_BACKLASH_STEPS");
    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);
    clientManager->sendNewNumber(focusProp);

    // If steps = 0, disable compensation
    if (steps == 0 && focusToggle->at(0)->getState() == ISS_ON)
    {
        focusToggle->reset();
        focusToggle->at(0)->setState(ISS_OFF);
        focusToggle->at(1)->setState(ISS_ON);
        clientManager->sendNewSwitch(focusToggle);
    }
    return true;
}

int32_t Focuser::getBacklash()
{
    auto focusProp = baseDevice->getNumber("FOCUS_BACKLASH_STEPS");
    if (!focusProp)
        return -1;

    return focusProp->at(0)->getValue();
}

bool Focuser::hasDeviation()
{
    if (getDeviceName() == "Nikon DSLR Z6")
    {
        deviation = NIKONZ6;
        return true;
    }
    return false;
}

}
