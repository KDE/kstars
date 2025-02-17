/*  Ekos Scheduler Job
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulerjob.h"

#include "dms.h"
#include "artificialhorizoncomponent.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "Options.h"
#include "scheduler.h"
#include "schedulermodulestate.h"
#include "schedulerutils.h"
#include "ksmoon.h"

#include <knotification.h>

#include <ekos_scheduler_debug.h>

#define BAD_SCORE -1000
#define MIN_ALTITUDE 15.0

namespace Ekos
{
GeoLocation *SchedulerJob::storedGeo = nullptr;
KStarsDateTime *SchedulerJob::storedLocalTime = nullptr;
ArtificialHorizon *SchedulerJob::storedHorizon = nullptr;

QString SchedulerJob::jobStatusString(SchedulerJobStatus state)
{
    switch(state)
    {
        case SCHEDJOB_IDLE:
            return "IDLE";
        case SCHEDJOB_EVALUATION:
            return "EVAL";
        case SCHEDJOB_SCHEDULED:
            return "SCHEDULED";
        case SCHEDJOB_BUSY:
            return "BUSY";
        case SCHEDJOB_ERROR:
            return "ERROR";
        case SCHEDJOB_ABORTED:
            return "ABORTED";
        case SCHEDJOB_INVALID:
            return "INVALID";
        case SCHEDJOB_COMPLETE:
            return "COMPLETE";
    }
    return QString("????");
}

QString SchedulerJob::jobStageString(SchedulerJobStage state)
{
    switch(state)
    {
        case SCHEDSTAGE_IDLE:
            return "IDLE";
        case SCHEDSTAGE_SLEWING:
            return "SLEWING";
        case SCHEDSTAGE_SLEW_COMPLETE:
            return "SLEW_COMPLETE";
        case SCHEDSTAGE_FOCUSING:
            return "FOCUSING";
        case SCHEDSTAGE_FOCUS_COMPLETE:
            return "FOCUS_COMPLETE";
        case SCHEDSTAGE_ALIGNING:
            return "ALIGNING";
        case SCHEDSTAGE_ALIGN_COMPLETE:
            return "ALIGN_COMPLETE";
        case SCHEDSTAGE_RESLEWING:
            return "RESLEWING";
        case SCHEDSTAGE_RESLEWING_COMPLETE:
            return "RESLEWING_COMPLETE";
        case SCHEDSTAGE_POSTALIGN_FOCUSING:
            return "POSTALIGN_FOCUSING";
        case SCHEDSTAGE_POSTALIGN_FOCUSING_COMPLETE:
            return "POSTALIGN_FOCUSING_COMPLETE";
        case SCHEDSTAGE_GUIDING:
            return "GUIDING";
        case SCHEDSTAGE_GUIDING_COMPLETE:
            return "GUIDING_COMPLETE";
        case SCHEDSTAGE_CAPTURING:
            return "CAPTURING";
        case SCHEDSTAGE_COMPLETE:
            return "COMPLETE";
    }
    return QString("????");
}

QString SchedulerJob::jobStartupConditionString(StartupCondition condition) const
{
    switch(condition)
    {
        case START_ASAP:
            return "ASAP";
        case START_AT:
            return QString("AT %1").arg(getStartAtTime().toString("MM/dd hh:mm"));
    }
    return QString("????");
}

QString SchedulerJob::jobCompletionConditionString(CompletionCondition condition) const
{
    switch(condition)
    {
        case FINISH_SEQUENCE:
            return "FINISH";
        case FINISH_REPEAT:
            return "REPEAT";
        case FINISH_LOOP:
            return "LOOP";
        case FINISH_AT:
            return QString("AT %1").arg(getFinishAtTime().toString("MM/dd hh:mm"));
    }
    return QString("????");
}

SchedulerJob::SchedulerJob()
{
    if (KStarsData::Instance() != nullptr)
        moon = dynamic_cast<KSMoon *>(KStarsData::Instance()->skyComposite()->findByName(i18n("Moon")));
}

// Private constructor for unit testing.
SchedulerJob::SchedulerJob(KSMoon *moonPtr)
{
    moon = moonPtr;
}

void SchedulerJob::setName(const QString &value)
{
    name = value;
}

void SchedulerJob::setGroup(const QString &value)
{
    group = value;
}



void SchedulerJob::setCompletedIterations(int value)
{
    completedIterations = value;
    if (completionCondition == FINISH_REPEAT)
        setRepeatsRemaining(getRepeatsRequired() - completedIterations);
}

KStarsDateTime SchedulerJob::getLocalTime()
{
    return Ekos::SchedulerModuleState::getLocalTime();
}

ArtificialHorizon const *SchedulerJob::getHorizon()
{
    if (hasHorizon())
        return storedHorizon;
    if (KStarsData::Instance() == nullptr || KStarsData::Instance()->skyComposite() == nullptr
            || KStarsData::Instance()->skyComposite()->artificialHorizon() == nullptr)
        return nullptr;
    return &KStarsData::Instance()->skyComposite()->artificialHorizon()->getHorizon();
}

void SchedulerJob::setStartupCondition(const StartupCondition &value)
{
    startupCondition = value;

    /* Keep startup time and condition valid */
    if (value == START_ASAP)
        startupTime = QDateTime();

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);

    /* Refresh dawn and dusk for startup date */
    SchedulerModuleState::calculateDawnDusk(startupTime, nextDawn, nextDusk);
}

