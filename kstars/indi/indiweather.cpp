/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include "indiweather.h"
#include "clientmanager.h"

namespace ISD
{

Weather::Weather(GenericDevice *parent) : ConcreteDevice(parent)
{
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
}

Weather::Status Weather::getWeatherStatus()
{
    auto weatherLP = getLight("WEATHER_STATUS");

    if (!weatherLP)
        return WEATHER_IDLE;

    m_WeatherStatus = static_cast<Status>(weatherLP->getState());

    return static_cast<Status>(weatherLP->getState());
}

quint16 Weather::getUpdatePeriod()
{
    auto updateNP = getNumber("WEATHER_UPDATE");

    if (!updateNP)
        return 0;

    return static_cast<quint16>(updateNP->at(0)->getValue());
}

bool Weather::refresh()
{
    auto refreshSP = getSwitch("WEATHER_REFRESH");

    if (refreshSP == nullptr)
        return false;

    auto refreshSW = refreshSP->findWidgetByName("REFRESH");

    if (refreshSW == nullptr)
        return false;

    refreshSP->reset();
    refreshSW->setState(ISS_ON);
    sendNewSwitch(refreshSP);

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
