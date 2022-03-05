/*  KStars scheduler operations tests
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>

#include "test_ekos_scheduler_ops.h"
#include "test_ekos_scheduler_helper.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/greedyscheduler.h"

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

using Ekos::Scheduler;

TestEkosSchedulerOps::TestEkosSchedulerOps(QObject *parent) : QObject(parent)
{
}

void TestEkosSchedulerOps::initTestCase()
{

    QDBusConnection::sessionBus().registerObject("/MockKStars", this);
    QDBusConnection::sessionBus().registerService("org.kde.mockkstars");

    // This gets executed at the start of testing

    disableSkyMap();
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
    scheduler->setFocusInterfaceString("org.kde.mockkstars.MockEkos.MockFocus");
    scheduler->setMountInterfaceString("org.kde.mockkstars.MockEkos.MockMount");
    scheduler->setCaptureInterfaceString("org.kde.mockkstars.MockEkos.MockCapture");
    scheduler->setAlignInterfaceString("org.kde.mockkstars.MockEkos.MockAlign");
    scheduler->setGuideInterfaceString("org.kde.mockkstars.MockEkos.MockGuide");
    scheduler->setFocusPathString(Ekos::MockFocus::mockPath);
    scheduler->setMountPathString(Ekos::MockMount::mockPath);
    scheduler->setCapturePathString(Ekos::MockCapture::mockPath);
    scheduler->setAlignPathString(Ekos::MockAlign::mockPath);
    scheduler->setGuidePathString(Ekos::MockGuide::mockPath);

    // Let's not deal with the dome for now.
    scheduler->unparkDomeCheck->setChecked(false);

    // For now don't deal with files that were left around from previous testing.
    // Should put these is a temporary directory that will be removed, if we generate
    // them at all.
    Options::setRememberJobProgress(false);

    // This allows testing of the shutdown.
    Options::setStopEkosAfterShutdown(true);

    Options::setSchedulerAlgorithm(Scheduler::ALGORITHM_CLASSIC);

    Options::setSortSchedulerJobs(false);

    // define START_ASAP and FINISH_SEQUENCE as default startup/completion conditions.
    m_startupCondition.type = SchedulerJob::START_ASAP;
    m_completionCondition.type = SchedulerJob::FINISH_SEQUENCE;
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
    Options::setShowSolarSystem(false);
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
    const int tolerance = std::max(seconds, 3 * (scheduler->m_UpdatePeriodMs / 1000));
    return tolerance;
}

// Thos tests an empty scheduler job and makes sure dbus communications
// work between the scheduler and the mock modules.
void TestEkosSchedulerOps::testBasics()
{
    QVERIFY(scheduler->focusInterface.isNull());
    QVERIFY(scheduler->mountInterface.isNull());
    QVERIFY(scheduler->captureInterface.isNull());
    QVERIFY(scheduler->alignInterface.isNull());
    QVERIFY(scheduler->guideInterface.isNull());

    ekos->addModule(QString("Focus"));
    ekos->addModule(QString("Mount"));
    ekos->addModule(QString("Capture"));
    ekos->addModule(QString("Align"));
    ekos->addModule(QString("Guide"));

    // Allow Qt to pass around the messages.
    // Wait time is set short (10ms) for longer tests where the scheduler is
    // iterating and can miss on one iteration. Here we make it longer
    // for a more stable test.

    // Not sure why processEvents() doesn't always work. Would be quicker that way.
    //qApp->processEvents();
    QTest::qWait(10 * QWAIT_TIME);

    QVERIFY(!scheduler->focusInterface.isNull());
    QVERIFY(!scheduler->mountInterface.isNull());
    QVERIFY(!scheduler->captureInterface.isNull());
    QVERIFY(!scheduler->alignInterface.isNull());
    QVERIFY(!scheduler->guideInterface.isNull());

    // Verify the mocks can use the DBUS.
    QVERIFY(!focuser->isReset);
    scheduler->focusInterface->call(QDBus::AutoDetect, "resetFrame");

    //qApp->processEvents();  // this fails, is it because dbus calls are on a separate thread?
    QTest::qWait(10 * QWAIT_TIME);

    QVERIFY(focuser->isReset);

    // Run the scheduler with nothing setup. Should quickly exit.
    scheduler->init();
    QVERIFY(scheduler->timerState == Scheduler::RUN_WAKEUP);
    int sleepMs = scheduler->runSchedulerIteration();
    QVERIFY(scheduler->timerState == Scheduler::RUN_SCHEDULER);
    sleepMs = scheduler->runSchedulerIteration();
    QVERIFY(sleepMs == 1000);
    QVERIFY(scheduler->timerState == Scheduler::RUN_SHUTDOWN);
    sleepMs = scheduler->runSchedulerIteration();
    QVERIFY(scheduler->timerState == Scheduler::RUN_NOTHING);
}

// Runs the scheduler for a number of iterations between 1 and the arg "iterations".
// Each iteration it  increments the simulated clock (currentUTime, which is in Universal
// Time) by *sleepMs, then runs the scheduler, then calls fcn().
// If fcn() returns true, it stops iterating and returns true.
// It returns false if it completes all the with fnc() returning false.
bool TestEkosSchedulerOps::iterateScheduler(const QString &label, int iterations,
        int *sleepMs, KStarsDateTime* currentUTime, std::function<bool ()> fcn)
{
    fprintf(stderr, "\n----------------------------------------\n");
    fprintf(stderr, "Starting iterateScheduler(%s)\n",  label.toLatin1().data());

    for (int i = 0; i < iterations; ++i)
    {
        //qApp->processEvents();
        QTest::qWait(QWAIT_TIME); // this takes ~10ms per iteration!
        // Is there a way to speed up the above?
        // I didn't reduce it, because the basic test fails to call a dbus function
        // with less than 10ms wait time.

        *currentUTime = currentUTime->addSecs(*sleepMs / 1000.0);
        KStarsData::Instance()->changeDateTime(*currentUTime); // <-- 175ms
        *sleepMs = scheduler->runSchedulerIteration();
        fprintf(stderr, "current time LT %s UT %s\n",
                KStarsData::Instance()->lt().toString().toLatin1().data(),
                KStarsData::Instance()->ut().toString().toLatin1().data());
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
    const QString eslFilename = dir.filePath(QString("%1.esl").arg(label));
    const QString esqFilename = dir.filePath(QString("%1.esq").arg(label));
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
        scheduler->load(i == 0, QString("file://%1").arg(eslFile));
        QVERIFY(scheduler->jobs.size() == (i + 1));
        scheduler->jobs[i]->setSequenceFile(QUrl(QString("file://%1").arg(esqFile)));
        fprintf(stderr, "seq file: %s \"%s\"\n", esqFile.toLatin1().data(), QString("file://%1").arg(esqFile).toLatin1().data());
    }
}

// Sets up the scheduler in a particular location (geo) and a UTC start time.
void TestEkosSchedulerOps::initScheduler(const GeoLocation &geo, const QDateTime &startUTime, QTemporaryDir *dir,
        const QVector<QString> &esls, const QVector<QString> &esqs)
{
    initTimeGeo(geo, startUTime);
    initFiles(dir, esls, esqs);
    scheduler->evaluateJobs(false);
    scheduler->init();
    QVERIFY(scheduler->timerState == Scheduler::RUN_WAKEUP);
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
    KStarsDateTime currentUTime(startUTime);

    int sleepMs = 0;

    QVERIFY(scheduler->timerState == Scheduler::RUN_WAKEUP);
    QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
    }));

    if (wakeupTime.isValid())
    {
        // This is the sequence when it goes to sleep, then wakes up later to start.

        QVERIFY(iterateScheduler("Wait for RUN_WAKEUP", 10, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->timerState == Scheduler::RUN_WAKEUP);
        }));

        // Verify that it's near the original start time.
        const qint64 delta_t = KStarsData::Instance()->ut().secsTo(startUTime);
        QVERIFY2(std::abs(delta_t) < timeTolerance(60),
                 QString("Delta to original time %1 too large, failing.").arg(delta_t).toLatin1());

        QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
        }));

        // Verify that it wakes up at the right time, after the twilight constraint
        // and the stars rises above 30 degrees. See the time comment above.
        QVERIFY(std::abs(KStarsData::Instance()->ut().secsTo(wakeupTime)) < timeTolerance(60));
    }
    else
    {
        // check if there is a job scheduled
        bool scheduled_job = false;
        foreach (SchedulerJob *sched_job, scheduler->jobs)
            if (sched_job->state == SchedulerJob::JOB_SCHEDULED)
                scheduled_job = true;
        if (scheduled_job)
        {
            // This is the sequence when it can start-up right away.

            // Verify that it's near the original start time.
            const qint64 delta_t = KStarsData::Instance()->ut().secsTo(startUTime);
            QVERIFY2(std::abs(delta_t) < timeTolerance(60),
                     QString("Delta to original time %1 too large, failing.").arg(delta_t).toLatin1());

            QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
            {
                return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
            }));
        }
        else
            // if there is no job scheduled, we're done
            return;
    }
    // When the scheduler starts up, it sends connectDevices to Ekos
    // which sets Indi --> Ekos::Success,
    // and then it sends start() to Ekos which sets Ekos --> Ekos::Success
    bool sentOnce = false, readyOnce = false;
    QVERIFY(iterateScheduler("Wait for Indi and Ekos", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if ((scheduler->indiState == Scheduler::INDI_READY) &&
                (scheduler->ekosState == Scheduler::EKOS_READY))
            return true;
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
            else if (scheduler->mountInterface != nullptr &&
                     scheduler->captureInterface != nullptr && !readyOnce)
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

void TestEkosSchedulerOps::startModules(KStarsDateTime &currentUTime, int &sleepMs)
{
    QVERIFY(iterateScheduler("Wait for MountTracking", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Telescope::MOUNT_SLEWING)
            mount->setStatus(ISD::Telescope::MOUNT_TRACKING);
        else if (mount->status() == ISD::Telescope::MOUNT_TRACKING)
            return true;
        return false;
    }));

    QVERIFY(iterateScheduler("Wait for Focus", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (focuser->status() == Ekos::FOCUS_PROGRESS)
            focuser->setStatus(Ekos::FOCUS_COMPLETE);
        else if (focuser->status() == Ekos::FOCUS_COMPLETE)
            return true;
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
    for (int i = 0; i < scheduler->jobs.size(); ++i)
    {
        fprintf(stderr, "(%d) %s %-15s ", i, scheduler->jobs[i]->getName().toLatin1().data(),
                SchedulerJob::jobStatusString(scheduler->jobs[i]->getState()).toLatin1().data());
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
        return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
    }));

    // wait until the scheduler turns to the wakeup mode sleeping until the startup condition is met
    QVERIFY(iterateScheduler("Wait for RUN_WAKEUP", 10, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_WAKEUP);
    }));

    // wait until the scheduler starts the job
    QVERIFY(iterateScheduler("Wait for RUN_SCHEDULER", 2, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
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

    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");

    startupJob(geo, startUTime, &dir, TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition,
               m_completionCondition, {true, true, true, true},
               false, enforceArtificialHorizon),
               TestEkosSchedulerHelper::getDefaultEsqContent(), wakeupTime, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);
    QVERIFY(checkLastSlew(targetObject));

    QVERIFY(iterateScheduler("Wait for Capturing", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->currentJob != nullptr &&
                scheduler->currentJob->getStage() == SchedulerJob::STAGE_CAPTURING);
    }));

    // Tell the scheduler that capture is done.
    capture->setStatus(Ekos::CAPTURE_COMPLETE);

    QVERIFY(iterateScheduler("Wait for Abort Guider", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (guider->status() == Ekos::GUIDE_ABORTED);
    }));
    QVERIFY(iterateScheduler("Wait for Shutdown", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->shutdownState == Scheduler::SHUTDOWN_COMPLETE);
    }));

    // Here the scheduler sends a message to ekosInterface to disconnectDevices,
    // which will cause indi --> IDLE,
    // and then calls stop() which will cause ekos --> IDLE
    // This will cause the scheduler to shutdown.
    QVERIFY(iterateScheduler("Wait for Scheduler Complete", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_NOTHING);
    }));
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
    QDateTime startUTime(QDateTime(QDate(2021, 6, 13), QTime(22, 0, 0), Qt::UTC));
    const QDateTime wakeupTime(QDate(2021, 6, 14), QTime(06, 35, 0), Qt::UTC);
    runSimpleJob(geo, targetObject, startUTime, wakeupTime, true);
}

// This test has the same start as testSimpleJob, except that it but runs in NYC
// instead of silicon valley. This makes sure testing doesn't depend on timezone.
void TestEkosSchedulerOps::testTimeZone()
{
    GeoLocation geo(dms(-74, 0), dms(40, 42, 0), "NYC", "NY", "USA", -5);
    KStarsDateTime startUTime(QDateTime(QDate(2021, 6, 13), QTime(22, 0, 0), Qt::UTC));

    scheduler->setUpdateInterval(5000);
    KStarsDateTime currentUTime;
    int sleepMs = 0;

    // It crosses 30-degrees altitude around the same time locally, but that's
    // 3 hours earlier UTC.
    const QDateTime wakeupTime(QDate(2021, 6, 14), QTime(03, 26, 0), Qt::UTC);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Altair");
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    startupJob(geo, startUTime, &dir, TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition,
               m_completionCondition, {true, true, true, true},
               false, true),
               TestEkosSchedulerHelper::getDefaultEsqContent(), wakeupTime, currentUTime, sleepMs);
    startModules(currentUTime, sleepMs);
    QVERIFY(checkLastSlew(targetObject));
}

void TestEkosSchedulerOps::testDawnShutdown()
{
    // This test will iterate the scheduler every 40 simulated seconds (to save testing time).
    scheduler->setUpdateInterval(40000);

    // At this geo/date, Dawn is calculated = .1625 of a day = 3:53am local = 10:52 UTC
    // If we started at 23:35 local time, as before, it's a little over 4 hours
    // or over 4*3600 iterations. Too many? Instead we start at 3am local.

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject*> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));

    // We'll start the scheduler at 3am local time.
    QDateTime startUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 0, 0), Qt::UTC));
    // The job should start at 3:12am local time.
    QDateTime startJobUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 12, 0), Qt::UTC));
    // The job should be interrupted at the pre-dawn time, which is about 3:53am
    QDateTime preDawnUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 53, 0), Qt::UTC));
    // Consider pre-dawn security range
    preDawnUTime = preDawnUTime.addSecs(-60.0 * abs(Options::preDawnTime()));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");

    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[0], startJobUTime, preDawnUTime, currentUTime, sleepMs, 120);
    parkAndSleep(currentUTime, sleepMs);

    const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), Qt::UTC);
    wakeupAndRestart(restartTime, currentUTime, sleepMs);
}

// Expect the job to start running at startJobUTime.
// Check that the correct slew was made
// Expect the job to be interrupted at interruptUTime (if the time is valid)
void TestEkosSchedulerOps::slewAndRun(SkyObject *object, const QDateTime &startUTime, const QDateTime &interruptUTime,
                                      KStarsDateTime &currentUTime, int &sleepMs, int tolerance, const QString &label)
{
    QVERIFY(iterateScheduler("Wait for Job Startup", 10, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_JOBCHECK);
    }));

    double delta = KStarsData::Instance()->ut().secsTo(startUTime);
    QVERIFY2(std::abs(delta) < timeTolerance(tolerance),
             QString("Unexpected difference to job statup time: %1 secs (%2 vs %3) %4")
             .arg(delta).arg(KStarsData::Instance()->ut().toString("MM/dd hh:mm"))
             .arg(startUTime.toString("MM/dd hh:mm")).arg(label).toLocal8Bit());

    // We should be unparked at this point.
    QVERIFY(mount->parkStatus() == ISD::PARK_UNPARKED);

    QVERIFY(iterateScheduler("Wait for MountTracking", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (mount->status() == ISD::Telescope::MOUNT_SLEWING)
            mount->setStatus(ISD::Telescope::MOUNT_TRACKING);
        else if (mount->status() == ISD::Telescope::MOUNT_TRACKING)
            return true;
        return false;
    }));

    QVERIFY(checkLastSlew(object));

    if (interruptUTime.isValid())
    {
        // Wait until the job stops processing,
        // hen scheduler state JOBCHECK changes to RUN_SCHEDULER.
        QVERIFY(iterateScheduler("Wait for Job Interruption", 700, &sleepMs, &currentUTime, [&]() -> bool
        {
            return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
        }));

        delta = KStarsData::Instance()->ut().secsTo(interruptUTime);
        QVERIFY2(std::abs(delta) < timeTolerance(60),
                 QString("Unexpected difference to interrupt time: %1 secs (%2 vs %3) %4")
                 .arg(delta).arg(KStarsData::Instance()->ut().toString("MM/dd hh:mm"))
                 .arg(interruptUTime.toString("MM/dd hh:mm")).arg(label).toLocal8Bit());

        // It should start to shutdown now.
        QVERIFY(iterateScheduler("Wait for Guide Abort", 30, &sleepMs, &currentUTime, [&]() -> bool
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
    QVERIFY(iterateScheduler("Wait for Parked", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (mount->parkStatus() == ISD::PARK_PARKED);
    }));

    QVERIFY(iterateScheduler("Wait for Sleep State", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_WAKEUP);
    }));
}

void TestEkosSchedulerOps::wakeupAndRestart(const QDateTime &restartTime, KStarsDateTime &currentUTime, int &sleepMs)
{
    // Make sure it wakes up at the proper time.
    QVERIFY(iterateScheduler("Wait for Wakeup Tomorrow", 30, &sleepMs, &currentUTime, [&]() -> bool
    {
        return (scheduler->timerState == Scheduler::RUN_SCHEDULER);
    }));

    QVERIFY(std::abs(KStarsData::Instance()->ut().secsTo(restartTime)) < timeTolerance(60));

    // Verify the job starts up again, and the mount is once-again unparked.
    bool readyOnce = false;
    QVERIFY(iterateScheduler("Wait for Job Startup & Unparked", 50, &sleepMs, &currentUTime, [&]() -> bool
    {
        if (scheduler->mountInterface != nullptr &&
                scheduler->captureInterface != nullptr && !readyOnce)
        {
            // Send a ready signal since the scheduler expects it.
            readyOnce = true;
            mount->sendReady();
            capture->sendReady();
        }
        return (scheduler->timerState == Scheduler::RUN_JOBCHECK &&
                mount->parkStatus() == ISD::PARK_UNPARKED);
    }));
}

void TestEkosSchedulerOps::testCulminationStartup()
{
    // culmination offset
    const int offset = -60;

    // obtain location and target from test data
    QFETCH(QString, location);
    QFETCH(QString, target);
    GeoLocation * const geo = KStars::Instance()->data()->locationNamed(location);
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName(target);

    // move forward in 20s steps
    scheduler->setUpdateInterval(20000);

    // determine the transit time (UTC) for a fixed date
    KStarsDateTime midnight(QDate(2021, 7, 11), QTime(0, 0, 0), Qt::UTC);
    QTime transitTimeUT = targetObject->transitTimeUT(midnight, geo);
    KStarsDateTime transitUT(midnight.date(), transitTimeUT, Qt::UTC);

    // select start time three hours before transit
    KStarsDateTime startUTime(midnight.date(), transitTimeUT.addSecs(-3600 * 3), Qt::UTC);

    // define culmination offset of 1h as startup condition
    m_startupCondition.type = SchedulerJob::START_CULMINATION;
    m_startupCondition.culminationOffset = -60;

    // check whether the job startup is offset minutes before the calculated transit
    KStarsDateTime const jobStartUTime = transitUT.addSecs(60 * offset);
    // initialize the the scheduler
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    QVector<QString> esqVector;
    esqVector.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    QVector<QString> eslVector;
    eslVector.push_back(TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition, m_completionCondition, {true, true, true, true},
                        false, true));
    initScheduler(*geo, startUTime, &dir, eslVector, esqVector);
    // verify if the job starts at the expected time
    initJob(startUTime, jobStartUTime);
}

void TestEkosSchedulerOps::testFixedDateStartup()
{
    GeoLocation * const geo = KStars::Instance()->data()->locationNamed("Heidelberg");
    SkyObject *targetObject = KStars::Instance()->data()->skyComposite()->findByName("Rasalhague");

    KStarsDateTime jobStartUTime(QDate(2021, 7, 12), QTime(1, 0, 0), Qt::UTC);
    KStarsDateTime jobStartLocalTime(QDate(2021, 7, 12), QTime(3, 0, 0), Qt::UTC);
    // scheduler starts one hour earlier than lead time
    KStarsDateTime startUTime = jobStartUTime.addSecs(-1 * 3600 - Options::leadTime() * 60);

    // define culmination offset of 1h as startup condition
    m_startupCondition.type = SchedulerJob::START_AT;
    m_startupCondition.atLocalDateTime = jobStartLocalTime;

    // initialize the the scheduler
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    QVector<QString> esqVector;
    esqVector.push_back(TestEkosSchedulerHelper::getDefaultEsqContent());
    QVector<QString> eslVector;
    eslVector.push_back(TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition, m_completionCondition, {true, true, true, true},
                        false, true));
    initScheduler(*geo, startUTime, &dir, eslVector, esqVector);
    // verify if the job starts at the expected time
    initJob(startUTime, jobStartUTime);
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
    scheduler->setUpdateInterval(20000);
    // define culmination offset of 1h as startup condition
    m_startupCondition.type = SchedulerJob::START_ASAP;
    // initialize the the scheduler
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
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
                          const QVector<double> &azimuths, const QVector<double> &altitudes)
{
    std::shared_ptr<LineList> pointList(new LineList);
    for (int i = 0; i < azimuths.size(); ++i)
    {
        std::shared_ptr<SkyPoint> skyp1(new SkyPoint);
        skyp1->setAlt(altitudes[i]);
        skyp1->setAz(azimuths[i]);
        pointList->append(skyp1);
    }
    horizon->addRegion(name, enabled, pointList, false);
}

void TestEkosSchedulerOps::testArtificialHorizonConstraints()
{
    // In testSimpleJob, above, the wakeup time for the job was 11:35pm local time, and it used a 30-degrees min altitude.
    // Now let's add an artificial horizon constraint for 40-degrees at the azimuths where the object will be.
    // It should now wakeup and start processing at about 00:27am

    ArtificialHorizon horizon;
    addHorizonConstraint(&horizon, "r1", true, QVector<double>({100, 120}), QVector<double>({40, 40}));
    SchedulerJob::setHorizon(&horizon);

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject*> targetObjects;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    QDateTime startUTime(QDateTime(QDate(2021, 6, 13), QTime(22, 0, 0), Qt::UTC));

    const QDateTime wakeupTime(QDate(2021, 6, 14), QTime(07, 27, 0), Qt::UTC);
    runSimpleJob(geo, targetObjects[0], startUTime, wakeupTime, true);

    // Uncheck enforce artificial horizon and the wakeup time should go back to it's original time,
    // even though the artificial horizon is still there and enabled.
    init(); // Reset the scheduler.
    const QDateTime originalWakeupTime(QDate(2021, 6, 14), QTime(06, 35, 0), Qt::UTC);
    runSimpleJob(geo, targetObjects[0], startUTime, originalWakeupTime, /* enforce artificial horizon */false);

    // Re-check enforce artificial horizon, but remove the constraint, and the wakeup time also goes back to it's original time.
    init(); // Reset the scheduler.
    ArtificialHorizon emptyHorizon;
    SchedulerJob::setHorizon(&emptyHorizon);
    runSimpleJob(geo, targetObjects[0], startUTime, originalWakeupTime, /* enforce artificial horizon */ true);

    // Testing that the artificial horizon constraint will end a job
    // when the altitude of the running job is below the artificial horizon at the
    // target's azimuth.
    //
    // This repeats testDawnShutdown() above, except that an artifical horizon
    // constraint is added so that the job doesn't reach dawn but rather is interrupted
    // at 3:19 local time. That's the time the azimuth reaches 175.

    init(); // Reset the scheduler.
    scheduler->setUpdateInterval(40000);
    ArtificialHorizon shutdownHorizon;
    // Note, just putting a constraint at 175->180 will fail this test because Altair will
    // cross past 180 and the scheduler will want to restart it before dawn.
    addHorizonConstraint(&shutdownHorizon, "h", true,
                         QVector<double>({175, 200}), QVector<double>({70, 70}));
    SchedulerJob::setHorizon(&shutdownHorizon);

    // We'll start the scheduler at 3am local time.
    startUTime = QDateTime(QDate(2021, 6, 14), QTime(10, 0, 0), Qt::UTC);
    // The job should start at 3:12am local time.
    QDateTime startJobUTime(QDate(2021, 6, 14), QTime(10, 12, 0), Qt::UTC);
    // The job should be interrupted by the horizon limit, which is reached about 3:19am local.
    QDateTime horizonStopUTime(QDateTime(QDate(2021, 6, 14), QTime(10, 19, 0), Qt::UTC));

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");

    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[0], startJobUTime, horizonStopUTime, currentUTime, sleepMs, 120, "Horizon slewAndRun");
    parkAndSleep(currentUTime, sleepMs);

    const QDateTime restartTime(QDate(2021, 6, 15), QTime(06, 31, 0), Qt::UTC);
    wakeupAndRestart(restartTime, currentUTime, sleepMs);
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

    Options::setSchedulerAlgorithm(Scheduler::ALGORITHM_GREEDY);
    // This test will iterate the scheduler every 40 simulated seconds (to save testing time).
    scheduler->setUpdateInterval(40000);

    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    QVector<SkyObject*> targetObjects;
    constexpr int altairIndex = 0, denebIndex = 1;
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Altair"));
    targetObjects.push_back(KStars::Instance()->data()->skyComposite()->findByName("Deneb"));

    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true,
                         QVector<double>({175, 200}), QVector<double>({70, 70}));
    SchedulerJob::setHorizon(&shutdownHorizon);

    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), Qt::UTC);

    // Added some delay to the 1st start time--it takes a while for this simulator to get started.
    QDateTime d1Start (QDateTime(QDate(2021, 6, 14), QTime( 5, 55, 0), Qt::UTC)); // 10:48pm
    QDateTime a1Start (QDateTime(QDate(2021, 6, 14), QTime( 6, 34, 0), Qt::UTC)); // 11:34pm
    QDateTime d2Start (QDateTime(QDate(2021, 6, 14), QTime(10, 20, 0), Qt::UTC)); //  3:20am
    QDateTime d2End   (QDateTime(QDate(2021, 6, 14), QTime(10, 53, 0), Qt::UTC)); //  3:53am
    QDateTime d3Start (QDateTime(QDate(2021, 6, 15), QTime( 5, 44, 0), Qt::UTC)); // 10:43pm the next day

    KStarsDateTime currentUTime;
    int sleepMs = 0;
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");

    startup(geo, targetObjects, startUTime, currentUTime, sleepMs, dir);
    slewAndRun(targetObjects[denebIndex], d1Start, a1Start, currentUTime, sleepMs, 600, "Greedy job #1");
    slewAndRun(targetObjects[altairIndex], a1Start, d2Start, currentUTime, sleepMs, 600, "Greedy job #2");
    slewAndRun(targetObjects[denebIndex], d2Start, d2End, currentUTime, sleepMs, 600, "Greedy job #3");

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
    QDateTime startUTime(QDateTime(QDate(2021, 10, 30), QTime(18, 0, 0), Qt::UTC));
    const QDateTime wakeupTime;
    KStarsDateTime currentUTime;
    int sleepMs = 0;

    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
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
                frame.setFileName(QString(img_dir.absolutePath() + "/Kocab/Light/" + filter + "/Kocab_Light_%1.fits").arg(i));
                frame.open(QIODevice::WriteOnly | QIODevice::Text);
                frame.close();
            }
        }
    }

    // start up the scheduler job
    QFETCH(int, iterations);

    TestEkosSchedulerHelper::CompletionCondition completionCondition;
    completionCondition.type = SchedulerJob::FINISH_REPEAT;
    completionCondition.repeat = iterations;
    startupJob(geo, startUTime, &dir,
               TestEkosSchedulerHelper::getSchedulerFile(targetObject, m_startupCondition, completionCondition, {true, true, true, true},
                       false, false, sleepMs, nullptr, {false, false, false, false}),
               TestEkosSchedulerHelper::getEsqContent(capture_jobs), wakeupTime, currentUTime, sleepMs);

    // fetch the expected result from the test data
    QFETCH(bool, scheduled);

    // verify if the job is scheduled as expected
    QVERIFY(scheduler->jobs[0]->getState() == (scheduled ? SchedulerJob::JOB_SCHEDULED : SchedulerJob::JOB_COMPLETE));
}

