/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace Ekos
{
class Manager;
}
namespace MCP
{
class ToolRegistry;
}

namespace MCP::Tools
{

/**
 * @brief initEkosTools Register Ekos status tools into the tool registry.
 *
 * Registers:
 *  - ekos_status   Report INDI/Ekos connection status, active profile, and
 *                  connected device names. Read-only.
 */
void initEkosTools(MCP::ToolRegistry *registry, Ekos::Manager *manager);

} // namespace MCP::Tools
