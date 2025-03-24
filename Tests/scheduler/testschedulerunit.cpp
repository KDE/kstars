/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * This file contains unit tests for the scheduler, in particular for the
 * planning parts--evaluating jobs and setting proposed start/end times for them.
 */

#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerutils.h"
#include "ekos/scheduler/greedyscheduler.h"
#include "ekos/scheduler/schedulerjob.h"
#include "ekos/scheduler/schedulermodulestate.h"
#include "indi/indiproperty.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/capture/placeholderpath.h"
#include "geolocation.h"
#include "Options.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif
#include <memory>

#include <QObject>

using Ekos::SequenceJob;
using Ekos::Scheduler;

class TestSchedulerUnit : public QObject
{
        Q_OBJECT

    public:
        /** @short Constructor */
        TestSchedulerUnit();

        /** @short Destructor */
        ~TestSchedulerUnit() override = default;

    private slots:
        void setupGeoAndTimeTest();
        void setupJobTest_data();
        void setupJobTest();
        void loadSequenceQueueTest();
        void estimateJobTimeTest();
        void evaluateJobsTest();

    private:
        void runSetupJob(Ekos::SchedulerJob &job,
                         GeoLocation *geo, KStarsDateTime *localTime, const QString &name,
                         const dms &ra, const dms &dec, double positionAngle, const QUrl &sequenceUrl,
                         const QUrl &fitsUrl, Ekos::StartupCondition sCond, const QDateTime &sTime, Ekos::CompletionCondition eCond,
                         const QDateTime &eTime, int eReps,
                         double minAlt, double minMoonSep = 0, double maxMoonAlt = 90, bool enforceTwilight = true,
                         bool enforceArtificialHorizon = true, bool track = true, bool focus = true, bool align = true, bool guide = true);
};

#include "testschedulerunit.moc"

// The tests use this latitude/longitude, and happen around this QDateTime.
GeoLocation siliconValley(dms(-122, 10), dms(37, 26, 30), "Silicon Valley", "CA", "USA", -7);
KStarsDateTime midNight(QDateTime(QDate(2021, 4, 17), QTime(0, 0, 1), QTimeZone(-7 * 3600)));

// At the midNight (midnight start of 4/17/2021) a star at the zenith is about
// at DEC=37d33'36" ~37.56 degrees, RA=12h32'48" ~ 188.2 degrees
// The tests in this file use this RA/DEC.
dms midnightRA(188.2), testDEC(37.56);

// These altitudes were precomputed for the above GeoLocation and time (midnight), time offset
// by 12hours in the vector, one altitude per hour.
// That is, altitudes[0] corresponds to -12 hours from midNight.
QVector<double> svAltitudes =
{
    -15.11, -13.92, -10.28, -4.53, // 12,1,2,3pm
        2.94, 11.74, 21.53, 32.05,     // 4,5,6,7pm
        43.10, 54.53, 66.22, 78.08,    // 8,9,10,11pm
        89.99,                         // midnight
        78.07, 66.21, 54.52, 43.09,    // 1,2,3,4am
        32.04, 21.52, 11.73, 2.94,     // 5,6,7,8am
        -4.53, -10.29, -13.92, -15.11  // 9,10,11,12
    };

// Used to keep the "correct information" about what's stored in an .esq file
// in order to test loadSequenceQueue() and processJobInfo(XML, job) which it calls.
struct CaptureJobDetails
{
    QString filter;
    int count;
    double exposure;
    CCDFrameType type;
};

// This sequence corresponds to the contents of the sequence file 9filters.esq.
const QString seqFile9Filters = "9filters.esq";
QList<CaptureJobDetails> details9Filters =
{
    {"Luminance", 6,  60.0, FRAME_LIGHT},
    {"SII",      20,  30.0, FRAME_LIGHT},
    {"OIII",      7,  20.0, FRAME_LIGHT},
    {"H_Alpha",   5,  30.0, FRAME_LIGHT},
    {"Red",       7,  90.0, FRAME_LIGHT},
    {"Green",     7,  45.0, FRAME_LIGHT},
    {"Blue",      2, 120.0, FRAME_LIGHT},
    {"SII",       6,  30.0, FRAME_LIGHT},
    {"OIII",      6,  10.0, FRAME_LIGHT}
};

