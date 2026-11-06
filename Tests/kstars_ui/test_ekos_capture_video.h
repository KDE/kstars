/*  KStars UI tests
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include "test_ekos_capture_helper.h"

#include <QObject>
#include <QTest>

/**
 * @brief Video (SER) recording tests with the CCD Simulator.
 *
 * Kept apart from TestEkosCapture because the recordings run in real time for several minutes.
 */
class TestEkosCaptureVideo : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosCaptureVideo(QObject *parent = nullptr);

    private:
        TestEkosCaptureHelper *m_CaptureHelper = new TestEkosCaptureHelper();

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        /**
         * @brief Test that a SER video recording lasting longer than the capture timeout threshold
         * is not interrupted by the exposure watchdog (bug 528290). Runs for about three and a half minutes.
         */
        void testCaptureVideoLongRecording();
};

#endif // HAVE_INDI
