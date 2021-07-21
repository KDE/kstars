/*  KStars scheduler operations tests
    Copyright (C) 2021
    Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef TESTEKOSSCHEDULEROPS_H
#define TESTEKOSSCHEDULEROPS_H

#include "config-kstars.h"
#include "ekos/scheduler/schedulerjob.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QtTest>

namespace Ekos
{
class Scheduler;
class MockFocus;
class MockMount;
class MockCapture;
class MockAlign;
class MockGuide;
class MockEkos;
}
class KStarsDateTime;
class GeoLocation;

struct StartupCondition
{
    SchedulerJob::StartupCondition type;
    int culminationOffset;
    QDateTime atLocalDateTime;  // This is in local time, not universal time.
};

class TestEkosSchedulerOps : public QObject
{
        Q_OBJECT

    public:
        explicit TestEkosSchedulerOps(QObject *parent = nullptr);

    private slots:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        void testBasics();
        void testSimpleJob();
        void testTimeZone();
        void testDawnShutdown();
        void testCulminationStartup();
        void testFixedDateStartup();

        // test data
        void testCulminationStartup_data();

    protected:
        void prepareTestData(QList<QString> locationList, QList<QString> targetList);

    private:
        bool iterateScheduler(const QString &label, int iterations, int *sleepMs,
                              KStarsDateTime* testUTime,
                              std::function<bool ()> fcn);

        void initScheduler(const GeoLocation &geo, const QDateTime &startUTime, QTemporaryDir *dir,
                           const QString &eslContents, const QString &esqContents);

        void initJob(const KStarsDateTime &startUTime, const KStarsDateTime &jobStartUTime);

        void startupJob(
            const GeoLocation &geo, const QDateTime &startUTime,
            QTemporaryDir *dir, const QString &eslContents, const QString &esqContents,
            const QDateTime &wakeupTime, KStarsDateTime* endTestUTime, int *endSleepMs);
        void disableSkyMap();

        QSharedPointer<Ekos::Scheduler> scheduler;
        QSharedPointer<Ekos::MockFocus> focuser;
        QSharedPointer<Ekos::MockMount> mount;
        QSharedPointer<Ekos::MockCapture> capture;
        QSharedPointer<Ekos::MockAlign> align;
        QSharedPointer<Ekos::MockGuide> guider;
        QSharedPointer<Ekos::MockEkos> ekos;

        StartupCondition m_startupCondition;
        QElapsedTimer testTimer;
};

#endif // HAVE_INDI
#endif // TESTEKOSSCHEDULEROPS_H
