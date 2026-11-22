/*
    KStars UI test: a scheduler job resumes its capture sequence after guiding aborted it

    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

#if defined(HAVE_INDI)

#include <QMap>
#include <QObject>
#include <QStringList>

/**
 * @brief Runs a scheduler job with a guide step and a three row sequence on the INDI simulators (Telescope, CCD and
 * Guide Simulator, internal guider), aborts guiding during the third row and checks that the scheduler resumes the
 * sequence after it restarted guiding: the first two rows are not captured again.
 */
class TestEkosSchedulerGuideRecovery : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosSchedulerGuideRecovery(QObject *parent = nullptr);

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        void testGuideAbortResumesSequence();

    private:
        TestEkosCaptureHelper *m_CaptureHelper { nullptr };
        // captured light frames per filter
        QMap<QString, int> m_Frames;
        QStringList m_SchedulerLog;
        Ekos::SchedulerState m_SchedulerState { Ekos::SCHEDULER_IDLE };

        bool setupGuiding();
        bool setupSchedulerJob(const QString &sequenceFile);
        QString framesSummary() const;
};

#endif // HAVE_INDI