void SchedulerJob::setStartupTime(const QDateTime &value, bool refreshDawnDusk)
{
    startupTime = value;

    /* Keep startup time and condition valid */
    if (value.isValid())
        startupCondition = START_AT;
    else
        startupCondition = fileStartupCondition;

    // Refresh altitude - invalid date/time is taken care of when rendering
    altitudeAtStartup = SchedulerUtils::findAltitude(getTargetCoords(), startupTime, &settingAtStartup);

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);

    /* propagate it to all follower jobs, but avoid unnecessary dawn/dusk refresh */
    for (auto follower : followerJobs())
        follower->setStartupTime(value, false);

    /* Refresh dawn and dusk for startup date */
    if (refreshDawnDusk)
        SchedulerModuleState::calculateDawnDusk(startupTime, nextDawn, nextDusk);
}

void SchedulerJob::setSequenceFile(const QUrl &value)
{
    sequenceFile = value;
}

void SchedulerJob::setFITSFile(const QUrl &value)
{
    fitsFile = value;
}

void SchedulerJob::setMinAltitude(const double &value)
{
    minAltitude = value;
}

bool SchedulerJob::hasAltitudeConstraint() const
{
    return hasMinAltitude() ||
           (getEnforceArtificialHorizon() && (getHorizon() != nullptr) && getHorizon()->altitudeConstraintsExist()) ||
           (Options::enableAltitudeLimits() &&
            (Options::minimumAltLimit() > 0 ||
             Options::maximumAltLimit() < 90));
}

void SchedulerJob::setMinMoonSeparation(const double &value)
{
    minMoonSeparation = value;
}

void SchedulerJob::setMaxMoonAltitude(const double &value)
{
    maxMoonAltitude = value;
}

void SchedulerJob::setEnforceWeather(bool value)
{
    enforceWeather = value;
}

void SchedulerJob::setStopTime(const QDateTime &value)
{
    stopTime = value;

    // update altitude and setting at stop time
    if (value.isValid())
    {
        altitudeAtStop = SchedulerUtils::findAltitude(getTargetCoords(), stopTime, &settingAtStop);

        /* propagate it to all follower jobs, but avoid unnecessary dawn/dusk refresh */
        for (auto follower : followerJobs())
        {
            // if the lead job completes earlier as the follower would do, set its completion time to the lead job's
            if (follower->getStartupTime().isValid() && value.isValid()
                    && (follower->getEstimatedTime() < 0 || follower->getEstimatedTime() > getEstimatedTime()))
                follower->setEstimatedTime(getEstimatedTime());
        }
    }
}

