/*
    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <indiapi.h>

#include <QtGlobal>

namespace Ekos
{

/**
 * @class FlatBrightnessWait
 * @short Decides when the next flat calibration frame may be captured after the
 * brightness of a light box was changed.
 *
 * After a brightness request, an INDI driver confirms the new value with the state
 * IPS_OK, or rejects it with IPS_ALERT, possibly after reporting IPS_BUSY while it is
 * on its way. The next frame may only be captured once the light box has confirmed
 * the new brightness and the light has then been given the settle time. A light box
 * that never confirms is treated as a failure instead of being measured at an
 * unknown brightness. The whole wait is bounded by the confirmation timeout plus the
 * settle time, so a light box that keeps switching between busy and confirmed, or
 * keeps reporting new values, cannot postpone the next frame forever.
 *
 * The class has no timer and no device of its own: the caller feeds it the reports
 * of the light box together with the current time, and asks it again when the
 * deadline it returns has passed. That keeps it free of the event loop and makes
 * every case testable.
 *
 * All times are in milliseconds on one monotonic clock chosen by the caller.
 */
class FlatBrightnessWait
{
    public:
        /**
         * @brief What the caller should do next.
         */
        typedef enum
        {
            RESULT_WAIT,                   /*!< keep waiting until @see deadline() */
            RESULT_CAPTURE,                /*!< the light has settled, capture the next frame */
            RESULT_FAILED_REJECTED,        /*!< the light box reported an error (IPS_ALERT) */
            RESULT_FAILED_NO_CONFIRMATION  /*!< the light box did not confirm in time */
        } Result;

        /** Default time a light box gets to confirm a brightness, in milliseconds. */
        static constexpr qint64 DEFAULT_CONFIRMATION_TIMEOUT = 30000;

        /**
         * Time the light gets after the light box confirmed a brightness, in milliseconds.
         * Some drivers confirm a brightness before the light itself has settled.
         */
        static constexpr qint64 DEFAULT_SETTLE_TIME = 1000;

        FlatBrightnessWait() = default;

        /**
         * @brief startRequest start waiting after a new brightness has been sent to
         * the light box. The settle time only starts once the light box confirms it.
         * @param now current time
         * @param settleTime time the light gets after the confirmation
         * @param confirmationTimeout time the light box gets to confirm
         */
        void startRequest(qint64 now, qint64 settleTime,
                          qint64 confirmationTimeout = DEFAULT_CONFIRMATION_TIMEOUT);

        /**
         * @brief startWithoutRequest start waiting before the first calibration frame, or
         * before a frame that is taken again, when nothing has been requested but the light
         * box may still be changing its brightness on its own, e.g. to apply a per-filter
         * preset.
         * @param now current time
         * @param currentState state of the brightness property right now. IPS_BUSY means
         *        the light box is still changing and has to confirm first.
         * @param settleTime time the light gets before the first frame
         * @param confirmationTimeout time the light box gets to confirm, if it is busy
         */
        void startWithoutRequest(qint64 now, IPState currentState, qint64 settleTime,
                                 qint64 confirmationTimeout = DEFAULT_CONFIRMATION_TIMEOUT);

        /**
         * @brief reset stop waiting.
         */
        void reset();

        /**
         * @brief isActive true while waiting.
         */
        bool isActive() const
        {
            return m_Active;
        }

        /**
         * @brief isConfirmed true once the light box confirmed the brightness and the
         * settle time is running.
         */
        bool isConfirmed() const
        {
            return m_Confirmed;
        }

        /**
         * @brief report feed a brightness report of the light box.
         * @param now current time
         * @param value brightness reported by the light box
         * @param state state of the brightness property
         * @return RESULT_FAILED_REJECTED on IPS_ALERT, RESULT_WAIT otherwise. Ask again
         *         at the new @see deadline().
         */
        Result report(qint64 now, int value, IPState state);

        /**
         * @brief check ask whether waiting is over.
         * @param now current time
         * @return RESULT_CAPTURE once the settle time after the confirmation has passed,
         *         RESULT_FAILED_NO_CONFIRMATION if the light box did not confirm in time,
         *         RESULT_WAIT otherwise.
         */
        Result check(qint64 now);

        /**
         * @brief deadline the time at which @see check() has to be called next.
         */
        qint64 deadline() const
        {
            return qMin(m_Confirmed ? m_SettleEnd : m_ConfirmationDeadline, m_OverallDeadline);
        }

    private:
        void waitForConfirmation(qint64 now);
        void settleFrom(qint64 now);

        bool m_Active { false };
        bool m_Confirmed { false };
        qint64 m_SettleTime { 0 };
        qint64 m_ConfirmationTimeout { DEFAULT_CONFIRMATION_TIMEOUT };
        qint64 m_SettleEnd { 0 };
        qint64 m_ConfirmationDeadline { 0 };
        // The wait never lasts longer than the confirmation timeout plus the settle
        // time, however the light box reports.
        qint64 m_OverallDeadline { 0 };

        // The last report seen during this wait. Some drivers publish the brightness
        // on every poll, and a report that only repeats the previous one carries no
        // news.
        int m_LastValue { -1 };
        IPState m_LastState { IPS_IDLE };
};

} // namespace Ekos
