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

namespace Tools
{

void initFocusTools(ToolRegistry *registry, Ekos::Manager *manager);

} // namespace Tools
} // namespace MCP