void SchedulerJob::setFinishAtTime(const QDateTime &value)
{
    setStopTime(QDateTime());

    /* If completion time is valid, automatically switch condition to FINISH_AT */
    if (value.isValid())
    {
        setCompletionCondition(FINISH_AT);
        finishAtTime = value;
        setEstimatedTime(-1);
    }
    /* If completion time is invalid, and job is looping, keep completion time undefined */
    else if (FINISH_LOOP == completionCondition)
    {
        finishAtTime = QDateTime();
        setEstimatedTime(-1);
    }
    /* If completion time is invalid, deduce completion from startup and duration */
    else if (startupTime.isValid())
    {
        finishAtTime = startupTime.addSecs(estimatedTime);
    }
    /* Else just refresh estimated time - which update job cells */
    else setEstimatedTime(estimatedTime);


    /* Invariants */
    Q_ASSERT_X(finishAtTime.isValid() ?
               (FINISH_AT == completionCondition || FINISH_REPEAT == completionCondition || FINISH_SEQUENCE == completionCondition) :
               FINISH_LOOP == completionCondition,
               __FUNCTION__, "Valid completion time implies job is FINISH_AT/REPEAT/SEQUENCE, else job is FINISH_LOOP.");
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;

    // Update repeats requirement, looping jobs have none
    switch (completionCondition)
    {
        case FINISH_LOOP:
            setFinishAtTime(QDateTime());
        /* Fall through */
        case FINISH_AT:
            if (0 < getRepeatsRequired())
                setRepeatsRequired(0);
            break;

        case FINISH_SEQUENCE:
            if (1 != getRepeatsRequired())
                setRepeatsRequired(1);
            break;

        case FINISH_REPEAT:
            if (0 == getRepeatsRequired())
                setRepeatsRequired(1);
            break;

        default:
            break;
    }
}

void SchedulerJob::setStepPipeline(const StepPipeline &value)
{
    stepPipeline = value;
}

void SchedulerJob::setState(const SchedulerJobStatus &value, bool force)
{
    state = value;
    stateTime = getLocalTime();

    switch (state)
    {
        case SCHEDJOB_ERROR:
            /* FIXME: move this to Scheduler, SchedulerJob is mostly a model */
            lastErrorTime = getLocalTime();
            KNotification::event(QLatin1String("EkosSchedulerJobFail"), i18n("Ekos job failed (%1)", getName()));
            break;
        case SCHEDJOB_INVALID:
        case SCHEDJOB_IDLE:
            /* If job becomes invalid or idle, automatically reset its startup characteristics, and force its duration to be reestimated */
            setStartupCondition(fileStartupCondition);
            setStartupTime(startAtTime);
            setEstimatedTime(-1);
            break;
        case SCHEDJOB_ABORTED:
            /* If job is aborted, automatically reset its startup characteristics */
            lastAbortTime = getLocalTime();
            setStartupCondition(fileStartupCondition);
            /* setStartupTime(fileStartupTime); */
            break;
        default:
            /* do nothing */
            break;
    }
    // propagate it to the follower jobs
    if (isLead())
        setFollowerState(value, force);
}

void SchedulerJob::setFollowerState(const SchedulerJobStatus &value, bool force)
{
    for (auto follower : followerJobs())
    {
        // do not propagate the state if the job is running
        if (follower->getState() == SCHEDJOB_BUSY && force == false)
            continue;

        switch (value)
        {
            case SCHEDJOB_EVALUATION:
            case SCHEDJOB_IDLE:
                // always forward evaluation and startup, since only the lead job is part of the scheduling
                follower->setState(value);
                break;
            case SCHEDJOB_SCHEDULED:
                // if the lead job is scheduled, the follower job will be scheduled unless it is complete
                follower->setState(follower->getCompletionCondition() == FINISH_LOOP
                                   || follower->getEstimatedTime() > 0 ? value : SCHEDJOB_COMPLETE);
                break;
            default:
                // do NOT forward the state, each follower job needs to be started individually
                if (force)
                    follower->setState(value);
                break;
        }
    }
}

void SchedulerJob::updateSharedFollowerAttributes()
{
    if (isLead())
        for (auto follower : followerJobs())
        {
            follower->setStartupTime(getStartupTime(), false);
            follower->setStartAtTime(getStartAtTime());
            follower->setFollowerState(getState(), true);
        }
}


void SchedulerJob::setSequenceCount(const int count)
{
    sequenceCount = count;
}

void SchedulerJob::setCompletedCount(const int count)
{
    completedCount = count;
}


void SchedulerJob::setStage(const SchedulerJobStage &value)
{
    stage = value;
}

void SchedulerJob::setFileStartupCondition(const StartupCondition &value)
{
    fileStartupCondition = value;
}

void SchedulerJob::setStartAtTime(const QDateTime &value)
{
    startAtTime = value;
}

