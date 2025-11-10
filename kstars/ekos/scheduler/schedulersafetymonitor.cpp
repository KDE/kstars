/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulersafetymonitor.h"
#include "indi/indilistener.h"
#include "indi/driverinfo.h"

#include <KLocalizedString>
#include <QRegularExpression>
#include <ekos_scheduler_debug.h>

namespace Ekos
{

SchedulerSafetyMonitor::SchedulerSafetyMonitor(QObject *parent)
    : QObject(parent)
{
    // Set up reconnection timer
    m_SafetyMonitorReconnectTimer.setSingleShot(true);
    connect(&m_SafetyMonitorReconnectTimer, &QTimer::timeout, this, &SchedulerSafetyMonitor::attemptSafetyMonitorReconnection);
}

SchedulerSafetyMonitor::~SchedulerSafetyMonitor()
{
    // Clean up standalone safety monitor monitoring
    if (m_SafetyMonitorReconnectTimer.isActive())
        m_SafetyMonitorReconnectTimer.stop();

    if (m_StandaloneSafetyMonitorDevice)
    {
        m_StandaloneSafetyMonitorDevice = nullptr;
    }

    if (m_SafetyMonitorClientManager)
    {
        // Remove from INDI Listener before disconnecting
        INDIListener::Instance()->removeClient(m_SafetyMonitorClientManager);

        m_SafetyMonitorClientManager->disconnectServer();
        delete m_SafetyMonitorClientManager;
        m_SafetyMonitorClientManager = nullptr;
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << "SchedulerSafetyMonitor: cleanup complete";
}

void SchedulerSafetyMonitor::initStandaloneSafetyMonitor(const QString &connectionString)
{
    // Only initialize if not already set up
    if (m_SafetyMonitorClientManager != nullptr)
        return;

    parseConnectionString(connectionString);

    // Check if we have valid connection parameters
    if (m_SafetyMonitorHostname.isEmpty() || m_SafetyMonitorPort == 0)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Standalone safety monitor not configured, skipping initialization";
        return;
    }

    emit newLog(i18n("Initializing standalone safety monitor monitoring for %1@%2:%3",
                     m_SafetyMonitorDeviceName, m_SafetyMonitorHostname, QString::number(m_SafetyMonitorPort)));

    // Create client manager for safety monitor device
    m_SafetyMonitorClientManager = new ClientManager();

    // Create a DriverInfo for the safety monitor device
    QSharedPointer<DriverInfo> safetyMonitorDriver(new DriverInfo(m_SafetyMonitorDeviceName));
    safetyMonitorDriver->setHostParameters(m_SafetyMonitorHostname, m_SafetyMonitorPort);
    safetyMonitorDriver->setDriverSource(HOST_SOURCE);

    // Append the managed driver to the client manager
    m_SafetyMonitorClientManager->appendManagedDriver(safetyMonitorDriver);

    // Register with INDI Listener to receive device notifications
    INDIListener::Instance()->addClient(m_SafetyMonitorClientManager);

    // Connect ClientManager signals for connection state management
    connect(m_SafetyMonitorClientManager, &ClientManager::started, this,
            &SchedulerSafetyMonitor::handleSafetyMonitorClientStarted,
            Qt::UniqueConnection);
    connect(m_SafetyMonitorClientManager, &ClientManager::failed, this,
            &SchedulerSafetyMonitor::handleSafetyMonitorClientFailed,
            Qt::UniqueConnection);
    connect(m_SafetyMonitorClientManager, &ClientManager::terminated, this,
            &SchedulerSafetyMonitor::handleSafetyMonitorClientTerminated,
            Qt::UniqueConnection);

    // Connect signals for device management with unique connections
    connect(INDIListener::Instance(), &INDIListener::newDevice, this, &SchedulerSafetyMonitor::handleNewGenericDevice,
            Qt::UniqueConnection);

    connect(INDIListener::Instance(), &INDIListener::deviceRemoved, this,
            &SchedulerSafetyMonitor::handleDeviceRemovedFromListener, Qt::UniqueConnection);

    // Attempt initial connection
    connectToStandaloneSafetyMonitor();
}

void SchedulerSafetyMonitor::parseConnectionString(const QString &connectionString)
{
    if (connectionString.isEmpty())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "No safety monitor connection string provided";
        return;
    }

