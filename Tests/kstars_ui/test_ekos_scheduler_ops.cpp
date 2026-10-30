/*  KStars scheduler operations tests
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QDir>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <ctime>
#include <iterator>

#include "../testhelpers.h"
#include "test_ekos_scheduler_ops.h"
#include "test_ekos_scheduler_helper.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/greedyscheduler.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/scheduler/taskqueue/actions/delayaction.h"

#include "skymapcomposite.h"

#if defined(HAVE_INDI)

#include "artificialhorizoncomponent.h"
#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "linelist.h"
#include "mockmodules.h"
#include "Options.h"

#define QWAIT_TIME 10

#define DEFAULT_TOLERANCE 300
#define DEFAULT_ITERATIONS 50

using Ekos::Scheduler;
using Ekos::MeridianFlipState;

// Use this class to temporarily modify the scheduler's update interval
// by creating a WithInterval variable in a scope.
// The constructor sets the scheduler's update interval to the number of ms
// in the 1st arg, and restore it at the end of the scope. For example:
// {
//   WithInterval interval(10000, scheduler)
//   // scheduler update interval is now 10000
//   ...
// }
// // scheduler update interval is back to the original value.
class WithInterval
{
    public:
        WithInterval(int interval, QSharedPointer<Ekos::Scheduler> &_scheduler) : scheduler(_scheduler)
        {
            keepInterval = scheduler->moduleState()->updatePeriodMs();
            scheduler->moduleState()->setUpdatePeriodMs(interval);
        }
        ~WithInterval()
        {
            scheduler->moduleState()->setUpdatePeriodMs(keepInterval);
        }
    private:
        int keepInterval;
        QSharedPointer<Ekos::Scheduler> scheduler;
};

TestEkosSchedulerOps::TestEkosSchedulerOps(QObject *parent) : QObject(parent)
{
}

// Qt 6.8 changed QDateTime internals and emits a flood of
//   "QDateTime::setTimeSpec: Pass a QTimeZone instead of Qt::TimeZone."
// warnings from KStars code that uses the old API.  The warnings are harmless
// in the test context and drown out the real test output, so filter them here.
static void suppressKnownWarnings(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx);
    if (type == QtWarningMsg && msg.contains("QDateTime::setTimeSpec: Pass a QTimeZone"))
        return;
    // All other messages: print normally.
    fprintf(type == QtWarningMsg ? stderr : stderr, "%s\n", qPrintable(msg));
}

void TestEkosSchedulerOps::initTestCase()
{
    // Suppress the QDateTime::setTimeSpec deprecation flood from KStars source
    // (harmless in tests, but makes the output unreadable).
    qInstallMessageHandler(suppressKnownWarnings);

    QDBusConnection::sessionBus().registerObject("/MockKStars", this);
    QDBusConnection::sessionBus().registerService("org.kde.mockkstars");

    // This gets executed at the start of testing

    disableSkyMap();

    if (KStars::Instance() == nullptr || KStars::Instance()->data() == nullptr ||
            KStars::Instance()->data()->skyComposite()->findByName("Sun") == nullptr)
    {
        QSKIP("Solar system data not initialized; scheduler ops tests require the solar system.");
    }
}

void TestEkosSchedulerOps::cleanupTestCase()
{
    // This gets executed at the end of testing
}

void TestEkosSchedulerOps::init()
{
    // This gets executed at the start of each of the individual tests.
    testTimer.start();

    focuser.reset(new Ekos::MockFocus);
    mount.reset(new Ekos::MockMount);
    capture.reset(new Ekos::MockCapture);
    align.reset(new Ekos::MockAlign);
    guider.reset(new Ekos::MockGuide);
    ekos.reset(new Ekos::MockEkos);

    // Weather tests enable this after the scheduler exists. Keeping it off during
    // construction stops SchedulerProcess from connecting to a standalone safety
    // monitor configured in the user's settings.
    Options::setSchedulerWeather(false);
    m_schedulerLog.clear();
    m_weatherSlewBaseline = 0;
    m_weatherHoldViolation.clear();

    scheduler.reset(new Scheduler("/MockKStars/MockEkos/Scheduler", "org.kde.mockkstars",
                                  Ekos::MockEkos::mockPath, "org.kde.mockkstars.MockEkos"));
    // These org.kde.* interface strings are set up in the various .xml files.
    scheduler->process()->setFocusInterfaceString("org.kde.mockkstars.MockEkos.MockFocus");
    scheduler->process()->setMountInterfaceString("org.kde.mockkstars.MockEkos.MockMount");
    scheduler->process()->setCaptureInterfaceString("org.kde.mockkstars.MockEkos.MockCapture");
    scheduler->process()->setAlignInterfaceString("org.kde.mockkstars.MockEkos.MockAlign");
    scheduler->process()->setGuideInterfaceString("org.kde.mockkstars.MockEkos.MockGuide");
    scheduler->process()->setFocusPathString(Ekos::MockFocus::mockPath);
    scheduler->process()->setMountPathString(Ekos::MockMount::mockPath);
    scheduler->process()->setCapturePathString(Ekos::MockCapture::mockPath);
    scheduler->process()->setAlignPathString(Ekos::MockAlign::mockPath);
    scheduler->process()->setGuidePathString(Ekos::MockGuide::mockPath);

    // Let's not deal with the startup devices for now.
    scheduler->schedulerStartupEnabled->setChecked(false);

    // Clear the pre-shutdown task queue URL so the shutdown state machine does not
    // enter SHUTDOWN_PRE_QUEUE_RUNNING and stall waiting for a live Ekos profile.
    // (The queue executor requires an active profile; in unit tests there is none.)
    scheduler->process()->moduleState()->setPreShutdownScriptURL(QUrl());

    // For now don't deal with files that were left around from previous testing.
    // Should put these is a temporary directory that will be removed, if we generate
    // them at all.
    Options::setRememberJobProgress(false);

    // This allows testing of the shutdown.
    Options::setStopEkosAfterShutdown(true);
    Options::setDitherEnabled(false);

    // Ensure preemptive shutdown is OFF by default.  testDawnShutdown tests the
    // park-and-sleep path, not the full Ekos/INDI shutdown path.  Without this
    // reset the user's persistent KConfig (kstarsrc) can leak into the test suite:
    // if the user has "Preemptive Shutdown" enabled in their KStars installation,
    // PreemptiveShutdown=true would be read from ~/.config/kstarsrc and
    // testDawnShutdown would silently take the wrong code path, stopping Ekos/INDI
    // instead of just parking the mount and sleeping.
    // testPreemptiveShutdown() sets this back to true itself when it needs it.
    Options::setPreemptiveShutdown(false);

    // define START_ASAP and FINISH_SEQUENCE as default startup/completion conditions.
    m_startupCondition.type = Ekos::START_ASAP;
    m_completionCondition.type = Ekos::FINISH_SEQUENCE;

    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);
    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    Options::setGreedyScheduling(true);

    // Tests were originally constructed with 48 hour schedules.
    // Now that they run 72 hours, that would invalidate many schedules.
    scheduler->process()->getGreedyScheduler()->setSimHours(48);
}

void TestEkosSchedulerOps::cleanup()
{
    // This gets executed at the end of each of the individual tests.

    // The signal and/or dbus communications seems to get confused
    // without explicit resetting of these objects.
    focuser.reset();
    mount.reset();
    capture.reset();
    align.reset();
    guider.reset();
    ekos.reset();
    Ekos::SchedulerJob::setHorizon(nullptr);
    scheduler.reset();
    fprintf(stderr, "Test took %.1fs\n", testTimer.elapsed() / 1000.0);
}

void TestEkosSchedulerOps::disableSkyMap()
{
    Options::setShowAsteroids(false);
    Options::setShowComets(false);
    Options::setShowSupernovae(false);
    Options::setShowDeepSky(false);
    Options::setShowEcliptic(false);
    Options::setShowEquator(false);
    Options::setShowLocalMeridian(false);
    Options::setShowGround(false);
    Options::setShowHorizon(false);
    Options::setShowFlags(false);
    Options::setShowOther(false);
    Options::setShowMilkyWay(false);
    Options::setShowSolarSystem(true);
    Options::setShowStars(false);
    Options::setShowSatellites(false);
    Options::setShowHIPS(false);
    Options::setShowTerrain(false);
}

// When checking that something happens near a certain time, the tolerance of the
// check is affected by how often the scheduler iterates. E.g. if the scheduler only
// runs once a minute (to speed up simulation), it is unreasonable to check for 1 second
// tolerances. This function returns the max of the tolerance passed in and 3 times
// the scheduler's iteration period to compensate for that.
int TestEkosSchedulerOps::timeTolerance(int seconds)
{
    const int tolerance = std::max(seconds, 3 * (scheduler->moduleState()->updatePeriodMs() / 1000));
    return tolerance;
}

// testBasics verifies two things:
//
// 1. D-Bus plumbing between the Scheduler and the mock Ekos modules.
//    Each mock module (Focus, Mount, Capture, Align, Guide) is registered on the
//    session bus.  The test confirms that:
//      a) Before the modules are added, all scheduler interfaces are null.
//      b) After addModule() and a brief Qt event-loop spin, all interfaces become
//         non-null — proving the scheduler correctly discovers each module via D-Bus.
//      c) A real D-Bus method call (Focus::resetFrame) round-trips correctly and
//         mutates state inside the mock object (focuser->isReset).
//
// 2. The scheduler idle-shutdown state machine.
//    With no jobs loaded the scheduler should boot up and stop cleanly:
//      RUN_WAKEUP  (init state)
//        → iterate → RUN_SCHEDULER  (scheduler evaluates jobs)
//        → iterate → "No executable jobs" → stop()
//          stop() sets RUN_NOTHING directly when no devices are connected
//          (it no longer passes through the intermediate RUN_SHUTDOWN step).
//          If a future change reintroduces the intermediate step the test
//          handles that too: it consumes the RUN_SHUTDOWN iteration and then
//          asserts RUN_NOTHING.
//    The returned sleepMs of 1000 is also verified so that the scheduler does
//    not spin-wait (sleep = 0) on an empty job list.
void TestEkosSchedulerOps::testBasics()
{
    // -------------------------------------------------------------------------
    // Part 1: D-Bus interface discovery
    // Before any mock modules are registered the scheduler should have no
    // D-Bus interfaces for any of the Ekos sub-modules.
    // -------------------------------------------------------------------------
    QVERIFY(scheduler->process()->focusInterface().isNull());
    QVERIFY(scheduler->process()->mountInterface().isNull());
    QVERIFY(scheduler->process()->captureInterface().isNull());
    QVERIFY(scheduler->process()->alignInterface().isNull());
    QVERIFY(scheduler->process()->guideInterface().isNull());

    // Register mock modules on the session bus. The scheduler will discover
    // them asynchronously via D-Bus service/object notifications.
    ekos->addModule(QString("Focus"));
    ekos->addModule(QString("Mount"));
    ekos->addModule(QString("Capture"));
    ekos->addModule(QString("Align"));
    ekos->addModule(QString("Guide"));

    // Spin the Qt event loop long enough for the D-Bus notifications to arrive.
    // processEvents() alone is not reliable here because D-Bus delivery happens
    // on a separate thread; qWait() yields the thread and lets it complete.
    QTest::qWait(10 * QWAIT_TIME);

    // Now all interfaces must be non-null — the scheduler discovered every module.
    QVERIFY(!scheduler->process()->focusInterface().isNull());
    QVERIFY(!scheduler->process()->mountInterface().isNull());
    QVERIFY(!scheduler->process()->captureInterface().isNull());
    QVERIFY(!scheduler->process()->alignInterface().isNull());
    QVERIFY(!scheduler->process()->guideInterface().isNull());

    // -------------------------------------------------------------------------
    // Part 1b: D-Bus method call round-trip
    // Call Focus::resetFrame via the scheduler's D-Bus proxy and verify the
    // mock object's in-process state is updated, proving the call went through.
    // -------------------------------------------------------------------------
    QVERIFY(!focuser->isReset);
    QList<QVariant> dBusArgs;
    dBusArgs.append("MockCamera");
    scheduler->process()->focusInterface()->callWithArgumentList(QDBus::AutoDetect, "resetFrame", dBusArgs);
    QTest::qWait(10 * QWAIT_TIME);  // wait for the async D-Bus call to complete
    QVERIFY(focuser->isReset);

    // -------------------------------------------------------------------------
    // Part 2: Scheduler state machine with no jobs
    // After init() the scheduler enters RUN_WAKEUP.  Two iterations should
    // bring it to a fully stopped state (RUN_NOTHING) with a 1000 ms sleep
    // interval, confirming it does not busy-spin on an empty job list.
    //
    // State path:
    //   RUN_WAKEUP → [iter 1] → RUN_SCHEDULER → [iter 2, no jobs] → RUN_NOTHING
    //
    // Note: stop() currently sets RUN_NOTHING directly (no device connections
    // to tear down).  If an intermediate RUN_SHUTDOWN state is ever reintroduced
    // the conditional below handles it gracefully.
    // -------------------------------------------------------------------------
    scheduler->moduleState()->init();
    QVERIFY(scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);

    // Iteration 1: wakeup → evaluate jobs
    int sleepMs = scheduler->process()->runSchedulerIteration();
    QVERIFY(scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);

    // Iteration 2: no jobs found → stop(); sleepMs must be 1000 (not 0)
    sleepMs = scheduler->process()->runSchedulerIteration();
    QVERIFY(sleepMs == 1000);

    // Consume the optional RUN_SHUTDOWN intermediate state if present.
    if (scheduler->moduleState()->timerState() == Ekos::RUN_SHUTDOWN)
        sleepMs = scheduler->process()->runSchedulerIteration();

    QVERIFY(scheduler->moduleState()->timerState() == Ekos::RUN_NOTHING);
}

// Runs the scheduler for a number of iterations between 1 and the arg "iterations".
// Each iteration it  increments the simulated clock (currentUTime, which is in Universal
// Time) by *sleepMs, then runs the scheduler, then calls fcn().
// If fcn() returns true, it stops iterating and returns true.
// It returns false if it completes all the with fnc() returning false.
bool TestEkosSchedulerOps::iterateScheduler(const QString &label, int iterations,
        int *sleepMs, KStarsDateTime* currentUTime, std::function<bool ()> fcn,
        const QDateTime &captureCompleteUTime)
{
    fprintf(stderr, "\n----------------------------------------\n");
    fprintf(stderr, "Starting iterateScheduler(%s)%s\n",  label.toLatin1().data(),
            captureCompleteUTime.isValid() ?
            QString(" will send CAPTURE_COMPLETE at %1 ***************************")
            .arg(captureCompleteUTime.toString()).toLatin1().data() : "");
    bool captureCompleteDone = false;
    for (int i = 0; i < iterations; ++i)
    {
        //qApp->processEvents();
        QTest::qWait(QWAIT_TIME); // this takes ~10ms per iteration!
        // Is there a way to speed up the above?
        // I didn't reduce it, because the basic test fails to call a dbus function
        // with less than 10ms wait time.

        *currentUTime = currentUTime->addSecs(*sleepMs / 1000.0);

        if (captureCompleteUTime.isValid() && !captureCompleteDone &&
                captureCompleteUTime.secsTo(*currentUTime) >= 0)
        {
            captureCompleteDone = true;
            fprintf(stderr, "IterateScheduler(%s) sending CAPTURE_COMPLETE at %s\n",  label.toLatin1().data(),
                    currentUTime->toString().toLatin1().data());
            capture->setStatus(Ekos::CAPTURE_COMPLETE);
        }
        KStarsData::Instance()->changeDateTime(*currentUTime); // <-- 175ms
        *sleepMs = scheduler->process()->runSchedulerIteration();
        fprintf(stderr, "current time LT %s UT %s scheduler %s %s %s %d\n",
                KStarsData::Instance()->lt().toString().toLatin1().data(),
                KStarsData::Instance()->ut().toString().toLatin1().data(),
                Ekos::getSchedulerStatusString(
                    scheduler->moduleState()->schedulerState()).toLatin1().data(),
                scheduler->moduleState()->activeJob() ?
                Ekos::SchedulerJob::jobStageString(
                    scheduler->moduleState()->activeJob()->getStage()).toLatin1().data() : "",
                scheduler->moduleState()->activeJob() ?
                Ekos::SchedulerJob::jobStatusString(
                    scheduler->moduleState()->activeJob()->getState()).toLatin1().data() : "",
                scheduler->moduleState()->timerState()
               );

        if (fcn())
        {
            fprintf(stderr, "IterateScheduler %s returning TRUE at %s %s after %d iterations\n",
                    label.toLatin1().data(),
                    KStarsData::Instance()->lt().toString().toLatin1().data(),
                    KStarsData::Instance()->ut().toString().toLatin1().data(), i + 1);
            return true;
        }
    }
    fprintf(stderr, "IterateScheduler %s returning FALSE at %s %s after %d iterations\n",
            label.toLatin1().data(),
            KStarsData::Instance()->lt().toString().toLatin1().data(),
            KStarsData::Instance()->ut().toString().toLatin1().data(), iterations);
    return false;
}

// Sets up the scheduler in a particular location (geo) and a UTC start time.
void TestEkosSchedulerOps::initTimeGeo(const GeoLocation &geo, const QDateTime &startUTime)
{
    KStarsData::Instance()->geo()->setLat(*(geo.lat()));
    KStarsData::Instance()->geo()->setLong(*(geo.lng()));
    // Note, the actual TZ would be -7 as there is a DST correction for these dates.
    KStarsData::Instance()->geo()->setTZ0(geo.TZ0());

    KStarsDateTime currentUTime(startUTime);
    KStarsData::Instance()->changeDateTime(currentUTime);
    KStarsData::Instance()->clock()->setManualMode(true);
}

QString TestEkosSchedulerOps::writeFiles(const QString &label, QTemporaryDir &dir,
        const QVector<TestEkosSchedulerHelper::CaptureJob> &captureJob,
        const QString &schedulerXML)
{
    // nanoseconds since epoch will be tacked on to the label to keep them unique.
    long int nn = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();

    const QString eslFilename = dir.filePath(QString("%1-%2.esl").arg(label).arg(nn));
    const QString esqFilename = dir.filePath(QString("%1-%2.esq").arg(label).arg(nn));
    const QString eslContents = QString(schedulerXML);
    const QString esqContents = TestEkosSchedulerHelper::getEsqContent(captureJob);

    TestEkosSchedulerHelper::writeSimpleSequenceFiles(eslContents, eslFilename, esqContents, esqFilename);
    return eslFilename;
}

void TestEkosSchedulerOps::initFiles(QTemporaryDir *dir, const QVector<QString> &esls, const QVector<QString> &esqs)
{
    QVERIFY(dir->isValid());
    QVERIFY(dir->autoRemove());

    for (int i = 0; i < esls.size(); ++i)
    {
        const QString eslFile = dir->filePath(QString("test%1.esl").arg(i));
        const QString esqFile = dir->filePath(QString("test%1.esq").arg(i));

        QVERIFY(TestEkosSchedulerHelper::writeSimpleSequenceFiles(esls[i], eslFile, esqs[i], esqFile));

        fprintf(stderr, "Loading\n");
        scheduler->load(i == 0, QString("%1").arg(eslFile));
        QVERIFY(scheduler->moduleState()->jobs().size() == (i + 1));
        scheduler->moduleState()->jobs()[i]->setSequenceFile(QUrl(QString("file://%1").arg(esqFile)));
        fprintf(stderr, "seq file: %s \"%s\"\n", esqFile.toLatin1().data(), QString("file://%1").arg(esqFile).toLatin1().data());
    }
}

// Sets up the scheduler in a particular location (geo) and a UTC start time.
void TestEkosSchedulerOps::initScheduler(const GeoLocation &geo, const QDateTime &startUTime, QTemporaryDir *dir,
        const QVector<QString> &esls, const QVector<QString> &esqs)
{
    initTimeGeo(geo, startUTime);
    initFiles(dir, esls, esqs);
    scheduler->process()->evaluateJobs(false);
    scheduler->moduleState()->init();
    QVERIFY(scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
}

void TestEkosSchedulerOps::startupJob(
    const GeoLocation &geo, const QDateTime &startUTime,
    QTemporaryDir *dir, const QString &esl, const QString &esq,
    const QDateTime &wakeupTime, KStarsDateTime &endUTime, int &endSleepMs)
{
    QVector<QString> esls, esqs;
    esls.push_back(esl);
    esqs.push_back(esq);
    startupJobs(geo, startUTime, dir, esls, esqs, wakeupTime, endUTime, endSleepMs);
}

void TestEkosSchedulerOps::startupJobs(
    const GeoLocation &geo, const QDateTime &startUTime,
    QTemporaryDir *dir, const QVector<QString> &esls, const QVector<QString> &esqs,
    const QDateTime &wakeupTime, KStarsDateTime &endUTime, int &endSleepMs)
{
    initScheduler(geo, startUTime, dir, esls, esqs);
    startupJobs2(startUTime, wakeupTime, endUTime, endSleepMs);
}

void TestEkosSchedulerOps::startupJobs2(
    const QDateTime &startUTime, const QDateTime &wakeupTime, KStarsDateTime &endUTime, int &endSleepMs)
{
    {
        // Have the scheduler update quickly when running these init routines.
        WithInterval interval(1000, scheduler);

        KStarsDateTime currentUTime(startUTime);
        int sleepMs = 0;
        QVERIFY(scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
        QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
        }));

        if (wakeupTime.isValid())
        {
            // This is the sequence when it goes to sleep, then wakes up later to start.

            QVERIFY(iterateScheduler("Wait for RUN_WAKEUP", 10, &sleepMs, &currentUTime, [&]() -> bool
            {
                return (scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
            }));

            // Verify that it's near the original start time.
            const qint64 delta_t = KStarsData::Instance()->ut().secsTo(startUTime);
            QVERIFY2(std::abs(delta_t) < timeTolerance(300),
                     QString("Delta to original time %1 too large, failing.").arg(delta_t).toLatin1());

            QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
            {
                return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
            }));

            // Verify that it wakes up at the right time, after the twilight constraint
            // and the stars rises above 30 degrees. See the time comment above.
            QVERIFY(std::abs(KStarsData::Instance()->ut().secsTo(wakeupTime)) < timeTolerance(DEFAULT_TOLERANCE));
        }
        else
        {
            // check if there is a job scheduled
            bool scheduled_job = false;
            for (auto sched_job : scheduler->moduleState()->jobs())
                if (sched_job->state == Ekos::SCHEDJOB_SCHEDULED)
                    scheduled_job = true;
            if (scheduled_job)
            {
                // This is the sequence when it can start-up right away.

                // Verify that it's near the original start time.
                const qint64 delta_t = KStarsData::Instance()->ut().secsTo(startUTime);
                QVERIFY2(std::abs(delta_t) < timeTolerance(DEFAULT_TOLERANCE),
                         QString("Delta to original time %1 too large, failing.").arg(delta_t).toLatin1());

                QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
                {
                    return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
                }));
            }
            else
                // if there is no job scheduled, we're done
            {
                return;
            }
        }
        // When the scheduler starts up, it sends connectDevices to Ekos
        // which sets Indi --> Ekos::Success,
        // and then it sends start() to Ekos which sets Ekos --> Ekos::Success
        bool sentOnce = false, readyOnce = false;
        QVERIFY(iterateScheduler("Wait for Indi and Ekos", 30, &sleepMs, &currentUTime, [&]() -> bool
        {
            if ((scheduler->moduleState()->indiState() == Ekos::INDI_READY) &&
                    (scheduler->moduleState()->ekosState() == Ekos::EKOS_READY))
            {
                return true;
            }
            //else if (scheduler->m_EkosCommunicationStatus == Ekos::Success)
            else if (ekos->ekosStatus() == Ekos::Success)
            {
                // Once Ekos is woken up, say mount and capture are ready.
                if (!sentOnce)
                {
                    // Add the modules once ekos is started up.
                    sentOnce = true;
                    ekos->addModule("Focus");
                    ekos->addModule("Capture");
                    ekos->addModule("Mount");
                    ekos->addModule("Align");
                    ekos->addModule("Guide");
                }
                else if (scheduler->process()->mountInterface() != nullptr &&
                         scheduler->process()->captureInterface() != nullptr && !readyOnce)
                {
                    // Can't send the ready messages until the devices are registered.
                    readyOnce = true;
                    mount->sendReady();
                    capture->sendReady();
                }
            }
            return false;
        }));

        endUTime = currentUTime;
        endSleepMs = sleepMs;
    }
}

void TestEkosSchedulerOps::startModules(KStarsDateTime &currentUTime, int &sleepMs)
{
    WithInterval interval(1000, scheduler);
    QVERIFY(iterateScheduler("Wait for MountSlewing", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Mount::MOUNT_SLEWING)
        {
            mount->setStatus(ISD::Mount::MOUNT_TRACKING);
            return true;
        }
        return false;
    }));

    QVERIFY(iterateScheduler("Wait for MountTracking", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Mount::MOUNT_SLEWING)
            mount->setStatus(ISD::Mount::MOUNT_TRACKING);
        else if (mount->status() == ISD::Mount::MOUNT_TRACKING)
            return true;
        return false;
    }));

    QVERIFY(iterateScheduler("Wait for Focus", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (focuser->status() == Ekos::FOCUS_PROGRESS)
            focuser->setStatus(Ekos::FOCUS_COMPLETE);
        else if (focuser->status() == Ekos::FOCUS_COMPLETE)
        {
            focuser->setStatus(Ekos::FOCUS_IDLE);
            return true;
        }
        return false;
    }));

    QVERIFY(iterateScheduler("Wait for Align", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (align->status() == Ekos::ALIGN_PROGRESS)
            align->setStatus(Ekos::ALIGN_COMPLETE);
        else if (align->status() == Ekos::ALIGN_COMPLETE)
            return true;
        return false;
    }));

    QVERIFY(iterateScheduler("Wait for Guide", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (guider->status() == Ekos::GUIDE_GUIDING);
    }));
    QVERIFY(guider->connected);
}

// Roughly compare the slew coordinates sent to the mount to Deneb's.
// Rough comparison because these will have been converted to JNow.
// Should be called after simulated slew has been completed.
bool TestEkosSchedulerOps::checkLastSlew(const SkyObject* targetObject)
{
    constexpr double halfDegreeInHours = 1 / (15 * 2.0);
    bool success = (std::abs(mount->lastRaHoursSlew - targetObject->ra().Hours()) < halfDegreeInHours) &&
                   (std::abs(mount->lastDecDegreesSlew - targetObject->dec().Degrees()) < 0.5);
    if (!success)
        fprintf(stderr, "Expected slew RA: %f DEC: %F but got %f %f\n",
                targetObject->ra().Hours(), targetObject->dec().Degrees(),
                mount->lastRaHoursSlew, mount->lastDecDegreesSlew);
    return success;
}

// Utility to print the state of the current scheduler job list.
void TestEkosSchedulerOps::printJobs(const QString &label)
{
    fprintf(stderr, "%-30s: ", label.toLatin1().data());
    for (int i = 0; i < scheduler->moduleState()->jobs().size(); ++i)
    {
        fprintf(stderr, "(%d) %s %-15s ", i, scheduler->moduleState()->jobs()[i]->getName().toLatin1().data(),
                Ekos::SchedulerJob::jobStatusString(scheduler->moduleState()->jobs()[i]->getState()).toLatin1().data());
    }
    fprintf(stderr, "\n");
}

void TestEkosSchedulerOps::initJob(const KStarsDateTime &startUTime, const KStarsDateTime &jobStartUTime)
{
    KStarsDateTime currentUTime(startUTime);
    int sleepMs = 0;

    // wait for the scheduler select the configured job for execution
    QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
    }));

    // wait until the scheduler turns to the wakeup mode sleeping until the startup condition is met
    QVERIFY(iterateScheduler("Wait for RUN_WAKEUP", 10, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
    }));

    // wait until the scheduler starts the job
    QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
    }));

    // check the distance from the expected start time
    qint64 delta = KStars::Instance()->data()->ut().secsTo(jobStartUTime);
    // real offset should be maximally 5 min off the configured offset
    QVERIFY2(std::abs(delta) < 300,
             QString("wrong startup time: %1 secs distance to planned %2.").arg(delta).arg(jobStartUTime.toString(
                         Qt::ISODate)).toLocal8Bit());
}

// This tests a simple scheduler job.
// The job initializes Ekos and Indi, slews, plate-solves, focuses, starts guiding, and
// captures. Capture completes and the scheduler shuts down.
void TestEkosSchedulerOps::runSimpleJob(const GeoLocation &geo, const SkyObject *targetObject, const QDateTime &startUTime,
                                        const QDateTime &wakeupTime, bool enforceArtificialHorizon)
{
    KStarsDateTime currentUTime;
    int sleepMs = 0;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    startupJob(geo, startUTime, &dir, TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition,
               m_completionCondition, {true, true, true, true},
               false, enforceArtificialHorizon),
               TestEkosSchedulerHelper::getDefaultEsqContent(), wakeupTime, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);
    QVERIFY(checkLastSlew(targetObject));

    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));

    {
        WithInterval interval(1000, scheduler);

        // Tell the scheduler that capture is done.
        capture->setStatus(Ekos::CAPTURE_COMPLETE);

        QVERIFY(iterateScheduler("Wait for Abort Guider", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (guider->status() == Ekos::GUIDE_ABORTED);
        }));
        // SHUTDOWN_COMPLETE is transient: completeShutdown() sets it and then immediately
        // calls stop() which resets shutdownState to SHUTDOWN_IDLE. Check schedulerState
        // instead, which remains SCHEDULER_IDLE after stop() returns.
        QVERIFY(iterateScheduler("Wait for Shutdown", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_IDLE);
        }));

        // Here the scheduler sends a message to ekosInterface to disconnectDevices,
        // which will cause indi --> IDLE,
        // and then calls stop() which will cause ekos --> IDLE
        // This will cause the scheduler to shutdown.
        QVERIFY(iterateScheduler("Wait for Scheduler Complete", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->moduleState()->timerState() == Ekos::RUN_NOTHING);
        }));
    }
}

// Regression test: the job editor's "Train" selection must survive a rebuild of the
// optical train list. The list is rebuilt on every INDI state change, i.e. several times
// when the scheduler starts, and used to fall back to "--" each time.
void TestEkosSchedulerOps::testKeepOpticalTrainSelection()
{
    QComboBox * const combo = scheduler->opticalTrainCombo;
    const QStringList trains {"Primary", "Secondary"};
    QSignalSpy indexChanged(combo, &QComboBox::currentIndexChanged);

    // A selected train is kept when the list is rebuilt with the same trains.
    scheduler->fillOpticalTrainCombo(trains);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->currentText(), QString("--"));
    combo->setCurrentText("Secondary");
    QCOMPARE(combo->currentText(), QString("Secondary"));
    indexChanged.clear();

    scheduler->fillOpticalTrainCombo(trains);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->currentText(), QString("Secondary"));
    // The rebuild must not look like a user edit (it would mark the job as modified).
    QCOMPARE(indexChanged.count(), 0);

    // The selection follows the train name, not its position in the list.
    scheduler->fillOpticalTrainCombo({"Secondary", "Primary"});
    QCOMPARE(combo->currentText(), QString("Secondary"));

    // A train that no longer exists falls back to "--".
    scheduler->fillOpticalTrainCombo({"Primary"});
    QCOMPARE(combo->currentIndex(), 0);
    QCOMPARE(combo->currentText(), QString("--"));

    // "--" stays "--".
    scheduler->fillOpticalTrainCombo(trains);
    QCOMPARE(combo->currentText(), QString("--"));

    // The rebuild is triggered by INDI state changes when the scheduler starts. It uses the
    // trains known to the optical train manager, so a train from that list must survive.
    const QStringList managerTrains = Ekos::OpticalTrainManager::Instance()->getTrainNames();
    scheduler->fillOpticalTrainCombo(managerTrains);
    const QString expected = managerTrains.isEmpty() ? QString("--") : managerTrains.last();
    combo->setCurrentText(expected);
    scheduler->moduleState()->setIndiState(Ekos::INDI_CONNECTING);
    scheduler->moduleState()->setIndiState(Ekos::INDI_PROPERTY_CHECK);
    scheduler->moduleState()->setIndiState(Ekos::INDI_READY);
    QCOMPARE(combo->count(), managerTrains.count() + 1);
    QCOMPARE(combo->currentText(), expected);
}

void TestEkosSchedulerOps::testSimpleJob()
{
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Altair");

    // Setup an initial time.
    // Note that the start time is 3pm local (10pm UTC - 7 TZ).
    // Altair, the target, should be at about -40 deg altitude at this time,.
    // The dawn/dusk constraints are 4:03am and 10:12pm (lst=13:43)
    // At 10:12pm it should have an altitude of about 14 degrees, still below the 30-degree constraint.
    // It achieves 30-degrees altitude at about 23:35.
    QDateTime startUTime(QDateTime(QDate(2021, 6, 13), QTime(22, 0, 0), QTimeZone::utc()));
    const QDateTime wakeupTime(QDate(2021, 6, 14), QTime(06, 35, 0), QTimeZone::utc());
    runSimpleJob(geo, targetObject, startUTime, wakeupTime, true);
    QVERIFY(checkLastSlew(targetObject));
}

// This test has the same start as testSimpleJob, except that it but runs in NYC
// instead of silicon valley. This makes sure testing doesn't depend on timezone.
void TestEkosSchedulerOps::testTimeZone()
{
    WithInterval interval(5000, scheduler);
    GeoLocation geo(dms(-74, 0), dms(40, 42, 0), "NYC", "NY", "USA", -5);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Altair");
    KStarsDateTime startUTime(QDateTime(QDate(2021, 6, 13), QTime(22, 0, 0), QTimeZone::utc()));

    // It crosses 30-degrees altitude around the same time locally, but that's
    // 3 hours earlier UTC.
    const QDateTime wakeupTime(QDate(2021, 6, 14), QTime(03, 26, 0), QTimeZone::utc());

    runSimpleJob(geo, targetObject, startUTime, wakeupTime, true);
    QVERIFY(checkLastSlew(targetObject));
}

void TestEkosSchedulerOps::testDawnShutdown()
{
    // remove the various options that play with the dawn/dusk times
    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);

    // This test will iterate the scheduler every 40 simulated seconds (to save testing time).
    WithInterval interval(40000, scheduler);

    // At this geo/date, Dawn is calculated = .1625 of a day = 3:53am local = 10:52 UTC
    // If we started at 23:35 local time, as before, it's a little over 4 hours
    // or over 4*3600 iterations. Too many? Instead we start at 3am local.

    // According to https://www.timeanddate.com/sun/usa/san-francisco?month=6&year=2021
    // astronomical dawn for SF was 3:52am on 6/14/2021

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject*> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));

    // We'll start the scheduler at 3am local time.
    QDateTime startUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc()));
    // The job should start at 3:12am local time.
    QDateTime startJobUTime = startUTime.addSecs(180);
    // The job should be interrupted at the pre-dawn time, which is about 3:53am
    QDateTime preDawnUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 53, 0), QTimeZone::utc()));
    // Consider pre-dawn security range
    preDawnUTime = preDawnUTime.addSecs(-60.0 * std::abs(Options::preDawnTime()));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[0], startJobUTime, preDawnUTime, currentUTime, sleepMs, DEFAULT_TOLERANCE);
    parkAndSleep(currentUTime, sleepMs);

    const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), QTimeZone::utc());
    wakeupAndRestart(restartTime, currentUTime, sleepMs);
}

// Regression test: resuming a paused scheduler while Ekos and INDI are still connected must
// complete the startup again. The startup state is reset when the scheduler wakes up, but the
// INDI property check that moves startup on to the post-startup phase is not run again because
// INDI is already ready. The startup then never reached STARTUP_COMPLETE, so at the end of the
// night the scheduler neither parked the mount nor ran the preemptive shutdown.
void TestEkosSchedulerOps::testResumeCompletesStartup()
{
    const auto state = scheduler->moduleState();
    const auto process = scheduler->process();
    const bool startupEnabled = Options::schedulerStartupEnabled();
    Options::setSchedulerStartupEnabled(false);

    // A running session: observatory up, startup complete, then the scheduler was paused.
    state->setEkosCommunicationStatus(Ekos::Success);
    state->setIndiCommunicationStatus(Ekos::Success);
    state->setEkosState(Ekos::EKOS_READY);
    state->setIndiState(Ekos::INDI_READY);
    state->setStartupState(Ekos::STARTUP_COMPLETE);
    state->setSchedulerState(Ekos::SCHEDULER_PAUSED);

    // Resuming calls start() -> init(), which wakes the scheduler as after a preemptive shutdown.
    state->enablePreemptiveShutdown(Ekos::SchedulerModuleState::getLocalTime());
    process->wakeUpScheduler();
    QCOMPARE(state->schedulerState(), Ekos::SCHEDULER_RUNNING);
    QCOMPARE(state->startupState(), Ekos::STARTUP_IDLE);

    // Step the startup, Ekos and INDI checks in the order checkStatus() runs them on each tick.
    for (int tick = 0; tick < 10 && state->startupState() != Ekos::STARTUP_COMPLETE; ++tick)
    {
        if (process->checkStartupState() && process->checkEkosState())
            process->checkINDIState();
    }

    QCOMPARE(state->startupState(), Ekos::STARTUP_COMPLETE);
    QCOMPARE(state->indiState(), Ekos::INDI_READY);
    // Startup stays complete on the next tick, with nothing left to do.
    QVERIFY(process->checkStartupState());
    QVERIFY(process->checkINDIState());
    QCOMPARE(state->startupState(), Ekos::STARTUP_COMPLETE);

    Options::setSchedulerStartupEnabled(startupEnabled);
}

// Regression test: Pause lets the running job finish and then waits. If the job finishes while the
// scheduler is paused, resuming must not run that job again and leave it waiting for a capture that
// has already completed. The resume has to handle the end of the job as it would have without the pause:
// - a job that is done after one sequence, nothing else to do: the scheduler runs the shutdown,
// - a looping job, or a repeating job with batches left: its capture is started again,
// - a second job in the queue: the scheduler moves on to that job.
void TestEkosSchedulerOps::testResumeAfterJobCompletedWhilePaused_data()
{
    QTest::addColumn<int>("completion");
    QTest::addColumn<int>("repeats");
    QTest::addColumn<bool>("secondJob");

    QTest::newRow("Sequence") << static_cast<int>(Ekos::FINISH_SEQUENCE) << 0 << false;
    QTest::newRow("Loop") << static_cast<int>(Ekos::FINISH_LOOP) << 0 << false;
    QTest::newRow("Repeat") << static_cast<int>(Ekos::FINISH_REPEAT) << 2 << false;
    QTest::newRow("TwoJobs") << static_cast<int>(Ekos::FINISH_SEQUENCE) << 0 << true;
}

void TestEkosSchedulerOps::testResumeAfterJobCompletedWhilePaused()
{
    QFETCH(int, completion);
    QFETCH(int, repeats);
    QFETCH(bool, secondJob);
    TestEkosSchedulerHelper::CompletionCondition completionCondition;
    completionCondition.type = static_cast<Ekos::CompletionCondition>(completion);
    completionCondition.repeat = repeats;

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    SkyObject *altair = KStars::Instance()->data()->skyComposite()->findByName("Altair");
    SkyObject *vega = KStars::Instance()->data()->skyComposite()->findByName("Vega");
    QVERIFY(altair != nullptr && vega != nullptr);
    // 00:30 local time: both targets are well above the 30 degree minimum altitude, so the first job starts right away.
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(7, 30, 0), QTimeZone::utc());

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    const TestEkosSchedulerHelper::ScheduleSteps steps = {true, true, true, true};
    QVector<QString> esls, esqs;
    esls.push_back(TestEkosSchedulerHelper::getSchedulerFile(altair, m_startupCondition, completionCondition, steps,
                   false, true));
    esqs.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    if (secondJob)
    {
        esls.push_back(TestEkosSchedulerHelper::getSchedulerFile(vega, m_startupCondition, m_completionCondition, steps,
                       false, true));
        esqs.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    }
    startupJobs(geo, startUTime, &dir, esls, esqs, QDateTime(), currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);

    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    Ekos::SchedulerJob * const job = scheduler->activeJob();
    QCOMPARE(job->getName(), altair->name());

    {
        WithInterval interval(1000, scheduler);

        // Pause while capturing, then let the capture finish while the scheduler is paused.
        scheduler->pause();
        QCOMPARE(scheduler->moduleState()->schedulerState(), Ekos::SCHEDULER_PAUSED);
        capture->setStatus(Ekos::CAPTURE_COMPLETE);
        // Step the scheduler without changing the time: iterateScheduler() sets the clock on each step,
        // and while the scheduler isn't running every clock change re-evaluates and resets all jobs
        // (SchedulerProcess::simClockTimeChanged()). A running clock doesn't do that in the application.
        bool completed = false;
        for (int i = 0; i < DEFAULT_ITERATIONS && !completed; ++i)
        {
            QTest::qWait(QWAIT_TIME);
            scheduler->process()->runSchedulerIteration();
            completed = (job->getState() == Ekos::SCHEDJOB_COMPLETE);
        }
        QVERIFY2(completed, "Job did not complete while the scheduler was paused.");

        // Resume, as the Start button does. start() also arms the scheduler's own iteration timer;
        // disconnect it so that the test keeps stepping the scheduler alone.
        m_schedulerLog.clear();
        scheduler->process()->start();
        QObject::disconnect(&scheduler->moduleState()->iterationTimer(), &QTimer::timeout,
                            scheduler->process().data(), &Ekos::SchedulerProcess::iterate);

        const auto jobString = [&]()
        {
            return QString("state %1, stage %2, capture %3")
                   .arg(Ekos::SchedulerJob::jobStatusString(job->getState()),
                        Ekos::SchedulerJob::jobStageString(job->getStage()),
                        Ekos::getCaptureStatusString(capture->status()));
        };
        // Run long enough to also cover the capture inactivity check (CAPTURE_INACTIVITY_TIMEOUT, 120 s).
        if (completionCondition.type == Ekos::FINISH_SEQUENCE)
        {
            // The finished job must not be run again.
            bool reExecuted = false;
            if (secondJob)
            {
                // The scheduler moves on to the second job.
                const bool nextJob = iterateScheduler("Wait for Next Job After Resume", 4 * DEFAULT_ITERATIONS, &sleepMs,
                                                      &currentUTime, [&]() -> bool
                {
                    if (job->getState() == Ekos::SCHEDJOB_BUSY)
                        reExecuted = true;
                    return (scheduler->activeJob() != nullptr && scheduler->activeJob() != job &&
                            scheduler->activeJob()->getState() == Ekos::SCHEDJOB_BUSY);
                });
                QVERIFY2(!reExecuted, qPrintable("Job completed while paused was executed again after resume: " + jobString()));
                QVERIFY2(nextJob, qPrintable("Scheduler did not start the next job after resume: " + jobString()));
                QCOMPARE(scheduler->activeJob()->getName(), vega->name());
            }
            else
            {
                // Nothing left to do: the scheduler shuts the observatory down, as at the end of a night.
                const bool stopped = iterateScheduler("Wait for Shutdown After Resume", 4 * DEFAULT_ITERATIONS, &sleepMs,
                                                      &currentUTime, [&]() -> bool
                {
                    if (job->getState() == Ekos::SCHEDJOB_BUSY)
                        reExecuted = true;
                    return (scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_IDLE);
                });
                QVERIFY2(!reExecuted, qPrintable("Job completed while paused was executed again after resume: " + jobString()));
                QVERIFY2(stopped, qPrintable("Scheduler did not stop after resuming with no job left to run: " + jobString()));
                QVERIFY2(!m_schedulerLog.filter("proceeding to stop Ekos/INDI").isEmpty(),
                         qPrintable("Scheduler stopped without the shutdown procedure: " + m_schedulerLog.join(" | ")));
            }
            QCOMPARE(job->getState(), Ekos::SCHEDJOB_COMPLETE);
        }
        else
        {
            // The looping or repeating job continues: its capture must be started again.
            const bool restarted = iterateScheduler("Wait for Capture Restart After Resume", 4 * DEFAULT_ITERATIONS,
                                                    &sleepMs, &currentUTime, [&]() -> bool
            {
                return (job->getState() == Ekos::SCHEDJOB_BUSY &&
                        job->getStage() == Ekos::SCHEDSTAGE_CAPTURING &&
                        capture->status() == Ekos::CAPTURE_CAPTURING);
            });
            QVERIFY2(restarted, qPrintable("Job did not start capturing again after resume: " + jobString()));
            QVERIFY2(!m_schedulerLog.filter("is repeating").isEmpty(),
                     qPrintable("Job end was not handled after resume: " + m_schedulerLog.join(" | ")));
            // The batch finished while paused is counted once.
            if (completionCondition.type == Ekos::FINISH_REPEAT)
                QCOMPARE(job->getRepeatsRemaining(), repeats - 1);
        }
    }
}

// Regression test: a preemptive shutdown whose pre-shutdown queue fails leaves Ekos and INDI connected. The
// failure ends the shutdown before INDI is disconnected, and the scheduler keeps its wakeup timer. When the
// scheduler wakes up for the next job, it resets the startup state, but the INDI property check that moves the
// startup on to the post-startup phase is not run again because INDI is still ready. The startup then stayed at
// STARTUP_PRE_DEVICES: the next job ran without the post-startup queue (e.g. without unparking the mount), and at
// the end of the schedule the scheduler logged "No executable jobs found. Stopping scheduler." and stopped
// without the shutdown procedure.
//
// Job 1 (Altair) runs right away, job 2 (Vega) starts 90 minutes later, more than the preemptive shutdown time
// of 30 minutes. Both jobs only slew and capture. The queues are task queue files with one task without a device
// each, so the queue executor doesn't need an Ekos profile, which the mock modules don't have: the pre-shutdown
// task runs a script that exits with code 3, the post-startup task is a delay of 0 s. Rows:
// - PreQueueFails: Ekos and INDI are to be stopped after the shutdown (the default),
// - PreQueueFailsStopEkosOff: they are to be kept running, the same as the failure keeps them connected anyway,
// - PreQueueFailsPostStartupQueue: with a post-startup queue, which has to run after the wakeup, before job 2.
// The test checks the startup, and that the end of the schedule starts the shutdown procedure, not its result.
namespace
{
// Write a task queue file, as the queue manager saves it, with one task without a device: a script that exits
// with code 3 if fails is set, else a delay of 0 s. Returns the path of the file, or an empty string if it
// couldn't be written.
QString writeDevicelessQueue(const QTemporaryDir &dir, const QString &name, bool fails)
{
    QJsonObject task;
    QJsonObject action;
    if (fails)
    {
        // The script exists and is executable, so the action starts it and fails when it exits
        QFile script(dir.filePath(name + ".sh"));
        if (!script.open(QIODevice::WriteOnly) || script.write("#!/bin/sh\nexit 3\n") < 0)
            return QString();
        script.close();
        if (!script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner))
            return QString();
        task["name"] = QStringLiteral("Failing Script");
        task["template_id"] = QStringLiteral("script_execute");
        action["type"] = QStringLiteral("SCRIPT");
        action["script_path"] = script.fileName();
    }
    else
    {
        task["name"] = QStringLiteral("Short Delay");
        task["template_id"] = QStringLiteral("delay");
        action["type"] = QStringLiteral("DELAY");
        action["duration"] = 0;
        action["unit"] = static_cast<int>(Ekos::DelayAction::SECONDS);
    }
    action["timeout"] = 30;
    action["retries"] = 0;
    action["failure_action"] = static_cast<int>(Ekos::TaskAction::ABORT_QUEUE);
    task["device"] = QString();
    task["actions"] = QJsonArray{action};

    QJsonObject item;
    item["id"] = name;
    item["task"] = task;
    QJsonObject queue;
    queue["version"] = QStringLiteral("1.0");
    queue["items"] = QJsonArray{item};

    const QString path = dir.filePath(name + ".kstarsqueue");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(queue).toJson()) < 0)
        return QString();
    return path;
}

// The number of lines of the log that start with text
int countLogLines(const QStringList &log, const QString &text)
{
    int count = 0;
    for (const QString &line : log)
        if (line.startsWith(text))
            count++;
    return count;
}
}

void TestEkosSchedulerOps::testPreemptiveWakeupCompletesStartup_data()
{
    QTest::addColumn<bool>("stopEkosAfterShutdown");
    QTest::addColumn<bool>("postStartupQueue");

    QTest::newRow("PreQueueFails") << true << false;
    QTest::newRow("PreQueueFailsStopEkosOff") << false << false;
    QTest::newRow("PreQueueFailsPostStartupQueue") << true << true;
}

void TestEkosSchedulerOps::testPreemptiveWakeupCompletesStartup()
{
    QFETCH(bool, stopEkosAfterShutdown);
    QFETCH(bool, postStartupQueue);
    const auto state = scheduler->moduleState();

    // Restore the options that init() doesn't reset, also when a check fails
    struct KeepOptions
    {
        const bool startupEnabled = Options::schedulerStartupEnabled();
        const bool shutdownEnabled = Options::schedulerShutdownEnabled();
        const double preemptiveShutdownTime = Options::preemptiveShutdownTime();
        ~KeepOptions()
        {
            Options::setSchedulerStartupEnabled(startupEnabled);
            Options::setSchedulerShutdownEnabled(shutdownEnabled);
            Options::setPreemptiveShutdownTime(preemptiveShutdownTime);
        }
    } keepOptions;
    Options::setPreemptiveShutdown(true);
    Options::setPreemptiveShutdownTime(0.5);
    Options::setStopEkosAfterShutdown(stopEkosAfterShutdown);

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });
    // The shutdown states in the order they were set. The context object ends the connection with this function.
    QList<int> shutdownStates;
    QObject stateContext;
    connect(state.data(), &Ekos::SchedulerModuleState::shutdownStateChanged, &stateContext,
            [&shutdownStates](Ekos::ShutdownState newState)
    {
        shutdownStates.append(newState);
    });

    QTemporaryDir queueDir(KTest::tempDirPattern(QStringLiteral("task-queues")));
    QVERIFY(queueDir.isValid());
    const QString preShutdownQueueFile = writeDevicelessQueue(queueDir, "pre-shutdown", true);
    QVERIFY(!preShutdownQueueFile.isEmpty());
    const QString postStartupQueueFile = postStartupQueue ? writeDevicelessQueue(queueDir, "post-startup", false) : QString();
    QVERIFY(!postStartupQueue || !postStartupQueueFile.isEmpty());

    // 00:30 local time, as in testResumeAfterJobCompletedWhilePaused: Altair is well above the minimum altitude of
    // 30 degrees, and so is Vega at 02:00 local time, when job 2 starts.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    SkyObject *altair = KStars::Instance()->data()->skyComposite()->findByName("Altair");
    SkyObject *vega = KStars::Instance()->data()->skyComposite()->findByName("Vega");
    QVERIFY(altair != nullptr && vega != nullptr);
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(7, 30, 0), QTimeZone::utc());
    const QDateTime job2UTime(QDate(2021, 6, 14), QTime(9, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);
    // The start time is local time, with the scheduler's UTC offset for this date, which includes DST
    const QTimeZone geoTZ(static_cast<int>(KStarsData::Instance()->geo()->TZ() * 3600));
    TestEkosSchedulerHelper::StartupCondition job2Startup;
    job2Startup.type = Ekos::START_AT;
    job2Startup.culminationOffset = 0;
    job2Startup.atLocalDateTime = job2UTime.toTimeZone(geoTZ);

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    const TestEkosSchedulerHelper::ScheduleSteps trackOnly = {true, false, false, false};
    QVector<QString> esls, esqs;
    esls.push_back(TestEkosSchedulerHelper::getSchedulerFile(altair, m_startupCondition, m_completionCondition, trackOnly,
                   false, true));
    esqs.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    esls.push_back(TestEkosSchedulerHelper::getSchedulerFile(vega, job2Startup, m_completionCondition, trackOnly,
                   false, true));
    esqs.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    startupJobs(geo, startUTime, &dir, esls, esqs, QDateTime(), currentUTime, sleepMs);
    if (QTest::currentTestFailed())
        return;
    Ekos::SchedulerJob *job1 = nullptr;
    Ekos::SchedulerJob *job2 = nullptr;
    for (const auto job : state->jobs())
    {
        if (job->getName() == altair->name())
            job1 = job;
        else if (job->getName() == vega->name())
            job2 = job;
    }
    QVERIFY(job1 != nullptr && job2 != nullptr);

    WithInterval interval(1000, scheduler);
    QVERIFY(iterateScheduler("Wait for Job 1 Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Mount::MOUNT_SLEWING)
            mount->setStatus(ISD::Mount::MOUNT_TRACKING);
        return (scheduler->activeJob() == job1 && job1->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    QCOMPARE(state->startupState(), Ekos::STARTUP_COMPLETE);

    // The queues are set once the job runs, so that loading the schedule can't change them
    Options::setSchedulerShutdownEnabled(true);
    state->setPreShutdownScriptURL(QUrl::fromLocalFile(preShutdownQueueFile));
    if (postStartupQueue)
    {
        Options::setSchedulerStartupEnabled(true);
        state->setPreStartupScriptURL(QUrl());
        state->setPostStartupScriptURL(QUrl::fromLocalFile(postStartupQueueFile));
    }

    // Job 1 ends. Job 2 is due in about 85 minutes, so the scheduler shuts the observatory down until then
    // and runs the pre-shutdown queue.
    m_schedulerLog.clear();
    capture->setStatus(Ekos::CAPTURE_COMPLETE);
    QVERIFY2(iterateScheduler("Wait for Preemptive Shutdown", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (state->preemptiveShutdown() && state->timerState() == Ekos::RUN_WAKEUP &&
                state->shutdownState() != Ekos::SHUTDOWN_IDLE);
    }), qPrintable("No preemptive shutdown after job 1: " + m_schedulerLog.join(" | ")));
    QCOMPARE(job1->getState(), Ekos::SCHEDJOB_COMPLETE);
    QCOMPARE(static_cast<int>(m_schedulerLog.filter("Observatory scheduled for shutdown until next job is ready.").size()), 1);
    QCOMPARE(countLogLines(m_schedulerLog, "Executing pre-shutdown tasks from "), 1);
    // The queue fails while the scheduler waits for the wakeup. Wait for that without stepping the scheduler,
    // its next step is the wakeup.
    QTRY_COMPARE_WITH_TIMEOUT(state->shutdownState(), Ekos::SHUTDOWN_ERROR, 10000);
    QCOMPARE(countLogLines(m_schedulerLog, "Pre-shutdown queue failed: "), 1);
    // Ekos and INDI stay connected until the wakeup, also when they are to be stopped after a shutdown
    QCOMPARE(state->timerState(), Ekos::RUN_WAKEUP);
    QCOMPARE(state->ekosState(), Ekos::EKOS_READY);
    QCOMPARE(state->indiState(), Ekos::INDI_READY);
    QCOMPARE(ekos->indiStatus(), Ekos::Success);

    // The scheduler wakes up when job 2 is due and starts the observatory up again before running job 2
    const int wakeupLogStart = m_schedulerLog.size();
    int startupStateAtJob2 = -1;
    int logSizeAtJob2 = -1;
    QVERIFY2(iterateScheduler("Wait for Job 2 After Wakeup", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (job2->getState() != Ekos::SCHEDJOB_BUSY)
            return false;
        startupStateAtJob2 = state->startupState();
        logSizeAtJob2 = m_schedulerLog.size();
        return true;
    }), qPrintable("Job 2 didn't start after the wakeup: " + m_schedulerLog.mid(wakeupLogStart).join(" | ")));
    QVERIFY2(std::abs(KStarsData::Instance()->ut().secsTo(job2UTime)) < timeTolerance(DEFAULT_TOLERANCE),
             qPrintable(QString("Job 2 started at %1 UTC").arg(KStarsData::Instance()->ut().toString("hh:mm:ss"))));
    const QStringList wakeupLog = m_schedulerLog.mid(wakeupLogStart, logSizeAtJob2 - wakeupLogStart);
    QVERIFY(wakeupLog.contains("Scheduler is awake."));

    QVERIFY(iterateScheduler("Wait for Job 2 Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Mount::MOUNT_SLEWING)
            mount->setStatus(ISD::Mount::MOUNT_TRACKING);
        return (scheduler->activeJob() == job2 && job2->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    QVERIFY(checkLastSlew(vega));

    // Job 2 ends and nothing is left to run: the end of the schedule starts the shutdown procedure, which runs
    // the pre-shutdown queue again. Its failure doesn't stop the shutdown, which ends with the scheduler stopped.
    const int endLogStart = m_schedulerLog.size();
    shutdownStates.clear();
    capture->setStatus(Ekos::CAPTURE_COMPLETE);
    QVERIFY2(iterateScheduler("Wait for Scheduler Stop", 2 * DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (state->schedulerState() == Ekos::SCHEDULER_IDLE);
    }), qPrintable("The scheduler didn't stop after job 2: " + m_schedulerLog.mid(endLogStart).join(" | ")));
    QCOMPARE(job2->getState(), Ekos::SCHEDJOB_COMPLETE);
    const QStringList endLog = m_schedulerLog.mid(endLogStart);

    // What happened after the wakeup, for the messages
    const QString summary = QString("startup state when job 2 started: \"%1\", log after the wakeup: %2")
                            .arg(Ekos::startupStateString(static_cast<Ekos::StartupState>(startupStateAtJob2)),
                                 m_schedulerLog.mid(wakeupLogStart).join(" | "));
    // The startup is complete before job 2 runs
    QVERIFY2(startupStateAtJob2 == Ekos::STARTUP_COMPLETE,
             qPrintable("Job 2 ran before the startup was complete, " + summary));
    QVERIFY2(wakeupLog.contains("INDI devices ready. Proceeding to post-startup phase..."),
             qPrintable("The startup didn't go on to the post-startup phase after the wakeup, " + summary));
    // The post-startup queue runs after the wakeup, before job 2
    if (postStartupQueue)
    {
        QVERIFY2(countLogLines(wakeupLog, "Executing post-startup tasks from " + postStartupQueueFile) == 1 &&
                 wakeupLog.contains("Post-startup queue completed successfully."),
                 qPrintable("The post-startup queue didn't run before job 2, " + summary));
    }
    // The end of the schedule runs the shutdown procedure, starting with the pre-shutdown queue
    QVERIFY2(!m_schedulerLog.contains("No executable jobs found. Stopping scheduler."),
             qPrintable("The scheduler stopped without the shutdown procedure, " + summary));
    QVERIFY2(shutdownStates.contains(Ekos::SHUTDOWN_PRE_QUEUE_RUNNING) &&
             countLogLines(endLog, "Executing pre-shutdown tasks from ") == 1,
             qPrintable("The end of the schedule didn't run the pre-shutdown queue, " + summary));
}

void TestEkosSchedulerOps::testPreemptiveShutdown()
{
    // Verifies the preemptive shutdown + automatic wakeup cycle for a FINISH_LOOP job.
    //
    // Scenario (mirrors user bug report with ngc3675 / FINISH_LOOP job):
    //   1. A FINISH_LOOP job runs from 3 am local until astronomical dawn (~3:53 am).
    //   2. The greedy scheduler bumps the job at dawn because the target drops below
    //      minAltitude. The next observable window is ~19 hours away (next evening).
    //   3. With preemptive shutdown enabled and the gap exceeding the threshold (2 h),
    //      shouldSchedulerSleep() must call enablePreemptiveShutdown() and initiate a
    //      full Ekos/INDI shutdown rather than just parking and sleeping.
    //   4. The scheduler enters RUN_WAKEUP state with the preemptive-shutdown flag set.
    //   5. When the wakeup timer fires the scheduler logs "Scheduler is awake.",
    //      restarts Ekos/INDI, and schedules the job for the new evening window.
    //
    // The "Scheduler is awake." message can only originate from wakeUpScheduler(),
    // which is the automatic path. A manual start logs only "Scheduler is starting…".
    // The 22:53:38 entry in the user log therefore confirms automatic wakeup, not a
    // manual restart.

    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);

    // Enable preemptive shutdown: if the next observation window is more than 2 hours
    // away the scheduler should shut down Ekos/INDI and set a wakeup timer.
    Options::setPreemptiveShutdown(true);
    Options::setPreemptiveShutdownTime(2.0);

    // Iterate the scheduler every 40 simulated seconds (matches testDawnShutdown).
    WithInterval interval(40000, scheduler);

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject *> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));

    // FINISH_LOOP: the job never self-completes; it runs until the scheduler decides
    // there is no more observable time (just like the ngc3675 job in the bug report).
    m_completionCondition.type = Ekos::FINISH_LOOP;

    // Start the scheduler at 3:10 am local (10:10 UTC).  Altair is above the horizon.
    QDateTime startUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc()));
    // The job should begin about 3 minutes after the scheduler starts.
    QDateTime startJobUTime = startUTime.addSecs(180);
    // Astronomical dawn for Silicon Valley on this date is ~3:53 am local = 10:53 UTC.
    // With preDawnTime = 0 the interrupt happens right at astronomical dawn.
    QDateTime preDawnUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 53, 0), QTimeZone::utc()));
    // Consider pre-dawn security range (same expression as testDawnShutdown)
    preDawnUTime = preDawnUTime.addSecs(-60.0 * std::abs(Options::preDawnTime()));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    // Bring up the scheduler, start Ekos/INDI, slew, and begin capturing.
    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[0], startJobUTime, preDawnUTime, currentUTime, sleepMs, DEFAULT_TOLERANCE);

    // At this point the job has been interrupted by dawn.  Because FINISH_LOOP never
    // marks the job complete, the greedy scheduler will find the next Altair window
    // on the same evening (~10:30 pm local = 19 h away).  With preemptive shutdown
    // enabled (threshold = 2 h) shouldSchedulerSleep() must:
    //   a) call enablePreemptiveShutdown(nextStartupTime)
    //   b) trigger checkShutdownState() → full Ekos/INDI shutdown
    //   c) leave the scheduler in RUN_WAKEUP state (not RUN_NOTHING / fully stopped)
    //
    // If the preemptive-shutdown flag is NOT set the scheduler would either fully stop
    // (and never wake up automatically) or log a misleading warning suggesting the user
    // enable preemptive shutdown.

    // Wait for the scheduler to reach RUN_WAKEUP (preemptive sleep) state.
    QVERIFY(iterateScheduler("Wait for Preemptive Shutdown (RUN_WAKEUP)",
                             DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
    }));

    // The preemptive-shutdown flag must be set; without it stop() would have
    // switched to RUN_NOTHING and the wakeup timer would never fire.
    QVERIFY2(scheduler->moduleState()->preemptiveShutdown(),
             "Preemptive shutdown flag must be set so the scheduler can wake up automatically");

    // The wakeup time stored in the module state must be valid (non-null).
    QVERIFY2(scheduler->moduleState()->preemptiveShutdownWakeupTime().isValid(),
             "Preemptive shutdown wakeup time must be valid");

    // Verify the stored wakeup time is in the future relative to the dawn interrupt
    // time, which would be wrong if the scheduler woke up immediately.
    QVERIFY(preDawnUTime < scheduler->moduleState()->preemptiveShutdownWakeupTime());

    // The scheduler should automatically wake up when the stored wakeup time arrives,
    // log "Scheduler is awake.", and restart Ekos/INDI.
    //
    // Implementation note: wakeupAndRestart() cannot be used directly here because
    // after a *preemptive* shutdown Ekos is fully stopped, whereas in testDawnShutdown
    // (no preemptive shutdown) Ekos stays running.  After the wakeup timer fires the
    // test must re-add the mock Ekos modules before the scheduler can finish starting
    // its devices.  The block below mirrors the startup logic from startupJobs2().
    //
    // Altair rises above 30° at ~10:31 pm local = 06:31 UTC on 6/15.
    const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), QTimeZone::utc());

    // Step 1 – advance the simulated clock to the wakeup time.
    // The scheduler is currently in RUN_WAKEUP; the next call to runSchedulerIteration()
    // returns sleepMs = (time-to-wakeup * 1000 / clock-scale).  iterateScheduler will
    // add that many simulated seconds to currentUTime and invoke wakeUpScheduler(),
    // which: disables preemptiveShutdown, logs "Scheduler is awake.", and calls start().
    QVERIFY(iterateScheduler("Wait for Automatic Wakeup (RUN_SCHEDULER)",
                             DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
    }));

    // The preemptive shutdown flag must be cleared by wakeUpScheduler().
    QVERIFY2(!scheduler->moduleState()->preemptiveShutdown(),
             "wakeUpScheduler() must clear preemptiveShutdown flag");

    // The simulated clock must be approximately at the expected wakeup window.
    QVERIFY2(std::abs(KStarsData::Instance()->ut().secsTo(restartTime)) < timeTolerance(DEFAULT_TOLERANCE),
             QString("Scheduler woke at wrong time: expected ~%1 UTC, got ~%2 UTC (diff %3 s)")
             .arg(restartTime.toString("hh:mm"))
             .arg(KStarsData::Instance()->ut().toString("hh:mm"))
             .arg(KStarsData::Instance()->ut().secsTo(restartTime))
             .toLocal8Bit());

    // Step 2 – re-add mock Ekos modules and verify the scheduler can restart Ekos/INDI.
    // (Ekos was fully stopped by the preemptive shutdown, so devices must be re-registered.)
    {
        WithInterval interval(1000, scheduler);
        bool sentOnce = false, readyOnce = false;
        QVERIFY(iterateScheduler("Wait for Ekos/INDI Restart after Preemptive Wakeup",
                                 30, &sleepMs, &currentUTime, [&]() -> bool
        {
            if ((scheduler->moduleState()->indiState() == Ekos::INDI_READY) &&
                    (scheduler->moduleState()->ekosState() == Ekos::EKOS_READY))
                return true;

            if (ekos->ekosStatus() == Ekos::Success)
            {
                if (!sentOnce)
                {
                    sentOnce = true;
                    ekos->addModule("Focus");
                    ekos->addModule("Capture");
                    ekos->addModule("Mount");
                    ekos->addModule("Align");
                    ekos->addModule("Guide");
                }
                else if (scheduler->process()->mountInterface() != nullptr &&
                         scheduler->process()->captureInterface() != nullptr && !readyOnce)
                {
                    readyOnce = true;
                    mount->sendReady();
                    capture->sendReady();
                }
            }
            return false;
        }));
    }
}

// Regression test for the bug where Ekos was not stopped during a preemptive shutdown
// when a pre-shutdown task queue was configured.
//
// Root cause: in queueExecutionCompleted(), when the pre-shutdown queue finished during a
// preemptive shutdown (timer = RUN_WAKEUP), the code set shutdownState = SHUTDOWN_STOPPING_EKOS
// but left the iteration timer in RUN_WAKEUP mode.  Because runSchedulerIteration() only
// dispatches to checkShutdownState() when the timer is RUN_SHUTDOWN, the SHUTDOWN_STOPPING_EKOS
// state was never processed and Ekos/INDI remained running for the entire sleep period.
//
// Fix: queueExecutionCompleted() now calls setupNextIteration(RUN_SHUTDOWN) unconditionally
// after setting SHUTDOWN_STOPPING_EKOS, guaranteeing that checkShutdownState() is reached
// on the very next scheduler tick regardless of what the timer was set to before.
void TestEkosSchedulerOps::testPreemptiveShutdownTimerSwitchOnQueueComplete()
{
    // This test manipulates the scheduler module state directly — it does not need to
    // run a full simulation cycle (no jobs, no mount, no capture).  The init() fixture
    // already created a fully wired Scheduler + SchedulerProcess + SchedulerModuleState.

    auto state = scheduler->process()->moduleState();

    // -----------------------------------------------------------------------
    // Set up the exact pre-condition that caused the regression:
    //   • The scheduler is RUNNING (not idle/paused).
    //   • A preemptive shutdown is active: shouldSchedulerSleep() found the next
    //     observable window is > preemptiveShutdownTime hours away and called
    //     enablePreemptiveShutdown(), which also called setupNextIteration(RUN_WAKEUP).
    //   • The pre-shutdown queue has *just started* executing (SHUTDOWN_PRE_QUEUE_RUNNING).
    //   • The iteration timer is still RUN_WAKEUP — checkShutdownState() has NOT been
    //     given its own timer slot yet, which is the race that caused the bug.
    //   • This is a time-based preemptive shutdown, NOT a weather grace-period event.
    // -----------------------------------------------------------------------
    state->setSchedulerState(Ekos::SCHEDULER_RUNNING);
    state->enablePreemptiveShutdown(QDateTime::currentDateTime().addSecs(3600));
    QVERIFY(state->preemptiveShutdown());

    state->setShutdownState(Ekos::SHUTDOWN_PRE_QUEUE_RUNNING);
    state->setTimerState(Ekos::RUN_WAKEUP);     // ← the buggy pre-state
    state->setWeatherGracePeriodActive(false);

    // Confirm pre-conditions are exactly right before calling the slot.
    QCOMPARE(state->timerState(), Ekos::RUN_WAKEUP);
    QCOMPARE(state->shutdownState(), Ekos::SHUTDOWN_PRE_QUEUE_RUNNING);

    // -----------------------------------------------------------------------
    // Simulate the pre-shutdown queue completing by directly invoking the slot.
    // In normal operation this is triggered by QueueExecutor::completed signal.
    // Qt's meta-object system allows invoking private slots from outside the class
    // (they are registered in the meta-object table just like public slots).
    // -----------------------------------------------------------------------
    const bool invoked = QMetaObject::invokeMethod(
                             scheduler->process().data(),
                             "queueExecutionCompleted",
                             Qt::DirectConnection);
    QVERIFY2(invoked, "queueExecutionCompleted() must be invocable as a Qt slot");

    // -----------------------------------------------------------------------
    // Part 1 regression assertions:
    //
    //   1. The timer MUST have switched from RUN_WAKEUP to RUN_SHUTDOWN.
    //      Without this switch runSchedulerIteration() keeps dispatching to
    //      wakeUpScheduler() and never calls checkShutdownState(), so
    //      SHUTDOWN_STOPPING_EKOS is processed by nobody and Ekos never stops.
    //
    //   2. The shutdown state MUST be SHUTDOWN_STOPPING_EKOS so that the very
    //      next checkShutdownState() tick proceeds to disconnect INDI and stop Ekos.
    // -----------------------------------------------------------------------
    QVERIFY2(state->timerState() == Ekos::RUN_SHUTDOWN,
             "REGRESSION: timer must switch from RUN_WAKEUP to RUN_SHUTDOWN after "
             "the pre-shutdown queue completes so that SHUTDOWN_STOPPING_EKOS is "
             "processed and Ekos is actually stopped during preemptive shutdown.");

    QVERIFY2(state->shutdownState() == Ekos::SHUTDOWN_STOPPING_EKOS,
             "Shutdown state must advance to SHUTDOWN_STOPPING_EKOS after the "
             "pre-shutdown queue completes.");

    // -----------------------------------------------------------------------
    // Part 2 – verify the complete dispatch chain reaches stop().
    //
    // With timer = RUN_SHUTDOWN, one call to runSchedulerIteration() must
    // dispatch to checkShutdownState() which handles SHUTDOWN_STOPPING_EKOS
    // by calling completeShutdown():
    //
    //   completeShutdown():
    //     INDI state  = INDI_IDLE  (never started in this test) → no disconnection
    //     Ekos state  = EKOS_IDLE  (never started in this test) → no stop D-Bus call
    //     Post-queue  = empty                                   → no post-shutdown queue
    //     ∴  shutdownState = SHUTDOWN_COMPLETE → stop() is called
    //
    //   stop():
    //     schedulerState = SCHEDULER_IDLE
    //     timerState     = RUN_NOTHING
    //
    // Without the fix, the timer stays RUN_WAKEUP after queueExecutionCompleted().
    // runSchedulerIteration() would therefore call wakeUpScheduler() instead of
    // checkShutdownState(), stop() would never be reached, and the scheduler
    // would remain SCHEDULER_RUNNING for the entire ~15-hour sleep period (exactly
    // what the user observed in the ngc3675 session log).
    // -----------------------------------------------------------------------
    scheduler->process()->runSchedulerIteration();

    // schedulerState() == SCHEDULER_IDLE is the key discriminator:
    //   • WITH the fix   — checkShutdownState() is dispatched → completeShutdown()
    //                      → stop() → schedulerState = SCHEDULER_IDLE
    //   • WITHOUT the fix — wakeUpScheduler() is dispatched → scheduler stays
    //                        SCHEDULER_RUNNING, Ekos alive for the whole sleep period
    //
    // Note: because the preemptiveShutdown flag is still set when stop() runs,
    // stop() calls setupNextIteration(RUN_WAKEUP, 10) to re-enter sleep mode
    // (waiting for the next scheduled window) rather than RUN_NOTHING.  This is
    // correct preemptive-shutdown behaviour: Ekos/INDI are now stopped and the
    // scheduler will wake up automatically at the configured wakeup time.
    // We do NOT assert timerState() == RUN_NOTHING here — that would be wrong for
    // the preemptive shutdown path.
    QVERIFY2(scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_IDLE,
             "REGRESSION: after the RUN_SHUTDOWN iteration the scheduler must have "
             "reached stop() via checkShutdownState→completeShutdown. Without the "
             "fix wakeUpScheduler() is called instead and the scheduler remains "
             "SCHEDULER_RUNNING, keeping Ekos alive throughout the sleep period.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Shutdown and park wait for a meridian flip or a slew (mock modules, simulated clock)
//
// When a meridian flip comes due during the last frame, capture releases it when the
// sequence ends, and the flip slew starts while the scheduler begins the shutdown. A park
// sent then is refused by the driver ("Can not park while slewing"). The scheduler must
// wait until the flip (planned, waiting, accepted or running) or the slew is over before
// it starts the shutdown or parks the mount until the next job, at most 5 minutes (2 minutes
// during a weather shutdown, see testMountMotionWaitTimeout and
// testWeatherShutdownWaitsForMountMotion).
//
// The mock mount reports the flip state through the read property meridianFlipStatus
// (MeridianFlipState::MeridianFlipMountState as int) and the slew through status.
// No pre-shutdown queue is configured (see init()), so "the shutdown started" is the log
// line "No pre-shutdown tasks, proceeding to stop Ekos/INDI.". A wait shows as timer state
// RUN_SHUTDOWN with shutdown state SHUTDOWN_IDLE.
//
// That no flip is planned or started while the pre-shutdown queue parks the mount
// (Requirement 1.5) is decided by MeridianFlipState, which the mock mount doesn't have:
// see TestEkosMeridianFlipState::testNoFlipWhileParking.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
enum MountMotionScenario
{
    // The last job ends (capture complete), the scheduler shuts down at the end of the schedule
    MOTION_AT_JOB_END,
    // Dawn interrupts the job, the scheduler parks the mount until the next night (testDawnShutdown)
    MOTION_AT_PARK_UNTIL_NEXT_JOB,
    // Dawn interrupts the job, the scheduler shuts down until the next night (testPreemptiveShutdown)
    MOTION_AT_PREEMPTIVE_SHUTDOWN
};

const QString kFlipWaitLog("Waiting for the meridian flip to finish before parking the mount...");
const QString kSlewWaitLog("Waiting for the mount to stop slewing before parking...");
const QString kMotionTimeoutLog("Warning: the mount is still busy after 5 minutes, continuing.");
const QString kShutdownStartLog("No pre-shutdown tasks, proceeding to stop Ekos/INDI.");
const QString kShutdownCompleteLog("Shutdown complete.");
const QString kParkUntilNextJobLog("Parking the mount until the job is ready.");

// Iteration period of the job end rows: the 5 minute wait timeout takes 30 iterations.
constexpr int kJobEndIntervalMs = 10000;

// Index of the first log line that contains text, -1 if there is none.
int logIndex(const QStringList &log, const QString &text)
{
    for (int i = 0; i < log.size(); ++i)
        if (log[i].contains(text))
            return i;
    return -1;
}
}

void TestEkosSchedulerOps::testShutdownWaitsForMountMotion_data()
{
    QTest::addColumn<int>("scenario");
    // Mount status and flip state while the mount is busy
    QTest::addColumn<int>("busyMountStatus");
    QTest::addColumn<int>("busyFlipStatus");
    // Flip state once the motion is over (the mount status is then MOUNT_TRACKING)
    QTest::addColumn<int>("doneFlipStatus");
    // The expected wait message, empty if the scheduler must not wait
    QTest::addColumn<QString>("waitLog");
    // The motion never ends: the scheduler must go on after the wait timeout
    QTest::addColumn<bool>("neverDone");

    const int tracking = ISD::Mount::MOUNT_TRACKING;
    QTest::newRow("Slewing") << int(MOTION_AT_JOB_END) << int(ISD::Mount::MOUNT_SLEWING)
                             << int(MeridianFlipState::MOUNT_FLIP_NONE) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                             << kSlewWaitLog << false;
    QTest::newRow("FlipPlanned") << int(MOTION_AT_JOB_END) << tracking
                                 << int(MeridianFlipState::MOUNT_FLIP_PLANNED) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                                 << kFlipWaitLog << false;
    QTest::newRow("FlipAccepted") << int(MOTION_AT_JOB_END) << tracking
                                  << int(MeridianFlipState::MOUNT_FLIP_ACCEPTED) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                                  << kFlipWaitLog << false;
    // The flip ends with COMPLETED, which must not block (it goes to NONE on the next mount update)
    QTest::newRow("FlipRunning") << int(MOTION_AT_JOB_END) << tracking
                                 << int(MeridianFlipState::MOUNT_FLIP_RUNNING) << int(MeridianFlipState::MOUNT_FLIP_COMPLETED)
                                 << kFlipWaitLog << false;
    // No motion: the shutdown starts as on master, without any wait
    QTest::newRow("NotMoving") << int(MOTION_AT_JOB_END) << tracking
                               << int(MeridianFlipState::MOUNT_FLIP_NONE) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                               << QString() << false;
    QTest::newRow("NotMovingFlipCompleted") << int(MOTION_AT_JOB_END) << tracking
                                            << int(MeridianFlipState::MOUNT_FLIP_COMPLETED)
                                            << int(MeridianFlipState::MOUNT_FLIP_COMPLETED) << QString() << false;
    QTest::newRow("NotMovingFlipError") << int(MOTION_AT_JOB_END) << tracking
                                        << int(MeridianFlipState::MOUNT_FLIP_ERROR) << int(MeridianFlipState::MOUNT_FLIP_ERROR)
                                        << QString() << false;
    QTest::newRow("Timeout") << int(MOTION_AT_JOB_END) << tracking
                             << int(MeridianFlipState::MOUNT_FLIP_ACCEPTED) << int(MeridianFlipState::MOUNT_FLIP_ACCEPTED)
                             << kFlipWaitLog << true;
    QTest::newRow("ParkUntilNextJob") << int(MOTION_AT_PARK_UNTIL_NEXT_JOB) << tracking
                                      << int(MeridianFlipState::MOUNT_FLIP_ACCEPTED) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                                      << kFlipWaitLog << false;
    QTest::newRow("PreemptiveShutdown") << int(MOTION_AT_PREEMPTIVE_SHUTDOWN) << tracking
                                        << int(MeridianFlipState::MOUNT_FLIP_ACCEPTED) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                                        << kFlipWaitLog << false;
}

void TestEkosSchedulerOps::testShutdownWaitsForMountMotion()
{
    QFETCH(int, scenario);
    QFETCH(int, busyMountStatus);
    QFETCH(int, busyFlipStatus);
    QFETCH(int, doneFlipStatus);
    QFETCH(QString, waitLog);
    QFETCH(bool, neverDone);

    const bool keepPreemptiveShutdown = Options::preemptiveShutdown();
    const double keepPreemptiveShutdownTime = Options::preemptiveShutdownTime();
    auto restoreOptions = qScopeGuard([ = ]()
    {
        Options::setPreemptiveShutdown(keepPreemptiveShutdown);
        Options::setPreemptiveShutdownTime(keepPreemptiveShutdownTime);
    });

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });

    if (scenario == MOTION_AT_JOB_END)
        checkShutdownWaitAtJobEnd(busyMountStatus, busyFlipStatus, doneFlipStatus, waitLog, neverDone);
    else
        checkMountWaitAtDawn(scenario == MOTION_AT_PREEMPTIVE_SHUTDOWN, busyFlipStatus, doneFlipStatus, waitLog);
}

// Number of scheduler log lines (since the last clear) that contain text.
int TestEkosSchedulerOps::schedulerLogCount(const QString &text) const
{
    return m_schedulerLog.filter(text).size();
}

// Run an Altair job from 3:10am local in Silicon Valley (as the weather tests do) and end it
// with the mount in the given state. The shutdown at the end of the schedule must wait until
// the motion is over, or until the wait timeout, and then run as before.
void TestEkosSchedulerOps::checkShutdownWaitAtJobEnd(int busyMountStatus, int busyFlipStatus, int doneFlipStatus,
        const QString &waitLog, bool neverDone)
{
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject *> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc());

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    capture->setStatus(Ekos::CAPTURE_CAPTURING);
    QCOMPARE(scheduler->moduleState()->startupState(), Ekos::STARTUP_COMPLETE);

    WithInterval interval(kJobEndIntervalMs, scheduler);
    auto state = scheduler->moduleState();
    const auto busyText = QString("mount status %1, meridian flip state %2").arg(busyMountStatus).arg(busyFlipStatus);

    // The last frame ends while the mount is busy, e.g. with the flip that capture released when its sequence ended
    mount->setStatus(static_cast<ISD::Mount::Status>(busyMountStatus));
    mount->setMeridianFlipStatus(busyFlipStatus);
    m_schedulerLog.clear();
    capture->setStatus(Ekos::CAPTURE_COMPLETE);

    // An iteration that ends waiting: the shutdown is due (RUN_SHUTDOWN) but has not started
    int waitingIterations = 0;
    auto countWaiting = [&]()
    {
        if (state->timerState() == Ekos::RUN_SHUTDOWN && state->shutdownState() == Ekos::SHUTDOWN_IDLE &&
                state->schedulerState() == Ekos::SCHEDULER_RUNNING)
            waitingIterations++;
    };

    if (waitLog.isEmpty())
    {
        // Nothing moves: the shutdown starts right after the job, as on master
        QVERIFY2(iterateScheduler("Shutdown without mount motion", 5, &sleepMs, &currentUTime, [&]() -> bool
        {
            countWaiting();
            return schedulerLogCount(kShutdownStartLog) > 0;
        }), "The shutdown didn't start after the last job");
        QCOMPARE(waitingIterations, 0);
        QCOMPARE(schedulerLogCount(kFlipWaitLog), 0);
        QCOMPARE(schedulerLogCount(kSlewWaitLog), 0);
    }
    else
    {
        // Two minutes with the mount busy, well within the wait timeout: the job completes, the shutdown waits
        QDateTime waitStartUTime;
        iterateScheduler("Mount busy at the end of the job", 12, &sleepMs, &currentUTime, [&]() -> bool
        {
            countWaiting();
            if (!waitStartUTime.isValid() && schedulerLogCount(waitLog) > 0)
                waitStartUTime = KStarsData::Instance()->ut();
            return schedulerLogCount(kShutdownStartLog) > 0;
        });
        QCOMPARE(state->jobs()[0]->getState(), Ekos::SCHEDJOB_COMPLETE);
        QVERIFY2(schedulerLogCount(kShutdownStartLog) == 0,
                 qPrintable(QString("The shutdown started while the mount was busy (%1)").arg(busyText)));
        QVERIFY2(schedulerLogCount(waitLog) == 1,
                 qPrintable(QString("Expected the wait message \"%1\" once, got %2 times")
                            .arg(waitLog).arg(schedulerLogCount(waitLog))));
        QVERIFY(waitStartUTime.isValid());
        QVERIFY(waitingIterations > 0);
        QCOMPARE(state->timerState(), Ekos::RUN_SHUTDOWN);
        QCOMPARE(state->shutdownState(), Ekos::SHUTDOWN_IDLE);
        QCOMPARE(ekos->ekosStatus(), Ekos::Success);

        if (neverDone)
        {
            // The flip never ends: after 5 minutes the scheduler warns once and shuts down anyway
            QVERIFY2(iterateScheduler("Mount busy until the wait timeout", 30, &sleepMs, &currentUTime, [&]() -> bool
            {
                return schedulerLogCount(kShutdownStartLog) > 0;
            }), "The shutdown didn't start after the wait timeout");
            QCOMPARE(schedulerLogCount(kMotionTimeoutLog), 1);
            const qint64 waitedSecs = waitStartUTime.secsTo(KStarsData::Instance()->ut());
            QVERIFY2(waitedSecs >= 300 && waitedSecs <= 300 + 2 * kJobEndIntervalMs / 1000,
                     qPrintable(QString("The shutdown started %1 s after the wait started, expected 300 s").arg(waitedSecs)));
        }
        else
        {
            // The motion is over: the shutdown starts in the next iteration
            mount->setStatus(ISD::Mount::MOUNT_TRACKING);
            mount->setMeridianFlipStatus(doneFlipStatus);
            QVERIFY2(iterateScheduler("Mount motion finished", 1, &sleepMs, &currentUTime, [&]() -> bool
            {
                return schedulerLogCount(kShutdownStartLog) > 0;
            }), qPrintable(QString("The shutdown didn't start once the motion was over (flip state %1)").arg(doneFlipStatus)));
            QCOMPARE(schedulerLogCount(kMotionTimeoutLog), 0);
        }
    }

    // The rest of the shutdown runs as before: INDI disconnected, Ekos stopped, scheduler stopped
    QVERIFY(iterateScheduler("Wait for Shutdown", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (state->schedulerState() == Ekos::SCHEDULER_IDLE);
    }));
    QCOMPARE(state->timerState(), Ekos::RUN_NOTHING);
    QCOMPARE(ekos->ekosStatus(), Ekos::Idle);
    QCOMPARE(state->jobs()[0]->getState(), Ekos::SCHEDJOB_COMPLETE);
    QCOMPARE(schedulerLogCount(kShutdownStartLog), 1);
    QCOMPARE(schedulerLogCount(kShutdownCompleteLog), 1);
    // The log is in order of arrival: the wait (if any), the shutdown start, the end of the shutdown
    const int shutdownStartIndex = logIndex(m_schedulerLog, kShutdownStartLog);
    if (!waitLog.isEmpty())
        QVERIFY(logIndex(m_schedulerLog, waitLog) < shutdownStartIndex);
    QVERIFY(shutdownStartIndex < logIndex(m_schedulerLog, kShutdownCompleteLog));
}

// Dawn interrupts the job while a meridian flip is accepted. Without preemptive shutdown the
// scheduler parks the mount until the next night (as in testDawnShutdown), with it the scheduler
// shuts down until then (as in testPreemptiveShutdown). Both must wait until the flip is over,
// and the scheduler must still wake up at the time of the next job.
void TestEkosSchedulerOps::checkMountWaitAtDawn(bool preemptive, int busyFlipStatus, int doneFlipStatus,
        const QString &waitLog)
{
    if (preemptive)
    {
        // The next window is ~19 hours away, more than the 2 hour threshold. FINISH_LOOP: the job never
        // completes, so the next window is the next evening (see testPreemptiveShutdown).
        Options::setPreemptiveShutdown(true);
        Options::setPreemptiveShutdownTime(2.0);
        m_completionCondition.type = Ekos::FINISH_LOOP;
    }

    // 40 simulated seconds per iteration (as testDawnShutdown): the dawn wait below lasts at most
    // 5 iterations, well within the 5 minute wait timeout.
    WithInterval interval(40000, scheduler);
    auto state = scheduler->moduleState();

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject *> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc());
    const QDateTime startJobUTime = startUTime.addSecs(180);
    const QDateTime preDawnUTime(QDate(2021, 6, 14), QTime(10, 53, 0), QTimeZone::utc());
    // Altair rises above 30 degrees at about 06:31 UTC on 2021-06-15.
    const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), QTimeZone::utc());

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    if (QTest::currentTestFailed())
        return;

    // The scheduler reads the flip state only before it shuts down or parks, so it can be set for the whole job
    mount->setMeridianFlipStatus(busyFlipStatus);
    m_schedulerLog.clear();
    slewAndRun(targetObjects[0], startJobUTime, preDawnUTime, currentUTime, sleepMs, DEFAULT_TOLERANCE);
    if (QTest::currentTestFailed())
        return;

    // A few more iterations with the flip accepted: no park, no shutdown
    const QString parkOrShutdownLog = preemptive ? kShutdownStartLog : kParkUntilNextJobLog;
    iterateScheduler("Flip accepted at dawn", 4, &sleepMs, &currentUTime, [&]() -> bool
    {
        return schedulerLogCount(parkOrShutdownLog) > 0 || mount->parkStatus() != ISD::PARK_UNPARKED;
    });
    QVERIFY2(schedulerLogCount(parkOrShutdownLog) == 0 && mount->parkStatus() == ISD::PARK_UNPARKED,
             qPrintable(QString("The scheduler %1 during the meridian flip (flip state %2, park status %3)")
                        .arg(preemptive ? "shut down" : "parked the mount").arg(busyFlipStatus).arg(mount->parkStatus())));
    QVERIFY2(schedulerLogCount(waitLog) == 1,
             qPrintable(QString("Expected the wait message \"%1\" once, got %2 times")
                        .arg(waitLog).arg(schedulerLogCount(waitLog))));
    QCOMPARE(state->schedulerState(), Ekos::SCHEDULER_RUNNING);
    QCOMPARE(ekos->ekosStatus(), Ekos::Success);

    if (!preemptive)
    {
        // The scheduler checks again every iteration
        QCOMPARE(state->timerState(), Ekos::RUN_SCHEDULER);

        // The flip is over: the mount is parked, the scheduler sleeps and wakes up for the next night
        mount->setMeridianFlipStatus(doneFlipStatus);
        parkAndSleep(currentUTime, sleepMs);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(schedulerLogCount(kParkUntilNextJobLog), 1);
        QCOMPARE(schedulerLogCount(kMotionTimeoutLog), 0);
        QVERIFY(logIndex(m_schedulerLog, waitLog) < logIndex(m_schedulerLog, kParkUntilNextJobLog));
        wakeupAndRestart(restartTime, currentUTime, sleepMs);
        return;
    }

    // Preemptive shutdown decided, wakeup time stored, the shutdown itself waits
    QVERIFY(state->preemptiveShutdown());
    const QDateTime wakeupTime = state->preemptiveShutdownWakeupTime();
    QVERIFY(wakeupTime.isValid());
    QVERIFY(preDawnUTime < wakeupTime);
    QCOMPARE(state->timerState(), Ekos::RUN_SHUTDOWN);
    QCOMPARE(state->shutdownState(), Ekos::SHUTDOWN_IDLE);

    // The flip is over: the shutdown runs (Ekos/INDI stopped), and the scheduler sleeps until the wakeup time
    mount->setMeridianFlipStatus(doneFlipStatus);
    QVERIFY(iterateScheduler("Wait for Preemptive Shutdown (RUN_WAKEUP)", DEFAULT_ITERATIONS, &sleepMs, &currentUTime,
                             [&]() -> bool
    {
        return (state->timerState() == Ekos::RUN_WAKEUP && state->schedulerState() == Ekos::SCHEDULER_IDLE);
    }));
    QVERIFY(state->preemptiveShutdown());
    QCOMPARE(state->preemptiveShutdownWakeupTime(), wakeupTime);
    QCOMPARE(ekos->ekosStatus(), Ekos::Idle);
    QCOMPARE(schedulerLogCount(kShutdownStartLog), 1);
    QCOMPARE(schedulerLogCount(kShutdownCompleteLog), 1);
    QCOMPARE(schedulerLogCount(kMotionTimeoutLog), 0);
    QVERIFY(logIndex(m_schedulerLog, waitLog) < logIndex(m_schedulerLog, kShutdownStartLog));
    QVERIFY(logIndex(m_schedulerLog, kShutdownStartLog) < logIndex(m_schedulerLog, kShutdownCompleteLog));

    // The scheduler wakes up at the stored time, at the start of the next window
    QVERIFY(iterateScheduler("Wait for Automatic Wakeup (RUN_SCHEDULER)", DEFAULT_ITERATIONS, &sleepMs, &currentUTime,
                             [&]() -> bool
    {
        return (state->timerState() == Ekos::RUN_SCHEDULER);
    }));
    QVERIFY(!state->preemptiveShutdown());
    QVERIFY2(std::abs(KStarsData::Instance()->ut().secsTo(wakeupTime)) < timeTolerance(60),
             qPrintable(QString("Woke up at %1 UTC, the stored wakeup time is %2 UTC")
                        .arg(KStarsData::Instance()->ut().toString("hh:mm:ss"), wakeupTime.toUTC().toString("hh:mm:ss"))));
    QVERIFY2(std::abs(KStarsData::Instance()->ut().secsTo(restartTime)) < timeTolerance(DEFAULT_TOLERANCE),
             qPrintable(QString("Woke up at %1 UTC, expected about %2 UTC")
                        .arg(KStarsData::Instance()->ut().toString("hh:mm"), restartTime.toString("hh:mm"))));
}

// ─────────────────────────────────────────────────────────────────────────────
// The wait decision for every mount status x meridian flip state
//
// testShutdownWaitsForMountMotion runs whole jobs for a few combinations. This test checks
// the decision itself, SchedulerProcess::mountMotionPending() and waitForMountMotion(), for
// all 7 x 7 combinations, read over D-Bus from the mock mount as during a real shutdown.
// Both call sites (checkShutdownState() and the park in shouldSchedulerSleep()) start the
// shutdown or the park only when waitForMountMotion() returns true.
//
// Property 1, no park while the mount is busy: while the mount slews or moves, or a flip is
// planned, waiting, accepted or running, waitForMountMotion() returns false until the motion
// is over or 300 s have passed since the wait started. The wait is logged once per wait, the
// timeout once.
// Property 2, no delay when the mount is idle: for every other combination it returns true
// on the first call and logs nothing.
//
// The time comes from SchedulerModuleState::setLocalTime() (the test hook of getLocalTime()),
// so the KStars clock isn't changed. No jobs are loaded and the scheduler isn't started.
// ─────────────────────────────────────────────────────────────────────────────

void TestEkosSchedulerOps::testMountMotionWaitDecision_data()
{
    QTest::addColumn<int>("mountStatus");
    QTest::addColumn<int>("flipStatus");
    // The expected wait message, empty if the scheduler must not wait
    QTest::addColumn<QString>("waitLog");

    // Every value of both enums, in enum order
    const char *const mountStatusNames[] = {"Idle", "Moving", "Slewing", "Tracking", "Parking", "Parked", "Error"};
    static_assert(static_cast<int>(std::size(mountStatusNames)) == ISD::Mount::MOUNT_ERROR + 1,
                  "Name every ISD::Mount::Status value");
    const char *const flipStatusNames[] = {"None", "Planned", "Waiting", "Accepted", "Running", "Completed", "Error"};
    static_assert(static_cast<int>(std::size(flipStatusNames)) == MeridianFlipState::MOUNT_FLIP_ERROR + 1,
                  "Name every MeridianFlipState::MeridianFlipMountState value");

    // The mount is busy (Requirement 1.1) while it slews or moves, e.g. a goto or the flip slew ...
    QList<int> busyMountStatuses;
    busyMountStatuses << ISD::Mount::MOUNT_SLEWING << ISD::Mount::MOUNT_MOVING;
    // ... or while a flip is planned or active. COMPLETED doesn't block: the flip slew has ended.
    QList<int> activeFlipStatuses;
    activeFlipStatuses << MeridianFlipState::MOUNT_FLIP_PLANNED << MeridianFlipState::MOUNT_FLIP_WAITING
                       << MeridianFlipState::MOUNT_FLIP_ACCEPTED << MeridianFlipState::MOUNT_FLIP_RUNNING;

    for (int mountStatus = ISD::Mount::MOUNT_IDLE; mountStatus <= ISD::Mount::MOUNT_ERROR; ++mountStatus)
    {
        for (int flipStatus = MeridianFlipState::MOUNT_FLIP_NONE; flipStatus <= MeridianFlipState::MOUNT_FLIP_ERROR;
                ++flipStatus)
        {
            // The flip is named when both apply: a flip slew is also a slew
            QString waitLog;
            if (activeFlipStatuses.contains(flipStatus))
                waitLog = kFlipWaitLog;
            else if (busyMountStatuses.contains(mountStatus))
                waitLog = kSlewWaitLog;
            QTest::addRow("%s/%s", mountStatusNames[mountStatus], flipStatusNames[flipStatus])
                    << mountStatus << flipStatus << waitLog;
        }
    }
}

void TestEkosSchedulerOps::testMountMotionWaitDecision()
{
    QFETCH(int, mountStatus);
    QFETCH(int, flipStatus);
    QFETCH(QString, waitLog);
    const bool pending = !waitLog.isEmpty();
    const QString pair = QString("mount status %1, meridian flip state %2").arg(mountStatus).arg(flipStatus);

    // Local time for waitForMountMotion(), seconds after startTime. Back to the KStars clock at the end.
    const KStarsDateTime startTime(QDate(2021, 6, 14), QTime(3, 10, 0));
    KStarsDateTime localTime(startTime);
    Ekos::SchedulerModuleState::setLocalTime(&localTime);
    auto restoreLocalTime = qScopeGuard([]()
    {
        Ekos::SchedulerModuleState::setLocalTime(nullptr);
    });
    auto setTime = [&](int secs)
    {
        localTime = startTime.addSecs(secs);
    };

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });

    // Only the mount module, found by the scheduler over D-Bus (see testBasics)
    ekos->addModule(QString("Mount"));
    QTRY_VERIFY_WITH_TIMEOUT(!scheduler->process()->mountInterface().isNull(), 5000);
    auto process = scheduler->process();

    auto setBusy = [&]()
    {
        mount->setStatus(static_cast<ISD::Mount::Status>(mountStatus));
        mount->setMeridianFlipStatus(flipStatus);
    };
    setBusy();
    m_schedulerLog.clear();
    QStringList expectedLog;

    // The decision and its reason, the text logged when the wait starts
    QString reason;
    QVERIFY2(process->mountMotionPending(reason) == pending,
             qPrintable(QString("Expected motion %1 (%2)").arg(pending ? "pending" : "not pending", pair)));
    QCOMPARE(reason, waitLog);
    QCOMPARE(m_schedulerLog, expectedLog);

    if (!pending)
    {
        // Property 2: the shutdown or park goes on at once and nothing is logged, also when asked again
        QVERIFY2(process->waitForMountMotion(), qPrintable(QString("Waited although the mount is idle (%1)").arg(pair)));
        setTime(1);
        QVERIFY2(process->waitForMountMotion(), qPrintable(QString("Waited although the mount is idle (%1)").arg(pair)));
        QCOMPARE(m_schedulerLog, expectedLog);
        return;
    }

    // Property 1, the motion ends before the timeout: wait, logged once, then go on without a warning
    QVERIFY2(!process->waitForMountMotion(),
             qPrintable(QString("Didn't wait although the mount is busy (%1)").arg(pair)));
    expectedLog << waitLog;
    setTime(120);
    QVERIFY2(!process->waitForMountMotion(),
             qPrintable(QString("Stopped waiting after 120 s although the mount is busy (%1)").arg(pair)));
    QCOMPARE(m_schedulerLog, expectedLog);
    mount->setStatus(ISD::Mount::MOUNT_TRACKING);
    mount->setMeridianFlipStatus(MeridianFlipState::MOUNT_FLIP_NONE);
    QVERIFY2(process->waitForMountMotion(), "Still waiting after the motion was over");
    QCOMPARE(m_schedulerLog, expectedLog);

    // Property 1, the motion doesn't end: a new wait, logged again, times out 300 s after it started
    // (not 300 s after the first wait)
    const int waitStart = 121;
    setBusy();
    setTime(waitStart);
    QVERIFY2(!process->waitForMountMotion(),
             qPrintable(QString("Didn't wait again although the mount is busy (%1)").arg(pair)));
    expectedLog << waitLog;
    setTime(waitStart + 299);
    QVERIFY2(!process->waitForMountMotion(),
             qPrintable(QString("Stopped waiting after 299 s although the mount is busy (%1)").arg(pair)));
    QCOMPARE(m_schedulerLog, expectedLog);
    setTime(waitStart + 300);
    QVERIFY2(process->waitForMountMotion(), qPrintable(QString("Still waiting after the 300 s timeout (%1)").arg(pair)));
    expectedLog << kMotionTimeoutLog;
    QCOMPARE(m_schedulerLog, expectedLog);
}

// ─────────────────────────────────────────────────────────────────────────────
// The limit of the wait for mount motion
//
// At the end of the schedule, before the park until the next job and before a preemptive
// shutdown the wait ends after 300 s. During a weather (safety) shutdown it ends after 120 s,
// and the limit is logged when the wait starts: the pre-shutdown tasks also close the dome,
// so a mount that keeps reporting a slew must not hold them up for 5 minutes. 120 s is longer
// than a meridian flip takes. The soft shutdown after an alert runs with the grace period
// active, the full shutdown after the grace period with weather monitoring on.
//
// Same setup as testMountMotionWaitDecision: the time comes from
// SchedulerModuleState::setLocalTime(), only the mount module is there.
// ─────────────────────────────────────────────────────────────────────────────

void TestEkosSchedulerOps::testMountMotionWaitTimeout_data()
{
    QTest::addColumn<bool>("gracePeriodActive");
    QTest::addColumn<bool>("weatherMonitoring");
    QTest::addColumn<int>("mountStatus");
    QTest::addColumn<int>("flipStatus");
    QTest::addColumn<QString>("waitLog");
    // The expected limit in seconds
    QTest::addColumn<int>("timeout");

    const int slewing = ISD::Mount::MOUNT_SLEWING;
    const int tracking = ISD::Mount::MOUNT_TRACKING;
    const int noFlip = MeridianFlipState::MOUNT_FLIP_NONE;
    const int flipRunning = MeridianFlipState::MOUNT_FLIP_RUNNING;
    QTest::newRow("NoWeatherSlewing") << false << false << slewing << noFlip << kSlewWaitLog << 300;
    QTest::newRow("NoWeatherFlip") << false << false << tracking << flipRunning << kFlipWaitLog << 300;
    QTest::newRow("GracePeriodSlewing") << true << false << slewing << noFlip << kSlewWaitLog << 120;
    QTest::newRow("GracePeriodFlip") << true << false << tracking << flipRunning << kFlipWaitLog << 120;
    QTest::newRow("MonitoringSlewing") << false << true << slewing << noFlip << kSlewWaitLog << 120;
}

void TestEkosSchedulerOps::testMountMotionWaitTimeout()
{
    QFETCH(bool, gracePeriodActive);
    QFETCH(bool, weatherMonitoring);
    QFETCH(int, mountStatus);
    QFETCH(int, flipStatus);
    QFETCH(QString, waitLog);
    QFETCH(int, timeout);

    const KStarsDateTime startTime(QDate(2021, 6, 14), QTime(3, 10, 0));
    KStarsDateTime localTime(startTime);
    Ekos::SchedulerModuleState::setLocalTime(&localTime);
    auto restoreLocalTime = qScopeGuard([]()
    {
        Ekos::SchedulerModuleState::setLocalTime(nullptr);
    });

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });

    ekos->addModule(QString("Mount"));
    QTRY_VERIFY_WITH_TIMEOUT(!scheduler->process()->mountInterface().isNull(), 5000);
    auto process = scheduler->process();

    // The scheduler is new for every test function, so these flags don't leak
    scheduler->moduleState()->setWeatherGracePeriodActive(gracePeriodActive);
    scheduler->moduleState()->setWeatherShutdownMonitoring(weatherMonitoring);
    mount->setStatus(static_cast<ISD::Mount::Status>(mountStatus));
    mount->setMeridianFlipStatus(flipStatus);
    m_schedulerLog.clear();

    // The wait starts, with the weather limit logged right after the reason
    QStringList expectedLog;
    expectedLog << waitLog;
    if (timeout != 300)
        expectedLog << QString("Safety alert: waiting at most %1 minutes for the mount, then continuing the shutdown.")
                    .arg(timeout / 60);
    QVERIFY(!process->waitForMountMotion());
    QCOMPARE(m_schedulerLog, expectedLog);

    // Still busy one second before the limit
    localTime = startTime.addSecs(timeout - 1);
    QVERIFY2(!process->waitForMountMotion(), qPrintable(QString("Stopped waiting after %1 s").arg(timeout - 1)));
    QCOMPARE(m_schedulerLog, expectedLog);

    // The limit: go on, with a warning that names it
    localTime = startTime.addSecs(timeout);
    QVERIFY2(process->waitForMountMotion(), qPrintable(QString("Still waiting after %1 s").arg(timeout)));
    expectedLog << QString("Warning: the mount is still busy after %1 minutes, continuing.").arg(timeout / 60);
    QCOMPARE(m_schedulerLog, expectedLog);
}

// ─────────────────────────────────────────────────────────────────────────────
// A failed shutdown task is reported as a failure (mock modules, simulated clock)
//
// When a task of the pre-shutdown queue fails (e.g. the mount park was refused while the
// mount slewed), the scheduler goes on with the shutdown as before: it disconnects INDI,
// stops Ekos and runs the post-shutdown queue. It must then end the shutdown with
// "Shutdown procedure failed: <task>: <error>" and an alert instead of "Shutdown complete.",
// also when a post-shutdown queue ran after the failure.
//
// Property 5, honest shutdown result: the final shutdown line is "Shutdown complete." if and
// only if no shutdown queue task failed during that shutdown. The final shutdown line is the
// last scheduler log line that mentions the shutdown.
//
// The queues are queue files in a temporary directory with one task without a device each, so
// QueueExecutor::start() doesn't need an Ekos profile, which the mock modules don't have:
// - a failing task, "Execute Script" with a script that exits with code 3, no retries, failure
//   action ABORT_QUEUE. The failure arrives later through QueueExecutor::itemFailed, as a park
//   timeout does;
// - a succeeding task, "Add Delay" of 0 s.
// No row lets both queues fail: at the end of the schedule the post-shutdown queue then starts
// again after each failure (an existing problem, not covered here).
//
// The alert (KSNotification::event(), which goes to KNotification and EkosLive) can't be
// observed in this test. The log line checked here carries the same text.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
// The shutdown queues of testShutdownReportsQueueFailure
enum ShutdownQueue
{
    // No queue configured
    QUEUE_NONE,
    // One "Add Delay" task of 0 s
    QUEUE_SUCCEEDS,
    // One "Execute Script" task whose script exits with code 3
    QUEUE_FAILS
};

const QString kFailingTaskName("Failing Script");
const QString kDelayTaskName("Short Delay");
const QString kShutdownFailedLog("Shutdown procedure failed");

// The script of the failing task of the queue name in dir
QString failingScriptPath(const QTemporaryDir &dir, const QString &name)
{
    return dir.filePath(name + ".sh");
}

// Write a queue file as the queue manager saves it, with one task of the given kind and no
// device. Returns the path of the file, or an empty string if it couldn't be written.
QString writeShutdownQueue(const QTemporaryDir &dir, const QString &name, int kind)
{
    QJsonObject task;
    QJsonObject action;
    if (kind == QUEUE_FAILS)
    {
        // The script exists and is executable, so the action starts it and fails when it exits
        QFile script(failingScriptPath(dir, name));
        if (!script.open(QIODevice::WriteOnly) || script.write("#!/bin/sh\nexit 3\n") < 0)
            return QString();
        script.close();
        if (!script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner))
            return QString();
        task["name"] = kFailingTaskName;
        task["template_id"] = QStringLiteral("script_execute");
        action["type"] = QStringLiteral("SCRIPT");
        action["script_path"] = script.fileName();
    }
    else
    {
        task["name"] = kDelayTaskName;
        task["template_id"] = QStringLiteral("delay");
        action["type"] = QStringLiteral("DELAY");
        action["duration"] = 0;
        action["unit"] = static_cast<int>(Ekos::DelayAction::SECONDS);
    }
    action["timeout"] = 30;
    action["retries"] = 0;
    action["failure_action"] = static_cast<int>(Ekos::TaskAction::ABORT_QUEUE);
    task["device"] = QString();
    task["actions"] = QJsonArray{action};

    QJsonObject item;
    item["id"] = name;
    item["task"] = task;
    QJsonObject queue;
    queue["version"] = QStringLiteral("1.0");
    queue["items"] = QJsonArray{item};

    const QString path = dir.filePath(name + ".kstarsqueue");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(queue).toJson()) < 0)
        return QString();
    return path;
}

// Shutdown states as text, e.g. "1 5 4 0", for a readable comparison
QString shutdownStatesText(const QList<int> &states)
{
    QStringList text;
    for (const int oneState : states)
        text << QString::number(oneState);
    return text.join(' ');
}
}

void TestEkosSchedulerOps::testShutdownReportsQueueFailure_data()
{
    QTest::addColumn<int>("preQueue");
    QTest::addColumn<int>("postQueue");

    // alacant's case: the park task of the pre-shutdown queue fails, no post-shutdown queue
    QTest::newRow("PreQueueFails") << int(QUEUE_FAILS) << int(QUEUE_NONE);
    // The post-shutdown queue still runs and succeeds, the shutdown is still a failure
    QTest::newRow("PreQueueFailsPostQueueRuns") << int(QUEUE_FAILS) << int(QUEUE_SUCCEEDS);
    // Nothing fails: "Shutdown complete." as before
    QTest::newRow("PreQueueSucceeds") << int(QUEUE_SUCCEEDS) << int(QUEUE_NONE);
    // A failed post-shutdown task is reported too
    QTest::newRow("PostQueueFails") << int(QUEUE_SUCCEEDS) << int(QUEUE_FAILS);
}

void TestEkosSchedulerOps::testShutdownReportsQueueFailure()
{
    QFETCH(int, preQueue);
    QFETCH(int, postQueue);
    // At most one of the queues fails (see above)
    const bool taskFails = preQueue == QUEUE_FAILS || postQueue == QUEUE_FAILS;
    const QString failedQueue = preQueue == QUEUE_FAILS ? QString("pre-shutdown") : QString("post-shutdown");
    const QString failedQueueLog = preQueue == QUEUE_FAILS ? QString("Pre-shutdown queue failed: ")
                                   : QString("Post-shutdown queue failed: ");

    auto state = scheduler->moduleState();
    const bool keepShutdownEnabled = Options::schedulerShutdownEnabled();
    const QUrl keepPreShutdownURL = state->preShutdownScriptURL();
    const QUrl keepPostShutdownURL = state->postShutdownScriptURL();
    auto restoreOptions = qScopeGuard([ = ]()
    {
        Options::setSchedulerShutdownEnabled(keepShutdownEnabled);
        state->setPreShutdownScriptURL(keepPreShutdownURL);
        state->setPostShutdownScriptURL(keepPostShutdownURL);
    });

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });
    // The shutdown states in the order they were set, and the job state when the scheduler stopped (the next
    // clock change re-evaluates the jobs of a stopped scheduler). The context object ends the connections
    // with this function.
    QList<int> shutdownStates;
    int jobStateAtStop = -1;
    QObject stateContext;
    connect(state.data(), &Ekos::SchedulerModuleState::shutdownStateChanged, &stateContext,
            [&shutdownStates](Ekos::ShutdownState newState)
    {
        shutdownStates.append(newState);
    });
    connect(state.data(), &Ekos::SchedulerModuleState::schedulerStateChanged, &stateContext,
            [&jobStateAtStop, state](Ekos::SchedulerState newState)
    {
        if (newState == Ekos::SCHEDULER_IDLE && !state->jobs().isEmpty())
            jobStateAtStop = state->jobs()[0]->getState();
    });

    QTemporaryDir queueDir(KTest::tempDirPattern(QStringLiteral("task-queues")));
    QVERIFY(queueDir.isValid());
    QString preQueueFile, postQueueFile;
    if (preQueue != QUEUE_NONE)
    {
        preQueueFile = writeShutdownQueue(queueDir, "pre-queue", preQueue);
        QVERIFY(!preQueueFile.isEmpty());
    }
    if (postQueue != QUEUE_NONE)
    {
        postQueueFile = writeShutdownQueue(queueDir, "post-queue", postQueue);
        QVERIFY(!postQueueFile.isEmpty());
    }
    const QString failingScript = failingScriptPath(queueDir, preQueue == QUEUE_FAILS ? "pre-queue" : "post-queue");

    // An Altair job from 3:10am local in Silicon Valley, as in testShutdownWaitsForMountMotion
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject *> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc());

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    capture->setStatus(Ekos::CAPTURE_CAPTURING);
    QCOMPARE(state->startupState(), Ekos::STARTUP_COMPLETE);

    // The queues are set once the job runs, so loading the schedule can't change them (as in the weather tests)
    Options::setSchedulerShutdownEnabled(true);
    state->setPreShutdownScriptURL(preQueueFile.isEmpty() ? QUrl() : QUrl::fromLocalFile(preQueueFile));
    state->setPostShutdownScriptURL(postQueueFile.isEmpty() ? QUrl() : QUrl::fromLocalFile(postQueueFile));

    // The last frame ends, nothing is left to run: the shutdown runs until the scheduler stops
    m_schedulerLog.clear();
    shutdownStates.clear();
    capture->setStatus(Ekos::CAPTURE_COMPLETE);
    QVERIFY2(iterateScheduler("Wait for Shutdown", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (state->schedulerState() == Ekos::SCHEDULER_IDLE);
    }), qPrintable(QString("The scheduler didn't stop, shutdown states %1").arg(shutdownStatesText(shutdownStates))));

    // A failed task doesn't stop the shutdown: same states as without the report, INDI disconnected,
    // Ekos stopped, the post-shutdown queue run after that, the scheduler stopped
    QList<int> expectedStates;
    expectedStates << Ekos::SHUTDOWN_PRE_QUEUE_RUNNING
                   << (preQueue == QUEUE_FAILS ? Ekos::SHUTDOWN_ERROR : Ekos::SHUTDOWN_STOPPING_EKOS);
    if (postQueue != QUEUE_NONE)
        expectedStates << Ekos::SHUTDOWN_POST_QUEUE_RUNNING;
    expectedStates << (postQueue == QUEUE_FAILS ? Ekos::SHUTDOWN_ERROR : Ekos::SHUTDOWN_COMPLETE) << Ekos::SHUTDOWN_IDLE;
    QCOMPARE(shutdownStatesText(shutdownStates), shutdownStatesText(expectedStates));
    QCOMPARE(state->timerState(), Ekos::RUN_NOTHING);
    QCOMPARE(ekos->indiStatus(), Ekos::Idle);
    QCOMPARE(ekos->ekosStatus(), Ekos::Idle);
    QCOMPARE(jobStateAtStop, int(Ekos::SCHEDJOB_COMPLETE));

    // Each queue ran once, with the expected result
    QCOMPARE(schedulerLogCount(QString("Executing pre-shutdown tasks from %1").arg(preQueueFile)), 1);
    QCOMPARE(schedulerLogCount("Pre-shutdown queue completed successfully."), preQueue == QUEUE_SUCCEEDS ? 1 : 0);
    QCOMPARE(schedulerLogCount("Pre-shutdown queue failed: "), preQueue == QUEUE_FAILS ? 1 : 0);
    QCOMPARE(schedulerLogCount("Executing post-shutdown tasks from "), postQueue != QUEUE_NONE ? 1 : 0);
    QCOMPARE(schedulerLogCount("Post-shutdown queue completed successfully."), postQueue == QUEUE_SUCCEEDS ? 1 : 0);
    QCOMPARE(schedulerLogCount("Post-shutdown queue failed: "), postQueue == QUEUE_FAILS ? 1 : 0);

    // The final shutdown line
    QString finalLine;
    for (int i = m_schedulerLog.size() - 1; i >= 0 && finalLine.isEmpty(); --i)
        if (m_schedulerLog[i].contains("shutdown", Qt::CaseInsensitive))
            finalLine = m_schedulerLog[i];

    if (taskFails)
    {
        // The error of the failed task, as logged when its queue failed
        const QString failureLine = m_schedulerLog.filter(failedQueueLog).value(0);
        const QString error = failureLine.mid(failureLine.indexOf(failedQueueLog) + failedQueueLog.size());
        QVERIFY2(error.contains("exit code 3") && error.contains(failingScript),
                 qPrintable(QString("Unexpected error of the failed task: \"%1\"").arg(error)));

        QVERIFY2(schedulerLogCount(kShutdownCompleteLog) == 0,
                 qPrintable(QString("\"%1\" logged although the %2 queue failed").arg(kShutdownCompleteLog, failedQueue)));
        QVERIFY2(schedulerLogCount(kShutdownFailedLog) == 1,
                 qPrintable(QString("Expected \"%1: ...\" once after the %2 queue failed, got it %3 times")
                            .arg(kShutdownFailedLog, failedQueue).arg(schedulerLogCount(kShutdownFailedLog))));
        // The task's name and its error, as the last line of the shutdown
        QCOMPARE(finalLine, QString("%1: %2: %3").arg(kShutdownFailedLog, kFailingTaskName, error));
    }
    else
    {
        QCOMPARE(schedulerLogCount(kShutdownFailedLog), 0);
        QCOMPARE(schedulerLogCount(kShutdownCompleteLog), 1);
        QCOMPARE(finalLine, kShutdownCompleteLog);
    }
    // Property 5 as stated
    QVERIFY2((finalLine == kShutdownCompleteLog) == !taskFails,
             qPrintable(QString("Final shutdown line \"%1\", a task %2").arg(finalLine, taskFails ? "failed" : "didn't fail")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tracking safety net at the end of a failed shutdown (Requirement 6)
//
// When a task of the pre-shutdown queue failed and the mount is not parked and is tracking,
// the scheduler switches tracking off (Mount D-Bus setTracking(false)) before it disconnects
// INDI or ends the shutdown. It waits until the mount no longer reports tracking, at most
// 10 s, and then goes on. It logs a warning when tracking can't be switched off or doesn't
// stop in time.
//
// Property 6, tracking off only after a failed park: for any combination of pre-shutdown
// queue result, park status, mount status and tracking control, setTracking(false) is called
// if and only if the queue failed and the mount is unparked and tracking, before INDI is
// disconnected, and no motion command is sent (no slew, abort or park).
//
// The queues are those of testShutdownReportsQueueFailure. The mock mount records accepted
// setTracking() calls in trackingCalls. Without tracking control it returns false and records
// nothing, so that call shows only as the "can't be switched off" warning.
// "Before INDI is disconnected": when MockEkos reports INDI Idle (disconnectDevices), the test
// takes a copy of trackingCalls and of the scheduler log size. Each mount status change is
// recorded with the INDI status at that moment: MockMount::abort() would show as a change to
// MOUNT_IDLE without a setTracking() call.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
const QString kTrackingOffDisconnectLog("Warning: the mount is not parked, switching tracking off before disconnecting.");
const QString kTrackingOffLog("Warning: the mount is not parked, switching tracking off.");
const QString kTrackingStoppedLog("The mount no longer reports tracking.");
const QString kTrackingTimeoutLog(
    "Warning: the mount still reports tracking after 10 seconds, it may keep tracking after the shutdown.");
const QString kNoTrackingControlLog(
    "Warning: tracking can't be switched off, the mount may keep tracking after the shutdown.");
// SchedulerProcess::TRACKING_OFF_TIMEOUT_S
constexpr int kTrackingOffTimeoutS = 10;

// setTracking() calls as text, e.g. "off", for a readable comparison
QString trackingCallsText(const QList<bool> &calls)
{
    QStringList text;
    for (const bool enabled : calls)
        text << (enabled ? QString("on") : QString("off"));
    return text.isEmpty() ? QString("none") : text.join(' ');
}
}

void TestEkosSchedulerOps::testShutdownSafetyNetTrackingOff_data()
{
    QTest::addColumn<bool>("queueFails");
    QTest::addColumn<bool>("parked");
    QTest::addColumn<bool>("tracking");
    QTest::addColumn<bool>("canControlTrack");
    // The mount stops reporting MOUNT_TRACKING when tracking is switched off
    QTest::addColumn<bool>("trackingStops");
    // Options::stopEkosAfterShutdown: the shutdown disconnects INDI
    QTest::addColumn<bool>("stopEkos");

    // Property 6 over every combination of queue result, park status, mount status and tracking control.
    // The combinations of the task have their own names, the others are named after their values.
    // A parked mount that reports tracking isn't a real state: it checks that a parked mount is left alone.
    const bool values[] = {true, false};
    for (const bool queueFails : values)
        for (const bool parked : values)
            for (const bool tracking : values)
                for (const bool canControlTrack : values)
                {
                    QString name;
                    if (queueFails && !parked && tracking)
                        name = canControlTrack ? "FailedTracking" : "NoTrackingControl";
                    else if (queueFails && parked && tracking && canControlTrack)
                        name = "FailedParked";
                    else if (queueFails && !parked && !tracking && canControlTrack)
                        name = "FailedNotTracking";
                    else if (!queueFails && !parked && tracking && canControlTrack)
                        name = "SucceededTracking";
                    else
                        name = QString("%1-%2-%3-%4").arg(QString(queueFails ? "Failed" : "Succeeded"),
                                                          QString(parked ? "Parked" : "Unparked"),
                                                          QString(tracking ? "Tracking" : "NotTracking"),
                                                          QString(canControlTrack ? "TrackControl" : "NoTrackControl"));
                    QTest::newRow(qPrintable(name)) << queueFails << parked << tracking << canControlTrack << true << true;
                }

    // The mount accepts tracking off but keeps tracking: warning after 10 s, then the shutdown goes on
    QTest::newRow("TrackingDoesntStop") << true << false << true << true << false << true;
    // INDI stays connected: tracking is switched off before the shutdown ends, the warning doesn't say
    // "before disconnecting"
    QTest::newRow("StopEkosOff") << true << false << true << true << true << false;
}

void TestEkosSchedulerOps::testShutdownSafetyNetTrackingOff()
{
    QFETCH(bool, queueFails);
    QFETCH(bool, parked);
    QFETCH(bool, tracking);
    QFETCH(bool, canControlTrack);
    QFETCH(bool, trackingStops);
    QFETCH(bool, stopEkos);
    // Property 6: the safety net acts if and only if the queue failed and the mount is unparked and tracking
    const bool switchesOff = queueFails && !parked && tracking;
    // The mock records the call only with tracking control
    const bool callRecorded = switchesOff && canControlTrack;
    const QString trackingOffLog = stopEkos ? kTrackingOffDisconnectLog : kTrackingOffLog;
    // The safety net's last line, the result of the switch
    QString resultLog;
    if (switchesOff)
        resultLog = !canControlTrack ? kNoTrackingControlLog : (trackingStops ? kTrackingStoppedLog : kTrackingTimeoutLog);

    auto state = scheduler->moduleState();
    const bool keepShutdownEnabled = Options::schedulerShutdownEnabled();
    const bool keepStopEkos = Options::stopEkosAfterShutdown();
    const QUrl keepPreShutdownURL = state->preShutdownScriptURL();
    const QUrl keepPostShutdownURL = state->postShutdownScriptURL();
    auto restoreOptions = qScopeGuard([ = ]()
    {
        Options::setSchedulerShutdownEnabled(keepShutdownEnabled);
        Options::setStopEkosAfterShutdown(keepStopEkos);
        state->setPreShutdownScriptURL(keepPreShutdownURL);
        state->setPostShutdownScriptURL(keepPostShutdownURL);
    });

    QTemporaryDir queueDir(KTest::tempDirPattern(QStringLiteral("task-queues")));
    QVERIFY(queueDir.isValid());
    const QString preQueueFile = writeShutdownQueue(queueDir, "pre-queue", queueFails ? QUEUE_FAILS : QUEUE_SUCCEEDS);
    QVERIFY(!preQueueFile.isEmpty());

    // The Altair job of testShutdownReportsQueueFailure, run until it is capturing
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject *> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc());

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    capture->setStatus(Ekos::CAPTURE_CAPTURING);
    QCOMPARE(state->startupState(), Ekos::STARTUP_COMPLETE);

    // The queue is set once the job runs, so loading the schedule can't change it (as in the weather tests)
    Options::setSchedulerShutdownEnabled(true);
    Options::setStopEkosAfterShutdown(stopEkos);
    state->setPreShutdownScriptURL(QUrl::fromLocalFile(preQueueFile));
    state->setPostShutdownScriptURL(QUrl());

    // The mount as the last frame ends
    mount->setStatus(tracking ? ISD::Mount::MOUNT_TRACKING : ISD::Mount::MOUNT_IDLE);
    if (parked)
        mount->setParkStatus(ISD::PARK_PARKED);
    mount->canControlTrack = canControlTrack;
    mount->trackingOffChangesStatus = trackingStops;
    QVERIFY(mount->trackingCalls.isEmpty());
    const int slewCountBefore = mount->slewCount;
    const int parkStatusBefore = mount->parkStatus();
    const int statusBefore = mount->status();

    // Observers. The context object is declared after the variables they write, so the connections end first.
    // The scheduler log with the simulated time of each line
    QList<QDateTime> logTimes;
    // What had happened when MockEkos::disconnectDevices() disconnected INDI
    bool indiDisconnected = false;
    QList<bool> trackingCallsAtDisconnect;
    int logSizeAtDisconnect = -1;
    QDateTime disconnectUTime;
    // Every mount status change, with the number of setTracking() calls and the INDI status at that moment
    QList<int> statusChanges;
    QList<int> callsAtStatusChange;
    QList<int> indiAtStatusChange;
    int parkStatusChanges = 0;
    QObject context;
    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, &context,
            [this, &logTimes](const QString & text)
    {
        m_schedulerLog.append(text);
        logTimes.append(KStarsData::Instance()->ut());
    });
    connect(ekos.data(), &Ekos::MockEkos::indiStatusChanged, &context, [&](Ekos::CommunicationStatus status)
    {
        if (status != Ekos::Idle || indiDisconnected)
            return;
        indiDisconnected = true;
        trackingCallsAtDisconnect = mount->trackingCalls;
        logSizeAtDisconnect = static_cast<int>(m_schedulerLog.size());
        disconnectUTime = KStarsData::Instance()->ut();
    });
    connect(mount.data(), &Ekos::MockMount::newStatus, &context, [&](ISD::Mount::Status status)
    {
        statusChanges.append(status);
        callsAtStatusChange.append(static_cast<int>(mount->trackingCalls.size()));
        indiAtStatusChange.append(ekos->indiStatus());
    });
    connect(mount.data(), &Ekos::MockMount::newParkStatus, &context, [&parkStatusChanges](ISD::ParkStatus)
    {
        parkStatusChanges++;
    });
    // Exact matches of a scheduler log line
    auto logCount = [this](const QString & text)
    {
        return static_cast<int>(m_schedulerLog.count(text));
    };

    // The last frame ends, nothing is left to run: the shutdown runs until the scheduler stops. One second per
    // iteration, so the 10 s wait for the mount takes 10 iterations.
    WithInterval interval(1000, scheduler);
    m_schedulerLog.clear();
    logTimes.clear();
    capture->setStatus(Ekos::CAPTURE_COMPLETE);
    QVERIFY2(iterateScheduler("Wait for Shutdown", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (state->schedulerState() == Ekos::SCHEDULER_IDLE);
    }), "The scheduler didn't stop");

    // The shutdown went on to the end in every row (Requirements 6.2, 6.3)
    QCOMPARE(state->timerState(), Ekos::RUN_NOTHING);
    QCOMPARE(schedulerLogCount("Pre-shutdown queue failed: "), queueFails ? 1 : 0);
    QCOMPARE(schedulerLogCount("Pre-shutdown queue completed successfully."), queueFails ? 0 : 1);
    QCOMPARE(indiDisconnected, stopEkos);
    QCOMPARE(ekos->indiStatus(), stopEkos ? Ekos::Idle : Ekos::Success);
    QCOMPARE(ekos->ekosStatus(), stopEkos ? Ekos::Idle : Ekos::Success);
    QString finalLine;
    for (int i = m_schedulerLog.size() - 1; i >= 0 && finalLine.isEmpty(); --i)
        if (m_schedulerLog[i].contains("shutdown", Qt::CaseInsensitive))
            finalLine = m_schedulerLog[i];
    if (queueFails)
        QVERIFY2(finalLine.startsWith(QString("%1: %2: ").arg(kShutdownFailedLog, kFailingTaskName)),
                 qPrintable(QString("Final shutdown line \"%1\"").arg(finalLine)));
    else
        QCOMPARE(finalLine, kShutdownCompleteLog);

    // The safety net's lines, each at most once (Requirements 6.1 to 6.4)
    QCOMPARE(logCount(kTrackingOffDisconnectLog), switchesOff && stopEkos ? 1 : 0);
    QCOMPARE(logCount(kTrackingOffLog), switchesOff && !stopEkos ? 1 : 0);
    QCOMPARE(logCount(kNoTrackingControlLog), switchesOff && !canControlTrack ? 1 : 0);
    QCOMPARE(logCount(kTrackingStoppedLog), callRecorded && trackingStops ? 1 : 0);
    QCOMPARE(logCount(kTrackingTimeoutLog), callRecorded && !trackingStops ? 1 : 0);

    // setTracking(false) once, and nothing else on tracking
    QList<bool> expectedCalls;
    if (callRecorded)
        expectedCalls << false;
    QVERIFY2(mount->trackingCalls == expectedCalls,
             qPrintable(QString("setTracking() calls: %1, expected: %2")
                        .arg(trackingCallsText(mount->trackingCalls), trackingCallsText(expectedCalls))));

    if (switchesOff)
    {
        const int offIndex = static_cast<int>(m_schedulerLog.indexOf(trackingOffLog));
        const int resultIndex = static_cast<int>(m_schedulerLog.indexOf(resultLog));
        QVERIFY2(offIndex >= 0 && resultIndex > offIndex,
                 qPrintable(QString("\"%1\" at %2, \"%3\" at %4").arg(trackingOffLog).arg(offIndex).arg(resultLog)
                            .arg(resultIndex)));
        // Before the shutdown ends
        QVERIFY2(resultIndex < m_schedulerLog.lastIndexOf(finalLine),
                 qPrintable(QString("\"%1\" logged after the final shutdown line").arg(resultLog)));
        // Before INDI is disconnected: the call was made and its result logged, nothing was sent after it
        if (stopEkos)
        {
            QVERIFY2(trackingCallsAtDisconnect == expectedCalls,
                     qPrintable(QString("setTracking() calls when INDI was disconnected: %1, expected: %2")
                                .arg(trackingCallsText(trackingCallsAtDisconnect), trackingCallsText(expectedCalls))));
            QVERIFY2(resultIndex < logSizeAtDisconnect,
                     qPrintable(QString("\"%1\" logged after INDI was disconnected").arg(resultLog)));
            QVERIFY(logTimes.value(resultIndex).secsTo(disconnectUTime) >= 0);
        }
        // The wait for the mount in simulated time: the full 10 s if it keeps tracking, otherwise one iteration
        const qint64 waitedS = logTimes.value(offIndex).secsTo(logTimes.value(resultIndex));
        if (callRecorded && !trackingStops)
            QVERIFY2(waitedS >= kTrackingOffTimeoutS && waitedS <= kTrackingOffTimeoutS + 2,
                     qPrintable(QString("Waited %1 s for the mount to stop tracking").arg(waitedS)));
        else
            QVERIFY2(waitedS <= 2, qPrintable(QString("Waited %1 s for the mount to stop tracking").arg(waitedS)));
    }

    // No motion command (Requirement 6.5): no slew, park or unpark, and the only status change is the one of
    // setTracking(false), with INDI still connected (MockMount::abort() would also set MOUNT_IDLE)
    QCOMPARE(mount->slewCount, slewCountBefore);
    QCOMPARE(parkStatusChanges, 0);
    QCOMPARE(static_cast<int>(mount->parkStatus()), parkStatusBefore);
    if (callRecorded && trackingStops)
    {
        QCOMPARE(static_cast<int>(statusChanges.size()), 1);
        QCOMPARE(statusChanges[0], static_cast<int>(ISD::Mount::MOUNT_IDLE));
        QCOMPARE(callsAtStatusChange[0], 1);
        QCOMPARE(indiAtStatusChange[0], static_cast<int>(Ekos::Success));
        QCOMPARE(static_cast<int>(mount->status()), static_cast<int>(ISD::Mount::MOUNT_IDLE));
    }
    else
    {
        QVERIFY2(statusChanges.isEmpty(),
                 qPrintable(QString("Mount status changed %1 times during the shutdown").arg(statusChanges.size())));
        QCOMPARE(static_cast<int>(mount->status()), statusBefore);
    }

    // Property 6 as stated: setTracking(false) was called (recorded by the mock, or refused without tracking
    // control) if and only if the queue failed and the mount was unparked and tracking
    const bool called = !mount->trackingCalls.isEmpty() || m_schedulerLog.contains(kNoTrackingControlLog);
    QVERIFY2(called == switchesOff,
             qPrintable(QString("setTracking(false) %1, queue %2, %3, %4")
                        .arg(called ? "called" : "not called", queueFails ? "failed" : "succeeded",
                             parked ? "parked" : "unparked", tracking ? "tracking" : "not tracking")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Weather soft-shutdown tests (mock modules, simulated clock)
//
// These cover the scheduler's behaviour while a weather/safety alert holds the
// observatory in soft shutdown (pre-shutdown tasks done, Ekos/INDI still running).
// Unlike the simulator tests below, the clock here advances by whatever the
// scheduler asks to sleep, so the 5-minute monitoring wakeups of the "indefinite
// wait" grace period take milliseconds.
//
// No startup/shutdown task queues are configured (see init()), so "the post-startup
// phase ran" is observed as startupState reaching STARTUP_COMPLETE, and "the job
// restarted" as a mount slew or the job going BUSY.
// ─────────────────────────────────────────────────────────────────────────────

// Start a single Altair job at 3:10am local in Silicon Valley (2021-06-14) with weather
// monitoring enabled, and run it until it is capturing. Astronomical dawn is at about
// 10:53 UTC; the job starts at about 10:13 UTC (see testDawnShutdown).
void TestEkosSchedulerOps::startWeatherTestJob(int gracePeriodMinutes,
        const TestEkosSchedulerHelper::CompletionCondition &completion,
        KStarsDateTime &currentUTime, int &sleepMs, QTemporaryDir &dir)
{
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject *> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc());

    Options::setSchedulerWeather(true);
    Options::setSchedulerWeatherGracePeriod(gracePeriodMinutes);
    Options::setSchedulerWeatherShutdownDelay(0);

    connect(scheduler->process().data(), &Ekos::SchedulerProcess::newLog, this, [this](const QString & text)
    {
        m_schedulerLog.append(text);
    });

    m_completionCondition = completion;
    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);

    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));
    capture->setStatus(Ekos::CAPTURE_CAPTURING);
    QCOMPARE(scheduler->moduleState()->startupState(), Ekos::STARTUP_COMPLETE);
}

// Deliver a weather status change the way a weather device or safety monitor does.
void TestEkosSchedulerOps::injectWeather(ISD::Weather::Status status)
{
    QVERIFY(QMetaObject::invokeMethod(scheduler->process().data(), "setWeatherStatus", Qt::DirectConnection,
                                      Q_ARG(ISD::Weather::Status, status), Q_ARG(bool, false)));
    QCOMPARE(scheduler->moduleState()->weatherStatus(), status);
}

// Iterate the scheduler while it must hold the observatory closed, until done() is true.
// Fails (and records why in m_weatherHoldViolation) if in the meantime the scheduler runs
// the post-startup phase, starts the job or slews the mount.
bool TestEkosSchedulerOps::iterateDuringWeatherHold(const QString &label, int iterations, int &sleepMs,
        KStarsDateTime &currentUTime, std::function<bool ()> done)
{
    m_weatherHoldViolation.clear();
    const bool finished = iterateScheduler(label, iterations, &sleepMs, &currentUTime, [&]() -> bool
    {
        const QString now = KStarsData::Instance()->ut().toString("hh:mm:ss");
        if (scheduler->moduleState()->startupState() == Ekos::STARTUP_COMPLETE ||
                scheduler->moduleState()->startupState() == Ekos::STARTUP_POST_DEVICES_RUNNING)
            m_weatherHoldViolation = QString("post-startup phase ran at %1 UT").arg(now);
        else if (mount->slewCount != m_weatherSlewBaseline)
            m_weatherHoldViolation = QString("mount slewed at %1 UT").arg(now);
        else if (scheduler->activeJob() != nullptr && scheduler->activeJob()->getState() == Ekos::SCHEDJOB_BUSY)
            m_weatherHoldViolation = QString("job was started at %1 UT").arg(now);
        return !m_weatherHoldViolation.isEmpty() || done();
    });
    return finished && m_weatherHoldViolation.isEmpty();
}

// An alert that lasts longer than the 5-minute monitoring interval of the indefinite
// grace period. Each monitoring wakeup must keep the observatory closed; once safety
// is OK again the job resumes normally.
void TestEkosSchedulerOps::testWeatherMonitoringWakeupDuringAlert()
{
    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    TestEkosSchedulerHelper::CompletionCondition loop;
    loop.type = Ekos::FINISH_LOOP;
    startWeatherTestJob(0, loop, currentUTime, sleepMs, dir);

    m_weatherSlewBaseline = mount->slewCount;
    injectWeather(ISD::Weather::WEATHER_ALERT);
    QVERIFY(iterateScheduler("Wait for weather monitoring", 20, &sleepMs, &currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->weatherShutdownMonitoring();
    }));
    m_schedulerLog.clear();

    // Hold for 25 minutes of simulated time: several monitoring wakeups, well before dawn.
    const KStarsDateTime holdUntil = currentUTime.addSecs(25 * 60);
    QVERIFY2(iterateDuringWeatherHold("Hold during alert", 200, sleepMs, currentUTime, [&]() -> bool
    {
        return KStarsData::Instance()->ut() >= holdUntil;
    }), qPrintable(m_weatherHoldViolation));
    QCOMPARE(scheduler->moduleState()->schedulerState(), Ekos::SCHEDULER_RUNNING);
    QVERIFY(scheduler->moduleState()->weatherShutdownMonitoring());
    QVERIFY2(m_schedulerLog.filter("Scheduler is awake").isEmpty(),
             "Monitoring wakeups during an alert must not report the scheduler as awake");

    // Safety improves: the scheduler resumes the job, and reports waking up once.
    m_schedulerLog.clear();
    injectWeather(ISD::Weather::WEATHER_OK);
    QVERIFY(iterateScheduler("Wait for job restart", 50, &sleepMs, &currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->timerState() == Ekos::RUN_JOBCHECK &&
        mount->slewCount > m_weatherSlewBaseline;
    }));
    QCOMPARE(scheduler->moduleState()->startupState(), Ekos::STARTUP_COMPLETE);
    QVERIFY(!scheduler->moduleState()->weatherShutdownMonitoring());
    QVERIFY2(m_schedulerLog.filter("Scheduler is awake").size() <= 1,
             qPrintable(QString("Recovery woke the scheduler %1 times").arg(m_schedulerLog.filter("Scheduler is awake").size())));
}

// The reporter's scenario: an alert starts during a job with an end-at time and is still
// active when that time passes. The scheduler must run the full shutdown sequence without
// waiting for safety to improve, and without restarting the job first.
void TestEkosSchedulerOps::testWeatherAlertOutlastsJobEndTime()
{
    // End at 3:30am local. The scheduler reads the clock time of this value and local time
    // on this date is PDT (UTC-7), so that is 10:30 UT.
    const QDateTime endAtUTime(QDate(2021, 6, 14), QTime(10, 30, 0), QTimeZone::utc());
    TestEkosSchedulerHelper::CompletionCondition endAt;
    endAt.type = Ekos::FINISH_AT;
    endAt.atLocalDateTime = endAtUTime.toTimeZone(QTimeZone(-7 * 3600));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startWeatherTestJob(0, endAt, currentUTime, sleepMs, dir);
    QVERIFY(KStarsData::Instance()->ut() < endAtUTime);

    m_weatherSlewBaseline = mount->slewCount;
    injectWeather(ISD::Weather::WEATHER_ALERT);
    QVERIFY(iterateScheduler("Wait for weather monitoring", 20, &sleepMs, &currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->weatherShutdownMonitoring();
    }));

    m_schedulerLog.clear();
    QVERIFY2(iterateDuringWeatherHold("Hold until shutdown", 300, sleepMs, currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_IDLE;
    }), qPrintable(m_weatherHoldViolation.isEmpty() ? QString("scheduler never shut down") : m_weatherHoldViolation));

    // It shut down at the first monitoring check after the end time, while still in alert.
    QCOMPARE(scheduler->moduleState()->weatherStatus(), ISD::Weather::WEATHER_ALERT);
    QVERIFY(KStarsData::Instance()->ut() >= endAtUTime);
    QVERIFY2(endAtUTime.secsTo(KStarsData::Instance()->ut()) < 10 * 60,
             qPrintable(QString("Shutdown only at %1 UT").arg(KStarsData::Instance()->ut().toString("hh:mm:ss"))));
    QVERIFY2(!m_schedulerLog.filter("proceeding to stop Ekos/INDI").isEmpty(), "Full shutdown sequence did not run");
    QCOMPARE(ekos->ekosStatus(), Ekos::Idle);
    QCOMPARE(scheduler->moduleState()->jobs()[0]->getState(), Ekos::SCHEDJOB_COMPLETE);
}

// An alert that clears after the job's end-at time, while still inside a (non-zero) grace
// period. Recovery must see that the job has expired and shut down, instead of running
// the post-startup phase and restarting the job for a few seconds.
void TestEkosSchedulerOps::testWeatherClearsAfterJobEndTime()
{
    // End at 3:30am local. The scheduler reads the clock time of this value and local time
    // on this date is PDT (UTC-7), so that is 10:30 UT.
    const QDateTime endAtUTime(QDate(2021, 6, 14), QTime(10, 30, 0), QTimeZone::utc());
    TestEkosSchedulerHelper::CompletionCondition endAt;
    endAt.type = Ekos::FINISH_AT;
    endAt.atLocalDateTime = endAtUTime.toTimeZone(QTimeZone(-7 * 3600));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startWeatherTestJob(60, endAt, currentUTime, sleepMs, dir);

    m_weatherSlewBaseline = mount->slewCount;
    injectWeather(ISD::Weather::WEATHER_ALERT);
    // Only the event loop here: iterateScheduler() would run the grace period wakeup
    // immediately instead of an hour later.
    QTRY_VERIFY_WITH_TIMEOUT(scheduler->moduleState()->weatherGracePeriodActive() &&
                             scheduler->moduleState()->shutdownState() == Ekos::SHUTDOWN_COMPLETE, 5000);
    QCOMPARE(scheduler->moduleState()->timerState(), Ekos::RUN_WAKEUP);

    // Safety improves at 10:35 UT, after the end time but before the grace period expires.
    currentUTime = KStarsDateTime(QDateTime(QDate(2021, 6, 14), QTime(10, 35, 0), QTimeZone::utc()));
    KStarsData::Instance()->changeDateTime(currentUTime);
    sleepMs = 0;
    injectWeather(ISD::Weather::WEATHER_OK);

    QVERIFY2(iterateDuringWeatherHold("Recover after end time", 100, sleepMs, currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->schedulerState() == Ekos::SCHEDULER_IDLE;
    }), qPrintable(m_weatherHoldViolation.isEmpty() ? QString("scheduler never shut down") : m_weatherHoldViolation));
    QCOMPARE(ekos->ekosStatus(), Ekos::Idle);
    QCOMPARE(scheduler->moduleState()->jobs()[0]->getState(), Ekos::SCHEDJOB_COMPLETE);
}

// The overnight incident: an alert starts during the night and lasts past dawn. The
// observatory must stay closed through dawn, and when safety improves in daylight the
// scheduler must not run the post-startup phase. It sleeps until the job's next window,
// and only then starts up again.
void TestEkosSchedulerOps::testWeatherAlertThroughDawn()
{
    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    TestEkosSchedulerHelper::CompletionCondition loop;
    loop.type = Ekos::FINISH_LOOP;
    startWeatherTestJob(0, loop, currentUTime, sleepMs, dir);

    m_weatherSlewBaseline = mount->slewCount;
    injectWeather(ISD::Weather::WEATHER_ALERT);

    // Alert lasts until 13:00 UT (6am local), two hours after dawn.
    const QDateTime clearUTime(QDate(2021, 6, 14), QTime(13, 0, 0), QTimeZone::utc());
    QVERIFY2(iterateDuringWeatherHold("Hold through dawn", 300, sleepMs, currentUTime, [&]() -> bool
    {
        return KStarsData::Instance()->ut() >= clearUTime;
    }), qPrintable(m_weatherHoldViolation));
    QCOMPARE(scheduler->moduleState()->schedulerState(), Ekos::SCHEDULER_RUNNING);

    injectWeather(ISD::Weather::WEATHER_OK);
    QVERIFY2(iterateDuringWeatherHold("Wait for sleep until next night", 50, sleepMs, currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP &&
        !scheduler->moduleState()->weatherShutdownMonitoring();
    }), qPrintable(m_weatherHoldViolation.isEmpty() ? QString("scheduler did not go to sleep") : m_weatherHoldViolation));

    // Next window: Altair above 30 degrees at about 06:31 UT on 2021-06-15 (see testDawnShutdown).
    const QDateTime restartTime(QDate(2021, 6, 15), QTime(6, 31, 0), QTimeZone::utc());
    QVERIFY(iterateScheduler("Wait for job restart next night", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->timerState() == Ekos::RUN_JOBCHECK;
    }));
    QVERIFY2(std::abs(KStarsData::Instance()->ut().secsTo(restartTime)) < timeTolerance(DEFAULT_TOLERANCE),
             qPrintable(QString("Job restarted at %1 UT").arg(KStarsData::Instance()->ut().toString())));
    QCOMPARE(scheduler->moduleState()->startupState(), Ekos::STARTUP_COMPLETE);
}

// An alert already present when a freshly started job gets its first job-stage check
// (e.g. the scheduler started the job just as the alert came in). The scheduler must start
// the soft shutdown and not go on to slew the mount for the job.
void TestEkosSchedulerOps::testWeatherAlertBeforeFirstJobCheck()
{
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Altair");
    const QDateTime startUTime(QDate(2021, 6, 14), QTime(10, 10, 0), QTimeZone::utc());

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    startupJob(geo, startUTime, &dir, TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition,
               m_completionCondition, {true, true, true, true}, true, true),
               TestEkosSchedulerHelper::getDefaultEsqContent(), QDateTime(), currentUTime, sleepMs);

    WithInterval interval(1000, scheduler);
    QVERIFY(iterateScheduler("Wait for job start", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return scheduler->moduleState()->timerState() == Ekos::RUN_JOBCHECK;
    }));
    QCOMPARE(scheduler->activeJob()->getStage(), Ekos::SCHEDSTAGE_IDLE);

    // The alert is already in the module state, with no transition left for setWeatherStatus()
    // to react to, so the first job-stage check is the one that has to catch it.
    Options::setSchedulerWeather(true);
    Options::setSchedulerWeatherGracePeriod(0);
    scheduler->moduleState()->setWeatherStatus(ISD::Weather::WEATHER_ALERT);
    const int slews = mount->slewCount;

    QVERIFY(iterateScheduler("First job check", 3, &sleepMs, &currentUTime, [&]() -> bool
    {
        return false;
    }) == false);
    QCOMPARE(mount->slewCount, slews);
    QVERIFY(scheduler->moduleState()->weatherGracePeriodActive() ||
            scheduler->moduleState()->weatherShutdownMonitoring());
}

void TestEkosSchedulerOps::testWeatherShutdownWaitsForMountMotion_data()
{
    // Mount status and flip state when the alert arrives
    QTest::addColumn<int>("busyMountStatus");
    QTest::addColumn<int>("busyFlipStatus");
    // The expected wait message
    QTest::addColumn<QString>("waitLog");
    // The mount keeps reporting the slew: the soft shutdown must go on after the weather limit of 2 minutes
    QTest::addColumn<bool>("neverDone");

    QTest::newRow("FlipRunning") << int(ISD::Mount::MOUNT_SLEWING) << int(MeridianFlipState::MOUNT_FLIP_RUNNING)
                                 << kFlipWaitLog << false;
    QTest::newRow("StuckSlewing") << int(ISD::Mount::MOUNT_SLEWING) << int(MeridianFlipState::MOUNT_FLIP_NONE)
                                  << kSlewWaitLog << true;
}

// A weather alert arrives while a meridian flip runs, or while the mount keeps reporting a slew that never ends
// (a driver that is stuck). The soft shutdown waits for the motion as the other shutdowns do, but at most
// 2 minutes instead of 5, since the pre-shutdown tasks also close the dome. After the wait the soft shutdown
// runs, and the scheduler sleeps until the end of the grace period that the alert set, not longer.
// With the grace period expired and the alert still on, the full shutdown waits at most 2 minutes too.
//
// No pre-shutdown queue is configured (see init()), so "the soft shutdown ran" is the log line
// "No pre-shutdown tasks during weather grace period; soft shutdown complete.".
void TestEkosSchedulerOps::testWeatherShutdownWaitsForMountMotion()
{
    QFETCH(int, busyMountStatus);
    QFETCH(int, busyFlipStatus);
    QFETCH(QString, waitLog);
    QFETCH(bool, neverDone);

    const QString softShutdownLog("No pre-shutdown tasks during weather grace period; soft shutdown complete.");
    const QString graceExpiredLog("Weather grace period has expired");
    const QString weatherLimitLog("Safety alert: waiting at most 2 minutes for the mount, then continuing the shutdown.");
    const QString weatherTimeoutLog("Warning: the mount is still busy after 2 minutes, continuing.");
    const int gracePeriodMinutes = 60;
    const int intervalSecs = kJobEndIntervalMs / 1000;

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    TestEkosSchedulerHelper::CompletionCondition loop;
    loop.type = Ekos::FINISH_LOOP;
    startWeatherTestJob(gracePeriodMinutes, loop, currentUTime, sleepMs, dir);
    if (QTest::currentTestFailed())
        return;

    // 10 simulated seconds per iteration: the weather limit takes 12 iterations, the 5 minute limit 30
    WithInterval interval(kJobEndIntervalMs, scheduler);
    auto state = scheduler->moduleState();

    // The alert arrives with the mount busy. The soft shutdown starts from the weather shutdown timer (delay 0),
    // so the event loop runs it.
    mount->setStatus(static_cast<ISD::Mount::Status>(busyMountStatus));
    mount->setMeridianFlipStatus(busyFlipStatus);
    m_schedulerLog.clear();
    injectWeather(ISD::Weather::WEATHER_ALERT);
    QTRY_VERIFY_WITH_TIMEOUT(state->weatherGracePeriodActive(), 5000);
    const QDateTime alertUTime = KStarsData::Instance()->ut();
    const QDateTime wakeupTime = state->preemptiveShutdownWakeupTime();
    QVERIFY(state->preemptiveShutdown());
    QVERIFY(wakeupTime.isValid());
    QVERIFY2(std::abs(Ekos::SchedulerModuleState::getLocalTime().secsTo(wakeupTime) - gracePeriodMinutes * 60) <= 1,
             qPrintable(QString("The grace period ends at %1, the alert came at %2")
                        .arg(wakeupTime.toString(), Ekos::SchedulerModuleState::getLocalTime().toString())));

    // The soft shutdown waits for the motion, the wait and its limit are logged once
    QCOMPARE(state->shutdownState(), Ekos::SHUTDOWN_IDLE);
    QCOMPARE(state->timerState(), Ekos::RUN_SHUTDOWN);
    QCOMPARE(schedulerLogCount(softShutdownLog), 0);
    QVERIFY2(schedulerLogCount(waitLog) == 1, qPrintable(m_schedulerLog.join(" | ")));
    QVERIFY2(schedulerLogCount(weatherLimitLog) == 1, qPrintable(m_schedulerLog.join(" | ")));

    if (neverDone)
    {
        // The mount never stops: the soft shutdown runs 2 minutes after the wait started, not 5
        QVERIFY2(iterateScheduler("Mount stuck slewing at the weather alert", 20, &sleepMs, &currentUTime, [&]() -> bool
        {
            return schedulerLogCount(softShutdownLog) > 0;
        }), "The soft shutdown didn't start within 200 s of the alert");
        const qint64 waitedSecs = alertUTime.secsTo(KStarsData::Instance()->ut());
        QVERIFY2(waitedSecs >= 120 && waitedSecs <= 120 + 2 * intervalSecs,
                 qPrintable(QString("The soft shutdown started %1 s after the alert, expected 120 s").arg(waitedSecs)));
        QCOMPARE(schedulerLogCount(weatherTimeoutLog), 1);
        QCOMPARE(schedulerLogCount(kMotionTimeoutLog), 0);
    }
    else
    {
        // A minute with the flip running, as a real flip takes: no shutdown yet
        iterateScheduler("Flip running at the weather alert", 6, &sleepMs, &currentUTime, [&]() -> bool
        {
            return schedulerLogCount(softShutdownLog) > 0;
        });
        QVERIFY2(schedulerLogCount(softShutdownLog) == 0, "The soft shutdown started during the meridian flip");
        QCOMPARE(state->shutdownState(), Ekos::SHUTDOWN_IDLE);
        QCOMPARE(state->timerState(), Ekos::RUN_SHUTDOWN);

        // The flip is over: the soft shutdown runs in the next iteration
        mount->setStatus(ISD::Mount::MOUNT_TRACKING);
        mount->setMeridianFlipStatus(MeridianFlipState::MOUNT_FLIP_COMPLETED);
        QVERIFY2(iterateScheduler("Flip finished", 1, &sleepMs, &currentUTime, [&]() -> bool
        {
            return schedulerLogCount(softShutdownLog) > 0;
        }), "The soft shutdown didn't start once the flip was over");
        QCOMPARE(schedulerLogCount(weatherTimeoutLog), 0);
    }
    QCOMPARE(schedulerLogCount(waitLog), 1);
    QVERIFY(logIndex(m_schedulerLog, waitLog) < logIndex(m_schedulerLog, softShutdownLog));

    // Soft shutdown done, Ekos/INDI still up, the wakeup is still the end of the grace period
    QCOMPARE(state->shutdownState(), Ekos::SHUTDOWN_COMPLETE);
    QVERIFY(state->weatherGracePeriodActive());
    QVERIFY(state->preemptiveShutdown());
    QCOMPARE(state->preemptiveShutdownWakeupTime(), wakeupTime);
    QCOMPARE(state->timerState(), Ekos::RUN_WAKEUP);
    QCOMPARE(ekos->ekosStatus(), Ekos::Success);

    // The alert outlasts the grace period: the scheduler wakes up at its end (the simulated clock jumps by the
    // iteration's sleep time) and shuts down completely
    m_schedulerLog.clear();
    QVERIFY(iterateScheduler("Wait for the end of the grace period", 3, &sleepMs, &currentUTime, [&]() -> bool
    {
        return schedulerLogCount(graceExpiredLog) > 0;
    }));
    const QDateTime expiredUTime = KStarsData::Instance()->ut();
    const qint64 graceSecs = alertUTime.secsTo(expiredUTime);
    QVERIFY2(std::abs(graceSecs - gracePeriodMinutes * 60) <= timeTolerance(60),
             qPrintable(QString("Woke up %1 s after the alert, the grace period is %2 s")
                        .arg(graceSecs).arg(gracePeriodMinutes * 60)));

    // The full shutdown waits again while the mount is still busy, with the weather limit
    QVERIFY(iterateScheduler("Wait for the full shutdown", 20, &sleepMs, &currentUTime, [&]() -> bool
    {
        return schedulerLogCount(kShutdownStartLog) > 0;
    }));
    const qint64 fullShutdownSecs = expiredUTime.secsTo(KStarsData::Instance()->ut());
    if (neverDone)
    {
        QVERIFY2(fullShutdownSecs >= 120 && fullShutdownSecs <= 120 + 2 * intervalSecs,
                 qPrintable(QString("The full shutdown started %1 s after the grace period, expected 120 s")
                            .arg(fullShutdownSecs)));
        QCOMPARE(schedulerLogCount(weatherTimeoutLog), 1);
    }
    else
    {
        QVERIFY2(fullShutdownSecs <= 2 * intervalSecs,
                 qPrintable(QString("The full shutdown started %1 s after the grace period").arg(fullShutdownSecs)));
        QCOMPARE(schedulerLogCount(waitLog), 0);
    }
    QCOMPARE(schedulerLogCount(kMotionTimeoutLog), 0);
}

// Expect the job to start running at startJobUTime.
// Check that the correct slew was made
// Expect the job to be interrupted at interruptUTime (if the time is valid)
void TestEkosSchedulerOps::slewAndRun(SkyObject *object, const QDateTime &startUTime, const QDateTime &interruptUTime,
                                      KStarsDateTime &currentUTime, int &sleepMs, int tolerance, const QString &label,
                                      const QDateTime &captureCompleteUTime)
{
    QVERIFY(iterateScheduler("Wait for Job Startup", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_JOBCHECK);
    }));

    double delta = KStarsData::Instance()->ut().secsTo(startUTime);
    QVERIFY2(std::abs(delta) < timeTolerance(tolerance),
             QString("Unexpected difference to job startup time: %1 secs (%2 vs %3) %4")
             .arg(delta).arg(KStarsData::Instance()->ut().toString("MM/dd hh:mm"))
             .arg(startUTime.toString("MM/dd hh:mm")).arg(label).toLocal8Bit());

    // We should be unparked at this point.
    QVERIFY(mount->parkStatus() == ISD::PARK_UNPARKED);

    QVERIFY(iterateScheduler("Wait for MountTracking", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Mount::MOUNT_SLEWING)
            mount->setStatus(ISD::Mount::MOUNT_TRACKING);
        else if (mount->status() == ISD::Mount::MOUNT_TRACKING)
            return true;
        return false;
    }));

    QVERIFY(checkLastSlew(object));

    if (interruptUTime.isValid())
    {
        // Wait until the job stops processing,
        // When scheduler state JOBCHECK changes to RUN_SCHEDULER.
        capture->setStatus(Ekos::CAPTURE_CAPTURING);
        QVERIFY(iterateScheduler("Wait for Job Interruption", 1000, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER ||
                    scheduler->moduleState()->timerState() == Ekos::RUN_NOTHING);
        }, captureCompleteUTime));

        delta = KStarsData::Instance()->ut().secsTo(interruptUTime);
        QVERIFY2(std::abs(delta) < timeTolerance(tolerance),
                 QString("Unexpected difference to interrupt time: %1 secs (%2 vs %3) %4")
                 .arg(delta).arg(KStarsData::Instance()->ut().toString("MM/dd hh:mm"))
                 .arg(interruptUTime.toString("MM/dd hh:mm")).arg(label).toLocal8Bit());

        // It should start to shutdown now.
        QVERIFY(iterateScheduler("Wait for Guide Abort", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (guider->status() == Ekos::GUIDE_ABORTED);
        }));
    }
}

// Set up the target objects to run in the scheduler (in the order they're given)
// Scheduler running at location geo.
// Start the scheduler at startSchedulerUTime.
// currentUTime and sleepMs can be set up as: KStarsDateTime currentUTime; int sleepMs = 0; and
// their latest values are returned, if you want to continue the simulation after this call.
// Similarly, dir is passed in so the temporary directory continues to exist for continued simulation.
void TestEkosSchedulerOps::startup(const GeoLocation &geo, const QVector<SkyObject*> targetObjects,
                                   const QDateTime &startSchedulerUTime, KStarsDateTime &currentUTime, int &sleepMs, QTemporaryDir &dir)
{
    const QDateTime wakeupTime;  // Not valid--it starts up right away.
    QVector<QString> esls, esqs;
    auto schedJob200x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {200, 60, "Red", "."});
    auto esqContent = TestEkosSchedulerHelper::getEsqContent(schedJob200x60);
    for (int i = 0; i < targetObjects.size(); ++i)
    {
        esls.push_back(TestEkosSchedulerHelper::getSchedulerFile(targetObjects[i], m_startupCondition, m_completionCondition, {true, true, true, true},
                       true, true));
        esqs.push_back(esqContent);
    }
    startupJobs(geo, startSchedulerUTime, &dir, esls, esqs, wakeupTime, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);
}

void TestEkosSchedulerOps::parkAndSleep(KStarsDateTime &currentUTime, int &sleepMs)
{
    // shouldSchedulerSleep() parks the mount and calls setupNextIteration(RUN_WAKEUP,
    // <wakeup-delay-ms>) in the *same* scheduler iteration.  The returned sleepMs is
    // therefore the full simulated time until the next job window (potentially many
    // hours).  If we run two separate iterateScheduler loops — one for PARK_PARKED,
    // one for RUN_WAKEUP — the first loop will advance the clock by the entire
    // sleep period on its next iteration, fire wakeUpScheduler(), and transition the
    // timer to RUN_SCHEDULER before the second loop ever starts, causing it to fail.
    //
    // The solution is to check both conditions in a single combined loop so they are
    // evaluated in the same iteration in which the park+sleep transition occurs.
    QVERIFY(iterateScheduler("Wait for Parked and Sleeping", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (mount->parkStatus() == ISD::PARK_PARKED &&
                scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
    }));

    // The critical distinction between park-and-sleep (this path) and preemptive
    // shutdown (testPreemptiveShutdown) is that Ekos/INDI must remain running here.
    // The scheduler only parked the mount and set a wakeup timer; it did NOT stop Ekos.
    // Asserting this explicitly ensures a regression in shouldSchedulerSleep() that
    // accidentally triggers a full Ekos/INDI teardown would be caught immediately.
    QVERIFY2(scheduler->moduleState()->ekosState() == Ekos::EKOS_READY,
             "Ekos must remain running during park-and-sleep (non-preemptive) shutdown");
    QVERIFY2(scheduler->moduleState()->indiState() == Ekos::INDI_READY,
             "INDI must remain connected during park-and-sleep (non-preemptive) shutdown");
}

void TestEkosSchedulerOps::wakeupAndRestart(const QDateTime &restartTime, KStarsDateTime &currentUTime, int &sleepMs)
{
    // Make sure it wakes up at the proper time.
    QVERIFY(iterateScheduler("Wait for Wakeup Tomorrow", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->moduleState()->timerState() == Ekos::RUN_SCHEDULER);
    }));

    fprintf(stderr, "Times instance %s vs reference %s, diff %lld\n",
            KStarsData::Instance()->ut().toString().toLatin1().data(),
            restartTime.toString().toLatin1().data(),
            KStarsData::Instance()->ut().secsTo(restartTime));

    // If the scheduler didn't enter sleep mode (e.g. next job is close), fast-forward
    // the simulated time until we're close to the restart time.
    qint64 deltaSecs = KStarsData::Instance()->ut().secsTo(restartTime);
    if (std::abs(deltaSecs) >= timeTolerance(DEFAULT_TOLERANCE) && deltaSecs > 0)
    {
        const int stepSecs = std::max(1, scheduler->moduleState()->updatePeriodMs() / 1000);
        const int iterations = std::max(DEFAULT_ITERATIONS, static_cast<int>(deltaSecs / stepSecs) + 5);
        QVERIFY(iterateScheduler("Advance to Restart Time", iterations, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (std::abs(KStarsData::Instance()->ut().secsTo(restartTime)) < timeTolerance(DEFAULT_TOLERANCE));
        }));
    }

    QVERIFY(std::abs(KStarsData::Instance()->ut().secsTo(restartTime)) < timeTolerance(DEFAULT_TOLERANCE));

    {
        WithInterval interval(1000, scheduler);
        // Verify the job starts up again, and the mount is once-again unparked.
        bool readyOnce = false;
        QVERIFY(iterateScheduler("Wait for Job Startup & Unparked", 50, &sleepMs, &currentUTime, [&]() -> bool
        {
            if (scheduler->process()->mountInterface() != nullptr &&
                    scheduler->process()->captureInterface() != nullptr && !readyOnce)
            {
                // Send a ready signal since the scheduler expects it.
                readyOnce = true;
                mount->sendReady();
                capture->sendReady();
            }
            return (scheduler->moduleState()->timerState() == Ekos::RUN_JOBCHECK &&
                    mount->parkStatus() == ISD::PARK_UNPARKED);
        }));
    }
}

void TestEkosSchedulerOps::testTwilightStartup_data()
{
    QTest::addColumn<QString>("city");
    QTest::addColumn<QString>("state");
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("startTimeUTC");
    QTest::addColumn<QString>("jobStartTimeUTC");

    QTest::newRow("SF")
            << "San Francisco" << "California" << "Rasalhague"
            << "Sun Jun 13 20:00:00 2021 GMT" <<  "Mon Jun 14 05:28:00 2021 GMT";

    QTest::newRow("Melbourne")
            << "Melbourne" << "Victoria" << "Arcturus"
            << "Sun Jun 13 02:00:00 2021 GMT" <<  "Mon Jun 13 08:42:00 2021 GMT";
}

void TestEkosSchedulerOps::testTwilightStartup()
{
    QFETCH(QString, city);
    QFETCH(QString, state);
    QFETCH(QString, target);
    QFETCH(QString, startTimeUTC);
    QFETCH(QString, jobStartTimeUTC);

    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName(target);
    GeoLocation * const geoPtr = KStars::Instance()->data()->locationNamed(city, state, "");
    GeoLocation &geo = *geoPtr;

    const KStarsDateTime startUTime(QDateTime::fromString(startTimeUTC));
    const KStarsDateTime jobStartUTime(QDateTime::fromString(jobStartTimeUTC));

    // move forward in 20s steps
    WithInterval interval(20000, scheduler);
    // define culmination offset of 1h as startup condition
    m_startupCondition.type = Ekos::START_ASAP;
    // initialize the scheduler
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    QVector<QString> esqVector;
    esqVector.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    QVector<QString> eslVector;
    // 3rd arg is the true for twilight enforced. 0 is minAltitude.
    eslVector.push_back(TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition, m_completionCondition, {true, true, true, true},
                        true, false, 0));
    initScheduler(geo, startUTime, &dir, eslVector, esqVector);
    initJob(startUTime, jobStartUTime);
}

void addHorizonConstraint(ArtificialHorizon *horizon, const QString &name, bool enabled,
                          const QVector<double> &azimuths, const QVector<double> &altitudes, bool ceiling = false)
{
    std::shared_ptr<LineList> pointList(new LineList);
    for (int i = 0; i < azimuths.size(); ++i)
    {
        std::shared_ptr<SkyPoint> skyp1(new SkyPoint);
        skyp1->setAlt(altitudes[i]);
        skyp1->setAz(azimuths[i]);
        pointList->append(skyp1);
    }
    horizon->addRegion(name, enabled, pointList, ceiling);
}

void TestEkosSchedulerOps::testArtificialHorizonConstraints()
{
    // In testSimpleJob, above, the wakeup time for the job was 11:35pm local time, and it used a 30-degrees min altitude.
    // Now let's add an artificial horizon constraint for 40-degrees at the azimuths where the object will be.
    // It should now wakeup and start processing at about 00:27am

    ArtificialHorizon horizon;
    addHorizonConstraint(&horizon, "r1", true, QVector<double>({100, 120}), QVector<double>({40, 40}));
    Ekos::SchedulerJob::setHorizon(&horizon);

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject*> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    QDateTime startUTime(QDateTime(QDate(2021, 6, 13), QTime(22, 0, 0), QTimeZone::utc()));

    const QDateTime wakeupTime(QDate(2021, 6, 14), QTime(07, 27, 0), QTimeZone::utc());
    runSimpleJob(geo, targetObjects[0], startUTime, wakeupTime, true);

    // Uncheck enforce artificial horizon and the wakeup time should go back to it's original time,
    // even though the artificial horizon is still there and enabled.
    init(); // Reset the scheduler.
    const QDateTime originalWakeupTime(QDate(2021, 6, 14), QTime(06, 35, 0), QTimeZone::utc());
    runSimpleJob(geo, targetObjects[0], startUTime, originalWakeupTime, /* enforce artificial horizon */false);

    // Re-check enforce artificial horizon, but remove the constraint, and the wakeup time also goes back to it's original time.
    init(); // Reset the scheduler.
    ArtificialHorizon emptyHorizon;
    Ekos::SchedulerJob::setHorizon(&emptyHorizon);
    runSimpleJob(geo, targetObjects[0], startUTime, originalWakeupTime, /* enforce artificial horizon */ true);

    // Testing that the artificial horizon constraint will end a job
    // when the altitude of the running job is below the artificial horizon at the
    // target's azimuth.
    //
    // This repeats testDawnShutdown() above, except that an artificial horizon
    // constraint is added so that the job doesn't reach dawn but rather is interrupted
    // at 3:19 local time. That's the time the azimuth reaches 175.

    init(); // Reset the scheduler.
    {
        WithInterval interval(40000, scheduler);
        ArtificialHorizon shutdownHorizon;
        // Note, just putting a constraint at 175->180 will fail this test because Altair will
        // cross past 180 and the scheduler will want to restart it before dawn.
        addHorizonConstraint(&shutdownHorizon, "h", true,
                             QVector<double>({175, 200}), QVector<double>({70, 70}));
        Ekos::SchedulerJob::setHorizon(&shutdownHorizon);

        // We'll start the scheduler at 3am local time.
        startUTime = QDateTime(QDate(2021, 6, 14), QTime(10, 0, 0), QTimeZone::utc());
        // The job should start at 3:12am local time.
        QDateTime startJobUTime = startUTime.addSecs(120);
        // The job should be interrupted by the horizon limit, which is reached about 3:19am local.
        QDateTime horizonStopUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 19, 0), QTimeZone::utc()));

        KStarsDateTime currentUTime;
        int sleepMs = 0;
        QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

        startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
        slewAndRun(targetObjects[0], startJobUTime, horizonStopUTime, currentUTime, sleepMs, DEFAULT_TOLERANCE,
                   "Horizon slewAndRun");
        parkAndSleep(currentUTime, sleepMs);
        const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), QTimeZone::utc());
        wakeupAndRestart(restartTime, currentUTime, sleepMs);
    }
}