void SchedulerJob::setEstimatedTime(const int64_t &value)
{
    /* Estimated time is generally the difference between startup and completion times:
     * - It is fixed when startup and completion times are fixed, that is, we disregard the argument
     * - Else mostly it pushes completion time from startup time
     *
     * However it cannot advance startup time when completion time is fixed because of the way jobs are scheduled.
     * This situation requires a warning in the user interface when there is not enough time for the job to process.
     */

    /* If startup and completion times are fixed, estimated time cannot change - disregard the argument */
    if (START_ASAP != fileStartupCondition && FINISH_AT == completionCondition)
    {
        estimatedTime = startupTime.secsTo(finishAtTime);
    }
    /* If completion time isn't fixed, estimated time adjusts completion time */
    else if (FINISH_AT != completionCondition && FINISH_LOOP != completionCondition)
    {
        estimatedTime = value;
    }
    /* Else estimated time is simply stored as is - covers FINISH_LOOP from setCompletionTime */
    else estimatedTime = value;
}

void SchedulerJob::setInSequenceFocus(bool value)
{
    inSequenceFocus = value;
}

void SchedulerJob::setEnforceTwilight(bool value)
{
    enforceTwilight = value;
    SchedulerModuleState::calculateDawnDusk(startupTime, nextDawn, nextDusk);
}

void SchedulerJob::setEnforceArtificialHorizon(bool value)
{
    enforceArtificialHorizon = value;
}

void SchedulerJob::setLightFramesRequired(bool value)
{
    lightFramesRequired = value;
}

void SchedulerJob::setCalibrationMountPark(bool value)
{
    m_CalibrationMountPark = value;
}

void SchedulerJob::setRepeatsRequired(const uint16_t &value)
{
    repeatsRequired = value;

    // Update completion condition to be compatible
    if (1 < repeatsRequired)
    {
        if (FINISH_REPEAT != completionCondition)
            setCompletionCondition(FINISH_REPEAT);
    }
    else if (0 < repeatsRequired)
    {
        if (FINISH_SEQUENCE != completionCondition)
            setCompletionCondition(FINISH_SEQUENCE);
    }
    else
    {
        if (FINISH_LOOP != completionCondition)
            setCompletionCondition(FINISH_LOOP);
    }
}

void SchedulerJob::setRepeatsRemaining(const uint16_t &value)
{
    repeatsRemaining = value;
}

void SchedulerJob::setCapturedFramesMap(const CapturedFramesMap &value)
{
    capturedFramesMap = value;
}

void SchedulerJob::setTargetCoords(const dms &ra, const dms &dec, double djd)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.apparentCoord(static_cast<long double>(J2000), djd);
}

void SchedulerJob::setPositionAngle(double value)
{
    m_PositionAngle = value;
}

void SchedulerJob::reset()
{
    state = SCHEDJOB_IDLE;
    stage = SCHEDSTAGE_IDLE;
    stateTime = getLocalTime();
    lastAbortTime = QDateTime();
    lastErrorTime = QDateTime();
    estimatedTime = -1;
    startupCondition = fileStartupCondition;
    startupTime = fileStartupCondition == START_AT ? startAtTime : QDateTime();

    /* Refresh dawn and dusk for startup date */
    SchedulerModuleState::calculateDawnDusk(startupTime, nextDawn, nextDusk);

    stopTime = QDateTime();
    stopReason.clear();

    /* No change to culmination offset */
    repeatsRemaining = repeatsRequired;
    completedIterations = 0;
    clearProgress();

    clearCache();
}

bool SchedulerJob::decreasingAltitudeOrder(SchedulerJob const *job1, SchedulerJob const *job2, QDateTime const &when)
{
    bool A_is_setting = job1->settingAtStartup;
    double const altA = when.isValid() ?
                        SchedulerUtils::findAltitude(job1->getTargetCoords(), when, &A_is_setting) :
                        job1->altitudeAtStartup;

    bool B_is_setting = job2->settingAtStartup;
    double const altB = when.isValid() ?
                        SchedulerUtils::findAltitude(job2->getTargetCoords(), when, &B_is_setting) :
                        job2->altitudeAtStartup;

    // Sort with the setting target first
    if (A_is_setting && !B_is_setting)
        return true;
    else if (!A_is_setting && B_is_setting)
        return false;

    // If both targets rise or set, sort by decreasing altitude, considering a setting target is prioritary
    return (A_is_setting && B_is_setting) ? altA < altB : altB < altA;
}