TestSchedulerUnit::TestSchedulerUnit() : QObject()
{
    // Remove the dither-enabled option. It adds a complexity to estimating the job time.
    Options::setDitherEnabled(false);

    // Setting this true winds up calling KStarsData::Instance() in the scheduler via SkyPoint::apparentCoord().
    // Unit tests don't instantiate KStarsData::Instance() and will crash.
    Options::setUseRelativistic(false);
}

// Tests that the doubles are within tolerance.
bool compareFloat(double d1, double d2, double tolerance = .0001)
{
    return (fabs(d1 - d2) < tolerance);
}

// Tests that the QDateTimes are within the tolerance in seconds.
bool compareTimes(const QDateTime &t1, const QDateTime &t2, int toleranceSecs = 1)
{
    int toleranceMsecs = toleranceSecs * 1000;
    if (std::abs(t1.msecsTo(t2)) >= toleranceMsecs)
    {
        QWARN(qPrintable(QString("Comparison of %1 with %2 is out of %3s tolerance.").arg(t1.toString()).arg(t2.toString()).arg(
                             toleranceSecs)));
        return false;
    }
    else return true;
}

// The runSetupJob() utility calls the static function Scheduler::setupJob() with all the args passed in
// and tests to see that the resulting Ekos::SchedulerJob object has the values that were requested.
void TestSchedulerUnit::runSetupJob(Ekos::SchedulerJob &job, GeoLocation *geo, KStarsDateTime *localTime,
                                    const QString &name,
                                    const dms &ra, const dms &dec, double positionAngle, const QUrl &sequenceUrl,
                                    const QUrl &fitsUrl, Ekos::StartupCondition sCond, const QDateTime &sTime,
                                    Ekos::CompletionCondition eCond, const QDateTime &eTime, int eReps,
                                    double minAlt, double minMoonSep, double maxMoonAlt, bool enforceTwilight,
                                    bool enforceArtificialHorizon, bool track, bool focus, bool align, bool guide)
{
    // Setup the time and geo.
    KStarsDateTime ut = geo->LTtoUT(*localTime);
    Ekos::SchedulerModuleState::setGeo(geo);
    Ekos::SchedulerModuleState::setLocalTime(localTime);
    QVERIFY(Ekos::SchedulerModuleState::hasLocalTime() && Ekos::SchedulerModuleState::hasGeo());

    QString group = "";
    QString train = "";
    Ekos::SchedulerUtils::setupJob(job, name, true, group, train, ra, dec, ut.djd(), positionAngle,
                                   sequenceUrl, fitsUrl,
                                   sCond, sTime,
                                   eCond, eTime, eReps,
                                   minAlt, minMoonSep, maxMoonAlt,
                                   enforceTwilight, enforceArtificialHorizon,
                                   track, focus, align, guide);
    QVERIFY(name == job.getName());
    QVERIFY(ra == job.getTargetCoords().ra0());
    QVERIFY(dec == job.getTargetCoords().dec0());
    QVERIFY(positionAngle == job.getPositionAngle());
    QVERIFY(sequenceUrl == job.getSequenceFile());
    QVERIFY(fitsUrl == job.getFITSFile());
    QVERIFY(minAlt == job.getMinAltitude());
    QVERIFY(minMoonSep == job.getMinMoonSeparation());
    QVERIFY(maxMoonAlt == job.getMaxMoonAltitude());
    QVERIFY(enforceTwilight == job.getEnforceTwilight());
    QVERIFY(enforceArtificialHorizon == job.getEnforceArtificialHorizon());

    QVERIFY(sCond == job.getStartupCondition());
    switch (sCond)
    {
        case Ekos::START_AT:
            QVERIFY(sTime == job.getStartupTime());
            break;
        case Ekos::START_ASAP:
            QVERIFY(QDateTime() == job.getStartupTime());
            break;
    }

    QVERIFY(eCond == job.getCompletionCondition());
    switch (eCond)
    {
        case Ekos::FINISH_AT:
            QVERIFY(eTime == job.getFinishAtTime());
            QVERIFY(0 == job.getRepeatsRequired());
            QVERIFY(0 == job.getRepeatsRemaining());
            break;
        case Ekos::FINISH_REPEAT:
            QVERIFY(QDateTime() == job.getFinishAtTime());
            QVERIFY(eReps == job.getRepeatsRequired());
            QVERIFY(eReps == job.getRepeatsRemaining());
            break;
        case Ekos::FINISH_SEQUENCE:
            QVERIFY(QDateTime() == job.getFinishAtTime());
            QVERIFY(1 == job.getRepeatsRequired());
            QVERIFY(1 == job.getRepeatsRemaining());
            break;
        case Ekos::FINISH_LOOP:
            QVERIFY(QDateTime() == job.getFinishAtTime());
            QVERIFY(0 == job.getRepeatsRequired());
            QVERIFY(0 == job.getRepeatsRemaining());
            break;
    }

    Ekos::SchedulerJob::StepPipeline pipe = job.getStepPipeline();
    QVERIFY((track && (pipe & Ekos::SchedulerJob::USE_TRACK)) || (!track && !(pipe & Ekos::SchedulerJob::USE_TRACK)));
    QVERIFY((focus && (pipe & Ekos::SchedulerJob::USE_FOCUS)) || (!focus && !(pipe & Ekos::SchedulerJob::USE_FOCUS)));
    QVERIFY((align && (pipe & Ekos::SchedulerJob::USE_ALIGN)) || (!align && !(pipe & Ekos::SchedulerJob::USE_ALIGN)));
    QVERIFY((guide && (pipe & Ekos::SchedulerJob::USE_GUIDE)) || (!guide && !(pipe & Ekos::SchedulerJob::USE_GUIDE)));
}

