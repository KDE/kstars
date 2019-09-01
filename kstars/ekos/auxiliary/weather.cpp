/*  Ekos Weather Interface
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "weather.h"

#include "ekos/manager.h"
#include "kstars.h"
#include "weatheradaptor.h"

#include <basedevice.h>

namespace Ekos
{
Weather::Weather()
{
    new WeatherAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Weather", this);

    qRegisterMetaType<ISD::Weather::Status>("ISD::Weather::Status");
    qDBusRegisterMetaType<ISD::Weather::Status>();
}

void Weather::setWeather(ISD::GDInterface *newWeather)
{
    if (newWeather == currentWeather)
        return;

    currentWeather = static_cast<ISD::Weather *>(newWeather);
    currentWeather->disconnect(this);
    connect(currentWeather, &ISD::Weather::newStatus, this, &Weather::newStatus);
    connect(currentWeather, &ISD::Weather::newWeatherData, this, &Weather::newWeatherData);
    connect(currentWeather, &ISD::Weather::ready, this, &Weather::ready);
    connect(currentWeather, &ISD::Weather::Connected, this, &Weather::ready);
    connect(currentWeather, &ISD::Weather::Disconnected, this, &Weather::disconnected);
}

void Weather::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentWeather && !strcmp(currentWeather->getDeviceName(), device->getDeviceName()))
    {
        currentWeather = nullptr;
    }
}

ISD::Weather::Status Weather::status()
{
    if (currentWeather == nullptr)
        return ISD::Weather::WEATHER_IDLE;

    return currentWeather->getWeatherStatus();
}

quint16 Weather::getUpdatePeriod()
{
    if (currentWeather == nullptr)
        return 0;

    return currentWeather->getUpdatePeriod();
}
}
