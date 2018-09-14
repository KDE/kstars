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

Weather::Weather(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType = KSTARS_WEATHER;

    readyTimer.reset(new QTimer());
    readyTimer.get()->setInterval(250);
    readyTimer.get()->setSingleShot(true);
    connect(readyTimer.get(), &QTimer::timeout, this, &Weather::ready);
}

void Weather::registerProperty(INDI::Property *prop)
{
    if (isConnected())
        readyTimer.get()->start();

    DeviceDecorator::registerProperty(prop);
}

void Weather::processLight(ILightVectorProperty *lvp)
{
    if (!strcmp(lvp->name, "WEATHER_STATUS"))
    {
        if (lvp->s != m_WeatherStatus)
        {
            m_WeatherStatus = lvp->s;
            emit newStatus(m_WeatherStatus);
        }
    }

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

    if (weatherLP == nullptr)
        return IPS_ALERT;

    m_WeatherStatus = weatherLP->s;

    return weatherLP->s;
}

uint16_t Weather::getUpdatePeriod()
{
    INumberVectorProperty *updateNP = baseDevice->getNumber("WEATHER_UPDATE");

    if (updateNP == nullptr)
        return 0;

    return static_cast<uint16_t>(updateNP->np[0].value);
}
}
