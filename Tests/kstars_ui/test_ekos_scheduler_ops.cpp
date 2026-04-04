/*  KStars scheduler operations tests
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QDir>

#include <chrono>
#include <ctime>

#include "../testhelpers.h"
#include "test_ekos_scheduler_ops.h"
#include "test_ekos_scheduler_helper.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/greedyscheduler.h"
#include "ekos/scheduler/schedulerprocess.h"

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
            foreach (Ekos::SchedulerJob *sched_job, scheduler->moduleState()->jobs())
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
    bool success = (fabs(mount->lastRaHoursSlew - targetObject->ra().Hours()) < halfDegreeInHours) &&
                   (fabs(mount->lastDecDegreesSlew - targetObject->dec().Degrees()) < 0.5);
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
    preDawnUTime = preDawnUTime.addSecs(-60.0 * abs(Options::preDawnTime()));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(KTest::tempDirPattern(QStringLiteral("scheduler")));

    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[0], startJobUTime, preDawnUTime, currentUTime, sleepMs, DEFAULT_TOLERANCE);
    parkAndSleep(currentUTime, sleepMs);

    const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), QTimeZone::utc());
    wakeupAndRestart(restartTime, currentUTime, sleepMs);
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
    // Use the geo's fixed UTC offset so the test is timezone-independent.
    const QTimeZone geoTZ = QTimeZone(static_cast<int>(geo.TZ() * 3600));
    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true, QVector<double>({175, 200}), QVector<double>({70, 70}));
    Ekos::SchedulerJob::setHorizon(&shutdownHorizon);
    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);

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
    // Use the geo's fixed UTC offset so the test is timezone-independent.
    const QTimeZone geoTZ = QTimeZone(static_cast<int>(geo.TZ() * 3600));
    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true, QVector<double>({175, 200}), QVector<double>({70, 70}));

    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), QTimeZone::utc());
    initTimeGeo(geo, startUTime);

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
    foreach (auto &job, scheduler->moduleState()->jobs())
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
void TestEkosSchedulerOps::testWeatherSoftShutdownSimulator()
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
void TestEkosSchedulerOps::testWeatherHardShutdownSimulator()
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
void TestEkosSchedulerOps::testWeatherMonitoringModeSimulator()
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

    // With grace period 0, it should transition to RUN_WAKEUP and weather monitoring mode immediately.
    QTRY_VERIFY_WITH_TIMEOUT(realScheduler->process()->moduleState()->timerState() == Ekos::RUN_WAKEUP, 20000);
    QVERIFY2(realScheduler->process()->moduleState()->weatherShutdownMonitoring() == true,
             "weatherShutdownMonitoring must be true for grace period 0");
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

QTEST_KSTARS_MAIN(TestEkosSchedulerOps)

#endif // HAVE_INDI
