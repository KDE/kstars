/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonObject>
#include <QList>

#include "ekos/ekos.h"

namespace Ekos
{
class Manager;
}

namespace MCP
{

class ToolRegistry;

namespace Tools
{

void initAlignTools(ToolRegistry *registry, Ekos::Manager *manager);

// Build the align_result JSON payload from raw solver outputs. Exposed for
// regression testing of the unit (degrees → hours) and epoch (J2000 → JNow)
// conversions, plus the error/accuracy fields.
//
// solution:   [orientation_deg, ra_deg_j2000, dec_deg_j2000] as returned by
//             Ekos::Align::getSolutionResult().
// fov:        [width, height, pixscale_arcsec_per_pixel] as returned by
//             Ekos::Align::fov(); pixscale is read from index 2 if present.
// alignError: {total, dRA, dDE, dAZ, dAL} in arcsec as returned by
//             Ekos::Align::getAlignErrorResult(). The total is the 1e6 sentinel
//             before the first solve; in that case the numeric error fields are
//             omitted and "error_available" is false.
// accuracyArcsec: the alignment accuracy threshold (arcsec) from
//             Ekos::Align::getAccuracyThreshold(); drives within_accuracy and the
//             green/yellow/red accuracy_status, mirroring the Align UI exactly.
//
// Returns {"available": false} when the solution is missing or invalid.
// Otherwise returns a payload with ra/dec in JNow (hours/degrees) and
// ra_j2000/dec_j2000 in J2000 (hours/degrees). The JNow conversion uses
// the current KStarsData clock, so callers must ensure KStarsData is
// initialised before invoking this in production code.
QJsonObject makeAlignResultPayload(const QList<double> &solution, const QList<double> &fov,
                                   const QList<double> &alignError = {}, double accuracyArcsec = 0.0);

// True when the align module is mid-operation. Defined as the negation of the
// terminal-state set {ALIGN_IDLE, ALIGN_COMPLETE, ALIGN_FAILED, ALIGN_ABORTED}
// so it stays correct if new non-terminal states are added later. This is the
// done/ongoing signal MCP clients poll on so they never interpret the status
// enum themselves; in particular ALIGN_SUCCESSFUL is a per-solve transient
// during the slew_to_target convergence loop and is reported busy=true.
// Exposed for testing (the handler reads a live Ekos::Align::status()).
bool alignBusy(Ekos::AlignState state);

} // namespace Tools
} // namespace MCP
