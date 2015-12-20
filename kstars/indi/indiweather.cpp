/*  INDI Weather
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <basedevice.h>
#include "indiweather.h"
#include "clientmanager.h"

namespace ISD
{


void Weather::processLight(ILightVectorProperty *lvp)
{
    DeviceDecorator::processLight(lvp);
}

void Weather::processNumber(INumberVectorProperty *nvp)
{

    DeviceDecorator::processNumber(nvp);
}

void Weather::processSwitch(ISwitchVectorProperty *svp)
{
    DeviceDecorator::processSwitch(svp);

}

void Weather::processText(ITextVectorProperty *tvp)
{
    DeviceDecorator::processText(tvp);
}

IPState Weather::getWeatherStatus()
{
    ILightVectorProperty *weatherLP = baseDevice->getLight("WEATHER_STATUS");
    if (weatherLP == NULL)
        return IPS_ALERT;

    return weatherLP->s;
}

uint16_t Weather::getUpdatePeriod()
{
    INumberVectorProperty *updateNP = baseDevice->getNumber("WEATHER_UPDATE");
    if (updateNP == NULL)
        return 0;

    return ( (uint16_t) updateNP->np[0].value);
}

}