    // Parse "host:port" or "host" format
    QStringList parts = connectionString.trimmed().split(':');

    if (parts.size() < 1 || parts.size() > 2)
    {
        emit newLog(i18n("Warning: Invalid safety monitor connection string format. Expected 'host' or 'host:port'"));
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Invalid safety monitor connection string:" << connectionString;
        return;
    }

    // Always use "Safety Monitor" as the device name
    m_SafetyMonitorDeviceName = "Safety Monitor";

    // Extract hostname
    m_SafetyMonitorHostname = parts[0].trimmed();

    // Extract port (optional, defaults to 7624)
    if (parts.size() == 2)
    {
        bool ok;
        m_SafetyMonitorPort = parts[1].trimmed().toUInt(&ok);

        if (!ok || m_SafetyMonitorPort == 0)
        {
            // If invalid port, use default
            m_SafetyMonitorPort = 7624;
            emit newLog(i18n("Invalid safety monitor port number, using default port 7624"));
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Invalid port number:" << parts[1] << ", using default 7624";
        }
    }
    else
    {
        // No port specified, use default
        m_SafetyMonitorPort = 7624;
        qCDebug(KSTARS_EKOS_SCHEDULER) << "No port specified, using default 7624";
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Parsed safety monitor connection - Device:" << m_SafetyMonitorDeviceName
                                   << "Host:" << m_SafetyMonitorHostname << "Port:" << m_SafetyMonitorPort;
}

void SchedulerSafetyMonitor::connectToStandaloneSafetyMonitor()
{
    if (m_SafetyMonitorClientManager == nullptr)
        return;

    // Check if already connected - don't attempt reconnection
    if (m_StandaloneSafetyMonitorConnected && m_StandaloneSafetyMonitorDevice != nullptr)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Safety monitor already connected, skipping reconnection attempt";
        return;
    }

    if (m_SafetyMonitorConnectionRetries >= MAX_SAFETY_MONITOR_RETRIES)
    {
        emit newLog(i18n("Failed to connect to standalone safety monitor after %1 attempts, giving up",
                         MAX_SAFETY_MONITOR_RETRIES));
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Max safety monitor connection retries reached";
        return;
    }

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Attempting to connect to standalone safety monitor (attempt"
                                  << (m_SafetyMonitorConnectionRetries + 1) << "of" << MAX_SAFETY_MONITOR_RETRIES << ")";

    m_SafetyMonitorClientManager->watchDevice(m_SafetyMonitorDeviceName.toLatin1().constData());

    // Set server parameters and connect
    m_SafetyMonitorClientManager->setServer(m_SafetyMonitorHostname.toLatin1().constData(), m_SafetyMonitorPort);

    // Use QString::number() to avoid locale-specific formatting of port number
    emit newLog(i18n("Connecting to standalone safety monitor at %1:%2...", m_SafetyMonitorHostname,
                     QString::number(m_SafetyMonitorPort)));
    m_SafetyMonitorClientManager->establishConnection();
}

void SchedulerSafetyMonitor::handleNewGenericDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device == nullptr || m_SafetyMonitorDeviceName.isEmpty())
        return;

    // Check if this is our safety monitor device
    if (device->getDeviceName() == m_SafetyMonitorDeviceName)
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Found standalone safety monitor device:" << m_SafetyMonitorDeviceName;

        m_StandaloneSafetyMonitorDevice = device.get();

        if (m_StandaloneSafetyMonitorDevice)
        {
            emit newLog(i18n("Standalone safety monitor device %1 is ready", m_SafetyMonitorDeviceName));

            // Connect to property updates
            connect(m_StandaloneSafetyMonitorDevice, &ISD::GenericDevice::propertyUpdated, this,
                    &SchedulerSafetyMonitor::processSafetyProperty, Qt::UniqueConnection);

            m_StandaloneSafetyMonitorConnected = true;
            m_SafetyMonitorConnectionRetries = 0;

            // Check initial property value - use correct property name
            auto safetyProp = m_StandaloneSafetyMonitorDevice->getProperty("SAFETY_STATUS");
            if (safetyProp.isValid())
            {
                processSafetyProperty(safetyProp);
            }
        }
        else
        {
            emit newLog(i18n("Warning: Device %1 is not valid", m_SafetyMonitorDeviceName));
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Device is not valid";
        }
    }
}