void TestSchedulerUnit::setupGeoAndTimeTest()
{
    Ekos::SchedulerJob job(nullptr);
    QVERIFY(!Ekos::SchedulerModuleState::hasLocalTime() && !Ekos::SchedulerModuleState::hasGeo());
    Ekos::SchedulerModuleState::setGeo(&siliconValley);
    Ekos::SchedulerModuleState::setLocalTime(&midNight);
    QVERIFY(Ekos::SchedulerModuleState::hasLocalTime() && Ekos::SchedulerModuleState::hasGeo());
    QVERIFY(Ekos::SchedulerModuleState::getGeo()->lat() == siliconValley.lat());
    QVERIFY(Ekos::SchedulerModuleState::getGeo()->lng() == siliconValley.lng());
    QVERIFY(job.getLocalTime() == midNight);
}

Q_DECLARE_METATYPE(Ekos::StartupCondition);
Q_DECLARE_METATYPE(Ekos::CompletionCondition);

// Test Scheduler::setupJob().
// Calls runSetupJob (which calls Ekos::SchedulerJob::setupJob(...)) in a few different ways
// to test different kinds of Ekos::SchedulerJob initializations.
void TestSchedulerUnit::setupJobTest_data()
{
    QTest::addColumn<Ekos::StartupCondition>("START_CONDITION");
    QTest::addColumn<QDateTime>("START_TIME");
    QTest::addColumn<Ekos::CompletionCondition>("END_CONDITION");
    QTest::addColumn<QDateTime>("END_TIME");
    QTest::addColumn<int>("REPEATS");
    QTest::addColumn<bool>("ENFORCE_TWILIGHT");
    QTest::addColumn<bool>("ENFORCE_ARTIFICIAL_HORIZON");
    QTest::addColumn<bool>("TRACK");
    QTest::addColumn<bool>("FOCUS");
    QTest::addColumn<bool>("ALIGN");
    QTest::addColumn<bool>("GUIDE");

    QTest::newRow("ASAP_TO_FINISH")
            << Ekos::START_ASAP << QDateTime() // start conditions
            << Ekos::FINISH_SEQUENCE << QDateTime() << 1 // end conditions
            << true   // enforce twilight
            << true   // enforce artificial horizon
            << false  // track
            << true   // focus
            << true   // align
            << true;  // guide

    QTest::newRow("START_AT_FINISH_AT")
            << Ekos::START_AT // start conditions
            << QDateTime(QDate(2021, 4, 17), QTime(0, 1, 0), QTimeZone(-7 * 3600))
            << Ekos::FINISH_AT // end conditions
            << QDateTime(QDate(2021, 4, 17), QTime(0, 2, 0), QTimeZone(-7 * 3600))
            << 1
            << false  // enforce twilight
            << true   // enforce artificial horizon
            << true   // track
            << false  // focus
            << true   // align
            << true;  // guide

    QTest::newRow("ASAP_TO_LOOP")
            << Ekos::START_ASAP << QDateTime() // start conditions
            << Ekos::FINISH_SEQUENCE << QDateTime() << 1 // end conditions
            << false  // enforce twilight
            << true   // enforce artificial horizon
            << true   // track
            << true   // focus
            << true   // align
            << false; // guide
}

