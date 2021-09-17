/*  KStars scheduler operations tests
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        void testArtificialHorizonConstraints();

        // test data
        void testCulminationStartup_data();

    protected:
        void prepareTestData(QList<QString> locationList, QList<QString> targetList);
        void runSimpleJob(const GeoLocation &geo, const SkyObject *targetObject, const QDateTime &startUTime,
                          const QDateTime &wakeupTime, bool enforceArtificialHorizon);
        void runDawnShutdown(const GeoLocation &geo, const SkyObject *targetObject,
                             const QDateTime &startUTime, const QDateTime &preDawnUTime);

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