// Similar to the above testArtificialHorizonConstraints test,
// Schedule Altair and give it an artificial horizon constraint that will stop it at 3:19am.
// However, here we also have a second job, Deneb, and test to see that the 2nd job will
// start up after Altair stops and run until dawn.

// Test of running the full scheduler with the greedy algorithm.
// This is the schedule that Greedy predicts
// Deneb      starts 06/13 22:48 done:   2760/12225 s stops 23:34  2760s (interrupted) (Tcomp 02:11 Tint 23:34 Tconstraint 03:53)
// Altair     starts 06/13 23:34 done:  12225/12225 s stops 02:57 12225s (completion) (Tcomp 02:57 Tint  Tconstraint 03:20)
// Deneb      starts 06/14 02:57 done:   6059/12225 s stops 03:52  3299s (constraint) (Tcomp 05:35 Tint  Tconstraint 03:52)
// Deneb      starts 06/14 22:43 done:  12225/12225 s stops 00:26  6166s (completion) (Tcomp 00:26 Tint  Tconstraint 03:52)
// however the code below doesn't simulate completion, so instead the Altair should end at its 3:20 constraint time

void TestEkosSchedulerOps::testGreedySchedulerRun()
{
    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    // This test will iterate the scheduler every 40 simulated seconds (to save testing time).
    WithInterval interval(40000, scheduler);

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject*> targetObjects;
    constexpr int altairIndex = 0, denebIndex = 1;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Deneb"));

    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true,
                         QVector<double>({175, 200}), QVector<double>({70, 70}));
    Ekos::SchedulerJob::setHorizon(&shutdownHorizon);

    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), QTimeZone::utc());

    QDateTime d1Start (QDateTime(QDate(2021, 6, 14), QTime( 5, 50, 0), QTimeZone::utc())); // 10:48pm
    QDateTime a1Start (QDateTime(QDate(2021, 6, 14), QTime( 6, 34, 0), QTimeZone::utc())); // 11:34pm
    QDateTime d2Start (QDateTime(QDate(2021, 6, 14), QTime(10, 20, 0), QTimeZone::utc())); //  3:20am
    QDateTime d2End   (QDateTime(QDate(2021, 6, 14), QTime(10, 53, 0), QTimeZone::utc())); //  3:53am
    QDateTime d3Start (QDateTime(QDate(2021, 6, 15), QTime( 5, 44, 0), QTimeZone::utc())); // 10:43pm the next day

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[denebIndex], d1Start, a1Start, currentUTime, sleepMs, 600, "Greedy job #1");
    startModules(currentUTime, sleepMs);
    slewAndRun(targetObjects[altairIndex], a1Start, d2Start, currentUTime, sleepMs, 600, "Greedy job #2");
    startModules(currentUTime, sleepMs);
    slewAndRun(targetObjects[denebIndex], d2Start, d2End, currentUTime, sleepMs, 600, "Greedy job #3", d2End);

    parkAndSleep(currentUTime, sleepMs);
    wakeupAndRestart(d3Start, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);
    slewAndRun(targetObjects[denebIndex], d3Start.addSecs(500), QDateTime(), currentUTime, sleepMs, 600, "Greedy job #4");
}