bool SchedulerJob::satisfiesAltitudeConstraint(double azimuth, double altitude, QString *altitudeReason) const
{
    if (m_LeadJob != nullptr)
        return m_LeadJob->satisfiesAltitudeConstraint(azimuth, altitude, altitudeReason);

    // Check the mount's altitude constraints.
    if (Options::enableAltitudeLimits() &&
            (altitude < Options::minimumAltLimit() ||
             altitude > Options::maximumAltLimit()))
    {
        if (altitudeReason != nullptr)
        {
            if (altitude < Options::minimumAltLimit())
                *altitudeReason = QString("altitude %1 < mount altitude limit %2")
                                  .arg(altitude, 0, 'f', 1).arg(Options::minimumAltLimit(), 0, 'f', 1);
            else
                *altitudeReason = QString("altitude %1 > mount altitude limit %2")
                                  .arg(altitude, 0, 'f', 1).arg(Options::maximumAltLimit(), 0, 'f', 1);
        }
        return false;
    }
    // Check the global min-altitude constraint.
    if (altitude < getMinAltitude())
    {
        if (altitudeReason != nullptr)
            *altitudeReason = QString("altitude %1 < minAltitude %2").arg(altitude, 0, 'f', 1).arg(getMinAltitude(), 0, 'f', 1);
        return false;
    }
    // Check the artificial horizon.
    if (getHorizon() != nullptr && enforceArtificialHorizon)
        return getHorizon()->isAltitudeOK(azimuth, altitude, altitudeReason);

    return true;
}

bool SchedulerJob::moonConstraintsOK(QDateTime const &when, QString *reason) const
{
    if (moon == nullptr) return true;

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? SchedulerModuleState::getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    CachingDms LST = SchedulerModuleState::getGeo()->GSTtoLST(SchedulerModuleState::getGeo()->LTtoUT(ltWhen).gst());
    moon->updateCoords(&numbers, true, SchedulerModuleState::getGeo()->lat(), &LST, true);
    moon->EquatorialToHorizontal(&LST, SchedulerModuleState::getGeo()->lat());

    bool const separationOK = (getMinMoonSeparation() < 0 || (moon->angularDistanceTo(&o).Degrees() >= getMinMoonSeparation()));
    bool const altitudeOK   = (getMaxMoonAltitude() >= 90 || (moon->alt().Degrees() <= getMaxMoonAltitude()));
    bool result             = separationOK && altitudeOK;

    // set the result string if at least one of the constraints is not met
    if (reason != nullptr && !result)
    {
        if (!separationOK && !altitudeOK)
            *reason = QString("moon separation and altitude");
        else if (!separationOK)
            *reason = QString("moon separation");
        else if (!altitudeOK)
            *reason = QString("moon altitude");
    }

    return result;
}

QDateTime SchedulerJob::calculateNextTime(QDateTime const &when, bool checkIfConstraintsAreMet, int increment,
        QString *reason, bool runningJob, const QDateTime &until) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? SchedulerModuleState::getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Calculate the UT at the argument time
    KStarsDateTime const ut = SchedulerModuleState::getGeo()->LTtoUT(ltWhen);

    auto maxMinute = 1e8;
    if (!runningJob && until.isValid())
        maxMinute = when.secsTo(until) / 60;

    if (maxMinute > 24 * 60)
        maxMinute = 24 * 60;

    // Within the next 24 hours, search when the job target matches the altitude and moon constraints
    for (unsigned int minute = 0; minute < maxMinute; minute += increment)
    {
        KStarsDateTime const ltOffset(ltWhen.addSecs(minute * 60));

        // Is this violating twilight?
        QDateTime nextSuccess;
        if (getEnforceTwilight() && !runsDuringAstronomicalNightTime(ltOffset, &nextSuccess))
        {
            if (checkIfConstraintsAreMet)
            {
                // Change the minute to increment-minutes before next success.
                if (nextSuccess.isValid())
                {
                    const int minutesToSuccess = ltOffset.secsTo(nextSuccess) / 60 - increment;
                    if (minutesToSuccess > 0)
                        minute += minutesToSuccess;
                }
                continue;
            }
            else
            {
                if (reason) *reason = "twilight";
                return ltOffset;
            }
        }

        // Update RA/DEC of the target for the current fraction of the day
        KSNumbers numbers(ltOffset.djd());
        o.updateCoordsNow(&numbers);

        // Compute local sidereal time for the current fraction of the day, calculate altitude
        CachingDms const LST = SchedulerModuleState::getGeo()->GSTtoLST(SchedulerModuleState::getGeo()->LTtoUT(ltOffset).gst());
        o.EquatorialToHorizontal(&LST, SchedulerModuleState::getGeo()->lat());
        double const altitude = o.alt().Degrees();
        double const azimuth = o.az().Degrees();

        bool const altitudeOK = satisfiesAltitudeConstraint(azimuth, altitude, reason);
        if (altitudeOK)
        {
            // Don't test proximity to dawn in this situation, we only cater for altitude here

            // Check moon constraints (moon altitude and distance between target and moon)
            if (!moonConstraintsOK(ltOffset, checkIfConstraintsAreMet ? nullptr : reason))
            {
                if (checkIfConstraintsAreMet)
                    continue;
                else
                    return ltOffset;
            }

            if (checkIfConstraintsAreMet)
                return ltOffset;
        }
        else if (!checkIfConstraintsAreMet)
            return ltOffset;
    }

    return QDateTime();
}

