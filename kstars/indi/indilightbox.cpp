/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include "indilightbox.h"

namespace ISD
{

bool LightBox::isLightOn()
{
    auto lightSP = getSwitch("FLAT_LIGHT_CONTROL");
    if (!lightSP)
        return false;

    auto lightON = lightSP->findWidgetByName("FLAT_LIGHT_ON");
    if (!lightON)
        return false;

    return lightON->getState() == ISS_ON;
}

bool LightBox::setLightEnabled(bool enable)
{
    auto lightSP = getSwitch("FLAT_LIGHT_CONTROL");

    if (!lightSP)
        return false;

    auto lightON  = lightSP->findWidgetByName("FLAT_LIGHT_ON");
    auto lightOFF = lightSP->findWidgetByName("FLAT_LIGHT_OFF");

    if (!lightON || !lightOFF)
        return false;

    lightSP->reset();

    if (enable)
        lightON->setState(ISS_ON);
    else
        lightOFF->setState(ISS_ON);

    sendNewProperty(lightSP);

    return true;
}

bool LightBox::setBrightness(uint16_t val)
{
    auto lightIntensity = getNumber("FLAT_LIGHT_INTENSITY");
    if (!lightIntensity)
        return false;

    lightIntensity->at(0)->setValue(val);
    sendNewProperty(lightIntensity);
    return true;
}


}