// Check if already existing captures are recognized properly and schedules are
// recognized are started properly or as completed.
void TestEkosSchedulerOps::testRememberJobProgress()
{
    // turn on remember job progress
    Options::setRememberJobProgress(true);
    QVERIFY(Options::rememberJobProgress());

    // a well known place and target :)
    GeoLocation geo(dms(9, 45, 54), dms(49, 6, 22), "Schwaebisch Hall", "Baden-Wuerttemberg", "Germany", +1);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Kocab");

    // Take 20:00 GMT (incl. +1h DST) and invalid wakeup time since the scheduler will start immediately
    QDateTime startUTime(QDateTime(QDate(2021, 10, 30), QTime(18, 0, 0), QTimeZone::utc()));
    const QDateTime wakeupTime;
    KStarsDateTime currentUTime;
    int sleepMs = 0;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    QDir fits_dir(dir.path() + "/images");

    QVector<TestEkosSchedulerHelper::CaptureJob> capture_jobs = QVector<TestEkosSchedulerHelper::CaptureJob>();

    // parse test data to create the capture jobs
    QFETCH(QString, jobs);
    if (jobs != "")
    {
        for (QString value : jobs.split(","))
        {
            QVERIFY(value.indexOf(":") > -1);
            QString filter = value.left(value.indexOf(":")).trimmed();
            int count      = value.right(value.length() - value.indexOf(":") - 1).toInt();
            capture_jobs.append({1000, count, filter, fits_dir.absolutePath()});
        }
    }

    // parse test data to create the existing frame files
    QFETCH(QString, frames);
    if (frames != "")
    {
        for (QString value : frames.split(","))
        {
            QVERIFY(value.indexOf(":") > -1);
            QString filter = value.left(value.indexOf(":")).trimmed();
            int count      = value.right(value.length() - value.indexOf(":") - 1).toInt();
            QDir img_dir(fits_dir);
            img_dir.mkpath("Kocab/Light/" + filter);

            // create files
            for (int i = 0; i < count; i++)
            {
                QFile frame;
                frame.setFileName(QString(img_dir.absolutePath() + "/Kocab/Light/" + filter + "/Kocab_Light_%1_%2.fits").arg(filter).arg(
                                      i));
                if (frame.open(QIODevice::WriteOnly | QIODevice::Text))
                    frame.close();
            }
        }
    }

    // start up the scheduler job
    QFETCH(int, iterations);

    TestEkosSchedulerHelper::CompletionCondition completionCondition;
    completionCondition.type = Ekos::FINISH_REPEAT;
    completionCondition.repeat = iterations;
    startupJob(geo, startUTime, &dir,
               TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition, completionCondition, {true, true, true, true},
                       false, false, 0, 90.0, nullptr, {false, false, false, false}, sleepMs),
               TestEkosSchedulerHelper::getEsqContent(capture_jobs), wakeupTime, currentUTime, sleepMs);

    // fetch the expected result from the test data
    QFETCH(bool, scheduled);

    // verify if the job is scheduled as expected
    QVERIFY(scheduler->moduleState()->jobs()[0]->getState() == (scheduled ? Ekos::SCHEDJOB_SCHEDULED :
            Ekos::SCHEDJOB_COMPLETE));
}

