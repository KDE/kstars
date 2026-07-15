/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "devicelookup.h"

#include "indi/indilistener.h"
#include "indi/indistd.h"

namespace MCP::Tools
{

QSharedPointer<ISD::GenericDevice> findDeviceByName(const QString &name, QString &error)
{
    QSharedPointer<ISD::GenericDevice> device;
    if (!INDIListener::Instance() || !INDIListener::Instance()->getDevice(name, device) || !device)
    {
        error = QStringLiteral("Device not found: %1").arg(name);
        return {};
    }
    return device;
}

QSharedPointer<ISD::GenericDevice> findFirstDeviceByInterface(uint32_t interfaceFlag, QString &error)
{
    if (!INDIListener::Instance())
    {
        error = QStringLiteral("INDI listener not available");
        return {};
    }
    const auto matches = INDIListener::devicesByInterface(interfaceFlag);
    if (matches.isEmpty())
    {
        error = QStringLiteral("No connected device matches interface flag 0x%1")
                .arg(interfaceFlag, 0, 16);
        return {};
    }
    return matches.first();
}

} // namespace MCP::Tools