void TestSchedulerUnit::setupJobTest()
{
    QFETCH(Ekos::StartupCondition, START_CONDITION);
    QFETCH(QDateTime, START_TIME);
    QFETCH(Ekos::CompletionCondition, END_CONDITION);
    QFETCH(QDateTime, END_TIME);
    QFETCH(int, REPEATS);
    QFETCH(bool, ENFORCE_TWILIGHT);
    QFETCH(bool, ENFORCE_ARTIFICIAL_HORIZON);
    QFETCH(bool, TRACK);
    QFETCH(bool, FOCUS);
    QFETCH(bool, ALIGN);
    QFETCH(bool, GUIDE);

    Ekos::SchedulerJob job(nullptr);
    runSetupJob(job, &siliconValley, &midNight, "Job1",
                midnightRA, testDEC, 5.0, QUrl("1"), QUrl("2"),
                START_CONDITION, START_TIME,
                END_CONDITION, END_TIME, REPEATS,
                30.0, 5.0, ENFORCE_TWILIGHT,
                ENFORCE_ARTIFICIAL_HORIZON, TRACK, FOCUS, ALIGN, GUIDE);
}

namespace
{
// compareCaptureSequeuce() is a utility to use the CaptureJobDetails structure as a truth value
// to see if the capture sequeuce was loaded properly.
void compareCaptureSequence(const QList<CaptureJobDetails> &details, const QList<QSharedPointer<Ekos::SequenceJob>> &jobs)
{
    QVERIFY(details.size() == jobs.size());
    for (int i = 0; i < jobs.size(); ++i)
    {
        QVERIFY(details[i].filter == jobs[i]->getCoreProperty(SequenceJob::SJ_Filter).toString());
        QVERIFY(details[i].count == jobs[i]->getCoreProperty(SequenceJob::SJ_Count).toInt());
        QVERIFY(details[i].exposure == jobs[i]->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
        QVERIFY(details[i].type == jobs[i]->getFrameType());
    }
}
}  // namespace

// Test Scheduler::loadSequeuceQueue().
// Load sequenceQueue doesn't load all details of the sequence. It just loads what it
// needs to compute a duration estimate for the job.
// Hence, compareCaptureSequence just tests a few things that a capture sequence can contain.
// A full test for capture sequences should be written in capture testing.
void TestSchedulerUnit::loadSequenceQueueTest()
{
    // Create a new Ekos::SchedulerJob and pass in a null moon pointer.
    Ekos::SchedulerJob schedJob(nullptr);

    QList<QSharedPointer<Ekos::SequenceJob>> jobs;
    bool hasAutoFocus = false;
    // Read in the 9 filters file.
    // The last arg is for logging. Use nullptr for testing.
    QVERIFY(Ekos::SchedulerUtils::loadSequenceQueue(seqFile9Filters, &schedJob, jobs, hasAutoFocus, nullptr));
    // Makes sure we have the basic details of the capture sequence were read properly.
    compareCaptureSequence(details9Filters, jobs);
}

namespace
{
// This utility computes the sum of time taken for all exposures in a capture sequence.
double computeExposureDurations(const QList<CaptureJobDetails> &details)
{
    double sum = 0;
    for (int i = 0; i < details.size(); ++i)
        sum += details[i].count * details[i].exposure;
    return sum;
}
}  // namespace

// Test Scheduler::estimateJobTime(). Tests the estimates of job completion time.
// Focuses on the non-heuristic aspects (e.g. sum of num_exposures * exposure_duration for all the
// jobs in the capture sequence).
void TestSchedulerUnit::estimateJobTimeTest()
{
    // Some computations use the local time, which is normally taken from KStars::Instance()->lt()
    // unless this is set. The Instance does not exist when running this unit test.
    Ekos::SchedulerModuleState::setLocalTime(&midNight);

    // First test, start ASAP and finish when the sequence is done.
    Ekos::SchedulerJob job(nullptr);
    runSetupJob(job, &siliconValley, &midNight, "Job1",
                midnightRA, testDEC, 5.0,
                QUrl(QString("file:%1").arg(seqFile9Filters)), QUrl(""),
                Ekos::START_ASAP, QDateTime(),
                Ekos::FINISH_SEQUENCE, QDateTime(), 1,
                30.0, 5.0, false, false);

    // Initial map has no previous captures.
    Ekos::CapturedFramesMap capturedFramesCount;
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));

    // The time estimate is essentially the sum of (exposure times * the number of exposures) for each filter.
    // There are other heuristics added to take initial track, focus, align & guide into account.
    // We're not testing these heuristics here, so they are set to false in the job setup above (last 4 bools).
    const double exposureDuration = computeExposureDurations(details9Filters);
    const int overhead = Ekos::SchedulerUtils::timeHeuristics(&job);
    QVERIFY(compareFloat(exposureDuration + overhead, job.getEstimatedTime()));

    // Repeat the above test, but repeat the sequence 1,2,3,4,...,10 times.
    for (int i = 1; i <= 10; ++i)
    {
        job.setCompletionCondition(Ekos::FINISH_REPEAT);
        job.setRepeatsRequired(i);
        QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
        QVERIFY(compareFloat(overhead + i * exposureDuration, job.getEstimatedTime()));
    }

    // Resetting the number of repeats. This has a side-effect of changing the completion condition,
    // so we must change completion condition below.
    job.setRepeatsRequired(1);

    // Test again, this time looping indefinitely.
    // In this case the scheduler should estimate negative completion time, as the sequence doesn't
    // end until the user stops it (or it is interrupted by altitude or daylight). The scheduler
    // doesn't estimate those stopping conditions at this point.
    job.setCompletionCondition(Ekos::FINISH_LOOP);
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
    QVERIFY(job.getEstimatedTime() < 0);

    // Test again with a fixed end time. The scheduler estimates the time from "now" until the end time.
    // Perhaps it should estimate the max of that and the FINISH_SEQUENCE time??
    job.setCompletionCondition(Ekos::FINISH_AT);
    KStarsDateTime stopTime(QDateTime(QDate(2021, 4, 17), QTime(1, 0, 0), QTimeZone(-7 * 3600)));
    job.setFinishAtTime(stopTime);
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
    QVERIFY(midNight.secsTo(stopTime) == job.getEstimatedTime());

    // Test again, similar to above but given a START_AT time as well.
    // Now it should return the interval between the start and end times.
    // Again, perhaps that should be max'd with the FINISH_SEQUENCE time?
    job.setStartupCondition(Ekos::START_AT);
    job.setStartupTime(midNight.addSecs(1800));
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
    QVERIFY(midNight.secsTo(stopTime) - 1800 == job.getEstimatedTime());

    // Small test of accounting for already completed captures.
    // 1. Explicitly load the capture jobs
    job.setStartupCondition(Ekos::START_ASAP);
    job.setCompletionCondition(Ekos::FINISH_SEQUENCE);
    QList<QSharedPointer<Ekos::SequenceJob>> jobs;
    bool hasAutoFocus = false;
    // The last arg is for logging. Use nullptr for testing.
    QVERIFY(Ekos::SchedulerUtils::loadSequenceQueue(seqFile9Filters, &job, jobs, hasAutoFocus, nullptr));
    // 2. Get the signiture of the first job
    QSharedPointer<SequenceJob> &seqJob = jobs[0];
    seqJob->setCoreProperty(Ekos::SequenceJob::SJ_TargetName, job.getName());
    auto placeholderPath = Ekos::PlaceholderPath();
    QString signature = placeholderPath.generateSequenceFilename(*seqJob, true, true, 1, ".fits", "", false, true);
    seqJob->setCoreProperty(SequenceJob::SJ_Signature, signature);
    QString sig0 = jobs[0]->getSignature();
    // 3. The first job has 6 exposures, each of 20s duration. Set it up that 2 are already done.
    capturedFramesCount[sig0] = 2;
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
    // 4. Now expect that we have 2*60s=120s less job time, but only if we're remembering job progress.
    // First don't remember job progress, and the scheduler should provide the standard estimate.
    Options::setRememberJobProgress(false);
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
    QVERIFY(compareFloat(overhead + exposureDuration, job.getEstimatedTime()));
    // Next remember the progress, the job should reduce the estimate by 120s (the 2 completed exposures).
    Options::setRememberJobProgress(true);
    QVERIFY(Ekos::SchedulerUtils::estimateJobTime(&job, capturedFramesCount, nullptr));
    QVERIFY(compareFloat(overhead + exposureDuration - 120, job.getEstimatedTime()));
}