void SchedulerSafetyMonitor::processSafetyProperty(INDI::Property property)
{
    if (property.isNameMatch("SAFETY_STATUS") && property.getType() == INDI_LIGHT)
    {
        setSafetyStatus(property.getState());
    }
}

void SchedulerSafetyMonitor::handleStandaloneSafetyMonitorDisconnection()
{
    emit newLog(i18n("Warning: Standalone safety monitor device %1 disconnected", m_SafetyMonitorDeviceName));
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Standalone safety monitor device disconnected";

    m_StandaloneSafetyMonitorConnected = false;

    if (m_StandaloneSafetyMonitorDevice)
    {
        m_StandaloneSafetyMonitorDevice = nullptr;
    }

    // Attempt to reconnect if we haven't exceeded retry limit
    if (m_SafetyMonitorConnectionRetries < MAX_SAFETY_MONITOR_RETRIES)
    {
        m_SafetyMonitorConnectionRetries++;
        emit newLog(i18n("Will attempt to reconnect in 10 seconds (attempt %1 of %2)...",
                         m_SafetyMonitorConnectionRetries, MAX_SAFETY_MONITOR_RETRIES));
        m_SafetyMonitorReconnectTimer.start(10000); // 10 seconds delay for reconnection
    }
    else
    {
        emit newLog(i18n("Maximum reconnection attempts reached for standalone safety monitor"));
    }
}

void SchedulerSafetyMonitor::attemptSafetyMonitorReconnection()
{
    if (!m_StandaloneSafetyMonitorConnected && m_SafetyMonitorConnectionRetries < MAX_SAFETY_MONITOR_RETRIES)
    {
        emit newLog(i18n("Attempting to reconnect to standalone safety monitor..."));
        connectToStandaloneSafetyMonitor();
    }
}

void SchedulerSafetyMonitor::setSafetyStatus(IPState status)
{
    if (status == m_SafetyStatus)
        return;

    m_SafetyStatus = status;
    emit newSafetyStatus(status);
}

void SchedulerSafetyMonitor::handleSafetyMonitorClientStarted()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Standalone safety monitor client connected successfully";
    emit newLog(i18n("Standalone safety monitor client connected successfully"));
    m_SafetyMonitorConnectionRetries = 0;
}

void SchedulerSafetyMonitor::handleSafetyMonitorClientFailed(const QString &message)
{
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Standalone safety monitor client connection failed:" << message;
    emit newLog(i18n("Standalone safety monitor client connection failed: %1", message));

    // Attempt to reconnect if we haven't exceeded retry limit
    if (m_SafetyMonitorConnectionRetries < MAX_SAFETY_MONITOR_RETRIES)
    {
        m_SafetyMonitorConnectionRetries++;
        emit newLog(i18n("Will attempt to reconnect in 10 seconds (attempt %1 of %2)...",
                         m_SafetyMonitorConnectionRetries, MAX_SAFETY_MONITOR_RETRIES));
        m_SafetyMonitorReconnectTimer.start(10000); // 10 seconds delay for reconnection
    }
    else
    {
        emit newLog(i18n("Maximum reconnection attempts reached for standalone safety monitor"));
    }
}

