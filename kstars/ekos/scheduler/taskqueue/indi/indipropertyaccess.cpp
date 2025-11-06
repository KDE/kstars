/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indipropertyaccess.h"
#include "ekos_scheduler_debug.h"

#include <QDBusReply>
#include <QDebug>

namespace Ekos
{

QVariant INDIPropertyAccess::getProperty(QDBusInterface *interface, const QString &propertyName)
{
    if (!isInterfaceValid(interface))
        return QVariant();

    QVariant value = interface->property(propertyName.toUtf8());

    if (!value.isValid())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Failed to get property" << propertyName
                                         << "from interface" << interface->service();
    }

    return value;
}

bool INDIPropertyAccess::callMethod(QDBusInterface *interface, const QString &methodName,
                                    const QList<QVariant> &arguments)
{
    if (!isInterfaceValid(interface))
        return false;

    QDBusMessage reply;

    if (arguments.isEmpty())
    {
        reply = interface->call(methodName);
    }
    else
    {
        reply = interface->callWithArgumentList(QDBus::AutoDetect, methodName, arguments);
    }

    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "DBus call to" << methodName
                                         << "failed:" << reply.errorMessage();
        return false;
    }

    return true;
}

ISD::ParkStatus INDIPropertyAccess::getParkStatus(QDBusInterface *interface)
{
    if (!isInterfaceValid(interface))
        return ISD::PARK_UNKNOWN;

    QVariant parkStatus = interface->property("parkStatus");

    if (!parkStatus.isValid())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Failed to get park status from interface"
                                         << interface->service();
        return ISD::PARK_UNKNOWN;
    }

    return static_cast<ISD::ParkStatus>(parkStatus.toInt());
}

QString INDIPropertyAccess::getPropertyState(QDBusInterface *interface, const QString &propertyName)
{
    if (!isInterfaceValid(interface))
        return QString();

    // Try to get the property state - different interfaces may use different property names
    QString statePropertyName = propertyName + "State";
    QVariant state = interface->property(statePropertyName.toUtf8());

    if (!state.isValid())
    {
        // Try alternative property name
        state = interface->property("status");
    }

    if (!state.isValid())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Failed to get state for property" << propertyName
                                         << "from interface" << interface->service();
        return QString();
    }

    return state.toString();
}

bool INDIPropertyAccess::isInterfaceValid(QDBusInterface *interface)
{
    if (!interface)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Null DBus interface";
        return false;
    }

    if (!interface->isValid())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Invalid DBus interface:" << interface->lastError().message();
        return false;
    }

    return true;
}

} // namespace Ekos
