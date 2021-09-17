/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indistd.h"
#include "indi/indiweather.h"

#include <QtDBus>

namespace Ekos
{
/**
 * @class Weather
 * @short Supports basic weather station functions
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class Weather : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Weather")
        Q_PROPERTY(ISD::Weather::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(quint16 updatePeriod READ getUpdatePeriod)

    public:
        Weather();
        virtual ~Weather() override = default;

        /**
         * @defgroup WeatherDBusInterface Ekos DBus Interface - Weather Interface
         * Ekos::Weather interface provides basic weather operations.
         */

        /*@{*/

        /**
         * DBUS interface function.
         * Get Weather status.
         * @return Either IPS_OK for OK acceptable weather, IPS_BUSY for weather in warning zone, and IPS_ALERT for weather in danger zone. The zones ranges are defined by the INDI weather driver.
         */
        Q_SCRIPTABLE ISD::Weather::Status status();

        /**
         * DBUS interface function.
         * Get Weather Update Period
         * @return Return weather update period in minute. The weather is updated every X minutes from the weather source.
         */
        Q_SCRIPTABLE quint16 getUpdatePeriod();

        /**
         * DBUS interface function.
         * @brief Refresh the weather status
         */
        Q_SCRIPTABLE bool refresh();

        /** @}*/

        /**
         * @brief setWeather set the Weather device
         * @param newWeather pointer to Weather device.
         */
        void setWeather(ISD::GDInterface *newWeather);

        void removeDevice(ISD::GDInterface *device);

    signals:
        /**
         * @brief newStatus Weather Status
         * @param status IPS_OK --> Good, IPS_BUSY --> Warning, IPS_ALERT --> Alert
         */
        void newStatus(ISD::Weather::Status status);
        void newWeatherData(const std::vector<ISD::Weather::WeatherData> &data);
        void ready();
        // Signal when the underlying ISD::Weather signals a Disconnected()
        void disconnected();

    private:
        // Devices needed for Weather operation
        ISD::Weather *currentWeather { nullptr };
};
}
