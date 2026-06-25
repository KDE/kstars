/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekostools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/ekos.h"
#include "indi/indilistener.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

// ---------------------------------------------------------------------------
// Helper: Ekos::CommunicationStatus  →  string
// ---------------------------------------------------------------------------
static QString commStatusString(Ekos::CommunicationStatus s)
{
    switch (s)
    {
        case Ekos::Idle:
            return QStringLiteral("Idle");
        case Ekos::Pending:
            return QStringLiteral("Pending");
        case Ekos::Success:
            return QStringLiteral("Connected");
        case Ekos::Error:
            return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
// Helper: ekos status string (reuses same enum, different labels for ekos)
// ---------------------------------------------------------------------------
static QString ekosStatusString(Ekos::CommunicationStatus s)
{
    switch (s)
    {
        case Ekos::Idle:
            return QStringLiteral("Idle");
        case Ekos::Pending:
            return QStringLiteral("Pending");
        case Ekos::Success:
            return QStringLiteral("Success");
        case Ekos::Error:
            return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
void initEkosTools(MCP::ToolRegistry *registry, Ekos::Manager *manager)
{
    // -----------------------------------------------------------------------
    // ekos_status
    // -----------------------------------------------------------------------
    registry->registerTool(
    {
        QStringLiteral("ekos_status"),
        QStringLiteral("Return the current INDI/Ekos connection status, active profile, and connected device names."),
        {},
        [manager](const QJsonObject &, QString &) -> QJsonValue
        {
            QJsonArray devices;
            if (INDIListener::Instance())
            {
                for (const auto &dev : INDIListener::Instance()->getDevices())
                    devices.append(dev->getDeviceName());
            }

            return QJsonObject
            {
                { QStringLiteral("indi"),    commStatusString(manager->indiStatus()) },
                { QStringLiteral("ekos"),    ekosStatusString(manager->ekosStatus()) },
                { QStringLiteral("profile"), manager->getCurrentProfile() },
                { QStringLiteral("devices"), devices }
            };
        },
        QStringLiteral("Ekos Status"),
        false,
        false,
        false,
        false
    });

    registry->classify(QStringLiteral("ekos_status"), /*ro*/true, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
