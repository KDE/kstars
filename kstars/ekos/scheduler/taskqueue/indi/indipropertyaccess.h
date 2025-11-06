/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QDBusInterface>

#include "indi/indistd.h"

namespace Ekos
{

class SchedulerModuleState;

/**
 * @class INDIPropertyAccess
 * @brief Helper class for accessing INDI properties via existing DBus interfaces
 *
 * This class provides a simplified interface for task actions to interact with
 * INDI devices using the DBus interfaces already established by SchedulerProcess.
 * It acts as a thin wrapper to make property access cleaner and more consistent.
 */
class INDIPropertyAccess
{
    public:
        /**
         * @brief Get a property value from a DBus interface
         * @param interface DBus interface to query
         * @param propertyName Name of the property
         * @return Property value, or invalid QVariant if property doesn't exist
         */
        static QVariant getProperty(QDBusInterface *interface, const QString &propertyName);

        /**
         * @brief Set a property value via DBus interface
         * @param interface DBus interface to use
         * @param methodName DBus method to call (e.g., "park", "unpark", "abort")
         * @param arguments Arguments to pass to the method
         * @return true if call succeeded, false otherwise
         */
        static bool callMethod(QDBusInterface *interface, const QString &methodName,
                               const QList<QVariant> &arguments = QList<QVariant>());

        /**
         * @brief Get parking status for parkable devices
         * @param interface DBus interface to query
         * @return Park status
         */
        static ISD::ParkStatus getParkStatus(QDBusInterface *interface);

        /**
         * @brief Get INDI property state
         * @param interface DBus interface to query
         * @param propertyName Name of the property
         * @return Property state string ("Idle", "OK", "Busy", "Alert")
         */
        static QString getPropertyState(QDBusInterface *interface, const QString &propertyName);

        /**
         * @brief Check if interface is valid and connected
         * @param interface DBus interface to check
         * @return true if valid and usable
         */
        static bool isInterfaceValid(QDBusInterface *interface);
};

} // namespace Ekos
