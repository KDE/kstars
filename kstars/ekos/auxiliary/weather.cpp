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
}

void Weather::setWeather(ISD::GDInterface *newWeather)
{
    if (newWeather == currentWeather)
        return;

    currentWeather = static_cast<ISD::Weather *>(newWeather);
    currentWeather->disconnect(this);
    connect(currentWeather, &ISD::Weather::newStatus, this, &Weather::newStatus);
}

IPState Weather::getWeatherStatus()
{
    if (currentWeather == nullptr)
        return IPS_ALERT;

    return currentWeather->getWeatherStatus();
}

uint16_t Weather::getUpdatePeriod()
{
    if (currentWeather == nullptr)
        return 0;

    return currentWeather->getUpdatePeriod();
}
}