bool SchedulerJob::runsDuringAstronomicalNightTime(const QDateTime &time,
        QDateTime *nextPossibleSuccess) const
{
    if (m_LeadJob != nullptr)
        return m_LeadJob->runsDuringAstronomicalNightTime(time, nextPossibleSuccess);

    // We call this very frequently in the Greedy Algorithm, and the calls
    // below are expensive. Almost all the calls are redundent (e.g. if it's not nighttime
    // now, it's not nighttime in 10 minutes). So, cache the answer and return it if the next
    // call is for a time between this time and the next dawn/dusk (whichever is sooner).

    static QDateTime previousMinDawnDusk, previousTime;
    static GeoLocation const *previousGeo = nullptr;  // A dangling pointer, I suppose, but we never reference it.
    static bool previousAnswer;
    static double previousPreDawnTime = 0;
    static QDateTime nextSuccess;

    // Lock this method because of all the statics
    static std::mutex nightTimeMutex;
    const std::lock_guard<std::mutex> lock(nightTimeMutex);

    // We likely can rely on the previous calculations.
    if (previousTime.isValid() && previousMinDawnDusk.isValid() &&
            time >= previousTime && time < previousMinDawnDusk &&
            SchedulerModuleState::getGeo() == previousGeo &&
            Options::preDawnTime() == previousPreDawnTime)
    {
        if (!previousAnswer && nextPossibleSuccess != nullptr)
            *nextPossibleSuccess = nextSuccess;
        return previousAnswer;
    }
    else
    {
        previousAnswer = runsDuringAstronomicalNightTimeInternal(time, &previousMinDawnDusk, &nextSuccess);
        previousTime = time;
        previousGeo = SchedulerModuleState::getGeo();
        previousPreDawnTime = Options::preDawnTime();
        if (!previousAnswer && nextPossibleSuccess != nullptr)
            *nextPossibleSuccess = nextSuccess;
        return previousAnswer;
    }
}


bool SchedulerJob::runsDuringAstronomicalNightTimeInternal(const QDateTime &time, QDateTime *minDawnDusk,
        QDateTime *nextPossibleSuccess) const
{
    if (m_LeadJob != nullptr)
        return m_LeadJob->runsDuringAstronomicalNightTimeInternal(time, minDawnDusk, nextPossibleSuccess);

    QDateTime t;
    QDateTime nDawn = nextDawn, nDusk = nextDusk;
    if (time.isValid())
    {
        // Can't rely on the pre-computed dawn/dusk if we're giving it an arbitary time.
        SchedulerModuleState::calculateDawnDusk(time, nDawn, nDusk);
        t = time;
    }
    else
    {
        t = startupTime;
    }

    // Calculate the next astronomical dawn time, adjusted with the Ekos pre-dawn offset
    QDateTime const earlyDawn = nDawn.addSecs(-60.0 * abs(Options::preDawnTime()));

    *minDawnDusk = earlyDawn < nDusk ? earlyDawn : nDusk;

    // Dawn and dusk are ordered as the immediate next events following the observation time
    // Thus if dawn comes first, the job startup time occurs during the dusk/dawn interval.
    bool result = nDawn < nDusk && t <= earlyDawn;

    // Return a hint about when it might succeed.
    if (nextPossibleSuccess != nullptr)
    {
        if (result) *nextPossibleSuccess = QDateTime();
        else *nextPossibleSuccess = nDusk;
    }

    return result;
}

void SchedulerJob::setInitialFilter(const QString &value)
{
    m_InitialFilter = value;
}

const QString &SchedulerJob::getInitialFilter() const
{
    return m_InitialFilter;
}