void SchedulerSafetyMonitor::handleSafetyMonitorClientTerminated(const QString &message)
{
    qCWarning(KSTARS_EKOS_SCHEDULER) << "Standalone safety monitor client terminated:" << message;
    emit newLog(i18n("Standalone safety monitor client terminated: %1", message));

    m_StandaloneSafetyMonitorConnected = false;

    if (m_StandaloneSafetyMonitorDevice)
    {
        m_StandaloneSafetyMonitorDevice = nullptr;
    }

    // Attempt to reconnect if we haven't exceeded retry limit
    if (m_SafetyMonitorConnectionRetries < MAX_SAFETY_MONITOR_RETRIES)
    {
        m_SafetyMonitorConnectionRetries++;
        emit newLog(i18n("Will attempt to reconnect in 10 seconds (attempt %1 of %2)...",
                         m_SafetyMonitorConnectionRetries, MAX_SAFETY_MONITOR_RETRIES));
        m_SafetyMonitorReconnectTimer.start(10000); // 10 seconds delay for reconnection
    }
    else
    {
        emit newLog(i18n("Maximum reconnection attempts reached for standalone safety monitor"));
    }
}

void SchedulerSafetyMonitor::updateConnectionString(const QString &connectionString)
{
    // Parse the new connection string
    QString newHostname;
    uint newPort = 0;

    if (!connectionString.isEmpty())
    {
        QStringList parts = connectionString.trimmed().split(':');

        if (parts.size() >= 1 && parts.size() <= 2)
        {
            newHostname = parts[0].trimmed();

            if (parts.size() == 2)
            {
                bool ok;
                newPort = parts[1].trimmed().toUInt(&ok);
                if (!ok || newPort == 0)
                    newPort = 7624;
            }
            else
            {
                newPort = 7624;
            }
        }
    }

    // Check if connection parameters have changed
    bool hostnameChanged = (newHostname != m_SafetyMonitorHostname);
    bool portChanged = (newPort != m_SafetyMonitorPort);

    if (!hostnameChanged && !portChanged)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Safety monitor connection string unchanged, no action needed";
        return;
    }

    // Log the change
    if (hostnameChanged || portChanged)
    {
        emit newLog(i18n("Safety monitor connection string changed from %1:%2 to %3:%4",
                         m_SafetyMonitorHostname, QString::number(m_SafetyMonitorPort), newHostname, QString::number(newPort)));
    }

    // Disconnect existing connection if any
    if (m_SafetyMonitorClientManager != nullptr)
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Disconnecting existing safety monitor before reconnecting to new address";
        emit newLog(i18n("Disconnecting from existing safety monitor at %1:%2",
                         m_SafetyMonitorHostname, QString::number(m_SafetyMonitorPort)));

        // Stop any pending reconnection attempts
        if (m_SafetyMonitorReconnectTimer.isActive())
            m_SafetyMonitorReconnectTimer.stop();

        // Clean up device
        if (m_StandaloneSafetyMonitorDevice)
        {
            m_StandaloneSafetyMonitorDevice = nullptr;
        }

        // Remove from INDI Listener and disconnect
        INDIListener::Instance()->removeClient(m_SafetyMonitorClientManager);
        m_SafetyMonitorClientManager->disconnectServer();
        delete m_SafetyMonitorClientManager;
        m_SafetyMonitorClientManager = nullptr;

        // Reset state
        m_StandaloneSafetyMonitorConnected = false;
        m_SafetyMonitorConnectionRetries = 0;
    }

    // Initialize with new connection string if not empty
    if (!connectionString.isEmpty())
    {
        initStandaloneSafetyMonitor(connectionString);
    }
    else
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Empty connection string provided, safety monitor monitoring disabled";
        emit newLog(i18n("Safety monitor monitoring disabled"));
    }
}

void SchedulerSafetyMonitor::handleDeviceRemovedFromListener(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device && device->getDeviceName() == m_SafetyMonitorDeviceName)
    {
        handleStandaloneSafetyMonitorDisconnection();
    }
}

} // namespace Ekos
