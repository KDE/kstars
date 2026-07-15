/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSharedPointer>
#include <QString>
#include <cstdint>

namespace ISD
{
class GenericDevice;
}

namespace MCP::Tools
{

// Find a connected INDI device by exact name. Returns an empty pointer and
// populates `error` if no device matches.
QSharedPointer<ISD::GenericDevice> findDeviceByName(const QString &name, QString &error);

// Find the first connected INDI device whose driver interface bit-mask
// includes the given flag (e.g. CCD_INTERFACE, FOCUSER_INTERFACE). Returns
// an empty pointer and populates `error` if no such device is present.
QSharedPointer<ISD::GenericDevice> findFirstDeviceByInterface(uint32_t interfaceFlag, QString &error);

} // namespace MCP::Tools
