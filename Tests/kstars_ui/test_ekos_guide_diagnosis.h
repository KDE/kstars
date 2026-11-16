/*
    KStars UI tests for the Guide failure diagnosis

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"

#if defined(HAVE_INDI)

#include <QJsonObject>
#include <QObject>

/**
 * @brief Tests the Guide failure reasons and the diagnosis built from them, with the INDI simulators and the
 * internal guider.
 *
 * - A mount whose RA axis does not move (Telescope Simulator RA guide rate 0) fails calibration with "RA drift too
 *   short", and the diagnosis points at the RA axis of the mount first.
 * - Raising the calibration pulse from the diagnosis changes the option and starts calibration again.
 * - Stopping a calibration by hand is not a failure and gets no diagnosis.
 */
class TestEkosGuideDiagnosis : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosGuideDiagnosis(QObject *parent = nullptr);

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        void testRaDriftTooShort();
        void testStopGetsNoDiagnosis();

    private:
        TestEkosHelper *m_Helper { nullptr };
        QList<QJsonObject> m_Reports;
        QList<QJsonObject> m_Updates;
        QList<Ekos::GuideState> m_GuideStates;
        QStringList m_GuideLog;

        /** Set the W/E guide rate of the Telescope Simulator, in multiples of sidereal. */
        bool setRaGuideRate(double rate);
        /** Start calibration from scratch with short exposures and few iterations. */
        bool startCalibration(int pulse = 500);
        QStringList lastReportItems() const;
};

#endif // HAVE_INDI