// Regression test: a "Repeat until terminated" job with "Remember job progress" restarted its
// next cycle with a stale count for the last filter, so one extra frame of that filter was captured.
// Capture reports the last frame of a cycle with IMAGE_RECEIVED, IDLE and COMPLETE in a burst, and
// COMPLETE was handled while the scheduler was still counting the files for IMAGE_RECEIVED.
void TestEkosSchedulerOps::testLoopCycleLastFrameCount()
{
    Options::setRememberJobProgress(true);

    GeoLocation geo(dms(9, 45, 54), dms(49, 6, 22), "Schwaebisch Hall", "Baden-Wuerttemberg", "Germany", +1);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Kocab");
    QVERIFY(targetObject != nullptr);
    const QDateTime startUTime(QDate(2021, 10, 30), QTime(18, 0, 0), QTimeZone::utc());
    KStarsDateTime currentUTime;
    int sleepMs = 0;

    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    const QString fitsDir = dir.path() + "/images";

    constexpr int framesPerFilter = 2;
    const QStringList filters = {"Ha", "OIII", "SII"};
    QVector<TestEkosSchedulerHelper::CaptureJob> captureJobs;
    for (const auto &filter : filters)
        captureJobs.append({1000, framesPerFilter, filter, fitsDir});

    TestEkosSchedulerHelper::CompletionCondition loopCondition;
    loopCondition.type = Ekos::FINISH_LOOP;
    startupJob(geo, startUTime, &dir,
               TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition, loopCondition, {true, true, true, true},
                       false, false),
               TestEkosSchedulerHelper::getEsqContent(captureJobs), QDateTime(), currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);

    QVERIFY(iterateScheduler("Wait for Capturing", DEFAULT_ITERATIONS, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->activeJob() != nullptr &&
                scheduler->activeJob()->getStage() == Ekos::SCHEDSTAGE_CAPTURING);
    }));

    // Store the frames of the first cycle as the capture module would, reporting each one.
    for (int f = 0; f < filters.size(); ++f)
    {
        const QString filterDir = QString("%1/Kocab/Light/%2").arg(fitsDir, filters[f]);
        QVERIFY(QDir().mkpath(filterDir));
        for (int i = 1; i <= framesPerFilter; ++i)
        {
            QFile frame(QString("%1/Kocab_Light_%2_%3.fits").arg(filterDir, filters[f]).arg(i, 3, 10, QLatin1Char('0')));
            QVERIFY(frame.open(QIODevice::WriteOnly));
            frame.close();

            if (f == filters.size() - 1 && i == framesPerFilter)
                break;

            capture->setStatus(Ekos::CAPTURE_IMAGE_RECEIVED);
            QTest::qWait(QWAIT_TIME);
            capture->setStatus(Ekos::CAPTURE_CAPTURING);
            QTest::qWait(QWAIT_TIME);
        }
    }

    // Report the last frame of the cycle the way Capture does, without a pause in between.
    capture->m_capturedFramesMap.clear();
    capture->setStatus(Ekos::CAPTURE_IMAGE_RECEIVED);
    capture->setStatus(Ekos::CAPTURE_IDLE);
    capture->setStatus(Ekos::CAPTURE_COMPLETE);

    // The scheduler restarts capture for the next cycle, which must start from zero for every filter.
    QTRY_COMPARE_WITH_TIMEOUT(capture->m_capturedFramesMap.size(), static_cast<int>(filters.size()), 5000);
    for (auto it = capture->m_capturedFramesMap.constBegin(); it != capture->m_capturedFramesMap.constEnd(); ++it)
        QVERIFY2(it.value() == 0, QString("Next cycle starts with %1 frames already counted for %2")
                 .arg(it.value()).arg(it.key()).toLocal8Bit());
}

