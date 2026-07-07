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
 * @brief initMountTools Register mount control tools into the tool registry.
 *
 * Registers:
 *  - mount_coords             Read current RA/Dec, Alt/Az, HA, pier side, status.
 *  - mount_goto               Slew to RA/Dec (hours / degrees).
 *  - mount_goto_target        Slew to a named object.
 *  - mount_sync               Sync to RA/Dec.
 *  - mount_park               Park the mount.
 *  - mount_unpark             Unpark the mount.
 *  - mount_abort              Abort current motion.
 *  - mount_set_meridian_flip  Configure meridian flip (enabled, hours).
 */
void initMountTools(MCP::ToolRegistry *registry, Ekos::Manager *manager);

} // namespace MCP::Tools
