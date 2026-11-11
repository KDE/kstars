/*
    KStars UI tests for the slews of the alignment
    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_capture_helper.h"

#if defined(HAVE_INDI)

#include <QObject>

class TestEkosAlignSlew : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosAlignSlew(QObject *parent = nullptr);

    protected:
        TestEkosCaptureHelper *m_CaptureHelper { nullptr };
        // align states emitted during a test
        QList<Ekos::AlignState> m_AlignStates;

    protected Q_SLOTS:
        void alignStatusChanged(Ekos::AlignState status);

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        void testSlewEndsIdle_data();
        void testSlewEndsIdle();
};

#endif // HAVE_INDI