void TestEkosSchedulerOps::loadGreedySchedule(
    bool first, const QString &targetName,
    const TestEkosSchedulerHelper::StartupCondition &startupCondition,
    const TestEkosSchedulerHelper::CompletionCondition &completionCondition,
    QTemporaryDir &dir, const QVector<TestEkosSchedulerHelper::CaptureJob> &captureJob, int minAltitude, double maxMoonAltitude,
    const TestEkosSchedulerHelper::ScheduleSteps steps, bool enforceTwilight, bool enforceHorizon, int errorDelay)
{
    SkyObject *object = KStars::Instance()->data()->skyComposite()->findByName(targetName);
    QVERIFY(object != nullptr);
    const QString schedulerXML =
        TestEkosSchedulerHelper::getSchedulerFile(
            object, startupCondition, completionCondition, steps, enforceTwilight, enforceHorizon, minAltitude, maxMoonAltitude,
            nullptr, {false, false, true, false}, errorDelay);

    // Write the scheduler and sequence files.
    QString f1 = writeFiles(targetName, dir, captureJob, schedulerXML);
    scheduler->load(first, QString("%1").arg(f1));
}

struct SPlan
{
    QString name;
    QString start;
    QString stop;
};

bool checkSchedule(const QVector<SPlan> &ref, const QList<Ekos::GreedyScheduler::JobSchedule> &schedule, int tolerance)
{
    bool result = true;
    if (schedule.size() != ref.size())
    {
        qCInfo(KSTARS_EKOS_TEST) << QString("Sizes don't match %1 vs ref %2").arg(schedule.size()).arg(ref.size());
        return false;
    }
    for (int i = 0; i < ref.size(); ++i)
    {
        QDateTime startTime = QDateTime::fromString(ref[i].start, "yyyy/MM/dd hh:mm");
        QDateTime stopTime = QDateTime::fromString(ref[i].stop, "yyyy/MM/dd hh:mm");
        if (!startTime.isValid() || !stopTime.isValid())
        {
            qCInfo(KSTARS_EKOS_TEST) << QString("Reference start or stop time invalid: %1 %2").arg(ref[i].start).arg(ref[i].stop);
            result = false;
        }
        else if (!schedule[i].startTime.isValid() || !schedule[i].stopTime.isValid())
        {
            qCInfo(KSTARS_EKOS_TEST) << QString("Scheduled start or stop time %1 invalid.").arg(i);
            result = false;
        }
        else
        {
            auto geo = KStarsData::Instance()->geo();
            // Use the geo's fixed UTC offset, not the system timezone, so the test is
            // independent of which timezone the machine running the test happens to be in.
            const QTimeZone geoTZ = QTimeZone(static_cast<int>(geo->TZ() * 3600));
            KStarsDateTime refStartLocal(startTime.date(), startTime.time(), geoTZ);
            KStarsDateTime refStopLocal(stopTime.date(), stopTime.time(), geoTZ);
            const KStarsDateTime refStartUT = geo->LTtoUT(refStartLocal);
            const KStarsDateTime refStopUT = geo->LTtoUT(refStopLocal);

            KStarsDateTime schedStart(schedule[i].startTime);
            KStarsDateTime schedStop(schedule[i].stopTime);
            // Always call LTtoUT: KStarsDateTime::addSecs() uses the internal Julian Day (DJD)
            // which is set from the stored date/time values and is timezone-independent, so
            // LTtoUT produces the correct UT regardless of which timezone tag the QDateTime carries.
            const KStarsDateTime schedStartUT = geo->LTtoUT(schedStart);
            const KStarsDateTime schedStopUT = geo->LTtoUT(schedStop);

            const bool mismatch = (ref[i].name != schedule[i].job->getName()) ||
                                  (std::abs(schedStartUT.secsTo(refStartUT)) > tolerance) ||
                                  (std::abs(refStartUT.secsTo(refStopUT) - schedStartUT.secsTo(schedStopUT)) > tolerance);
            if (mismatch)
            {
                qCInfo(KSTARS_EKOS_TEST)
                        << QString("Mismatch on entry %1: ref \"%2\" \"%3\" \"%4\" result \"%5\" \"%6\" \"%7\"")
                        .arg(i)
                        .arg(ref[i].name, refStartUT.toString(), refStopUT.toString(),
                             schedule[i].job->getName(),
                             schedStartUT.toString(),
                             schedStopUT.toString());
                result = false;
            }
        }
    }
    return result;
}

