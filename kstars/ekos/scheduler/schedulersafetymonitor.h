/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indistd.h"
#include "indi/clientmanager.h"

#include <QObject>
#include <QTimer>
#include <QString>
#include <QSharedPointer>

namespace Ekos
{

/**
 * @class SchedulerSafetyMonitor
 * @brief Handles standalone safety monitor monitoring for the scheduler before equipment profile starts.
 *
 * This class manages a direct connection to an INDI Safety Monitor device independent of the main
 * Ekos session. It allows the scheduler to monitor safety conditions even before the
 * equipment profile is started, enabling early detection of unsafe conditions.
 */
class SchedulerSafetyMonitor : public QObject
{
        Q_OBJECT

    public:
        explicit SchedulerSafetyMonitor(QObject *parent = nullptr);
        ~SchedulerSafetyMonitor();

        /**
         * @brief Initialize standalone safety monitor monitoring
         * @param connectionString Connection string in format "device@hostname:port"
         */
        void initStandaloneSafetyMonitor(const QString &connectionString);

        /**
         * @brief Get the current safety status
         */
        IPState safetyStatus() const
        {
            return m_SafetyStatus;
        }

        /**
         * @brief Check if standalone safety monitor is connected
         */
        bool isConnected() const
        {
            return m_StandaloneSafetyMonitorConnected;
        }

        /**
         * @brief Get the safety monitor device name
         */
        QString deviceName() const
        {
            return m_SafetyMonitorDeviceName;
        }

    signals:
        /**
         * @brief Emitted when safety status changes
         */
        void newSafetyStatus(IPState status);

        /**
         * @brief Emitted for logging messages
         */
        void newLog(const QString &message);

    private slots:
        // INDI Listener signal handlers
        void handleDeviceRemovedFromListener(const QSharedPointer<ISD::GenericDevice> &device);
        void handleNewGenericDevice(const QSharedPointer<ISD::GenericDevice> &device);

    private:
        // Maximum number of reconnection attempts
        static constexpr uint MAX_SAFETY_MONITOR_RETRIES = 3;

        // Connection management
        void parseConnectionString(const QString &connectionString);
        void connectToStandaloneSafetyMonitor();
        void handleStandaloneSafetyMonitorDisconnection();
        void attemptSafetyMonitorReconnection();
        void setSafetyStatus(IPState status);
        void processSafetyProperty(INDI::Property property);

        // ClientManager signal handlers
        void handleSafetyMonitorClientStarted();
        void handleSafetyMonitorClientFailed(const QString &message);
        void handleSafetyMonitorClientTerminated(const QString &message);

        // Connection parameters
        QString m_SafetyMonitorDeviceName {"Safety Monitor"};
        QString m_SafetyMonitorHostname;
        uint m_SafetyMonitorPort {0};

        // State tracking
        IPState m_SafetyStatus {IPS_IDLE};
        uint m_SafetyMonitorConnectionRetries {0};
        bool m_StandaloneSafetyMonitorConnected {false};

        // INDI connection
        ClientManager *m_SafetyMonitorClientManager {nullptr};
        ISD::GenericDevice *m_StandaloneSafetyMonitorDevice {nullptr};
        QTimer m_SafetyMonitorReconnectTimer;
};

} // namespace Ekos
