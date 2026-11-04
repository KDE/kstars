/*
    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flatbrightnesswait.h"

namespace Ekos
{

void FlatBrightnessWait::startRequest(qint64 now, qint64 settleTime, qint64 confirmationTimeout)
{
    reset();
    m_Active              = true;
    m_SettleTime          = qMax<qint64>(0, settleTime);
    m_ConfirmationTimeout = qMax<qint64>(0, confirmationTimeout);
    m_OverallDeadline     = now + m_ConfirmationTimeout + m_SettleTime;

    // The settle time only means something once the light box has applied the
    // new value, so first wait for it to say so.
    waitForConfirmation(now);
}

void FlatBrightnessWait::startWithoutRequest(qint64 now, IPState currentState, qint64 settleTime,
        qint64 confirmationTimeout)
{
    reset();
    m_Active              = true;
    m_SettleTime          = qMax<qint64>(0, settleTime);
    m_ConfirmationTimeout = qMax<qint64>(0, confirmationTimeout);
    m_OverallDeadline     = now + m_ConfirmationTimeout + m_SettleTime;

    // Nothing was requested, so there is nothing to confirm, unless the light box
    // is busy changing its brightness on its own right now.
    if (currentState == IPS_BUSY)
        waitForConfirmation(now);
    else
        settleFrom(now);
}

void FlatBrightnessWait::reset()
{
    m_Active               = false;
    m_Confirmed            = false;
    m_SettleEnd            = 0;
    m_ConfirmationDeadline = 0;
    m_OverallDeadline      = 0;
    m_LastValue            = -1;
    m_LastState            = IPS_IDLE;
}

FlatBrightnessWait::Result FlatBrightnessWait::report(qint64 now, int value, IPState state)
{
    if (!m_Active)
        return RESULT_WAIT;

    // A report that only repeats the previous one carries no news. Restarting the
    // settle time for it could postpone the next frame forever with a driver that
    // publishes the brightness on every poll.
    if (value == m_LastValue && state == m_LastState)
        return RESULT_WAIT;
    m_LastValue = value;
    m_LastState = state;

    switch (state)
    {
        case IPS_ALERT:
            m_Active = false;
            return RESULT_FAILED_REJECTED;

        case IPS_BUSY:
            // Still on its way. If the light box starts changing again after it had
            // confirmed, it has to confirm again. While it is busy, the deadline for
            // the confirmation is not extended, and the overall deadline bounds a
            // light box that keeps switching between busy and confirmed.
            if (m_Confirmed)
                waitForConfirmation(now);
            return RESULT_WAIT;

        case IPS_OK:
        case IPS_IDLE:
            // The brightness has been applied. The light gets its full settle time
            // from this moment on.
            m_Confirmed = true;
            settleFrom(now);
            return RESULT_WAIT;
    }

    return RESULT_WAIT;
}

FlatBrightnessWait::Result FlatBrightnessWait::check(qint64 now)
{
    if (!m_Active)
        return RESULT_WAIT;

    // However the light box reports, the wait ends at the overall deadline: with the
    // brightness confirmed the frame is captured, without it the wait has failed.
    if (now >= m_OverallDeadline)
    {
        m_Active = false;
        return m_Confirmed ? RESULT_CAPTURE : RESULT_FAILED_NO_CONFIRMATION;
    }

    if (m_Confirmed)
    {
        if (now < m_SettleEnd)
            return RESULT_WAIT;

        m_Active = false;
        return RESULT_CAPTURE;
    }

    if (now < m_ConfirmationDeadline)
        return RESULT_WAIT;

    m_Active = false;
    return RESULT_FAILED_NO_CONFIRMATION;
}

void FlatBrightnessWait::waitForConfirmation(qint64 now)
{
    m_Confirmed            = false;
    m_ConfirmationDeadline = now + m_ConfirmationTimeout;
}

void FlatBrightnessWait::settleFrom(qint64 now)
{
    m_Confirmed = true;
    m_SettleEnd = now + m_SettleTime;
}

} // namespace Ekos
