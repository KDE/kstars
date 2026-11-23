// SPDX-FileCopyrightText: 2026 Chris Madson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "indiparkstate.h"

namespace ISD
{
ParkStateUpdate updateParkState(ParkStatus previous, bool recoveryPending, IPState state, bool parkSwitchOn)
{
    switch (state)
    {
        case IPS_ALERT:
            // Preserve the switch-based reset from 24654a40 and notify consumers of the error.
            return {parkSwitchOn ? PARK_PARKED : PARK_UNPARKED, true, true, PARK_ERROR};
        case IPS_BUSY:
        {
            const auto status = parkSwitchOn ? PARK_PARKING : PARK_UNPARKING;
            return {status, false, previous != status, status};
        }
        case IPS_OK:
        case IPS_IDLE:
        {
            const auto status = parkSwitchOn ? PARK_PARKED : PARK_UNPARKED;
            return {status, false, previous != status || recoveryPending, status};
        }
    }
    return {previous, recoveryPending, false, previous};
}
}
