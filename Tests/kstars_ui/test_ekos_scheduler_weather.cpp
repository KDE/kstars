/*  KStars scheduler weather tests with the INDI simulators
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// These tests start Ekos with the INDI simulators, including the Weather Simulator, and
// run the real scheduler and task queues through weather soft shutdowns and recovery.
// Each test must run in a process of its own: after one of them, the scheduler of the
// next one fails to schedule its job. CMake registers one test per function for this.
// The same scenarios, and those that need hours of simulated time, are also covered with
// mock modules in test_ekos_scheduler_ops.

#include <QScopeGuard>

#include "../testhelpers.h"
#include "test_ekos_scheduler_weather.h"
#include "test_ekos_scheduler_helper.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/schedulerprocess.h"

#include "skymapcomposite.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"
#include "test_ekos_simulator.h"
#include "Options.h"

TestEkosSchedulerWeather::TestEkosSchedulerWeather(QObject *parent) : QObject(parent)
{
}

void TestEkosSchedulerWeather::initTestCase()
{
    if (KStars::Instance() == nullptr || KStars::Instance()->data() == nullptr ||
            KStars::Instance()->data()->skyComposite()->findByName("Kocab") == nullptr)
        QSKIP("Star catalog not initialized; scheduler weather tests require it.");
}

void TestEkosSchedulerWeather::cleanupTestCase()
{
}

void TestEkosSchedulerWeather::init()
{
    // Same scheduler settings as test_ekos_scheduler_ops, so that the user's configuration
    // does not leak into these tests.
    Options::setRememberJobProgress(false);
    Options::setStopEkosAfterShutdown(true);
    Options::setDitherEnabled(false);
    Options::setPreemptiveShutdown(false);
    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);
    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    Options::setGreedyScheduling(true);
}

void TestEkosSchedulerWeather::cleanup()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// testWeatherSoftShutdownFullCycle
//
// Full end-to-end regression test for the weather soft-shutdown → recovery path.
//
// Bug summary (fixed in wakeUpScheduler()):
//   After a weather alert the pre-shutdown queue runs (parking dome/mount), the
//   scheduler enters a grace-period sleep (Ekos/INDI remain running), and
//   startupState is set to STARTUP_POST_DEVICES as a signal that on wakeup only
//   the post-startup phase (unpark) needs to run.
//
//   The bug: wakeUpScheduler() read weatherGracePeriodActive() to decide whether
//   to trigger the post-startup queue.  But weatherGracePeriodActive() is cleared
//   BEFORE wakeUpScheduler() reaches that check (either by the inner timer-path
//   block, or by setWeatherStatus() in the event-driven path).  The condition was
//   therefore always false — dead code — and startupState was reset to STARTUP_IDLE,
//   discarding the STARTUP_POST_DEVICES signal.  The post-startup queue (unpark)
//   never ran, leaving the observatory parked.
//
//   The fix: capture startupState == STARTUP_POST_DEVICES in a local bool
//   (needsPostStartupRecovery) BEFORE resetting the state, then use that bool to
//   decide whether to run the post-startup queue.
//
// Test flow:
//   1. Full Ekos/INDI mock startup + job running (startup() + startModules()).
//      No startup/shutdown queue procedures during this phase — init() leaves
//      schedulerStartupEnabled = false so checkStartupState(STARTUP_POST_DEVICES)
//      goes directly to STARTUP_COMPLETE without trying to run a queue.
//   2. AFTER the job is capturing, write delay-task queue JSON files to the
//      temp directory and enable startup/shutdown procedures.
//      Using a DELAY task (supported_interfaces = []) means QueueExecutor::start()
//      bypasses the Ekos::Manager::Instance()->indiStatus() check that would
//      otherwise fail in the mock environment (mock modules ≠ real Ekos Manager).
//   3. Weather alert injected → pre-shutdown queue fires (1-second delay task).
//      queueExecutionCompleted() sets shutdownState=SHUTDOWN_COMPLETE and
//      startupState=STARTUP_POST_DEVICES.
//   4. Weather OK injected (event-driven Path B recovery).
//   5. wakeUpScheduler() fires → detects needsPostStartupRecovery=true →
//      post-startup queue runs (1-second delay task) → STARTUP_COMPLETE.
//
// Important: phases 3-5 use QTest::qWait() rather than iterateScheduler()
// because iterateScheduler() would call runSchedulerIteration() every 10 ms,
// which dispatches to wakeUpScheduler() on every tick while timerState=RUN_WAKEUP.
// ─────────────────────────────────────────────────────────────────────────────
void TestEkosSchedulerWeather::testWeatherSoftShutdownSimulator()
{
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();

    TestEkosHelper helper;
    helper.m_MountDevice = "Telescope Simulator";
    helper.m_CCDDevice = "CCD Simulator";
    helper.m_FocuserDevice = "Focuser Simulator";
    helper.m_GuiderDevice = "Guide Simulator";
    helper.m_ExtraDevices = QStringList() << "Weather Simulator";
    QVERIFY(helper.setupEkosProfile("Simulators+Weather", false));

    KTRY_EKOS_START_PROFILE("Simulators+Weather");
    KHACK_RESET_EKOS_TIME();

    Ekos::Scheduler* realScheduler = Ekos::Manager::Instance()->schedulerModule();
    QSharedPointer<Ekos::Scheduler> realSchedulerPtr(realScheduler, [](Ekos::Scheduler*) {});

    // Configure default observatory queues
    QString shutdownFile = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/observatory_shutdown.json");
    QString startupFile = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/observatory_startup.json");

    QVERIFY2(!shutdownFile.isEmpty(), "observatory_shutdown.json not found in KStars data directory");
    QVERIFY2(!startupFile.isEmpty(), "observatory_startup.json not found in KStars data directory");

    realScheduler->process()->moduleState()->setPreShutdownScriptURL(QUrl::fromLocalFile(shutdownFile));
    realScheduler->process()->moduleState()->setPostStartupScriptURL(QUrl::fromLocalFile(startupFile));

    Options::setSchedulerStartupEnabled(true);
    Options::setSchedulerShutdownEnabled(true);

    Options::setSchedulerWeather(true);
    Options::setSchedulerWeatherGracePeriod(30);
    Options::setSchedulerWeatherShutdownDelay(0);

    GeoLocation geo(dms(47, 58), dms(29, 20), "Kuwait", "", "Kuwait", 3);
    KStarsData::Instance()->geo()->setLat(*(geo.lat()));
    KStarsData::Instance()->geo()->setLong(*(geo.lng()));
    KStarsData::Instance()->geo()->setTZ0(geo.TZ0());

    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Kocab");
    TestEkosSchedulerHelper::StartupCondition startupCond;
    startupCond.type = Ekos::START_ASAP;
    TestEkosSchedulerHelper::CompletionCondition completionCond;
    completionCond.type = Ekos::FINISH_LOOP;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    auto schedJob = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {2, 2, "Luminance", "."});
    QString schedulerXML = TestEkosSchedulerHelper::getSchedulerFile(
                               targetObject, startupCond, completionCond, {true, false, false, false}, false, false, 0);

    QString esqFilename = dir.filePath("test.esq");
    QString eslFilename = dir.filePath("test.esl");
    TestEkosSchedulerHelper::writeSimpleSequenceFiles(schedulerXML, eslFilename,
            TestEkosSchedulerHelper::getEsqContent(schedJob), esqFilename);

    realScheduler->load(true, eslFilename);
    realScheduler->moduleState()->jobs()[0]->setSequenceFile(QUrl(QString("file://%1").arg(esqFilename)));

    helper.prepareOpticalTrains();
    helper.prepareCaptureModule();

    // Start scheduler and wait for full startup sequence:
    KTRY_CLICK(realScheduler, startB);

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->activeJob() != nullptr
                             && realScheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING, 120000);

    // Weather alert injection
    QVERIFY2(TestEkosHelper::setSimulatedWeather(true, realSchedulerPtr), "Weather alert not set");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() == ISD::Weather::WEATHER_ALERT, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->parkStatus() == ISD::PARK_PARKED, 20000);

    QVERIFY2(realScheduler->process()->moduleState()->ekosState() == Ekos::EKOS_READY, "Ekos must remain running");
    QVERIFY2(realScheduler->process()->moduleState()->indiState() == Ekos::INDI_READY, "INDI must remain connected");
    QVERIFY2(realScheduler->process()->moduleState()->shutdownState() == Ekos::SHUTDOWN_COMPLETE,
             "shutdownState must be complete");
    QVERIFY2(realScheduler->process()->moduleState()->startupState() == Ekos::STARTUP_POST_DEVICES,
             "startupState must be POST_DEVICES");
    QVERIFY2(realScheduler->process()->moduleState()->preemptiveShutdown() == true, "preemptiveShutdown must be true");
    QVERIFY2(realScheduler->process()->moduleState()->weatherGracePeriodActive() == true,
             "weatherGracePeriodActive must be true");

    // Weather OK injection
    QVERIFY2(TestEkosHelper::setSimulatedWeather(false, realSchedulerPtr), "Weather OK not set");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() == ISD::Weather::WEATHER_OK, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->parkStatus() == ISD::PARK_UNPARKED, 15000);

    QVERIFY2(realScheduler->process()->moduleState()->preemptiveShutdown() == false, "preemptiveShutdown must be false");
    QVERIFY2(realScheduler->process()->moduleState()->shutdownState() == Ekos::SHUTDOWN_IDLE, "shutdownState must be IDLE");
    QVERIFY2(realScheduler->process()->moduleState()->startupState() == Ekos::STARTUP_COMPLETE,
             "startupState must be complete");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->activeJob() != nullptr
                             && realScheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING, 30000);

    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
}

// ─────────────────────────────────────────────────────────────────────────────
// testWeatherHardShutdownSimulator
//
// This test exercises the scenario where a weather alert is injected, the
// scheduler enters a grace period (weather soft-shutdown), but the weather
// remains bad until the grace period expires.  The expected behavior is that
// the scheduler initiates a full hard shutdown (stopping Ekos and disconnecting INDI),
// and stops itself.
// ─────────────────────────────────────────────────────────────────────────────
void TestEkosSchedulerWeather::testWeatherHardShutdownSimulator()
{
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();

    TestEkosHelper helper;
    helper.m_MountDevice = "Telescope Simulator";
    helper.m_CCDDevice = "CCD Simulator";
    helper.m_FocuserDevice = "Focuser Simulator";
    helper.m_GuiderDevice = "Guide Simulator";
    helper.m_ExtraDevices = QStringList() << "Weather Simulator";
    QVERIFY(helper.setupEkosProfile("Simulators+Weather", false));

    KTRY_EKOS_START_PROFILE("Simulators+Weather");
    KHACK_RESET_EKOS_TIME();

    Ekos::Scheduler* realScheduler = Ekos::Manager::Instance()->schedulerModule();
    QSharedPointer<Ekos::Scheduler> realSchedulerPtr(realScheduler, [](Ekos::Scheduler*) {});

    // Configure default observatory queues
    QString shutdownFile = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/observatory_shutdown.json");
    QString startupFile = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/observatory_startup.json");

    QVERIFY2(!shutdownFile.isEmpty(), "observatory_shutdown.json not found in KStars data directory");
    QVERIFY2(!startupFile.isEmpty(), "observatory_startup.json not found in KStars data directory");

    realScheduler->process()->moduleState()->setPreShutdownScriptURL(QUrl::fromLocalFile(shutdownFile));
    realScheduler->process()->moduleState()->setPostStartupScriptURL(QUrl::fromLocalFile(startupFile));

    Options::setSchedulerStartupEnabled(true);
    Options::setSchedulerShutdownEnabled(true);

    Options::setSchedulerWeather(true);
    // Use a very short grace period (1 minute) so we can wait it out in the test
    Options::setSchedulerWeatherGracePeriod(1);
    Options::setSchedulerWeatherShutdownDelay(0);

    GeoLocation geo(dms(47, 58), dms(29, 20), "Kuwait", "", "Kuwait", 3);
    KStarsData::Instance()->geo()->setLat(*(geo.lat()));
    KStarsData::Instance()->geo()->setLong(*(geo.lng()));
    KStarsData::Instance()->geo()->setTZ0(geo.TZ0());

    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Kocab");
    TestEkosSchedulerHelper::StartupCondition startupCond;
    startupCond.type = Ekos::START_ASAP;
    TestEkosSchedulerHelper::CompletionCondition completionCond;
    completionCond.type = Ekos::FINISH_LOOP;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    auto schedJob = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {2, 2, "Luminance", "."});
    QString schedulerXML = TestEkosSchedulerHelper::getSchedulerFile(
                               targetObject, startupCond, completionCond, {true, false, false, false}, false, false, 0);

    QString esqFilename = dir.filePath("test.esq");
    QString eslFilename = dir.filePath("test.esl");
    TestEkosSchedulerHelper::writeSimpleSequenceFiles(schedulerXML, eslFilename,
            TestEkosSchedulerHelper::getEsqContent(schedJob), esqFilename);

    realScheduler->load(true, eslFilename);
    realScheduler->moduleState()->jobs()[0]->setSequenceFile(QUrl(QString("file://%1").arg(esqFilename)));

    helper.prepareOpticalTrains();
    helper.prepareCaptureModule();

    // Start scheduler and wait for full startup sequence:
    KTRY_CLICK(realScheduler, startB);

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->activeJob() != nullptr
                             && realScheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING, 120000);

    // Weather alert injection
    QVERIFY2(TestEkosHelper::setSimulatedWeather(true, realSchedulerPtr), "Weather alert not set");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() == ISD::Weather::WEATHER_ALERT, 10000);
    // The mount should park as part of the soft-shutdown / pre-shutdown queue
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->parkStatus() == ISD::PARK_PARKED, 20000);

    // At this point we are in the grace period. Ekos/INDI are still ready.
    QVERIFY2(realScheduler->process()->moduleState()->ekosState() == Ekos::EKOS_READY,
             "Ekos must remain running during grace period");
    QVERIFY2(realScheduler->process()->moduleState()->indiState() == Ekos::INDI_READY,
             "INDI must remain connected during grace period");

    // Now we wait for the 1 minute grace period to expire. We do not inject WEATHER_OK.
    // The scheduler should detect the expiration, initiate hard shutdown.
    // Because KStars runs time fast we use 90s timeout.

    // Wait until Ekos stops and INDI disconnects
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->ekosState() == Ekos::EKOS_IDLE, 90000);
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->indiState() == Ekos::INDI_IDLE, 30000);

    // Check that scheduler transitions to ABORTED or IDLE state due to weather hard shutdown
    QTRY_VERIFY_WITH_TIMEOUT(
        realScheduler->process()->moduleState()->schedulerState() == Ekos::SCHEDULER_ABORTED ||
        realScheduler->process()->moduleState()->schedulerState() == Ekos::SCHEDULER_IDLE ||
        realScheduler->process()->moduleState()->schedulerState() == Ekos::SCHEDULER_RUNNING,
        10000);

    // Weather monitoring mode should be enabled after hard shutdown
    QVERIFY2(realScheduler->process()->moduleState()->weatherShutdownMonitoring() == true,
             "weatherShutdownMonitoring must be true after hard shutdown");

    KTRY_CLOSE_EKOS();
}

// ─────────────────────────────────────────────────────────────────────────────
// testWeatherMonitoringModeSimulator
//
// This test exercises the scenario where a weather alert is injected, the
// scheduler enters a grace period of 0 (wait indefinitely). The expected
// behavior is that the scheduler enters weather monitoring mode, keeps itself
// running, and waits for the weather to improve without fully shutting down.
// ─────────────────────────────────────────────────────────────────────────────
void TestEkosSchedulerWeather::testWeatherMonitoringModeSimulator()
{
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();

    TestEkosHelper helper;
    helper.m_MountDevice = "Telescope Simulator";
    helper.m_CCDDevice = "CCD Simulator";
    helper.m_FocuserDevice = "Focuser Simulator";
    helper.m_GuiderDevice = "Guide Simulator";
    helper.m_ExtraDevices = QStringList() << "Weather Simulator";
    QVERIFY(helper.setupEkosProfile("Simulators+Weather", false));

    KTRY_EKOS_START_PROFILE("Simulators+Weather");
    KHACK_RESET_EKOS_TIME();

    Ekos::Scheduler* realScheduler = Ekos::Manager::Instance()->schedulerModule();
    QSharedPointer<Ekos::Scheduler> realSchedulerPtr(realScheduler, [](Ekos::Scheduler*) {});

    // Configure default observatory queues
    QString shutdownFile = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/observatory_shutdown.json");
    QString startupFile = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/collections/observatory_startup.json");

    QVERIFY2(!shutdownFile.isEmpty(), "observatory_shutdown.json not found in KStars data directory");
    QVERIFY2(!startupFile.isEmpty(), "observatory_startup.json not found in KStars data directory");

    realScheduler->process()->moduleState()->setPreShutdownScriptURL(QUrl::fromLocalFile(shutdownFile));
    realScheduler->process()->moduleState()->setPostStartupScriptURL(QUrl::fromLocalFile(startupFile));

    Options::setSchedulerStartupEnabled(true);
    Options::setSchedulerShutdownEnabled(true);

    Options::setSchedulerWeather(true);
    // Use grace period 0 (indefinite wait)
    Options::setSchedulerWeatherGracePeriod(0);
    Options::setSchedulerWeatherShutdownDelay(0);

    GeoLocation geo(dms(47, 58), dms(29, 20), "Kuwait", "", "Kuwait", 3);
    KStarsData::Instance()->geo()->setLat(*(geo.lat()));
    KStarsData::Instance()->geo()->setLong(*(geo.lng()));
    KStarsData::Instance()->geo()->setTZ0(geo.TZ0());

    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Kocab");
    TestEkosSchedulerHelper::StartupCondition startupCond;
    startupCond.type = Ekos::START_ASAP;
    TestEkosSchedulerHelper::CompletionCondition completionCond;
    completionCond.type = Ekos::FINISH_LOOP;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    auto schedJob = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {2, 2, "Luminance", "."});
    QString schedulerXML = TestEkosSchedulerHelper::getSchedulerFile(
                               targetObject, startupCond, completionCond, {true, false, false, false}, false, false, 0);

    QString esqFilename = dir.filePath("test.esq");
    QString eslFilename = dir.filePath("test.esl");
    TestEkosSchedulerHelper::writeSimpleSequenceFiles(schedulerXML, eslFilename,
            TestEkosSchedulerHelper::getEsqContent(schedJob), esqFilename);

    realScheduler->load(true, eslFilename);
    realScheduler->moduleState()->jobs()[0]->setSequenceFile(QUrl(QString("file://%1").arg(esqFilename)));

    helper.prepareOpticalTrains();
    helper.prepareCaptureModule();

    // Start scheduler and wait for full startup sequence:
    KTRY_CLICK(realScheduler, startB);

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->activeJob() != nullptr
                             && realScheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING, 120000);

    // Weather alert injection
    QVERIFY2(TestEkosHelper::setSimulatedWeather(true, realSchedulerPtr), "Weather alert not set");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() == ISD::Weather::WEATHER_ALERT, 10000);
    // The mount should park as part of the soft-shutdown / pre-shutdown queue
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->parkStatus() == ISD::PARK_PARKED, 20000);

    // With grace period 0, it enters weather monitoring mode (RUN_WAKEUP) once the
    // pre-shutdown queue has completed, which can be a little after the mount parked.
    QTRY_VERIFY2_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherShutdownMonitoring() == true,
                              "weatherShutdownMonitoring must be true for grace period 0", 20000);
    QCOMPARE(realScheduler->process()->moduleState()->timerState(), Ekos::RUN_WAKEUP);
    QCOMPARE(realScheduler->process()->moduleState()->startupState(), Ekos::STARTUP_POST_DEVICES);
    QVERIFY2(realScheduler->process()->moduleState()->schedulerState() == Ekos::SCHEDULER_RUNNING,
             "schedulerState must remain RUNNING for grace period 0");

    // Ekos and INDI should remain ready
    QVERIFY2(realScheduler->process()->moduleState()->ekosState() == Ekos::EKOS_READY, "Ekos must remain running");
    QVERIFY2(realScheduler->process()->moduleState()->indiState() == Ekos::INDI_READY, "INDI must remain connected");

    // Weather OK injection to recover
    QVERIFY2(TestEkosHelper::setSimulatedWeather(false, realSchedulerPtr), "Weather OK not set");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() == ISD::Weather::WEATHER_OK, 10000);
    // Mount should unpark
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->parkStatus() == ISD::PARK_UNPARKED, 15000);

    QVERIFY2(realScheduler->process()->moduleState()->weatherShutdownMonitoring() == false,
             "weatherShutdownMonitoring must be false after recovery");
    QVERIFY2(realScheduler->process()->moduleState()->shutdownState() == Ekos::SHUTDOWN_IDLE, "shutdownState must be IDLE");
    QVERIFY2(realScheduler->process()->moduleState()->startupState() == Ekos::STARTUP_COMPLETE,
             "startupState must be complete");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->activeJob() != nullptr
                             && realScheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING, 30000);

    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
}

// ─────────────────────────────────────────────────────────────────────────────
// testWeatherRecoveryAfterDawnRunsShutdown
//
// Regression test for weather recovery after dawn, with the real simulators and
// task queues.
//
// Scenario (mirrors the real incidents from 2026-04-20 and 2026-09-26):
//   1. Scheduler is running, job is capturing.
//   2. Weather alert fires mid-session → pre-shutdown queue runs (parks mount,
//      closes dome).  Scheduler enters indefinite-wait monitoring mode
//      (grace period = 0).
//   3. Weather clears AFTER dawn.
//
// Earlier bugs: the post-startup queue ran on recovery (unparking the mount,
// opening the dome in daylight), and then either the scheduler never evaluated
// the jobs again (fixed by queueExecutionCompleted() forcing RUN_SCHEDULER), or it
// was stuck with a stale active job and never shut down.
//
// Expected now: recovery re-evaluates the jobs before any post-startup task runs.
// The twilight-constrained job cannot run before tonight, so the scheduler sleeps
// until then with the observatory still parked; the post-startup queue only runs
// when the job is due.
//
// Test flow:
//   1. Full Ekos/INDI simulator startup + job capturing.
//   2. Weather alert injected → pre-shutdown queue runs → mount parked.
//      Scheduler enters indefinite-wait monitoring mode (grace period = 0).
//   3. Advance simulated time past dawn so no job can run now.
//   4. Weather OK injected.
//   5. REGRESSION CHECK: the scheduler sleeps until the job's next window, the
//      post-startup queue has not run and the mount is still parked.
//
// The mock-based tests (testWeatherAlertThroughDawn etc.) also cover the alert
// outlasting dawn and the 5-minute monitoring wakeups, which this test cannot wait for.
// ─────────────────────────────────────────────────────────────────────────────
void TestEkosSchedulerWeather::testWeatherRecoveryAfterDawnRunsShutdown()
{
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();

    TestEkosHelper helper;
    helper.m_MountDevice = "Telescope Simulator";
    helper.m_CCDDevice = "CCD Simulator";
    helper.m_FocuserDevice = "Focuser Simulator";
    helper.m_GuiderDevice = "Guide Simulator";
    helper.m_ExtraDevices = QStringList() << "Weather Simulator";
    QVERIFY(helper.setupEkosProfile("Simulators+Weather", false));

    KTRY_EKOS_START_PROFILE("Simulators+Weather");
    KHACK_RESET_EKOS_TIME();

    Ekos::Scheduler *realScheduler = Ekos::Manager::Instance()->schedulerModule();
    QSharedPointer<Ekos::Scheduler> realSchedulerPtr(realScheduler, [](Ekos::Scheduler *) {});

    // Configure observatory queues (pre-shutdown parks mount/dome, post-startup unparks)
    QString shutdownFile = KSPaths::locate(QStandardPaths::AppDataLocation,
                                           "taskqueue/collections/observatory_shutdown.json");
    QString startupFile  = KSPaths::locate(QStandardPaths::AppDataLocation,
                                           "taskqueue/collections/observatory_startup.json");
    QVERIFY2(!shutdownFile.isEmpty(), "observatory_shutdown.json not found");
    QVERIFY2(!startupFile.isEmpty(),  "observatory_startup.json not found");

    realScheduler->process()->moduleState()->setPreShutdownScriptURL(QUrl::fromLocalFile(shutdownFile));
    realScheduler->process()->moduleState()->setPostStartupScriptURL(QUrl::fromLocalFile(startupFile));

    Options::setSchedulerStartupEnabled(true);
    Options::setSchedulerShutdownEnabled(true);
    Options::setSchedulerWeather(true);
    // Grace period = 0 → indefinite wait (the scenario from the bug report)
    Options::setSchedulerWeatherGracePeriod(0);
    Options::setSchedulerWeatherShutdownDelay(0);

    // Use a location and target that is only visible at night so that after we
    // advance the clock past dawn the GreedyScheduler finds no schedulable jobs.
    GeoLocation geo(dms(47, 58), dms(29, 20), "Kuwait", "", "Kuwait", 3);
    KStarsData::Instance()->geo()->setLat(*(geo.lat()));
    KStarsData::Instance()->geo()->setLong(*(geo.lng()));
    KStarsData::Instance()->geo()->setTZ0(geo.TZ0());

    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Kocab");
    QVERIFY2(targetObject != nullptr, "Kocab not found in sky catalog");

    TestEkosSchedulerHelper::StartupCondition startupCond;
    startupCond.type = Ekos::START_ASAP;
    TestEkosSchedulerHelper::CompletionCondition completionCond;
    completionCond.type = Ekos::FINISH_LOOP;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    auto schedJob = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {2, 2, "Luminance", "."});
    // enforceTwilight = true so the job is constrained to nighttime only
    QString schedulerXML = TestEkosSchedulerHelper::getSchedulerFile(
                               targetObject, startupCond, completionCond,
    {true, false, false, false}, /*enforceTwilight=*/true, false, 0);

    QString esqFilename = dir.filePath("test.esq");
    QString eslFilename = dir.filePath("test.esl");
    TestEkosSchedulerHelper::writeSimpleSequenceFiles(schedulerXML, eslFilename,
            TestEkosSchedulerHelper::getEsqContent(schedJob), esqFilename);

    realScheduler->load(true, eslFilename);
    realScheduler->moduleState()->jobs()[0]->setSequenceFile(QUrl(QString("file://%1").arg(esqFilename)));

    helper.prepareOpticalTrains();
    helper.prepareCaptureModule();

    // ── Phase 1: Start scheduler and wait until the job is capturing ──────────
    // The job is twilight-constrained, so start the session at 01:00 local: the job
    // can then run whatever the wall-clock time the test is started at. A real-time
    // clock ignores setUTC(), so run the clock normally for the duration of this test.
    SimClock *clock = KStarsData::Instance()->clock();
    const bool wasRealTime = clock->isRealTime();
    const auto restoreRealTime = qScopeGuard([clock, wasRealTime]()
    {
        clock->setRealTime(wasRealTime);
    });
    clock->setRealTime(false);
    const QDate sessionDate = KStarsData::Instance()->lt().date();
    KStarsData::Instance()->clock()->setUTC(
        KStarsData::Instance()->geo()->LTtoUT(KStarsDateTime(sessionDate, QTime(1, 0, 0))));
    KTRY_CLICK(realScheduler, startB);
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->activeJob() != nullptr
                             && realScheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING, 120000);

    // ── Phase 2: Inject weather alert ─────────────────────────────────────────
    QVERIFY2(TestEkosHelper::setSimulatedWeather(true, realSchedulerPtr), "Weather alert not set");

    // Wait for the pre-shutdown queue to complete: mount must be parked
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() ==
                             ISD::Weather::WEATHER_ALERT, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->mountModule()->parkStatus() ==
                             ISD::PARK_PARKED, 30000);

    // Scheduler must be in indefinite-wait monitoring mode (grace period = 0)
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherShutdownMonitoring() == true, 10000);
    QVERIFY2(realScheduler->process()->moduleState()->schedulerState() == Ekos::SCHEDULER_RUNNING,
             "schedulerState must remain RUNNING in monitoring mode");
    // Ekos/INDI must stay up (soft shutdown only)
    QVERIFY2(realScheduler->process()->moduleState()->ekosState() == Ekos::EKOS_READY,
             "Ekos must remain running during soft shutdown");
    QVERIFY2(realScheduler->process()->moduleState()->indiState() == Ekos::INDI_READY,
             "INDI must remain connected during soft shutdown");

    // ── Phase 3: Advance simulated clock past dawn so no jobs are schedulable ─
    // Move time to 10:00 local (well past dawn) so the twilight-constrained job
    // has no valid window when the scheduler evaluates after weather recovery.
    KStarsData::Instance()->clock()->setUTC(
        KStarsData::Instance()->geo()->LTtoUT(KStarsDateTime(sessionDate, QTime(10, 0, 0))));
    // Let the scheduler process the time change
    QTest::qWait(500);

    // ── Phase 4: Inject weather OK ────────────────────────────────────────────
    QVERIFY2(TestEkosHelper::setSimulatedWeather(false, realSchedulerPtr), "Weather OK not set");

    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->weatherStatus() ==
                             ISD::Weather::WEATHER_OK, 10000);

    // ── Phase 5: Regression check ─────────────────────────────────────────────
    // Recovery re-evaluates the jobs first. No job can run until tonight, so the
    // scheduler goes to sleep until then instead of running the post-startup queue.
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->timerState() == Ekos::RUN_WAKEUP &&
                             !realScheduler->process()->moduleState()->weatherShutdownMonitoring(), 30000);
    QVERIFY2(realScheduler->process()->moduleState()->startupState() == Ekos::STARTUP_POST_DEVICES,
             "The post-startup queue must not run while no job can start");
    QVERIFY2(realScheduler->activeJob() != nullptr &&
             realScheduler->activeJob()->getStartupTime() > KStarsData::Instance()->lt(),
             "The scheduler must be sleeping until the job's next window");
    QVERIFY2(Ekos::Manager::Instance()->mountModule()->parkStatus() == ISD::PARK_PARKED,
             "The mount must stay parked after weather recovery in daylight");

    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
}

QTEST_KSTARS_MAIN(TestEkosSchedulerWeather)

#endif // HAVE_INDI
