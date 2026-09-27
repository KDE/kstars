/*  KStars scheduler weather tests with the INDI simulators
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QtTest>

class TestEkosSchedulerWeather : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosSchedulerWeather(QObject *parent = nullptr);

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        // Startup → capture → weather alert → soft shutdown verified (Ekos/INDI still running,
        // mount parked, STARTUP_POST_DEVICES) → WEATHER_OK → post-startup queue runs → job resumes.
        void testWeatherSoftShutdownSimulator();
        // Weather remains bad and the grace period expires, leading to a full hard shutdown.
        void testWeatherHardShutdownSimulator();
        // Weather remains bad and the grace period is 0 (wait indefinitely): monitoring mode.
        void testWeatherMonitoringModeSimulator();
        // Weather clears after dawn: the scheduler sleeps until the job's next window, parked.
        void testWeatherRecoveryAfterDawnRunsShutdown();
};

#endif // HAVE_INDI
