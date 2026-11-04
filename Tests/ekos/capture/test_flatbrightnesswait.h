/*
    Tests for waiting on a light box after a brightness change.

    SPDX-FileCopyrightText: 2026 J.D. de Keijzer <jdk.ssvw+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/capture/flatbrightnesswait.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

/**
 * @brief Tests for Ekos::FlatBrightnessWait, which decides when the next flat
 * calibration frame may be captured after the brightness of a light box changed:
 * only once the light box confirmed the new brightness and the settle time has
 * passed since then. Time is simulated, so no device and no event loop is involved.
 */
class TestFlatBrightnessWait : public QObject
{
        Q_OBJECT
    public:
        explicit TestFlatBrightnessWait();

    private Q_SLOTS:
        /** @brief An immediate OK starts the settle time. */
        void testConfirmedImmediately();
        /** @brief Busy reports do not count, the settle time starts at the OK. */
        void testBusyThenConfirmed();
        /** @brief A late OK is waited for, however short the settle time. */
        void testConfirmationAfterTheSettleTime();
        /** @brief No OK at all stops the calibration at the timeout. */
        void testNoConfirmation();
        /** @brief Busy forever does not extend the timeout. */
        void testBusyForever();
        /** @brief An alert is a rejection. */
        void testRejected();
        /** @brief A long settle time after the OK does not trip the timeout. */
        void testLongSettleTime();
        /** @brief Repeated identical reports do not restart the settle time. */
        void testRepeatedReportsAreIgnored();
        /** @brief A new value while settling restarts the settle time. */
        void testNewValueWhileSettling();
        /** @brief Busy again while settling requires a new confirmation. */
        void testBusyAgainWhileSettling();
        /**
         * @brief A light box that keeps switching between busy and confirmed, or keeps
         * confirming new values, cannot postpone the frame beyond the overall deadline.
         */
        void testOverallDeadline_data();
        void testOverallDeadline();
        /** @brief Before the first frame, an idle light box only gets the settle time. */
        void testWithoutRequestSettlesOnly();
        /** @brief Before the first frame, a busy light box has to confirm first. */
        void testWithoutRequestWhileBusy();
        /** @brief Before the first frame, a preset applied while settling is waited for. */
        void testWithoutRequestPresetApplied();
        /** @brief Nothing happens before a wait was started or after it ended. */
        void testInactive();
        /** @brief The default settle time starts at the confirmation and lasts one second. */
        void testDefaultSettleTime();
};
