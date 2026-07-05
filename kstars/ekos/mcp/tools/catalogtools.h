/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP
{
class ToolRegistry;
}

namespace MCP::Tools
{

/**
 * @brief initCatalogTools Register KStars catalog search tools.
 *
 * Registers:
 *  - catalog_search  Look up sky objects by name (exact or substring),
 *                    optionally filtered by SkyObject type. Returns canonical
 *                    name, type, J2000 RA/Dec, and visual magnitude for each
 *                    match. Designed to resolve fuzzy queries ("M42",
 *                    "Polaris") into the canonical names that
 *                    mount_goto_target expects ("M 42", "Polaris").
 */
void initCatalogTools(MCP::ToolRegistry *registry);

} // namespace MCP::Tools