void TestEkosSchedulerOps::testGreedy()
{
    // Allow 10 minutes of slop in the schedule. The scheduler simulates every 2 minutes,
    // so 10 minutes is approx 5 of these timesteps.
    constexpr int checkScheduleTolerance = 600;

    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true, QVector<double>({175, 200}), QVector<double>({70, 70}));
    Ekos::SchedulerJob::setHorizon(&shutdownHorizon);
    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);
    // Local times below use the scheduler's UTC offset for this date, which includes DST.
    // (geo.TZ() of the GeoLocation above is the standard-time offset, an hour off in June.)
    const QTimeZone geoTZ(static_cast<int>(KStarsData::Instance()->geo()->TZ() * 3600));

    auto schedJob200x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {200, 60, "Red", "."});
    auto schedJob400x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {400, 60, "Red", "."});

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition, atStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition finishCompletionCondition, loopCompletionCondition;
    TestEkosSchedulerHelper::CompletionCondition repeat2CompletionCondition, atCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    atStartupCondition.type = Ekos::START_AT;
    atStartupCondition.atLocalDateTime = QDateTime(QDate(2021, 6, 14), QTime(1, 0, 0), geoTZ);
    finishCompletionCondition.type = Ekos::FINISH_SEQUENCE;
    loopCompletionCondition.type = Ekos::FINISH_LOOP;
    repeat2CompletionCondition.type = Ekos::FINISH_REPEAT;
    repeat2CompletionCondition.repeat = 2;
    atCompletionCondition.type = Ekos::FINISH_AT;
    atCompletionCondition.atLocalDateTime = QDateTime(QDate(2021, 6, 14), QTime(3, 30, 0), geoTZ);

    // Write the scheduler and sequence files.
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    // Altair is scheduled with first priority, but doesn't clear constraints immediately.
    // Deneb is also scheduled, but with twice as many captures.  Both start ASAP and run to completion.
    // Deneb runs first, gets interrupted by Altair which runs to completion.
    // Then Deneb runs for the rest of the night, and also again the next evening before it comletes.
    loadGreedySchedule(true, "Altair", asapStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, finishCompletionCondition, dir, schedJob400x60, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/13 23:35"},
        {"Altair", "2021/06/13 23:35", "2021/06/14 02:59"},
        {"Deneb",  "2021/06/14 02:59", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:48"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Disable greedy scheduling, and Deneb should NOT run until Altair is done.
    Options::setGreedyScheduling(false);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/13 23:34", "2021/06/14 03:00"},
        {"Deneb",  "2021/06/14 03:01", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:52"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    Options::setGreedyScheduling(true);

    // As above, except Altair has completion condition repeat 2. It should run longer.
    // This makes a mess of things, as Altair can't complete during the first night, running into an artificial horizon constraint.
    // It also can't start the 2nd night as early as Deneb, so the 2nd night is Deneb, Altair (completing), Deneb, and Deneb finishes the 3rd night.
    loadGreedySchedule(true, "Altair", asapStartupCondition, repeat2CompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, finishCompletionCondition, dir, schedJob400x60, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/13 23:35"},
        {"Altair", "2021/06/13 23:35", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/14 23:30"},
        {"Altair", "2021/06/14 23:31", "2021/06/15 02:29"},
        {"Deneb",  "2021/06/15 02:30", "2021/06/15 03:53"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Again disable greedy scheduling, and Deneb should NOT run until Altair is done.
    Options::setGreedyScheduling(false);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/13 23:34", "2021/06/14 03:18"},
        {"Altair", "2021/06/14 23:30", "2021/06/15 02:32"},
        {"Deneb",  "2021/06/15 02:33", "2021/06/15 03:53"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    Options::setGreedyScheduling(true);

    // Now we're using START_AT 6/14 1am for Altair (but not repeating twice).
    // Deneb will run until then (1am) the 1st night. Altair will run until it hits the horizon constraint.
    // Deneb runs through the end of the night, and again the next night until it completes.
    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, finishCompletionCondition, dir, schedJob400x60, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/14 01:00"},
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 02:44"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Again disable greedy scheduling, and Deneb should NOT run until Altair is done.
    Options::setGreedyScheduling(false);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:53"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    Options::setGreedyScheduling(true);

    // We again use START_AT 6/14 1am for Altair, but force Deneb to complete by 3:30am on 6/14.
    // So we get the same first two lines as above, but now Deneb stops on the 3rd line at 3:30.
    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, atCompletionCondition, dir, schedJob400x60, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/14 01:00"},
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:30"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Again disable greedy scheduling, and Deneb should NOT run until Altair is done.
    Options::setGreedyScheduling(false);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:30"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    Options::setGreedyScheduling(true);

    // We have the same Altair constraints, but this time allow Deneb to run forever.
    // It will look like the 3rd test, except Deneb keeps running through the end of the simulated time (2 days).
    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, loopCompletionCondition, dir, schedJob400x60, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/14 01:00"},
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:52"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Again disable greedy scheduling, and Deneb should NOT run until Altair is done.
    Options::setGreedyScheduling(false);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:19"},
        {"Deneb",  "2021/06/14 03:20", "2021/06/14 03:52"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:52"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    Options::setGreedyScheduling(true);

    // Altair stars asap, Deneb has an at 1am startup and loop finish.
    // Altair should start when it's able (about 23:35) but get interrupted by deneb which is higher priority
    // because of its startat. Altair will start up again the next evening because Deneb's startat will have expired.
    loadGreedySchedule(true, "Altair", asapStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", atStartupCondition, loopCompletionCondition, dir, schedJob400x60, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/13 23:34", "2021/06/14 01:00"},
        {"Deneb",  "2021/06/14 01:00", "2021/06/14 03:52"},
        {"Altair", "2021/06/14 23:30", "2021/06/15 01:31"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Again disable greedy scheduling. Nothing should change as no jobs were running before higher priority ones.
    Options::setGreedyScheduling(false);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair", "2021/06/13 23:34", "2021/06/14 01:00"},
        {"Deneb",  "2021/06/14 01:00", "2021/06/14 03:52"},
        {"Altair", "2021/06/14 23:30", "2021/06/15 01:31"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    Options::setGreedyScheduling(true);
}

void TestEkosSchedulerOps::testMaxMoonAltitude()
{
    // Allow 10 minutes of slop in the schedule. The scheduler simulates every 2 minutes,
    // so 10 minutes is approx 5 of these timesteps.
    constexpr int checkScheduleTolerance = 600;

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 4, 21), QTime(4, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);

    auto schedJob5x120 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {120, 5, "Red", "."});
    Options::setGreedyScheduling(true);

    // Write the scheduler and sequence files.
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition finishCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    finishCompletionCondition.type = Ekos::FINISH_SEQUENCE;

    loadGreedySchedule(true, "Dubhe", asapStartupCondition, finishCompletionCondition, dir, schedJob5x120, 30, 40.0);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Dubhe", "2021/04/20 23:40", "2021/04/20 23:55"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
}

void TestEkosSchedulerOps::testGreedyStartAt()
{
    // Allow 10 minutes of slop in the schedule. The scheduler simulates every 2 minutes,
    // so 10 minutes is approx 5 of these timesteps.
    constexpr int checkScheduleTolerance = 600;

    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    WithInterval interval(40000, scheduler);

    ArtificialHorizon horizon;
    Ekos::SchedulerJob::setHorizon(&horizon);

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true, QVector<double>({175, 200}), QVector<double>({70, 70}));

    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);
    // Local times below use the scheduler's UTC offset for this date, which includes DST.
    // (geo.TZ() of the GeoLocation above is the standard-time offset, an hour off in June.)
    const QTimeZone geoTZ(static_cast<int>(KStarsData::Instance()->geo()->TZ() * 3600));

    auto schedJob60x10 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {60, 30, "Red", "."});
    auto schedJob30x16 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {30, 40, "Red", "."});

    TestEkosSchedulerHelper::StartupCondition atStartupCondition, atStartupCondition2;
    TestEkosSchedulerHelper::CompletionCondition finishCompletionCondition;
    atStartupCondition.type = Ekos::START_AT;
    atStartupCondition.atLocalDateTime = QDateTime(QDate(2021, 6, 14), QTime(1, 0, 0), geoTZ);
    atStartupCondition2.type = Ekos::START_AT;
    atStartupCondition2.atLocalDateTime = QDateTime(QDate(2021, 6, 14), QTime(3, 0, 0), geoTZ);
    finishCompletionCondition.type = Ekos::FINISH_SEQUENCE;

    // Write the scheduler and sequence files.
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    initTimeGeo(geo, startUTime);

    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob60x10, 30);
    loadGreedySchedule(false, "Deneb", atStartupCondition2, finishCompletionCondition, dir, schedJob30x16, 30);
    scheduler->process()->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Altair",  "2021/06/14 01:00", "2021/06/14 01:33"},
        {"Deneb", "2021/06/14 03:00", "2021/06/14 03:23"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));


    scheduler->moduleState()->init();
    QVERIFY(scheduler->moduleState()->timerState() == Ekos::RUN_WAKEUP);
    int sleepMs = 0;
    KStarsDateTime currentUTime;
    const QDateTime wakeupTime;  // Not valid--it starts up right away.

    startupJobs2(startUTime, wakeupTime, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);

    QVector<SkyObject*> targetObjects;
    constexpr int altairIndex = 0, denebIndex = 1;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Deneb"));
    QDateTime a1Start (QDateTime(QDate(2021, 6, 14), QTime( 8, 00, 0), QTimeZone::utc())); // 1am
    QDateTime a1End   (QDateTime(QDate(2021, 6, 14), QTime( 8, 33, 0), QTimeZone::utc())); // 1:33am
    QDateTime d1Start (QDateTime(QDate(2021, 6, 14), QTime(10, 00, 0), QTimeZone::utc())); // 3am
    QDateTime d1End   (QDateTime(QDate(2021, 6, 14), QTime(10, 23, 0), QTimeZone::utc())); // 3:23am

    slewAndRun(targetObjects[altairIndex], a1Start, a1End, currentUTime, sleepMs, 600, "Greedy job #1", a1End);
    wakeupAndRestart(d1Start, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);
    slewAndRun(targetObjects[denebIndex], d1Start, d1End, currentUTime, sleepMs, 600, "Greedy job #2", d1End);

    Options::setGreedyScheduling(true);
}

void TestEkosSchedulerOps::testGroups()
{
    // Allow 10 minutes of slop in the schedule. The scheduler simulates every 2 minutes,
    // so 10 minutes is approx 5 of these timesteps.
    constexpr int checkScheduleTolerance = 600;

    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true, QVector<double>({175, 200}), QVector<double>({70, 70}));
    Ekos::SchedulerJob::setHorizon(&shutdownHorizon);
    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);

    // About a 30-minute job
    auto schedJob30minutes = QVector<TestEkosSchedulerHelper::CaptureJob>(
    {{180, 4, "Red", "."}, {180, 6, "Blue", "."}});

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition, atStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition finishCompletionCondition, loopCompletionCondition;
    TestEkosSchedulerHelper::CompletionCondition repeat2CompletionCondition, atCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    finishCompletionCondition.type = Ekos::FINISH_SEQUENCE;
    loopCompletionCondition.type = Ekos::FINISH_LOOP;
    repeat2CompletionCondition.type = Ekos::FINISH_REPEAT;
    repeat2CompletionCondition.repeat = 2;

    // Write the scheduler and sequence files.
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    // Create 3 jobs in the same group, of the same target, each should last ~30 minutes (not counting repetitions).
    // the first job just runs the 30-minute sequence, the 2nd repeats forever, the 3rd repeats twice.
    // Given the grouping we should see jobs scheduled about every 30 minutes
    // orderdc as: J1, J2, J3, J2 (skips the completed J1), J3, J2 (looing forever as J3 is now done).
    loadGreedySchedule(true, "Altair", asapStartupCondition, finishCompletionCondition, dir, schedJob30minutes, 30);
    scheduler->moduleState()->jobs().last()->setName("J1finish");
    scheduler->moduleState()->jobs().last()->setGroup("group1");
    loadGreedySchedule(false, "Altair", asapStartupCondition, loopCompletionCondition, dir, schedJob30minutes, 30);
    scheduler->moduleState()->jobs().last()->setName("J2loop");
    scheduler->moduleState()->jobs().last()->setGroup("group1");
    loadGreedySchedule(false, "Altair", asapStartupCondition, repeat2CompletionCondition, dir, schedJob30minutes, 30);
    scheduler->moduleState()->jobs().last()->setName("J3repeat2");
    scheduler->moduleState()->jobs().last()->setGroup("group1");
    scheduler->process()->evaluateJobs(false);

    QVERIFY(checkSchedule(
    {
        {"J1finish",  "2021/06/13 23:34", "2021/06/14 00:10"},
        {"J2loop",    "2021/06/14 00:11", "2021/06/14 00:47"},
        {"J3repeat2", "2021/06/14 00:48", "2021/06/14 01:24"},
        {"J2loop",    "2021/06/14 01:25", "2021/06/14 01:55"},
        {"J3repeat2", "2021/06/14 01:56", "2021/06/14 02:26"},
        {"J2loop",    "2021/06/14 02:27", "2021/06/14 03:19"},
        {"J2loop",    "2021/06/14 23:31", "2021/06/15 03:15"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Now do the same thing, but this time disable the group scheduling (by not assigning groups).
    // This time J1 should run then J2 will just run/repeat forever.
    loadGreedySchedule(true, "Altair", asapStartupCondition, finishCompletionCondition, dir, schedJob30minutes, 30);
    scheduler->moduleState()->jobs().last()->setName("J1finish");
    loadGreedySchedule(false, "Altair", asapStartupCondition, loopCompletionCondition, dir, schedJob30minutes, 30);
    scheduler->moduleState()->jobs().last()->setName("J2loop");
    loadGreedySchedule(false, "Altair", asapStartupCondition, repeat2CompletionCondition, dir, schedJob30minutes, 30);
    scheduler->moduleState()->jobs().last()->setName("J3repeat2");
    scheduler->process()->evaluateJobs(false);

    QVERIFY(checkSchedule(
    {
        {"J1finish",  "2021/06/13 23:34", "2021/06/14 00:10"},
        {"J2loop",    "2021/06/14 00:11", "2021/06/14 03:19"},
        {"J2loop",    "2021/06/14 23:30", "2021/06/15 03:16"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
}

void TestEkosSchedulerOps::testGreedyAborts()
{
    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);

    // Allow 10 minutes of slop in the schedule. The scheduler simulates every 2 minutes,
    // so 10 minutes is approx 5 of these timesteps.
    constexpr int checkScheduleTolerance = 600;

    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    Ekos::SchedulerJob::setHorizon(nullptr);

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    // Use the geo's fixed UTC offset so the test is timezone-independent.
    const QTimeZone geoTZ = QTimeZone(static_cast<int>(geo.TZ() * 3600));

    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2022, 2, 28), QTime(10, 29, 26), QTimeZone::utc());
    initTimeGeo(geo, startUTime);

    auto schedJob200x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {200, 60, "Red", "."});
    TestEkosSchedulerHelper::StartupCondition asapStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition loopCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    loopCompletionCondition.type = Ekos::FINISH_LOOP;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    // Parameters related to resheduling aborts and the delay times are in the scheduler .esl file created in loadGreedyScheduler.
    const TestEkosSchedulerHelper::ScheduleSteps steps = {true, true, true, true};
    const bool enforceTwilight = true;
    const bool enforceHorizon = true;
    const int errorDelay = 3600;

    loadGreedySchedule(true, "M 104", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 36, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);
    loadGreedySchedule(false, "NGC 3628", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 45, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);
    loadGreedySchedule(false, "M 5", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 30, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);
    loadGreedySchedule(false, "M 42", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 42, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);

    // start the scheduler at 1am
    KStarsDateTime evalUTime(QDate(2022, 2, 28), QTime(9, 00, 00), QTimeZone::utc());
    KStarsData::Instance()->changeDateTime(evalUTime);

    scheduler->process()->evaluateJobs(false);

    QVERIFY(checkSchedule(
    {
        {"M 104",      "2022/02/28 01:00", "2022/02/28 03:52"},
        {"M 5",        "2022/02/28 03:52", "2022/02/28 05:14"},
        {"M 42",       "2022/02/28 19:31", "2022/02/28 20:43"},
        {"NGC 3628",   "2022/02/28 22:02", "2022/03/01 00:38"},
        {"M 104",      "2022/03/01 00:39", "2022/03/01 03:49"},
        {"M 5",        "2022/03/01 03:50", "2022/03/01 05:12"},
        {"M 42",       "2022/03/01 19:31", "2022/03/01 20:39"},
        {"NGC 3628",   "2022/03/01 21:58", "2022/03/02 00:34"},
        {"M 104",      "2022/03/02 00:35", "2022/03/02 03:45"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));

    // Now load the same schedule, but set the M104 job to have been aborted a minute before.
    loadGreedySchedule(true, "M 104", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 36, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);
    loadGreedySchedule(false, "NGC 3628", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 45, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);
    loadGreedySchedule(false, "M 5", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 30, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);
    loadGreedySchedule(false, "M 42", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 42, 90.0, steps,
                       enforceTwilight, enforceHorizon, errorDelay);

    // Otherwise time changes below will trigger reschedules and mess up test.
    scheduler->moduleState()->setSchedulerState(Ekos::SCHEDULER_RUNNING);

    // Find the M 104 job and make it aborted at 12:59am
    KStarsDateTime abortUTime(QDate(2022, 2, 28), QTime(8, 59, 00), QTimeZone::utc());
    KStarsData::Instance()->changeDateTime(abortUTime);

    Ekos::SchedulerJob *m104Job = nullptr;
    for (auto job : scheduler->moduleState()->jobs())
        if (job->getName() == "M 104")
        {
            m104Job = job;
            m104Job->setState(Ekos::SCHEDJOB_ABORTED);
        }
    QVERIFY(m104Job != nullptr);

    // start the scheduler at 1am
    KStarsData::Instance()->changeDateTime(evalUTime);

    scheduler->process()->evaluateJobs(false);

    // The M104 job is no longer the first job, since aborted jobs are delayed an hour,
    QVERIFY(checkSchedule(
    {
        {"NGC 3628",   "2022/02/28 01:00", "2022/02/28 02:00"},
        {"M 104",      "2022/02/28 02:00", "2022/02/28 03:52"},
        {"M 5",        "2022/02/28 03:52", "2022/02/28 05:14"},
        {"M 42",       "2022/02/28 19:31", "2022/02/28 20:43"},
        {"NGC 3628",   "2022/02/28 22:02", "2022/03/01 00:38"},
        {"M 104",      "2022/03/01 00:39", "2022/03/01 03:49"},
        {"M 5",        "2022/03/01 03:50", "2022/03/01 05:12"},
        {"M 42",       "2022/03/01 19:31", "2022/03/01 20:39"},
        {"NGC 3628",   "2022/03/01 21:58", "2022/03/02 00:34"},
        {"M 104",      "2022/03/02 00:35", "2022/03/02 03:45"}},

    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
    auto ngc3628 = scheduler->process()->getGreedyScheduler()->getSchedule()[0].job;
    QVERIFY(ngc3628->getName() == "NGC 3628");

    // And ngc3628 should not be preempted right away,
    QDateTime localTime(QDate(2022, 2, 28), QTime(1, 00, 00), geoTZ);
    bool keepRunning = scheduler->process()->getGreedyScheduler()->checkJob(scheduler->moduleState()->jobs(), localTime,
                       ngc3628);
    QVERIFY(keepRunning);

    // nor in a half-hour.
    auto newTime = evalUTime.addSecs(1800);
    KStarsData::Instance()->changeDateTime(newTime);
    localTime = localTime.addSecs(1800);
    keepRunning = scheduler->process()->getGreedyScheduler()->checkJob(scheduler->moduleState()->jobs(), localTime, ngc3628);
    QVERIFY(keepRunning);

    // But if we wait until 2am, m104 should preempt it,
    newTime = newTime.addSecs(1800);
    KStarsData::Instance()->changeDateTime(newTime);
    localTime = localTime.addSecs(1800);
    keepRunning = scheduler->process()->getGreedyScheduler()->checkJob(scheduler->moduleState()->jobs(), localTime, ngc3628);
    QVERIFY(!keepRunning);

    // and M104 should be scheduled to start running "now" (2am).
    scheduler->process()->evaluateJobs(false);
    auto newSchedule = scheduler->process()->getGreedyScheduler()->getSchedule();
    QVERIFY(newSchedule.size() > 0);
    QVERIFY(newSchedule[0].job->getName() == "M 104");
    QVERIFY(std::abs(newSchedule[0].startTime.secsTo(
                         QDateTime(QDate(2022, 2, 28), QTime(2, 00, 00), geoTZ))) < 200);
}

void TestEkosSchedulerOps::testArtificialCeiling()
{
    constexpr int checkScheduleTolerance = 600;
    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);

    ArtificialHorizon horizon;
    QVector<double> az1  = {259.0, 260.0, 299.0, 300.0, 330.0,   0.0,  30.0,  70.0,  71.0,  90.0, 120.0, 150.0, 180.0, 210.0, 240.0, 259.99};
    QVector<double> alt1 = { 90.0,  26.0,  26.0,  26.0,  26.0,  26.0,  26.0,  26.0,  90.0,  90.0,  90.0,  90.0,  90.0,  90.0,  90.0,  90.0};
    addHorizonConstraint(&horizon, "floor", true, az1, alt1);

    QVector<double> az2  = {260.0, 300.0, 330.0,  0.0,  30.0,  70.0};
    QVector<double> alt2 = { 66.0,  66.0,  66.0, 66.0,  66.0,  66.0};
    addHorizonConstraint(&horizon, "ceiling", true, az2, alt2, true); // last true --> ceiling

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-75, 40), dms(45, 40), "New York", "NY", "USA", -5);
    Ekos::SchedulerJob::setHorizon(&horizon);
    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2022, 8, 21), QTime(16, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);

    auto schedJob200x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {200, 60, "Red", "."});
    auto schedJob400x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {400, 60, "Red", "."});

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition loopCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    loopCompletionCondition.type = Ekos::FINISH_LOOP;

    // Write the scheduler and sequence files.
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    loadGreedySchedule(true, "theta Bootis", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 0, 90.0,
    {true, true, true, true}, false, true); // min alt = 0, don't enforce twilight
    scheduler->process()->evaluateJobs(false);

    // There are no altitude constraints, just an artificial horizon with 2 lines, the top a ceiling.
    // It should scheduler from "now" until the star reaches the ceiling, then shut off until it lowers
    // back into the window, then shutoff again when the star goes below the artificial horizon, and
    // then start-up again when it once again raises above the artificial horizon into the window.
    QVERIFY(checkSchedule(
    {
        {"HD 126660", "2022/08/21 12:00", "2022/08/21 15:05"},
        {"HD 126660", "2022/08/21 19:50", "2022/08/22 00:34"},
        {"HD 126660", "2022/08/22 10:30", "2022/08/22 15:08"},
        {"HD 126660", "2022/08/22 19:46", "2022/08/23 00:30"},
        {"HD 126660", "2022/08/23 10:26", "2022/08/23 15:04"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), checkScheduleTolerance));
}

// This test checked the old settingAltitudeCutoff option, which has been removed,
// but the test is still worthwhile for checking general scheduling.
void TestEkosSchedulerOps::testSettingAltitudeBug()
{
    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);

    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    Ekos::SchedulerJob::setHorizon(nullptr);

    GeoLocation geo(dms(9, 45, 54), dms(49, 6, 22), "Schwaebisch Hall", "Baden-Wuerttemberg", "Germany", +1);
    const QDateTime time1 = QDateTime(QDate(2022, 3, 7), QTime(21, 28, 55), QTimeZone::utc()); //22:28 local
    initTimeGeo(geo, time1);

    auto wolfgangJob = QVector<TestEkosSchedulerHelper::CaptureJob>(
    {
        {360, 3, "L", "."}, {360, 1, "R", "."}, {360, 1, "G", "."},
        {360, 1, "B", "."}, {360, 2, "L", "."}});

    // Guessing he was using 40minute offsets
    Options::setDawnOffset(.666);
    Options::setDuskOffset(-.666);

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition loopCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    loopCompletionCondition.type = Ekos::FINISH_LOOP;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    loadGreedySchedule(true, "NGC 2359", asapStartupCondition, loopCompletionCondition, dir, wolfgangJob, 20);
    loadGreedySchedule(false, "NGC 2392", asapStartupCondition, loopCompletionCondition, dir, wolfgangJob, 20);
    loadGreedySchedule(false, "M 101", asapStartupCondition, loopCompletionCondition, dir, wolfgangJob, 20);

    scheduler->process()->evaluateJobs(false);

    // In the log with bug, the original schedule had 2359 running 20:30 -> 23:04
    //  "Greedy Scheduler plan for the next 48 hours (0.075)s:"
    //  "NGC 2359  03/07  20:30 --> 23:04 altitude 19.8 < 20.0"
    //  "NGC 2392  03/07  23:05 --> 02:23 altitude 19.7 < 20.0"
    //  "M 101     03/08  02:23 --> 05:43 twilight"
    //  "NGC 2359  03/08  19:22 --> 22:58 altitude 19.9 < 20.0"
    //  "NGC 2392  03/08  22:59 --> 02:17 altitude 19.9 < 20.0"
    //  "M 101     03/09  02:18 --> 05:40 twilight"
    //  "NGC 2359  03/09  19:23 --> 22:55 altitude 19.8 < 20.0"
    //
    // but the job was stopped at 22:28, and when the schedule was made then
    // it could no longer run ngc2359.
    //
    //  "Greedy Scheduler plan for the next 48 hours (0.079)s:"
    //  "NGC 2392  03/07  22:28 --> 02:22 altitude 19.7 < 20.0"
    //  "M 101     03/08  02:23 --> 05:43 twilight"
    //  "NGC 2359  03/08  19:22 --> 22:58 altitude 19.9 < 20.0"
    //  "NGC 2392  03/08  22:59 --> 02:17 altitude 19.9 < 20.0"
    //  "M 101     03/09  02:18 --> 05:42 twilight"
    //  "NGC 2359  03/09  19:23 --> 22:55 altitude 19.8 < 20.0"
    //
    // The issue was that settingAltitudeCutoff was being applied to the running
    // job, preempting it. The intention of that parameter is to stop new jobs
    // from being scheduled near their altitude cutoff times, not to preempt existing ones.

    KStarsDateTime time2(QDate(2022, 3, 7), QTime(19, 30, 00), QTimeZone::utc()); //20:30 local
    initTimeGeo(geo, time2);
    scheduler->process()->evaluateJobs(false);

    // This is fixed, and now, when re-evaluated at 22:28 it should not be preempted.
    auto greedy = scheduler->process()->getGreedyScheduler();
    Ekos::SchedulerJob *job2359 = scheduler->moduleState()->jobs()[0];
    auto time1Local = (Qt::UTC == time1.timeSpec() ? geo.UTtoLT(KStarsDateTime(time1)) : time1);
    QVERIFY(greedy->checkJob(scheduler->moduleState()->jobs(), time1Local, job2359));
}

// This creates empty/dummy fits files to be discovered inside estimateJobTime, when it
// tries to figure out how of of the job is already completed.
// Only useful when Options::rememberJobProgress() is true.
void TestEkosSchedulerOps::makeFitsFiles(const QString &base, int num)
{
    QFile f("/dev/null");
    QFileInfo info(base);
    info.dir().mkpath(".");

    for (int i = 0; i < num; ++i)
    {
        QString newName = QString("%1_%2.fits").arg(base).arg(i);
        QVERIFY(f.copy(newName));
    }
}

// In this test, we simulate pre-existing captures with the rememberJobProgress option,
// and make sure Greedy estimates the job completion time properly.
void TestEkosSchedulerOps::testEstimateTimeBug()
{
    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);
    Options::setRememberJobProgress(true);
    Options::setDitherEnabled(true);
    Options::setDitherFrames(1);

    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);
    Ekos::SchedulerJob::setHorizon(nullptr);
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    GeoLocation geo(dms(9, 45, 54), dms(49, 6, 22), "Schwaebisch Hall", "Baden-Wuerttemberg", "Germany", +1);
    const QDateTime time1 = QDateTime(QDate(2022, 3, 20), QTime(18, 52, 48), QTimeZone::utc()); //19:52 local
    initTimeGeo(geo, time1);

    auto path = dir.filePath(".");
    auto jobLRGB = QVector<TestEkosSchedulerHelper::CaptureJob>(
    {{360, 3, "L", path}, {360, 1, "R", path}, {360, 1, "G", path}, {360, 1, "B", path}, {360, 2, "L", path}});
    auto jobNB = QVector<TestEkosSchedulerHelper::CaptureJob>({{600, 1, "Ha", path}, {600, 1, "OIII", path}});

    // Guessing he was using 40minute offsets
    Options::setDawnOffset(.666);
    Options::setDuskOffset(-.666);

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition loopCompletionCondition;
    asapStartupCondition.type = Ekos::START_ASAP;
    loopCompletionCondition.type = Ekos::FINISH_LOOP;
    TestEkosSchedulerHelper::CompletionCondition repeat9;
    repeat9.type = Ekos::FINISH_REPEAT;
    repeat9.repeat = 9;

    makeFitsFiles(QString("%1%2").arg(path, "/NGC_2359/Light/L/NGC_2359_Light_L"), 41);
    makeFitsFiles(QString("%1%2").arg(path, "/NGC_2359/Light/R/NGC_2359_Light_R"), 8);
    makeFitsFiles(QString("%1%2").arg(path, "/NGC_2359/Light/G/NGC_2359_Light_G"), 8);
    makeFitsFiles(QString("%1%2").arg(path, "/NGC_2359/Light/B/NGC_2359_Light_B"), 8);
    makeFitsFiles(QString("%1%2").arg(path, "/NGC_2359/Light/Ha/NGC_2359_Light_Ha"), 12);
    makeFitsFiles(QString("%1%2").arg(path, "/NGC_2359/Light/OIII/NGC_2359_Light_OIII"), 11);

    // Not focusing in these schedule steps.
    TestEkosSchedulerHelper::ScheduleSteps steps = {true, false, true, true};

    loadGreedySchedule(true, "NGC 2359", asapStartupCondition, repeat9, dir, jobLRGB, 20, 90.0, steps);
    loadGreedySchedule(false, "NGC 2359", asapStartupCondition, loopCompletionCondition, dir, jobNB, 20, 90.0, steps);
    loadGreedySchedule(false, "M 53", asapStartupCondition, loopCompletionCondition, dir, jobLRGB, 20, 90.0, steps);

    scheduler->process()->evaluateJobs(false);

    // The first (LRGB) version of NGC 2359 is mostly completed and should just run for about 45 minutes.
    // At that point, the narrowband NGC2359 and LRGB M53 jobs run.
    QVERIFY(checkSchedule(
    {
        {"NGC 2359",   "2022/03/20 19:52", "2022/03/20 20:38"},
        {"NGC 2359",   "2022/03/20 20:39", "2022/03/20 22:11"},
        {"M 53",       "2022/03/20 22:12", "2022/03/21 05:14"},
        {"NGC 2359",   "2022/03/21 19:45", "2022/03/21 22:07"},
        {"M 53",       "2022/03/21 22:08", "2022/03/22 05:12"},
        {"NGC 2359",   "2022/03/22 19:47", "2022/03/22 22:03"}},
    scheduler->process()->getGreedyScheduler()->getSchedule(), 300));
}

