/*
    KStars UI tests for the GPG period of the guide module
    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"

#if defined(HAVE_INDI)

#include <QObject>

class TestEkosGuideGPGPeriod : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosGuideGPGPeriod(QObject *parent = nullptr);

    protected:
        TestEkosHelper *m_Helper { nullptr };

        /**
         * @brief Store the guide settings of an optical train, as the guide module saves them
         */
        bool storeGuideSettings(const QString &train, const QVariantMap &settings);

        /**
         * @brief Select an optical train in the guide module, which loads its stored settings
         */
        bool selectGuideTrain(const QString &train);

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void testPeriodOnTrainLoad_data();
        void testPeriodOnTrainLoad();
        void testPeriodOnEkosRestart();
};

#endif // HAVE_INDI
