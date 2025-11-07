/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indiweather.h"
#include "indi/clientmanager.h"
#include "indi/indistd.h"

#include <QObject>
#include <QTimer>
#include <QString>
#include <QSharedPointer>

namespace Ekos
{

/**
 * @class SchedulerWeather
 * @brief Handles standalone weather monitoring for the scheduler before equipment profile starts.
 *
 * This class manages a direct connection to an INDI weather device independent of the main
 * Ekos session. It allows the scheduler to monitor weather conditions even before the
 * equipment profile is started, enabling early detection of unsafe weather conditions.
 */
class SchedulerWeather : public QObject
{
        Q_OBJECT

    public:
        explicit SchedulerWeather(QObject *parent = nullptr);
        ~SchedulerWeather();

        /**
         * @brief Initialize standalone weather monitoring
         * @param connectionString Connection string in format "device@hostname:port"
         */
        void initStandaloneWeather(const QString &connectionString);

        /**
         * @brief Get the current weather status
         */
        ISD::Weather::Status weatherStatus() const
        {
            return m_WeatherStatus;
        }

        /**
         * @brief Check if standalone weather is connected
         */
        bool isConnected() const
        {
            return m_StandaloneWeatherConnected;
        }

        /**
         * @brief Get the weather device name
         */
        QString deviceName() const
        {
            return m_WeatherDeviceName;
        }

    signals:
        /**
         * @brief Emitted when weather status changes
         */
        void newWeatherStatus(ISD::Weather::Status status);

        /**
         * @brief Emitted for logging messages
         */
        void newLog(const QString &message);

    private slots:
        // INDI Listener signal handlers
        void handleDeviceRemovedFromListener(const QSharedPointer<ISD::GenericDevice> &device);
        void handleNewWeatherInterface(ISD::Weather *weather);

    private:
        // Maximum number of reconnection attempts
        static constexpr uint MAX_WEATHER_RETRIES = 3;

        // Connection management
        void parseWeatherConnectionString(const QString &connectionString);
        void connectToStandaloneWeather();
        void handleStandaloneWeatherDevice(const QSharedPointer<ISD::GenericDevice> &device);
        void handleStandaloneWeatherDisconnection();
        void attemptWeatherReconnection();
        void setWeatherStatus(ISD::Weather::Status status);

        // ClientManager signal handlers
        void handleWeatherClientStarted();
        void handleWeatherClientFailed(const QString &message);
        void handleWeatherClientTerminated(const QString &message);

        // Connection parameters
        QString m_WeatherDeviceName;
        QString m_WeatherHostname;
        uint m_WeatherPort {0};

        // State tracking
        ISD::Weather::Status m_WeatherStatus {ISD::Weather::WEATHER_IDLE};
        uint m_WeatherConnectionRetries {0};
        bool m_StandaloneWeatherConnected {false};

        // INDI connection
        ClientManager *m_WeatherClientManager {nullptr};
        ISD::Weather *m_StandaloneWeatherDevice {nullptr};
        QTimer m_WeatherReconnectTimer;
};

} // namespace Ekos
