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

#include "indi/indiweather.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

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

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void init();
        void cleanup();

        void testBasics();
        void testKeepOpticalTrainSelection();
        void testSimpleJob();
        void testTimeZone();
        void testDawnShutdown();
        void testPreemptiveShutdown();
        void testPreemptiveShutdownTimerSwitchOnQueueComplete();

        // Weather soft-shutdown tests driven through the mock modules and a simulated
        // clock, so the scheduler's 5-minute monitoring wakeups run in milliseconds.
        void testWeatherMonitoringWakeupDuringAlert();
        void testWeatherAlertOutlastsJobEndTime();
        void testWeatherClearsAfterJobEndTime();
        void testWeatherAlertThroughDawn();
        void testWeatherAlertBeforeFirstJobCheck();

        // Weather tests with the INDI simulators are in test_ekos_scheduler_weather.

        void testTwilightStartup();
        void testTwilightStartup_data();
        void testArtificialHorizonConstraints();
        void testGreedySchedulerRun();
        void testRememberJobProgress();
        void testGreedy();
        void testMaxMoonAltitude();
        void testGroups();
        void testArtificialCeiling();
        void testGreedyAborts();
        void testSettingAltitudeBug();
        void testEstimateTimeBug();
        void testGreedyMessier();
        void testGreedyStartAt();

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
                        KStarsDateTime &currentUTime, int &sleepMs, int tolerance, const QString &label = "",
                        const QDateTime &captureCompleteUTime = QDateTime());
        void parkAndSleep(KStarsDateTime &testUTime, int &sleepMs);
        void startWeatherTestJob(int gracePeriodMinutes, const TestEkosSchedulerHelper::CompletionCondition &completion,
                                 KStarsDateTime &currentUTime, int &sleepMs, QTemporaryDir &dir);
        void injectWeather(ISD::Weather::Status status);
        bool iterateDuringWeatherHold(const QString &label, int iterations, int &sleepMs, KStarsDateTime &currentUTime,
                                      std::function<bool ()> done);
        void wakeupAndRestart(const QDateTime &restartTime, KStarsDateTime &testUTime, int &sleepMs);


    private:
        bool iterateScheduler(const QString &label, int iterations, int *sleepMs,
                              KStarsDateTime* testUTime,
                              std::function<bool ()> fcn,
                              const QDateTime &captureCompleteUTime = QDateTime());

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
        void startupJobs2(
            const QDateTime &startUTime, const QDateTime &wakeupTime, KStarsDateTime &endTestUTime, int &endSleepMs);
        void startupJob(
            const GeoLocation &geo, const QDateTime &startUTime,
            QTemporaryDir *dir, const QString &esl, const QString &esq,
            const QDateTime &wakeupTime, KStarsDateTime &endTestUTime, int &endSleepMs);
        void startModules(KStarsDateTime &testUTime, int &sleepMs);

        void disableSkyMap();
        int timeTolerance(int seconds);
        bool checkLastSlew(const SkyObject* targetObject);
        void printJobs(const QString &label);
        void loadGreedySchedule(bool first, const QString &targetName,
                                const TestEkosSchedulerHelper::StartupCondition &startupCondition,
                                const TestEkosSchedulerHelper::CompletionCondition &completionCondition,
                                QTemporaryDir &dir, const QVector<TestEkosSchedulerHelper::CaptureJob> &captureJob, int minAltitude = 30,
                                double maxMoonAltitude = 90.0,
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

        // Scheduler log lines received since the last weather test job started.
        QStringList m_schedulerLog;
        // Mount slews issued before a weather alert, and whether iterateDuringWeatherHold()
        // saw the scheduler run a job, the post-startup phase or a slew while it should hold.
        int m_weatherSlewBaseline { 0 };
        QString m_weatherHoldViolation;
};

#endif // HAVE_INDI
#endif // TESTEKOSSCHEDULEROPS_H
