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

    ConcreteDevice::registerProperty(prop);
}

void Focuser::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();
    if (prop.isNameMatch("FOCUS_MAX"))
    {
        m_maxPosition = nvp->at(0)->getValue();
    }
}

bool Focuser::focusIn()
{
    auto focusProp = getSwitch("FOCUS_MOTION");

    if (!focusProp)
        return false;

    auto inFocus = focusProp->findWidgetByName("FOCUS_INWARD");

    if (!inFocus)
        return false;

    if (inFocus->getState() == ISS_ON)
        return true;

    focusProp->reset();
    inFocus->setState(ISS_ON);

    sendNewProperty(focusProp);

    return true;
}

bool Focuser::stop()
{
    auto focusStop = getSwitch("FOCUS_ABORT_MOTION");

    if (!focusStop)
        return false;

    focusStop->at(0)->setState(ISS_ON);
    sendNewProperty(focusStop);

    return true;
}

bool Focuser::focusOut()
{
    auto focusProp = getSwitch("FOCUS_MOTION");

    if (!focusProp)
        return false;

    auto outFocus = focusProp->findWidgetByName("FOCUS_OUTWARD");

    if (!outFocus)
        return false;

    if (outFocus->getState() == ISS_ON)
        return true;

    focusProp->reset();
    outFocus->setState(ISS_ON);

    sendNewProperty(focusProp);

    return true;
}

bool Focuser::getFocusDirection(ISD::Focuser::FocusDirection *dir)
{
    auto focusProp = getSwitch("FOCUS_MOTION");

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
    auto focusProp = getNumber("FOCUS_TIMER");

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(msecs);

    sendNewProperty(focusProp);

    return true;
}

bool Focuser::moveAbs(int steps)
{
    auto focusProp = getNumber("ABS_FOCUS_POSITION");

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);

    sendNewProperty(focusProp);

    return true;
}

bool Focuser::canAbsMove()
{
    auto focusProp = getNumber("ABS_FOCUS_POSITION");

    if (!focusProp)
        return false;
    else
        return true;
}

bool Focuser::moveRel(int steps)
{
    if(canManualFocusDriveMove())
    {
        auto focusProp = getNumber("manualfocusdrive");

        FocusDirection dir;
        if (!getFocusDirection(&dir))
            return false;
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

        focusProp[0].setValue(steps);
        sendNewProperty(focusProp);
        return true;
    }
    else
    {
        auto focusProp = getNumber("REL_FOCUS_POSITION");
        focusProp[0].setValue(steps);
        sendNewProperty(focusProp);
        return true;
    }

    return false;
}

bool Focuser::canRelMove()
{
    auto focusProp = getNumber("REL_FOCUS_POSITION");

    if (!focusProp)
        return false;
    else
        return true;
}

bool Focuser::canManualFocusDriveMove()
{
    auto focusProp = getNumber("manualfocusdrive");

    if (!focusProp)
        return false;
    else
        return true;
}

double Focuser::getLastManualFocusDriveValue()
{
    auto focusProp = getNumber("manualfocusdrive");

    if (!focusProp)
        return 0;

    return focusProp->at(0)->getValue();
}


bool Focuser::canTimerMove()
{
    auto focusProp = getNumber("FOCUS_TIMER");

    if (!focusProp)
        return false;
    else
        return true;
}

bool Focuser::setMaxPosition(uint32_t steps)
{
    auto focusProp = getNumber("FOCUS_MAX");

    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);
    sendNewProperty(focusProp);

    return true;
}

bool Focuser::hasBacklash()
{
    auto focusProp = getNumber("FOCUS_BACKLASH_STEPS");
    return (focusProp != nullptr);
}

bool Focuser::setBacklash(int32_t steps)
{
    auto focusToggle = getSwitch("FOCUS_BACKLASH_TOGGLE");
    if (!focusToggle)
        return false;

    // Make sure focus compensation is enabled.
    if (steps != 0 && focusToggle->at(0)->getState() != ISS_ON)
    {
        focusToggle->reset();
        focusToggle->at(0)->setState(ISS_ON);
        focusToggle->at(1)->setState(ISS_OFF);
        sendNewProperty(focusToggle);
    }

    auto focusProp = getNumber("FOCUS_BACKLASH_STEPS");
    if (!focusProp)
        return false;

    focusProp->at(0)->setValue(steps);
    sendNewProperty(focusProp);

    // If steps = 0, disable compensation
    if (steps == 0 && focusToggle->at(0)->getState() == ISS_ON)
    {
        focusToggle->reset();
        focusToggle->at(0)->setState(ISS_OFF);
        focusToggle->at(1)->setState(ISS_ON);
        sendNewProperty(focusToggle);
    }
    return true;
}

int32_t Focuser::getBacklash()
{
    auto focusProp = getNumber("FOCUS_BACKLASH_STEPS");
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
