/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    if (currentWeather && (currentWeather->getDeviceName() == device->getDeviceName()))
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

bool Weather::refresh()
{
    return currentWeather->refresh();

}

}
