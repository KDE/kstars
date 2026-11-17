/*
    KStars UI tests for the Align failure diagnosis

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

#if defined(HAVE_INDI)

#include <QJsonObject>
#include <QObject>

/**
 * @brief Tests the Align failure reasons and the diagnosis built from them, with the INDI simulators.
 *
 * - A parked mount fails Align before it captures, and the diagnosis unparks it.
 * - Disabled image transfer fails Align without a blocking dialog; enabling it from the diagnosis
 *   retries Align, and the success reports the fix.
 * - A user abort records the reason but gets no diagnosis.
 * - A solve without index files reports the index files to install, with the measured frame.
 * - Align warns when the mount does not track, and the diagnosis switches tracking on.
 * - Align waits for a slow dome move, a busy focus module or a turning rotator without restarting the camera driver.
 */
class TestEkosAlignDiagnosis : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosAlignDiagnosis(QObject *parent = nullptr);

    protected Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

    private Q_SLOTS:
        /** AG5: a parked mount fails Align before capturing; mount_unpark unparks it. */
        void testParkedMount();

        /** AG17: disabled image transfer fails Align without a modal dialog; the fix retries and succeeds. */
        void testImageTransferDisabled();

        /** AG14: a user abort is recorded as such and gets no diagnosis. */
        void testUserAbort();

        /** AG1, AG3, AG13: a solve without index files offers the index files for the field of view. */
        void testNoIndexFiles();

        /** AL4: Align warns about a mount that does not track; mount_tracking_on switches tracking on. */
        void testTrackingOff();

        /** Align waits for a moving dome without counting camera timeouts, so the camera driver is not restarted. */
        void testDomeMotionIsNotCameraTimeout();

        /** Align waits for a busy focus module without counting camera timeouts. */
        void testFocusBusyIsNotCameraTimeout();

        /** Align waits for a turning rotator without counting camera timeouts. */
        void testRotatorBusyIsNotCameraTimeout();

    private:
        TestEkosCaptureHelper *m_CaptureHelper = nullptr;

        Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };
        QList<QJsonObject> m_Reports;
        QList<QJsonObject> m_Updates;
        QStringList m_AlignLog;
        QStringList m_IndexFolders;

        bool waitForUpdate(const QString &itemId, const QString &state, int timeout);
        QString lastReportId() const;
        QStringList lastReportItems() const;
};

#endif // HAVE_INDI
