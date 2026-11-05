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