bool SchedulerJob::StartTimeCache::check(const QDateTime &from, const QDateTime &until,
        QDateTime *result, QDateTime *newFrom) const
{
    // Look at the cached results from getNextPossibleStartTime.
    // If the desired 'from' time is in one of them, that is, between computation.from and computation.until,
    // then we can re-use that result (as long as the desired until time is < computation.until).
    foreach (const StartTimeComputation &computation, startComputations)
    {
        if (from >= computation.from &&
                (!computation.until.isValid() || from < computation.until) &&
                (!computation.result.isValid() || from < computation.result))
        {
            if (computation.result.isValid() || until <= computation.until)
            {
                // We have a cached result.
                *result = computation.result;
                *newFrom = QDateTime();
                return true;
            }
            else
            {
                // No cached result, but at least we can constrain the search.
                *result = QDateTime();
                *newFrom = computation.until;
                return true;
            }
        }
    }
    return false;
}

void SchedulerJob::StartTimeCache::clear() const
{
    startComputations.clear();
}

void SchedulerJob::StartTimeCache::add(const QDateTime &from, const QDateTime &until, const QDateTime &result) const
{
    // Manage the cache size.
    if (startComputations.size() > 10)
        startComputations.clear();

    // The getNextPossibleStartTime computation (which calls calculateNextTime) searches ahead at most 24 hours.
    QDateTime endTime;
    if (!until.isValid())
        endTime = from.addSecs(24 * 3600);
    else
    {
        QDateTime oneDay = from.addSecs(24 * 3600);
        if (until > oneDay)
            endTime = oneDay;
        else
            endTime = until;
    }

    StartTimeComputation c;
    c.from = from;
    c.until = endTime;
    c.result = result;
    startComputations.push_back(c);
}

// When can this job start? For now ignores culmination constraint.
QDateTime SchedulerJob::getNextPossibleStartTime(const QDateTime &when, int increment, bool runningJob,
        const QDateTime &until) const
{
    QDateTime ltWhen(
        when.isValid() ? (Qt::UTC == when.timeSpec() ? SchedulerModuleState::getGeo()->UTtoLT(KStarsDateTime(when)) : when)
        : getLocalTime());

    // We do not consider job state here. It is the responsibility of the caller
    // to filter for that, if desired.

    if (!runningJob && START_AT == getFileStartupCondition())
    {
        int secondsFromNow = ltWhen.secsTo(getStartAtTime());
        if (secondsFromNow < -500)
            // We missed it.
            return QDateTime();
        ltWhen = secondsFromNow > 0 ? getStartAtTime() : ltWhen;
    }

    // Can't start if we're past the finish time.
    if (getCompletionCondition() == FINISH_AT)
    {
        const QDateTime &t = getFinishAtTime();
        if (t.isValid() && t < ltWhen)
            return QDateTime(); // return an invalid time.
    }

    if (runningJob)
        return calculateNextTime(ltWhen, true, increment, nullptr, runningJob, until);
    else
    {
        QDateTime result, newFrom;
        if (startTimeCache.check(ltWhen, until, &result, &newFrom))
        {
            if (result.isValid() || !newFrom.isValid())
                return result;
            if (newFrom.isValid())
                ltWhen = newFrom;
        }
        result = calculateNextTime(ltWhen, true, increment, nullptr, runningJob, until);
        result.setTimeZone(ltWhen.timeZone());
        startTimeCache.add(ltWhen, until, result);
        return result;
    }
}

// When will this job end (not looking at capture plan)?
QDateTime SchedulerJob::getNextEndTime(const QDateTime &start, int increment, QString *reason, const QDateTime &until) const
{
    QDateTime ltStart(
        start.isValid() ? (Qt::UTC == start.timeSpec() ? SchedulerModuleState::getGeo()->UTtoLT(KStarsDateTime(start)) : start)
        : getLocalTime());

    // We do not consider job state here. It is the responsibility of the caller
    // to filter for that, if desired.

    if (START_AT == getFileStartupCondition())
    {
        if (getStartAtTime().secsTo(ltStart) < -120)
        {
            // if the file startup time is in the future, then end now.
            // This case probably wouldn't happen in the running code.
            if (reason) *reason = "before start-at time";
            return QDateTime();
        }
        // otherwise, test from now.
    }

    // Can't start if we're past the finish time.
    if (getCompletionCondition() == FINISH_AT)
    {
        const QDateTime &t = getFinishAtTime();
        if (t.isValid() && t < ltStart)
        {
            if (reason) *reason = "end-at time";
            return QDateTime(); // return an invalid time.
        }
        auto result = calculateNextTime(ltStart, false, increment, reason, false, until);
        if (!result.isValid() || result.secsTo(getFinishAtTime()) < 0)
        {
            if (reason) *reason = "end-at time";
            return getFinishAtTime();
        }
        else return result;
    }

    return calculateNextTime(ltStart, false, increment, reason, false, until);
}

