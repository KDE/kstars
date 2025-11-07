/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulerweather.h"
#include "indi/indilistener.h"
#include "indi/driverinfo.h"

#include <KLocalizedString>
#include <QRegularExpression>
#include <ekos_scheduler_debug.h>

namespace Ekos
{

SchedulerWeather::SchedulerWeather(QObject *parent)
    : QObject(parent)
{
    // Set up reconnection timer
    m_WeatherReconnectTimer.setSingleShot(true);
    connect(&m_WeatherReconnectTimer, &QTimer::timeout, this, &SchedulerWeather::attemptWeatherReconnection);
}

SchedulerWeather::~SchedulerWeather()
{
    // Clean up standalone weather monitoring
    if (m_WeatherReconnectTimer.isActive())
        m_WeatherReconnectTimer.stop();

    if (m_StandaloneWeatherDevice)
    {
        disconnect(m_StandaloneWeatherDevice, &ISD::Weather::newStatus, this, &SchedulerWeather::setWeatherStatus);
        m_StandaloneWeatherDevice = nullptr;
    }

    if (m_WeatherClientManager)
    {
        // Remove from INDI Listener before disconnecting
        INDIListener::Instance()->removeClient(m_WeatherClientManager);

        m_WeatherClientManager->disconnectServer();
        delete m_WeatherClientManager;
        m_WeatherClientManager = nullptr;
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << "SchedulerWeather: cleanup complete";
}

void SchedulerWeather::initStandaloneWeather(const QString &connectionString)
{
    // Only initialize if not already set up
    if (m_WeatherClientManager != nullptr)
        return;

    parseWeatherConnectionString(connectionString);

    // Check if we have valid connection parameters
    if (m_WeatherDeviceName.isEmpty() || m_WeatherHostname.isEmpty() || m_WeatherPort == 0)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Standalone weather not configured, skipping initialization";
        return;
    }

    emit newLog(i18n("Initializing standalone weather monitoring for %1@%2:%3",
                     m_WeatherDeviceName, m_WeatherHostname, m_WeatherPort));

    // Create client manager for weather device
    m_WeatherClientManager = new ClientManager();

    // Create a DriverInfo for the weather device
    QSharedPointer<DriverInfo> weatherDriver(new DriverInfo(m_WeatherDeviceName));
    weatherDriver->setHostParameters(m_WeatherHostname, m_WeatherPort);
    weatherDriver->setDriverSource(HOST_SOURCE);

    // Append the managed driver to the client manager
    m_WeatherClientManager->appendManagedDriver(weatherDriver);

    // Register with INDI Listener to receive device notifications
    INDIListener::Instance()->addClient(m_WeatherClientManager);

    // Connect ClientManager signals for connection state management
    connect(m_WeatherClientManager, &ClientManager::started, this, &SchedulerWeather::handleWeatherClientStarted,
            Qt::UniqueConnection);
    connect(m_WeatherClientManager, &ClientManager::failed, this, &SchedulerWeather::handleWeatherClientFailed,
            Qt::UniqueConnection);
    connect(m_WeatherClientManager, &ClientManager::terminated, this, &SchedulerWeather::handleWeatherClientTerminated,
            Qt::UniqueConnection);

    // Connect signals for device management with unique connections
    connect(INDIListener::Instance(), &INDIListener::newDevice, this, &SchedulerWeather::handleStandaloneWeatherDevice,
            Qt::UniqueConnection);

    connect(INDIListener::Instance(), &INDIListener::deviceRemoved, this,
            &SchedulerWeather::handleDeviceRemovedFromListener, Qt::UniqueConnection);

    // Attempt initial connection
    connectToStandaloneWeather();
}

void SchedulerWeather::parseWeatherConnectionString(const QString &connectionString)
{
    if (connectionString.isEmpty())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "No weather connection string provided";
        return;
    }

