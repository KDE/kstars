/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP
{
class ToolRegistry;

namespace Tools
{

// Registers curated focuser tools on the given registry:
//   focuser_status, focuser_move_absolute, focuser_move_relative,
//   focuser_abort_move
//
// These talk directly to the ISD::Focuser device — distinct from the
// focus_* tools which go through the Ekos Focus autofocus state machine.
// Do not call these while autofocus is running; they will fight the
// autofocus algorithm for control of the focuser.
void initFocuserTools(ToolRegistry *registry);

} // namespace Tools
} // namespace MCP