// A helper for setting up the esl and esq files for the test below.
// The issue is the test loads an esl file, and that file has in it the name of the esq files
// it needs to load. These files are put in temporary directories, so the contents needs
// to modified to reflect the locations of the esq files.
namespace
{
QString setupMessierFiles(QTemporaryDir &dir, const QString &eslFilename, const QString esqFilename)
{
    QString esq = esqFilename;
    QString esl = eslFilename;

    const QString esqPath(dir.filePath(esq));
    const QString eslPath(dir.filePath(esl));

    // Confused about where the test runs...
    if (!QFile::exists(esq) || !QFile::exists(esl))
    {
        esq = QString("../Tests/kstars_ui/%1").arg(esqFilename);
        esl = QString("../Tests/kstars_ui/%1").arg(eslFilename);
        qCInfo(KSTARS_EKOS_TEST) << QString("Didn't find the files, looking in %1 %2").arg(esq, esl);
        if (!QFile::exists(esq) || !QFile::exists(esl))
            return QString();
    }

    // Copy the sequence file to the temp directory with test-path substitutions
    QFile esqFile(esq);
    if (!esqFile.open(QFile::ReadOnly | QFile::Text))
        return QString();
    QTextStream esqIn(&esqFile);
    QString esqContents = esqIn.readAll();
    esqFile.close();
    esqContents.replace(QStringLiteral("@TEST_HOME@"), KTest::homePath());
    esqContents.replace(QStringLiteral("@TEST_SCRATCH@"), QDir::toNativeSeparators(KTest::tempPath()));
    TestEkosSchedulerHelper::writeFile(esqPath, esqContents);

    // Read and modify the esl file: change __ESQ_FILE__ to the value of esqPath, and write it out to the temp directory.
    QFile eslFile(esl);
    if (!eslFile.open(QFile::ReadOnly | QFile::Text))
        return QString();
    QTextStream in(&eslFile);
    QString eslContents = in.readAll();
    eslFile.close();
    eslContents.replace(QStringLiteral("__ESQ_FILE__"), esqPath);
    eslContents.replace(QStringLiteral("@TEST_HOME@"), KTest::homePath());
    eslContents.replace(QStringLiteral("@TEST_SCRATCH@"), QDir::toNativeSeparators(KTest::tempPath()));
    TestEkosSchedulerHelper::writeFile(eslPath, eslContents);

    if (!QFile::exists(esqPath) || !QFile::exists(eslPath))
        return QString();

    return eslPath;
}
}  // namespace