    // Use regex for safer parsing: ["']?device["']?@host[:port]?
    // Pattern explanation:
    // ^["']? - optional quote at start
    // ([^"'@]+) - capture device name (any chars except quotes and @)
    // ["']? - optional quote after device name
    // @ - literal @ separator
    // ([^:]+) - capture hostname (any chars except :)
    // (?::(\d+))? - optional : followed by port number
    // $ - end of string
    QRegularExpression regex(R"(^["']?([^"'@]+)["']?@([^:]+)(?::(\d+))?$)");
    QRegularExpressionMatch match = regex.match(connectionString.trimmed());

    if (!match.hasMatch())
    {
        emit newLog(i18n("Warning: Invalid weather connection string format. Expected 'device@host:port'"));
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Invalid weather connection string:" << connectionString;
        return;
    }

    // Extract device name (captured group 1)
    m_WeatherDeviceName = match.captured(1).trimmed();

    // Extract hostname (captured group 2)
    m_WeatherHostname = match.captured(2).trimmed();

    // Extract port (captured group 3) or use default
    QString portStr = match.captured(3);
    if (portStr.isEmpty())
    {
        m_WeatherPort = 7624;
        qCDebug(KSTARS_EKOS_SCHEDULER) << "No port specified, using default port 7624";
    }
    else
    {
        bool ok;
        m_WeatherPort = portStr.toUInt(&ok);

        if (!ok || m_WeatherPort == 0)
        {
            // If invalid port, use default
            m_WeatherPort = 7624;
            emit newLog(i18n("Invalid weather port number, using default port 7624"));
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Invalid port number:" << portStr << ", using default 7624";
        }
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Parsed weather connection - Device:" << m_WeatherDeviceName
                                   << "Host:" << m_WeatherHostname << "Port:" << m_WeatherPort;
}

void SchedulerWeather::connectToStandaloneWeather()
{
    if (m_WeatherClientManager == nullptr)
        return;

    if (m_WeatherConnectionRetries >= MAX_WEATHER_RETRIES)
    {
        emit newLog(i18n("Failed to connect to standalone weather after %1 attempts, giving up",
                         MAX_WEATHER_RETRIES));
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Max weather connection retries reached";
        return;
    }

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Attempting to connect to standalone weather (attempt"
                                  << (m_WeatherConnectionRetries + 1) << "of" << MAX_WEATHER_RETRIES << ")";

    m_WeatherClientManager->watchDevice(m_WeatherDeviceName.toLatin1().constData());

    // Set server parameters and connect
    m_WeatherClientManager->setServer(m_WeatherHostname.toLatin1().constData(), m_WeatherPort);

    emit newLog(i18n("Connecting to standalone weather at %1:%2...", m_WeatherHostname, m_WeatherPort));
    m_WeatherClientManager->establishConnection();
}

void SchedulerWeather::handleStandaloneWeatherDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device == nullptr || m_WeatherDeviceName.isEmpty())
        return;

    // Check if this is our weather device
    if (device->getDeviceName() == m_WeatherDeviceName)
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Found standalone weather device:" << m_WeatherDeviceName;

        // Connect to newWeather signal to get the proper ISD::Weather interface
        connect(device.get(), &ISD::GenericDevice::newWeather, this,
                &SchedulerWeather::handleNewWeatherInterface, Qt::UniqueConnection);
    }
}

void SchedulerWeather::handleStandaloneWeatherDisconnection()
{
    emit newLog(i18n("Warning: Standalone weather device %1 disconnected", m_WeatherDeviceName));
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Standalone weather device disconnected";

    m_StandaloneWeatherConnected = false;

    if (m_StandaloneWeatherDevice)
    {
        disconnect(m_StandaloneWeatherDevice, &ISD::Weather::newStatus, this, &SchedulerWeather::setWeatherStatus);
        m_StandaloneWeatherDevice = nullptr;
    }

    // Attempt to reconnect if we haven't exceeded retry limit
    if (m_WeatherConnectionRetries < MAX_WEATHER_RETRIES)
    {
        m_WeatherConnectionRetries++;
        emit newLog(i18n("Will attempt to reconnect in 10 seconds (attempt %1 of %2)...",
                         m_WeatherConnectionRetries, MAX_WEATHER_RETRIES));
        m_WeatherReconnectTimer.start(10000); // 10 seconds delay for reconnection
    }
    else
    {
        emit newLog(i18n("Maximum reconnection attempts reached for standalone weather"));
    }
}