// Test Scheduler::evaluateJobs().
void TestSchedulerUnit::evaluateJobsTest()
{
    Options::setMaximumAltLimit(100);
    auto now = midNight;
    Ekos::GreedyScheduler scheduler;
    Ekos::SchedulerModuleState state;
    state.setLocalTime(&now);
    // The nullptr is moon pointer. Not currently tested.
    Ekos::SchedulerJob job(nullptr);

    const double _dawn = .25, _dusk = .75;
    QDateTime const dawn = midNight.addSecs(_dawn * 24.0 * 3600.0);
    QDateTime const dusk = midNight.addSecs(_dusk * 24.0 * 3600.0);
    const bool rescheduleErrors = true;
    const bool restart = true;
    const Ekos::CapturedFramesMap capturedFrames;
    QList<Ekos::SchedulerJob *> jobs;
    const double minAltitude = 30.0;

    // Test 1: Evaluating an empty jobs list should return an empty list.
    scheduler.setParams(restart, true, rescheduleErrors, 3600, 3600);
    scheduler.scheduleJobs(jobs, now, capturedFrames, nullptr);
    QVERIFY(jobs.empty());

    // Test 2: Add one job to the list.
    // It should be on the list, and scheduled starting right away (there are no conflicting constraints)
    // and ending at the estimated completion interval after "now" .
    runSetupJob(job, &siliconValley, &midNight, "Job1",
                midnightRA, testDEC, 0.0,
                QUrl(QString("file:%1").arg(seqFile9Filters)), QUrl(""),
                Ekos::START_ASAP, QDateTime(),
                Ekos::FINISH_SEQUENCE, QDateTime(), 1,
                minAltitude);
    jobs.append(&job);

    scheduler.scheduleJobs(jobs, now, capturedFrames, nullptr);
    // Should have the one same job on both lists.
    QVERIFY(jobs.size() == 1);
    QVERIFY(jobs[0] == &job);
    // The job should start now.
    QVERIFY(job.getStartupTime().secsTo(now) == 0);
    // It should finish when its exposures are done.
    QVERIFY(compareTimes(job.getFinishAtTime(),
                         now.addSecs(Ekos::SchedulerUtils::timeHeuristics(&job) +
                                     computeExposureDurations(details9Filters))));

    state.calculateDawnDusk();

    // The job should run inside the twilight interval and have the same twilight values as Scheduler current values
    QVERIFY(job.runsDuringAstronomicalNightTime());
    QVERIFY(job.getDawnAstronomicalTwilight() == Ekos::SchedulerModuleState::Dawn());
    QVERIFY(job.getDuskAstronomicalTwilight() == Ekos::SchedulerModuleState::Dusk());

    // The job can start now, thus the next events are dawn, then dusk
    QVERIFY(Ekos::SchedulerModuleState::Dawn() <= Ekos::SchedulerModuleState::Dusk());

    jobs.clear();

    // Test 3: In this case, there are two jobs.
    // The first must wait for to get above the min altitude (which is set to 80-degrees).
    // The second one has no constraints, but is scheduled after the first.

    // Start the scheduler at 8pm but minAltitude won't be reached until after 11pm.
    // Job repetition takes about 45 minutes plus a little overhead.
    // Thus, first job, with two repetitions will be scheduled 11:10pm --> 12:43am.
    Ekos::SchedulerJob job1(nullptr);
    auto localTime8pm = midNight.addSecs(-4 * 3600);
    runSetupJob(job1, &siliconValley, &localTime8pm, "Job1",
                midnightRA, testDEC, 0.0,
                QUrl(QString("file:%1").arg(seqFile9Filters)), QUrl(""),
                Ekos::START_ASAP, QDateTime(),
                Ekos::FINISH_REPEAT, QDateTime(), 2,
                80.0);
    jobs.append(&job1);

    // The second job has no blocking constraints, hence it will start immediately at 08:00 pm.
    // Since it lasts about 48 minutes, it should terminate at 08:48 pm and won't be suspended
    // by the first job.
    Ekos::SchedulerJob job2(nullptr);
    runSetupJob(job2, &siliconValley, &localTime8pm, "Job2",
                midnightRA, testDEC, 0.0,
                QUrl(QString("file:%1").arg(seqFile9Filters)), QUrl(""),
                Ekos::START_ASAP, QDateTime(),
                Ekos::FINISH_SEQUENCE, QDateTime(), 1,
                30.0);
    jobs.append(&job2);

    scheduler.scheduleJobs(jobs, localTime8pm, capturedFrames, nullptr);

    QVERIFY(jobs.size() == 2);
    QVERIFY(jobs[0] == &job1);
    QVERIFY(jobs[1] == &job2);
    QVERIFY(compareTimes(jobs[0]->getStartupTime(), midNight.addSecs(-50 * 60), 300));
    QVERIFY(compareTimes(jobs[0]->getFinishAtTime(), midNight.addSecs(43 * 60), 300));

    QVERIFY(compareTimes(jobs[1]->getStartupTime(), localTime8pm, 300));
    QVERIFY(compareTimes(jobs[1]->getFinishAtTime(), localTime8pm.addSecs(48 * 60), 300));

    state.calculateDawnDusk();

    // The two job should run inside the twilight interval and have the same twilight values as Scheduler current values
    QVERIFY(jobs[0]->runsDuringAstronomicalNightTime());
    QVERIFY(jobs[1]->runsDuringAstronomicalNightTime());
    QVERIFY(jobs[0]->getDawnAstronomicalTwilight() == Ekos::SchedulerModuleState::Dawn());
    QVERIFY(jobs[1]->getDuskAstronomicalTwilight() == Ekos::SchedulerModuleState::Dusk());

    // The two job can start now, thus the next events for today are dawn, then dusk
    QVERIFY(Ekos::SchedulerModuleState::Dawn() <= Ekos::SchedulerModuleState::Dusk());

    jobs.clear();

    // Test 4: Similar to above, but the first job estimate will run past dawn as it has 10 repetitions.
    // The second job, therefore, should be initially scheduled to start "tomorrow" just after dusk.

    // Start the scheduler at 8pm but minAltitude won't be reached until ~11:10pm, and job3 estimated conclusion is 6:39am.
    Ekos::SchedulerJob job3(nullptr);
    Ekos::SchedulerModuleState::setLocalTime(&localTime8pm);
    runSetupJob(job3, &siliconValley, &localTime8pm, "Job1",
                midnightRA, testDEC, 0.0,
                QUrl(QString("file:%1").arg(seqFile9Filters)), QUrl(""),
                Ekos::START_ASAP, QDateTime(),
                Ekos::FINISH_REPEAT, QDateTime(), 10,
                80.0);
    jobs.append(&job3);

    // The second job should be scheduled "tomorrow" starting after dusk
    Ekos::SchedulerJob job4(nullptr);
    runSetupJob(job4, &siliconValley, &localTime8pm, "Job2",
                midnightRA, testDEC, 0.0,
                QUrl(QString("file:%1").arg(seqFile9Filters)), QUrl(""),
                Ekos::START_ASAP, QDateTime(),
                Ekos::FINISH_SEQUENCE, QDateTime(), 1,
                80.0);
    jobs.append(&job4);

    scheduler.scheduleJobs(jobs, localTime8pm, capturedFrames, nullptr);

    QVERIFY(jobs.size() == 2);
    QVERIFY(jobs[0] == &job3);
    QVERIFY(jobs[1] == &job4);
    QVERIFY(compareTimes(jobs[0]->getStartupTime(), midNight.addSecs(-50 * 60), 300));
    QVERIFY(compareTimes(jobs[0]->getFinishAtTime(), midNight.addSecs(6 * 3600 + 39 * 60), 300));

    QVERIFY(compareTimes(jobs[1]->getStartupTime(), midNight.addSecs(18 * 3600 + 54 * 60), 300));
    QVERIFY(compareTimes(jobs[1]->getFinishAtTime(), midNight.addSecs(19 * 3600 + 44 * 60), 300));

    jobs.clear();
}

QTEST_GUILESS_MAIN(TestSchedulerUnit)
