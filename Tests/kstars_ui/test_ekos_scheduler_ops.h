/*  KStars scheduler operations tests
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTEKOSSCHEDULEROPS_H
#define TESTEKOSSCHEDULEROPS_H

#include "config-kstars.h"
#include "ekos/scheduler/schedulerjob.h"
#include "test_ekos_scheduler_helper.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTest>

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
        void testTwilightStartup();
        void testTwilightStartup_data();
        void testCulminationStartup();
        void testFixedDateStartup();
        void testArtificialHorizonConstraints();
        void testGreedySchedulerRun();
        void testRememberJobProgress();
        void testGreedy();
        void testArtificialCeiling();
        void testGreedyAborts();
        void testSettingAltitudeBug();
        void testEstimateTimeBug();
        void testGreedyMessier();

        // test data
        void testCulminationStartup_data();
        void testRememberJobProgress_data();

    protected:
        void prepareTestData(QList<QString> locationList, QList<QString> targetList);
        void runSimpleJob(const GeoLocation &geo, const SkyObject *targetObject, const QDateTime &startUTime,
                          const QDateTime &wakeupTime, bool enforceArtificialHorizon);
        void startup(const GeoLocation &geo, const QVector<SkyObject*> targetObjects,
                     const QDateTime &startSchedulerUTime, KStarsDateTime &currentUTime, int &sleepMs, QTemporaryDir &dir);
        void slewAndRun(SkyObject *object, const QDateTime &startUTime, const QDateTime &interruptUTime,
                        KStarsDateTime &currentUTime, int &sleepMs, int tolerance, const QString &label = "");
        void parkAndSleep(KStarsDateTime &testUTime, int &sleepMs);
        void wakeupAndRestart(const QDateTime &restartTime, KStarsDateTime &testUTime, int &sleepMs);


    private:
        bool iterateScheduler(const QString &label, int iterations, int *sleepMs,
                              KStarsDateTime* testUTime,
                              std::function<bool ()> fcn);

        void initScheduler(const GeoLocation &geo, const QDateTime &startUTime, QTemporaryDir *dir,
                           const QVector<QString> &eslContents, const QVector<QString> &esqContents);
        void initTimeGeo(const GeoLocation &geo, const QDateTime &startUTime);
        void initFiles(QTemporaryDir *dir, const QVector<QString> &esls, const QVector<QString> &esqs);
        QString writeFiles(const QString &label, QTemporaryDir &dir,
                           const QVector<TestEkosSchedulerHelper::CaptureJob> &captureJob,
                           const QString &schedulerXML);

        void initJob(const KStarsDateTime &startUTime, const KStarsDateTime &jobStartUTime);

        void startupJobs(
            const GeoLocation &geo, const QDateTime &startUTime,
            QTemporaryDir *dir, const QVector<QString> &esls, const QVector<QString> &esqs,
            const QDateTime &wakeupTime, KStarsDateTime &endTestUTime, int &endSleepMs);
        void startupJob(
            const GeoLocation &geo, const QDateTime &startUTime,
            QTemporaryDir *dir, const QString &esl, const QString &esq,
            const QDateTime &wakeupTime, KStarsDateTime &endTestUTime, int &endSleepMs);
        void startModules(KStarsDateTime &testUTime, int &sleepMs);

        void disableSkyMap();
        int timeTolerance(int seconds);
        bool checkLastSlew(const SkyObject* targetObject);
        void printJobs(const QString &label);
        void loadGreedySchedule(
            bool first, const QString &targetName,
            const TestEkosSchedulerHelper::StartupCondition &startupCondition,
            const TestEkosSchedulerHelper::CompletionCondition &completionCondition,
            QTemporaryDir &dir, const QVector<TestEkosSchedulerHelper::CaptureJob> &captureJob, int minAltitude = 30,
            const TestEkosSchedulerHelper::ScheduleSteps steps = {true, true, true, true}, bool enforceTwilight = true,
            bool enforceHorizon = true, int errorDelay = 0);
        void makeFitsFiles(const QString &base, int num);

        QSharedPointer<Ekos::Scheduler> scheduler;
        QSharedPointer<Ekos::MockFocus> focuser;
        QSharedPointer<Ekos::MockMount> mount;
        QSharedPointer<Ekos::MockCapture> capture;
        QSharedPointer<Ekos::MockAlign> align;
        QSharedPointer<Ekos::MockGuide> guider;
        QSharedPointer<Ekos::MockEkos> ekos;

        TestEkosSchedulerHelper::StartupCondition m_startupCondition;
        TestEkosSchedulerHelper::CompletionCondition m_completionCondition;
        QElapsedTimer testTimer;
};

#endif // HAVE_INDI
#endif // TESTEKOSSCHEDULEROPS_H