namespace
{

QString progressLineLabel(CCDFrameType frameType, const QMap<SequenceJob::PropertyID, QVariant> &properties,
                          bool isDarkFlat)
{
    QString jobTargetName = properties[SequenceJob::SJ_TargetName].toString();
    auto exposure    = properties[SequenceJob::SJ_Exposure].toDouble();
    QString label;

    int precisionRequired = 0;
    double fraction = exposure - fabs(exposure);
    if (fraction > .0001)
    {
        precisionRequired = 1;
        fraction = fraction * 10;
        fraction = fraction - fabs(fraction);
        if (fraction > .0001)
        {
            precisionRequired = 2;
            fraction = fraction * 10;
            fraction = fraction - fabs(fraction);
            if (fraction > .0001)
                precisionRequired = 3;
        }
    }
    if (precisionRequired == 0)
        label += QString("%1s").arg(static_cast<int>(exposure));
    else
        label += QString("%1s").arg(exposure, 0, 'f', precisionRequired);

    if (properties.contains(SequenceJob::SJ_Filter))
    {
        auto filterType = properties[SequenceJob::SJ_Filter].toString();
        if (label.size() > 0) label += " ";
        label += filterType;
    }

    if (isDarkFlat)
    {
        if (label.size() > 0) label += " ";
        label += i18n("DarkFlat");
    }
    else if (frameType != FRAME_LIGHT)
    {
        if (label.size() > 0) label += " ";
        label += (char)frameType;
    }

    return label;
}

QString progressLine(const SchedulerJob::JobProgress &progress)
{
    QString label = progressLineLabel(progress.type, progress.properties, progress.isDarkFlat).append(":");

    const double seconds = progress.numCompleted * progress.properties[SequenceJob::SJ_Exposure].toDouble();
    QString timeStr;
    if (seconds == 0)
        timeStr = "";
    else if (seconds < 60)
        timeStr = QString("%1 %2").arg(static_cast<int>(seconds)).arg(i18n("seconds"));
    else if (seconds < 60 * 60)
        timeStr = QString("%1 %2").arg(seconds / 60.0, 0, 'f', 1).arg(i18n("minutes"));
    else
        timeStr = QString("%1 %3").arg(seconds / 3600.0, 0, 'f', 1).arg(i18n("hours"));

    // Hacky formatting. I tried html and html tables, but the tooltips got narrow boxes.
    // Would be nice to redo with proper formatting, or fixed-width font.
    return QString("%1\t%2 %3 %4")
           .arg(label, -12, ' ')
           .arg(progress.numCompleted, 4)
           .arg(i18n("images"))
           .arg(timeStr);
}
}  // namespace

const QString SchedulerJob::getProgressSummary() const
{
    QString summary;
    for (const auto &p : m_Progress)
    {
        summary.append(progressLine(p));
        summary.append("\n");
    }
    return summary;
}

QJsonObject SchedulerJob::toJson() const
{
    bool is_setting = false;
    double const alt = SchedulerUtils::findAltitude(getTargetCoords(), QDateTime(), &is_setting);

    return
    {
        {"name", name},
        {"pa", m_PositionAngle},
        {"targetRA", getTargetCoords().ra0().Hours()},
        {"targetDEC", getTargetCoords().dec0().Degrees()},
        {"state", state},
        {"stage", stage},
        {"sequenceCount", sequenceCount},
        {"completedCount", completedCount},
        {"minAltitude", minAltitude},
        {"minMoonSeparation", minMoonSeparation},
        {"maxMoonAltitude", maxMoonAltitude},
        {"repeatsRequired", repeatsRequired},
        {"repeatsRemaining", repeatsRemaining},
        {"inSequenceFocus", inSequenceFocus},
        {"startupTime", startupTime.isValid() ? startupTime.toString() : "--"},
        {"completionTime", finishAtTime.isValid() ? finishAtTime.toString() : "--"},
        {"altitude", alt},
        {"altitudeFormatted", m_AltitudeFormatted},
        {"startupFormatted", m_StartupFormatted},
        {"endFormatted", m_EndFormatted},
        {"sequence", sequenceFile.toString() },
    };
}

} // Ekos namespace
