// SPDX-FileCopyrightText: 2026 Chris Madson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <indiapi.h>

namespace ISD
{
typedef enum { PARK_UNKNOWN, PARK_PARKED, PARK_PARKING, PARK_UNPARKING, PARK_UNPARKED, PARK_ERROR } ParkStatus;

/** The cached switch state and the notification owed by a Park property update. */
struct ParkStateUpdate
{
    ParkStatus status;
    bool recoveryPending;
    bool notify;
    ParkStatus notification;
};

/** Apply one valid TELESCOPE_PARK update, independently of GUI side effects. */
ParkStateUpdate updateParkState(ParkStatus previous, bool recoveryPending, IPState state, bool parkSwitchOn);

/** Shared by Mount::isParked() and the Park-state regression. */
constexpr bool isParkedState(ParkStatus state)
{
    return state == PARK_PARKED;
}
}