void TestEkosSchedulerOps::loadGreedySchedule(
    bool first, const QString &targetName,
    const TestEkosSchedulerHelper::StartupCondition &startupCondition,
    const TestEkosSchedulerHelper::CompletionCondition &completionCondition,
    QTemporaryDir &dir, const QVector<TestEkosSchedulerHelper::CaptureJob> &captureJob, int minAltitude)
{
    SkyObject *object = KStars::Instance()->data()->skyComposite()->findByName(targetName);
    QVERIFY(object != nullptr);
    const QString schedulerXML =
        TestEkosSchedulerHelper::getSchedulerFile(
            object, startupCondition, completionCondition, {true, true, true, true}, true, true, minAltitude);

    // Write the scheduler and sequence files.
    QString f1 = writeFiles(targetName, dir, captureJob, schedulerXML);
    scheduler->load(first, QString("file://%1").arg(f1));
}

struct SPlan
{
    QString name;
    QString start;
    QString stop;
};

bool checkSchedule(const QVector<SPlan> &ref, const QList<Ekos::GreedyScheduler::JobSchedule> &schedule, int tolerance)
{
    if (schedule.size() != ref.size()) return false;
    for (int i = 0; i < ref.size(); ++i)
    {
        QDateTime startTime = QDateTime::fromString(ref[i].start, "yyyy/MM/dd hh:mm");
        QDateTime stopTime = QDateTime::fromString(ref[i].stop, "yyyy/MM/dd hh:mm");
        if (!startTime.isValid()) return false;
        if (!stopTime.isValid()) return false;
        if (!schedule[i].startTime.isValid()) return false;
        if (!schedule[i].stopTime.isValid()) return false;

        if ((ref[i].name != schedule[i].job->getName()) ||
                (std::abs(schedule[i].startTime.secsTo(startTime)) > tolerance) ||
                (std::abs(schedule[i].stopTime.secsTo(stopTime)) > tolerance))
            return false;
    }
    return true;
}

