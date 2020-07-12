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
    if (!prop->getRegistered())
        return;

    if (isConnected())
        readyTimer.get()->start();

    DeviceDecorator::registerProperty(prop);
}

void Weather::processLight(ILightVectorProperty *lvp)
{
    if (!strcmp(lvp->name, "WEATHER_STATUS"))
    {
        Status currentStatus = static_cast<Status>(lvp->s);
        if (currentStatus != m_WeatherStatus)
        {
            m_WeatherStatus = currentStatus;
            emit newStatus(m_WeatherStatus);
        }
    }

    DeviceDecorator::processLight(lvp);
}

void Weather::processNumber(INumberVectorProperty *nvp)
{
    if (nvp == nullptr)
        return;

    if (!strcmp(nvp->name, "WEATHER_PARAMETERS"))
    {
        m_WeatherData.clear();

        // read all sensor values received
        for (int i = 0; i < nvp->nnp; i++)
        {
            INumber number = nvp->np[i];
            m_WeatherData.push_back({QString(number.name), QString(number.label), number.value});
        }
        emit newWeatherData(m_WeatherData);
    }

    // and now continue with the standard behavior
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

Weather::Status Weather::getWeatherStatus()
{
    ILightVectorProperty *weatherLP = baseDevice->getLight("WEATHER_STATUS");

    if (weatherLP == nullptr)
        return WEATHER_IDLE;

    m_WeatherStatus = static_cast<Status>(weatherLP->s);

    return static_cast<Status>(weatherLP->s);
}

quint16 Weather::getUpdatePeriod()
{
    INumberVectorProperty *updateNP = baseDevice->getNumber("WEATHER_UPDATE");

    if (updateNP == nullptr)
        return 0;

    return static_cast<quint16>(updateNP->np[0].value);
}

bool Weather::refresh()
{
    ISwitchVectorProperty *refreshSP = baseDevice->getSwitch("WEATHER_REFRESH");

    if (refreshSP == nullptr)
        return false;

    ISwitch *refreshSW = IUFindSwitch(refreshSP, "REFRESH");

    if (refreshSW == nullptr)
        return false;

    IUResetSwitch(refreshSP);
    refreshSW->s = ISS_ON;
    clientManager->sendNewSwitch(refreshSP);

    return true;

}
}

#ifndef KSTARS_LITE
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Weather::Status &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Weather::Status &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Weather::Status>(a);
    return argument;
}
#endif
