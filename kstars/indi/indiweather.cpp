/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indiweather.h"
#include "weatheradaptor.h"

#include <basedevice.h>
#include <QJsonObject>
#include <qdbusmetatype.h>

namespace ISD
{

Weather::Weather(GenericDevice *parent) : ConcreteDevice(parent)
{
    qRegisterMetaType<ISD::Weather::Status>("ISD::Weather::Status");
    qDBusRegisterMetaType<ISD::Weather::Status>();

    new WeatherAdaptor(this);
    m_DBusObjectPath = QString("/KStars/INDI/Weather/%1").arg(getID());
    QDBusConnection::sessionBus().registerObject(m_DBusObjectPath, this);
}

void Weather::processLight(INDI::Property prop)
{
    auto lvp = prop.getLight();
    if (lvp->isNameMatch("WEATHER_STATUS"))
    {
        Status currentStatus = static_cast<Status>(lvp->s);
        if (currentStatus != m_WeatherStatus)
        {
            m_WeatherStatus = currentStatus;
            emit newStatus(m_WeatherStatus);
        }
    }
}

void Weather::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();

    if (nvp->isNameMatch("WEATHER_PARAMETERS"))
    {
        m_Data = QJsonArray();

        // read all sensor values received
        for (int i = 0; i < nvp->nnp; i++)
        {
            auto number = nvp->at(i);
            QJsonObject sensor =
            {
                {"name", number->getName()},
                {"label", number->getLabel()},
                {"value", number->getValue()}
            };
            m_Data.push_back(sensor);
        }
        emit newData(m_Data);
        emit newJSONData(QJsonDocument(m_Data).toJson());
    }
}

Weather::Status Weather::status()
{
    auto weatherLP = getLight("WEATHER_STATUS");

    if (!weatherLP)
        return WEATHER_IDLE;

    m_WeatherStatus = static_cast<Status>(weatherLP->getState());

    return static_cast<Status>(weatherLP->getState());
}

int Weather::refreshPeriod()
{
    auto updateNP = getNumber("WEATHER_UPDATE");

    if (!updateNP)
        return 0;

    return static_cast<int>(updateNP->at(0)->getValue());
}

void Weather::setRefreshPeriod(int value)
{
    auto updateNP = getNumber("WEATHER_UPDATE");

    if (!updateNP)
        return;

    updateNP->at(0)->setValue(value);
    sendNewProperty(updateNP);
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
    sendNewProperty(refreshSP);

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