void SchedulerWeather::attemptWeatherReconnection()
{
    if (!m_StandaloneWeatherConnected && m_WeatherConnectionRetries < MAX_WEATHER_RETRIES)
    {
        emit newLog(i18n("Attempting to reconnect to standalone weather..."));
        connectToStandaloneWeather();
    }
}

void SchedulerWeather::setWeatherStatus(ISD::Weather::Status status)
{
    if (status == m_WeatherStatus)
        return;

    m_WeatherStatus = status;
    emit newWeatherStatus(status);
}

void SchedulerWeather::handleWeatherClientStarted()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Standalone weather client connected successfully";
    emit newLog(i18n("Standalone weather client connected successfully"));
    m_WeatherConnectionRetries = 0;
}

void SchedulerWeather::handleWeatherClientFailed(const QString &message)
{
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Standalone weather client connection failed:" << message;
    emit newLog(i18n("Standalone weather client connection failed: %1", message));

    // Attempt to reconnect if we haven't exceeded retry limit
    if (m_WeatherConnectionRetries < MAX_WEATHER_RETRIES)
    {
        m_WeatherConnectionRetries++;
        emit newLog(i18n("Will attempt to reconnect in 10 seconds (attempt %1 of %2)...",
                         m_WeatherConnectionRetries, MAX_WEATHER_RETRIES));
        m_WeatherReconnectTimer.start(10000); // 10 seconds delay for reconnection
    }
    else
    {
        emit newLog(i18n("Maximum reconnection attempts reached for standalone weather"));
    }
}

void SchedulerWeather::handleWeatherClientTerminated(const QString &message)
{
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Standalone weather client terminated:" << message;
    emit newLog(i18n("Standalone weather client terminated: %1", message));

    m_StandaloneWeatherConnected = false;

    if (m_StandaloneWeatherDevice)
    {
        disconnect(m_StandaloneWeatherDevice, &ISD::Weather::newStatus, this, &SchedulerWeather::setWeatherStatus);
        m_StandaloneWeatherDevice = nullptr;
    }

    // Attempt to reconnect if we haven't exceeded retry limit
    if (m_WeatherConnectionRetries < MAX_WEATHER_RETRIES)
    {
        m_WeatherConnectionRetries++;
        emit newLog(i18n("Will attempt to reconnect in 10 seconds (attempt %1 of %2)...",
                         m_WeatherConnectionRetries, MAX_WEATHER_RETRIES));
        m_WeatherReconnectTimer.start(10000); // 10 seconds delay for reconnection
    }
    else
    {
        emit newLog(i18n("Maximum reconnection attempts reached for standalone weather"));
    }
}


void SchedulerWeather::handleDeviceRemovedFromListener(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device && device->getDeviceName() == m_WeatherDeviceName)
    {
        handleStandaloneWeatherDisconnection();
    }
}

void SchedulerWeather::handleNewWeatherInterface(ISD::Weather *weather)
{
    m_StandaloneWeatherDevice = weather;

    if (m_StandaloneWeatherDevice)
    {
        emit newLog(i18n("Standalone weather device %1 is ready", m_WeatherDeviceName));

        // Connect to weather status updates
        connect(m_StandaloneWeatherDevice, &ISD::Weather::newStatus, this,
                &SchedulerWeather::setWeatherStatus, Qt::UniqueConnection);
        setWeatherStatus(m_StandaloneWeatherDevice->status());

        m_StandaloneWeatherConnected = true;
        m_WeatherConnectionRetries = 0;
    }
    else
    {
        emit newLog(i18n("Warning: Device %1 is not a weather device", m_WeatherDeviceName));
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Device is not ISD::Weather type";
    }
}

} // namespace Ekos
