/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP
{
class ToolRegistry;
}
namespace Ekos
{
class Manager;
}

namespace MCP::Tools
{

/**
 * @brief Register all capture-related MCP tools into the given registry.
 *
 * Tools registered:
 *  - capture_status
 *  - capture_get_jobs
 *  - capture_start
 *  - capture_stop
 *  - capture_abort
 *  - capture_suspend
 *  - capture_pause
 *  - capture_resume
 *  - capture_load_sequence
 *  - capture_set_target
 */
void initCaptureTools(MCP::ToolRegistry *registry, Ekos::Manager *manager);

} // namespace MCP::Tools