void TestEkosSchedulerOps::testGreedy()
{
    Options::setSchedulerAlgorithm(Scheduler::ALGORITHM_GREEDY);

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);
    ArtificialHorizon shutdownHorizon;
    addHorizonConstraint(&shutdownHorizon, "h", true, QVector<double>({175, 200}), QVector<double>({70, 70}));
    SchedulerJob::setHorizon(&shutdownHorizon);
    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2021, 6, 14), QTime(4, 0, 0), Qt::UTC);
    initTimeGeo(geo, startUTime);

    auto schedJob200x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {200, 60, "Red", "."});
    auto schedJob400x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {400, 60, "Red", "."});

    TestEkosSchedulerHelper::StartupCondition asapStartupCondition, atStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition finishCompletionCondition, loopCompletionCondition;
    TestEkosSchedulerHelper::CompletionCondition repeat2CompletionCondition, atCompletionCondition;
    asapStartupCondition.type = SchedulerJob::START_ASAP;
    atStartupCondition.type = SchedulerJob::START_AT;
    atStartupCondition.atLocalDateTime = QDateTime(QDate(2021, 6, 14), QTime(1, 0, 0), Qt::LocalTime);
    finishCompletionCondition.type = SchedulerJob::FINISH_SEQUENCE;
    loopCompletionCondition.type = SchedulerJob::FINISH_LOOP;
    repeat2CompletionCondition.type = SchedulerJob::FINISH_REPEAT;
    repeat2CompletionCondition.repeat = 2;
    atCompletionCondition.type = SchedulerJob::FINISH_AT;
    atCompletionCondition.atLocalDateTime = QDateTime(QDate(2021, 6, 14), QTime(3, 30, 0), Qt::LocalTime);

    // Write the scheduler and sequence files.
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");

    // Altair is scheduled with first priority, but doesn't clear constraints immediately.
    // Deneb is also scheduled, but with twice as many captures.  Both start ASAP and run to comletion.
    // Deneb runs first, gets interrupted by Altair which runs to completion.
    // Then Deneb runs for the rest of the night, and also again the next evening before it comletes.
    loadGreedySchedule(true, "Altair", asapStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, finishCompletionCondition, dir, schedJob400x60, 30);
    scheduler->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/13 23:35"},
        {"Altair", "2021/06/13 23:35", "2021/06/14 02:59"},
        {"Deneb",  "2021/06/14 02:59", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:48"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));

    // As above, except Altair has completion condition repeat 2. It should run longer.
    // This makes a mess of things, as Altair can't complete during the first night, running into an artificial horizon constraint.
    // It also can't start the 2nd night as early as Deneb, so the 2nd night is Deneb, Altair (completing), Deneb, and Deneb finishes the 3rd night.
    loadGreedySchedule(true, "Altair", asapStartupCondition, repeat2CompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, finishCompletionCondition, dir, schedJob400x60, 30);
    scheduler->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/13 23:35"},
        {"Altair", "2021/06/13 23:35", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/14 23:30"},
        {"Altair", "2021/06/14 23:31", "2021/06/15 02:29"},
        {"Deneb",  "2021/06/15 02:30", "2021/06/15 03:53"},
        {"Deneb",  "2021/06/15 22:41", "2021/06/16 01:59"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));

    // Now we're using START_AT 6/14 1am for Altair (but not repeating twice).
    // Deneb will run until then (1am) the 1st night. Altair will run until it hits the horizon constraint.
    // Deneb runs through the end of the night, and again the next night until it completes.
    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, finishCompletionCondition, dir, schedJob400x60, 30);
    scheduler->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/14 01:00"},
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 02:44"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));

    // We again use START_AT 6/14 1am for Altair, but force Deneb to complete by 3:30am on 6/14.
    // So we get the same first two lines as above, but now Deneb stops on the 3rd line at 3:30.
    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, atCompletionCondition, dir, schedJob400x60, 30);
    scheduler->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/14 01:00"},
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:30"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));

    // Finally, we have the same Altair constraints, but this time allow Deneb to run forever.
    // It will look like the 3rd test, except Deneb keeps running through the end of the simulated time (2 days).
    loadGreedySchedule(true, "Altair", atStartupCondition, finishCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "Deneb", asapStartupCondition, loopCompletionCondition, dir, schedJob400x60, 30);
    scheduler->evaluateJobs(false);
    QVERIFY(checkSchedule(
    {
        {"Deneb",  "2021/06/13 22:48", "2021/06/14 01:00"},
        {"Altair", "2021/06/14 01:00", "2021/06/14 03:21"},
        {"Deneb",  "2021/06/14 03:22", "2021/06/14 03:53"},
        {"Deneb",  "2021/06/14 22:44", "2021/06/15 03:52"},
        {"Deneb",  "2021/06/15 22:39", "2021/06/16 03:53"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));
}

void TestEkosSchedulerOps::testGreedyAborts()
{
    Options::setSchedulerAlgorithm(Scheduler::ALGORITHM_GREEDY);
    SchedulerJob::setHorizon(nullptr);

    // Setup geo and an artificial horizon.
    GeoLocation geo(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -8);

    // Start the scheduler about 9pm local
    const QDateTime startUTime = QDateTime(QDate(2022, 2, 28), QTime(10, 29, 26), Qt::UTC);
    initTimeGeo(geo, startUTime);

    auto schedJob200x60 = QVector<TestEkosSchedulerHelper::CaptureJob>(1, {200, 60, "Red", "."});
    TestEkosSchedulerHelper::StartupCondition asapStartupCondition;
    TestEkosSchedulerHelper::CompletionCondition loopCompletionCondition;
    asapStartupCondition.type = SchedulerJob::START_ASAP;
    loopCompletionCondition.type = SchedulerJob::FINISH_LOOP;
    QTemporaryDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");


    scheduler->getGreedyScheduler()->setRescheduleAbortsImmediate(false);
    scheduler->getGreedyScheduler()->setRescheduleAbortsQueue(true);
    scheduler->getGreedyScheduler()->setRescheduleErrors(true);
    scheduler->getGreedyScheduler()->setAbortDelaySeconds(3600);
    scheduler->getGreedyScheduler()->setErrorDelaySeconds(3600);

    loadGreedySchedule(true, "M 104", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 36);
    loadGreedySchedule(false, "NGC 3628", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 45);
    loadGreedySchedule(false, "M 5", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "M 42", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 42);

    // start the scheduler at ...
    KStarsDateTime evalUTime(QDate(2022, 2, 28), QTime(12, 30, 26), Qt::UTC);
    SchedulerJob::setLocalTime(&evalUTime);
    Scheduler::setLocalTime(&evalUTime);

    scheduler->evaluateJobs(false);

    QVERIFY(checkSchedule(
    {
        {"M 5",        "2022/02/28 04:30", "2022/02/28 05:14"},
        {"M 42",       "2022/02/28 19:31", "2022/02/28 20:43"},
        {"NGC 3628",   "2022/02/28 22:02", "2022/03/01 00:38"},
        {"M 104",      "2022/03/01 00:39", "2022/03/01 03:49"},
        {"M 5",        "2022/03/01 03:50", "2022/03/01 05:12"},
        {"M 42",       "2022/03/01 19:31", "2022/03/01 20:39"},
        {"NGC 3628",   "2022/03/01 21:58", "2022/03/02 00:34"},
        {"M 104",      "2022/03/02 00:35", "2022/03/02 03:45"},
        {"M 5",        "2022/03/02 03:46", "2022/03/02 05:12"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));

    // Now load the same schedule, but set the M5 job to have been aborted a minute before (2/28 12:29) the eval time (2/28 12:30am)

    scheduler->getGreedyScheduler()->setRescheduleAbortsImmediate(false);
    scheduler->getGreedyScheduler()->setRescheduleAbortsQueue(true);
    scheduler->getGreedyScheduler()->setRescheduleErrors(true);
    scheduler->getGreedyScheduler()->setAbortDelaySeconds(3600);
    scheduler->getGreedyScheduler()->setErrorDelaySeconds(3600);
    loadGreedySchedule(true, "M 104", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 36);
    loadGreedySchedule(false, "NGC 3628", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 45);
    loadGreedySchedule(false, "M 5", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 30);
    loadGreedySchedule(false, "M 42", asapStartupCondition, loopCompletionCondition, dir, schedJob200x60, 42);

    // Find the M 5 job and make it aborted at ....
    KStarsDateTime abortUTime(QDate(2022, 2, 28), QTime(12, 29, 26), Qt::UTC);
    SchedulerJob::setLocalTime(&abortUTime);
    Scheduler::setLocalTime(&abortUTime);

    foreach (auto &job, scheduler->jobs)
        if (job->getName() == "M 5")
            job->setState(SchedulerJob::JOB_ABORTED);

    // start the scheduler at ...
    SchedulerJob::setLocalTime(&evalUTime);
    Scheduler::setLocalTime(&evalUTime);

    scheduler->evaluateJobs(false);

    // The M5 job is no longer the first job, since aborted jobs are delayed an hour,
    // but it does return the next day.

    QVERIFY(checkSchedule(
    {
        {"M 42",       "2022/02/28 19:30", "2022/02/28 20:42"},
        {"NGC 3628",   "2022/02/28 22:03", "2022/03/01 00:39"},
        {"M 104",      "2022/03/01 00:40", "2022/03/01 03:48"},
        {"M 5",        "2022/03/01 03:49", "2022/03/01 05:13"},
        {"M 42",       "2022/03/01 19:30", "2022/03/01 20:38"},
        {"NGC 3628",   "2022/03/01 21:59", "2022/03/02 00:35"},
        {"M 104",      "2022/03/02 00:36", "2022/03/02 03:44"},
        {"M 5",        "2022/03/02 03:45", "2022/03/02 05:11"}},
    scheduler->getGreedyScheduler()->getSchedule(), 200));

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


