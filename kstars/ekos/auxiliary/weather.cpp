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

void Weather::addWeather(ISD::Weather *device)
{
    // No duplicates.
    for (auto &oneWeatherStation : m_WeatherSources)
    {
        if (oneWeatherStation->getDeviceName() == device->getDeviceName())
            return;
    }

    for (auto &oneWeatherStation : m_WeatherSources)
        oneWeatherStation->disconnect(this);

    // TODO add method to select active weather source
    m_WeatherSources.append(device);
    m_WeatherSource = device;
    connect(m_WeatherSource, &ISD::Weather::newStatus, this, &Weather::newStatus);
    connect(m_WeatherSource, &ISD::Weather::newWeatherData, this, &Weather::newWeatherData);
    connect(m_WeatherSource, &ISD::Weather::ready, this, &Weather::ready);
    connect(m_WeatherSource, &ISD::Weather::Connected, this, &Weather::ready);
    connect(m_WeatherSource, &ISD::Weather::Disconnected, this, &Weather::disconnected);
}

void Weather::removeDevice(ISD::GenericDevice *device)
{
    device->disconnect(this);
    for (auto &oneWeatherSource : m_WeatherSources)
    {
        if (oneWeatherSource->getDeviceName() == device->getDeviceName())
        {
            m_WeatherSource = nullptr;
            m_WeatherSources.removeOne(oneWeatherSource);
            break;
        }
    }
}

ISD::Weather::Status Weather::status()
{
    if (m_WeatherSource == nullptr)
        return ISD::Weather::WEATHER_IDLE;

    return m_WeatherSource->getWeatherStatus();
}

quint16 Weather::getUpdatePeriod()
{
    if (m_WeatherSource == nullptr)
        return 0;

    return m_WeatherSource->getUpdatePeriod();
}

bool Weather::refresh()
{
    return m_WeatherSource->refresh();

}

}