void TestEkosSchedulerOps::testGreedyMessier()
{
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));
    QString esl0Path = setupMessierFiles(dir, "Messier-1-40-alt0.esl", "Messier-1-40.esq");
    QString esl30Path = setupMessierFiles(dir, "Messier-1-40-alt30.esl", "Messier-1-40.esq");
    if (esl0Path.isEmpty() || esl30Path.isEmpty())
    {
        QSKIP("Skipping test because of missing esq or esl files");
        return;
    }

    Options::setDawnOffset(0);
    Options::setDuskOffset(0);
    Options::setPreDawnTime(0);
    Options::setRememberJobProgress(false);
    Options::setSchedulerAlgorithm(Ekos::ALGORITHM_GREEDY);

    GeoLocation geo(dms(9, 45, 54), dms(49, 6, 22), "Schwaebisch Hall", "Baden-Wuerttemberg", "Germany", +1);
    const QDateTime time1 = QDateTime(QDate(2022, 3, 7), QTime(21, 28, 55), QTimeZone::utc()); //22:28 local
    initTimeGeo(geo, time1);

    qCInfo(KSTARS_EKOS_TEST) << QString("Calculate schedule with no artificial horizon and 0 min altitude.");
    Ekos::SchedulerJob::setHorizon(nullptr);
    scheduler->load(true, QString("%1").arg(esl0Path));
    const QVector<SPlan> scheduleMinAlt0 =
    {
        {"M 39", "2022/03/07 22:28", "2022/03/07 22:39"},
        {"M 33", "2022/03/07 22:40", "2022/03/07 22:50"},
        {"M 32", "2022/03/07 22:51", "2022/03/07 23:02"},
        {"M 31", "2022/03/07 23:03", "2022/03/07 23:13"},
        {"M 34", "2022/03/07 23:14", "2022/03/07 23:25"},
        {"M 1",  "2022/03/07 23:26", "2022/03/07 23:36"},
        {"M 35", "2022/03/07 23:37", "2022/03/07 23:48"},
        {"M 38", "2022/03/07 23:49", "2022/03/07 23:59"},
        {"M 36", "2022/03/08 00:00", "2022/03/08 00:11"},
        {"M 40", "2022/03/08 00:12", "2022/03/08 00:22"},
        {"M 3",  "2022/03/08 00:23", "2022/03/08 00:34"},
        {"M 13", "2022/03/08 00:35", "2022/03/08 00:45"},
        {"M 29", "2022/03/08 00:46", "2022/03/08 00:57"},
        {"M 5",  "2022/03/08 00:58", "2022/03/08 01:08"},
        {"M 12", "2022/03/08 01:09", "2022/03/08 01:20"},
        {"M 27", "2022/03/08 01:23", "2022/03/08 01:34"},
        {"M 10", "2022/03/08 01:35", "2022/03/08 01:45"},
        {"M 14", "2022/03/08 01:46", "2022/03/08 01:57"},
        {"M 4",  "2022/03/08 02:04", "2022/03/08 02:14"},
        {"M 9",  "2022/03/08 02:15", "2022/03/08 02:26"},
        {"M 11", "2022/03/08 02:39", "2022/03/08 02:49"},
        {"M 19", "2022/03/08 02:50", "2022/03/08 03:01"},
        {"M 26", "2022/03/08 03:02", "2022/03/08 03:12"},
        {"M 16", "2022/03/08 03:13", "2022/03/08 03:24"},
        {"M 23", "2022/03/08 03:25", "2022/03/08 03:35"},
        {"M 17", "2022/03/08 03:36", "2022/03/08 03:47"},
        {"M 15", "2022/03/08 03:50", "2022/03/08 04:01"},
        {"M 18", "2022/03/08 04:02", "2022/03/08 04:12"},
        {"M 24", "2022/03/08 04:13", "2022/03/08 04:24"},
        {"M 21", "2022/03/08 04:25", "2022/03/08 04:35"},
        {"M 20", "2022/03/08 04:36", "2022/03/08 04:47"},
        {"M 2",  "2022/03/08 04:56", "2022/03/08 05:04"},
        {"M 25", "2022/03/09 03:21", "2022/03/09 03:32"},
        {"M 8",  "2022/03/09 03:33", "2022/03/09 03:43"},
        {"M 28", "2022/03/09 03:49", "2022/03/09 04:00"},
        {"M 6",  "2022/03/09 04:03", "2022/03/09 04:14"},
        {"M 22", "2022/03/09 04:15", "2022/03/09 04:25"},
        {"M 2",  "2022/03/09 04:51", "2022/03/09 04:54"},
        {"M 7",  "2022/03/09 04:55", "2022/03/09 05:01"}
    };
    QVERIFY(checkSchedule(scheduleMinAlt0, scheduler->process()->getGreedyScheduler()->getSchedule(), 300));

    qCInfo(KSTARS_EKOS_TEST) << QString("Calculate schedule with no artificial horizon and 30 min altitude.");
    Ekos::SchedulerJob::setHorizon(nullptr);
    scheduler->load(true, QString("%1").arg(esl30Path));
    const QVector<SPlan> scheduleMinAlt30 =
    {
        {"M 1",  "2022/03/07 22:28", "2022/03/07 22:39"},
        {"M 35", "2022/03/07 22:40", "2022/03/07 22:50"},
        {"M 38", "2022/03/07 22:51", "2022/03/07 23:02"},
        {"M 36", "2022/03/07 23:03", "2022/03/07 23:13"},
        {"M 40", "2022/03/07 23:14", "2022/03/07 23:25"},
        {"M 3",  "2022/03/07 23:26", "2022/03/07 23:36"},
        {"M 13", "2022/03/08 00:23", "2022/03/08 00:34"},
        {"M 5",  "2022/03/08 01:43", "2022/03/08 01:53"},
        {"M 12", "2022/03/08 03:40", "2022/03/08 03:51"},
        {"M 29", "2022/03/08 03:56", "2022/03/08 04:06"},
        {"M 39", "2022/03/08 04:15", "2022/03/08 04:26"},
        {"M 10", "2022/03/08 04:27", "2022/03/08 04:37"},
        {"M 27", "2022/03/08 04:38", "2022/03/08 04:49"},
        {"M 14", "2022/03/08 04:50", "2022/03/08 05:00"},
        {"M 34", "2022/03/08 20:03", "2022/03/08 20:14"}
    };
    QVERIFY(checkSchedule(scheduleMinAlt30, scheduler->process()->getGreedyScheduler()->getSchedule(), 300));
    // TODO: verify this test data.

    // The timing was affected by calculating horizon constraints.
    // Measure the time with and without a realistic artificial horizon.
    ArtificialHorizon largeHorizon;
    addHorizonConstraint(
        &largeHorizon, "h", true, QVector<double>(
    {
        // vector of azimuths
        67.623611, 71.494167, 73.817778, 75.726667, 77.536944, 79.640000, 81.505278, 82.337778, 83.820000, 84.479444,
        86.375556, 89.347500, 91.982500, 93.771667, 95.124722, 95.747778, 97.303889, 100.735278, 104.573611, 106.721389,
        108.360278, 110.640833, 111.963611, 114.940556, 116.497500, 118.858611, 119.981389, 122.832500, 124.695278, 125.882778,
        127.580278, 129.888889, 130.668333, 132.550833, 133.389167, 133.892222, 138.481111, 139.192778, 140.057500, 141.234722,
        142.308333, 144.151944, 145.714167, 146.290833, 149.275278, 151.138056, 152.107500, 153.526389, 154.321667, 155.640000,
        156.685833, 156.302778, 157.421667, 160.331389, 161.091389, 160.952778, 161.975556, 162.564167, 164.866944, 166.906389,
        167.750000, 167.782778, 169.212500, 170.241944, 170.642500, 172.948056, 174.382778, 174.738333, 175.333056, 175.878889,
        177.345000, 178.390278, 177.411111, 180.062500, 177.540278, 177.981111, 179.459444, 180.363056, 182.301667, 184.176111,
        185.036944, 188.303611, 190.110833, 191.809444, 196.293889, 197.398889, 196.634722, 196.238889, 198.553056, 199.896389,
        205.868333, 207.224722, 231.645278, 258.324167, 277.260833, 292.470833, 302.961111, 308.996389, 309.027500, 312.311667,
        313.423333, 316.827500, 316.471111, 322.656944, 329.775278, 330.606944, 333.355278, 340.709167, 342.927222, 344.010000,
        345.696389, 347.886111, 349.058611, 351.998889, 353.010278, 357.548611, 359.510278, 359.320278, 363.102500, 369.171389,
        371.129444, 372.717778, 375.897500, 379.531944, 380.118333, 383.015278, 385.493333, 32.556944, 35.456667, 35.773889,
        38.304167, 43.844722, 52.575556, 55.080000, 57.086667, 67.523333, 68.458056}),
    QVector<double>(
    {
        // vector of altitudes
        22.721111, 21.776944, 20.672222, 25.656667,
        27.865000, 29.283889, 29.107778, 27.240556, 26.704722, 28.126111, 28.722222, 29.286111, 28.931944, 26.810000,
        24.275278, 21.858333, 20.081944, 20.608333, 21.693056, 22.915833, 26.003333, 29.119167, 28.771667, 24.334444,
        22.909444, 21.960278, 20.691389, 24.635000, 24.556667, 22.429167, 24.333056, 24.526667, 24.417222, 26.273611,
        25.870833, 24.805556, 27.208333, 29.074167, 31.087500, 30.648333, 28.023889, 27.469722, 27.212222, 26.296667,
        24.987222, 23.888333, 25.336667, 25.510556, 24.482222, 24.494444, 23.861667, 22.288889, 19.551667, 20.260278,
        21.554722, 22.983056, 24.001111, 27.154722, 28.523056, 27.060278, 24.611944, 22.693333, 23.140833, 22.666389,
        20.984722, 27.248611, 27.015000, 24.659444, 23.478611, 21.943056, 20.511389, 21.130000, 23.937778, 23.787778,
        29.003056, 35.778056, 37.504167, 41.471111, 43.260556, 42.360833, 39.985000, 31.131389, 32.261111, 31.051944,
        32.915000, 31.470000, 30.330556, 29.764722, 28.692500, 24.937222, 24.907222, 41.375556, 44.511389, 43.528611,
        38.659722, 33.497500, 27.334444, 24.551667, 21.920278, 22.558333, 27.221111, 27.113611, 30.526667, 31.531667,
        25.062778, 27.808889, 24.439167, 23.505278, 22.182222, 23.534444, 22.537778, 23.492222, 22.485000, 23.643611,
        25.677222, 23.192778, 19.320556, 16.915556, 18.304444, 18.866389, 18.523889, 23.704167, 24.008611, 25.070000,
        28.784444, 30.421389, 35.479444, 33.950833, 35.842778, 34.506667, 34.424722, 28.031944, 30.806389, 29.865833,
        22.579722, 22.644444, 22.672222}));

    qCInfo(KSTARS_EKOS_TEST) << QString("Calculate schedule with large artificial horizon.");
    Ekos::SchedulerJob::setHorizon(&largeHorizon);
    scheduler->load(true, QString("%1").arg(esl30Path));
    const QVector<SPlan> scheduleAHMinAlt30 =
    {
        {"M 35", "2022/03/07 22:28", "2022/03/07 22:39"},
        {"M 38", "2022/03/07 22:40", "2022/03/07 22:50"},
        {"M 36", "2022/03/07 22:51", "2022/03/07 23:02"},
        {"M 40", "2022/03/07 23:03", "2022/03/07 23:13"},
        {"M 3",  "2022/03/07 23:14", "2022/03/07 23:25"},
        {"M 13", "2022/03/08 00:24", "2022/03/08 00:34"},
        {"M 5",  "2022/03/08 01:43", "2022/03/08 01:54"},
        {"M 12", "2022/03/08 03:41", "2022/03/08 03:51"},
        {"M 29", "2022/03/08 03:56", "2022/03/08 04:07"},
        {"M 39", "2022/03/08 04:16", "2022/03/08 04:26"},
        {"M 10", "2022/03/08 04:27", "2022/03/08 04:38"},
        {"M 27", "2022/03/08 04:39", "2022/03/08 04:49"},
        {"M 14", "2022/03/08 04:50", "2022/03/08 05:01"},
        {"M 34", "2022/03/08 20:03", "2022/03/08 20:13"},
        {"M 1",  "2022/03/08 20:14", "2022/03/08 20:25"}
    };
    QVERIFY(checkSchedule(scheduleAHMinAlt30, scheduler->process()->getGreedyScheduler()->getSchedule(), 300));
    // TODO: verify this test data.

}

void TestEkosSchedulerOps::prepareTestData(QList<QString> locationList, QList<QString> targetList)
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
    Q_UNUSED(locationList)
#else
    QTest::addColumn<QString>("location"); /*!< location the KStars test is running */
    QTest::addColumn<QString>("target");   /*!< scheduled target */
    for (QString location : locationList)
        for (QString target : targetList)
            QTest::newRow(QString("loc= \"%1\", target=\"%2\"").arg(location).arg(target).toLocal8Bit())
                    << location << target;
#endif
}

/* *********************************************************************************
 *
 * Test data
 *
 * ********************************************************************************* */
void TestEkosSchedulerOps::testCulminationStartup_data()
{
    prepareTestData({"Heidelberg", "New York"}, {"Rasalhague"});
}

void TestEkosSchedulerOps::testRememberJobProgress_data()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Bypassing fixture test on old Qt");
#else
    QTest::addColumn<QString>("jobs"); /*!< Capture sequences */
    QTest::addColumn<QString>("frames"); /*!< Existing frames */
    QTest::addColumn<int>("iterations"); /*!< Existing frames */
    QTest::addColumn<bool>("scheduled"); /*!< Expected result: scheduled (true) or completed (false) */

    QTest::newRow("{Red:2}, scheduled=true") << "Red:2" << "Red:1" << 1 << true;
    QTest::newRow("{Red:2}, scheduled=false") << "Red:2" << "Red:2" << 1 << false;
    QTest::newRow("{Red:2, Red:1}, scheduled=true") << "Red:2, Red:1" << "Red:2" << 1 << true;
    QTest::newRow("{Red:2, Green:1, Red:1}, scheduled=true") << "Red:2, Green:1, Red:1" << "Red:4" << 1 << true;
    QTest::newRow("{Red:3, Green:1, Red:2}, 3x, scheduled=true") << "Red:3, Green:1, Red:2" << "Red:14, Green:3" << 3 << true;
    QTest::newRow("{Red:3, Green:1, Red:2}, 3x, scheduled=false") << "Red:3, Green:1, Red:2" << "Red:15, Green:3" << 3 << false;

#endif
}

QTEST_KSTARS_MAIN(TestEkosSchedulerOps)

#endif // HAVE_INDI
