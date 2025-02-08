/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "schedulermodulestate.h"
#include "schedulerjob.h"
#include <ekos_scheduler_debug.h>
#include "schedulerprocess.h"
#include "schedulerjob.h"
#include "kstarsdata.h"
#include "ksalmanac.h"
#include "Options.h"

#define MAX_FAILURE_ATTEMPTS 5

namespace Ekos
{
// constants definition
QDateTime SchedulerModuleState::m_Dawn, SchedulerModuleState::m_Dusk, SchedulerModuleState::m_PreDawnDateTime;
GeoLocation *SchedulerModuleState::storedGeo = nullptr;

SchedulerModuleState::SchedulerModuleState() {}

void SchedulerModuleState::init()
{
    // This is needed to get wakeupScheduler() to call start() and startup,
    // instead of assuming it is already initialized (if preemptiveShutdown was not set).
    // The time itself is not used.
    enablePreemptiveShutdown(SchedulerModuleState::getLocalTime());

    setIterationSetup(false);
    setupNextIteration(RUN_WAKEUP, 10);
}

void SchedulerModuleState::setCurrentProfile(const QString &newName, bool signal)
{
    bool changed = (newName != m_currentProfile);

    if (m_profiles.contains(newName))
        m_currentProfile = newName;
    else
    {
        changed = (m_currentProfile !=  m_profiles.first());
        m_currentProfile = m_profiles.first();
    }
    // update the UI
    if (signal && changed)
        emit currentProfileChanged();
}

void SchedulerModuleState::updateProfiles(const QStringList &newProfiles)
{
    QString selected = currentProfile();
    // Default profile is always the first one
    QStringList allProfiles(i18n("Default"));
    allProfiles.append(newProfiles);

    m_profiles = allProfiles;
    // ensure that the selected profile still exists
    setCurrentProfile(selected, false);
    emit profilesChanged();
}

void SchedulerModuleState::setActiveJob(SchedulerJob *newActiveJob)
{
    m_activeJob = newActiveJob;
}

QList<SchedulerJob *> SchedulerModuleState::leadJobs()
{
    QList<SchedulerJob *> result;
    for (auto job : jobs())
        if (job->isLead())
            result.append(job);

    return result;
}

QList<SchedulerJob *> SchedulerModuleState::followerJobs()
{
    QList<SchedulerJob *> result;
    for (auto job : jobs())
        if (!job->isLead())
            result.append(job);

    return result;
}

void SchedulerModuleState::updateJobStage(SchedulerJobStage stage)
{
    if (activeJob() == nullptr)
    {
        emit jobStageChanged(SCHEDSTAGE_IDLE);
    }
    else
    {
        activeJob()->setStage(stage);
        emit jobStageChanged(stage);
    }
}

QJsonArray SchedulerModuleState::getJSONJobs()
{
    QJsonArray jobArray;

    for (const auto &oneJob : jobs())
        jobArray.append(oneJob->toJson());

    return jobArray;
}

void SchedulerModuleState::setSchedulerState(const SchedulerState &newState)
{
    m_schedulerState = newState;
    emit schedulerStateChanged(newState);
}

void SchedulerModuleState::setCurrentPosition(int newCurrentPosition)
{
    m_currentPosition = newCurrentPosition;
    emit currentPositionChanged(newCurrentPosition);
}

void SchedulerModuleState::setStartupState(StartupState state)
{
    if (m_startupState != state)
    {
        m_startupState = state;
        emit startupStateChanged(state);
    }
}

void SchedulerModuleState::setShutdownState(ShutdownState state)
{
    if (m_shutdownState != state)
    {
        m_shutdownState = state;
        emit shutdownStateChanged(state);
    }
}

void SchedulerModuleState::setParkWaitState(ParkWaitState state)
{
    if (m_parkWaitState != state)
    {
        m_parkWaitState = state;
        emit parkWaitStateChanged(state);
    }
}

bool SchedulerModuleState::removeJob(const int currentRow)
{
    /* Don't remove a row that is not selected */
    if (currentRow < 0)
        return false;

    /* Grab the job currently selected */
    SchedulerJob * const job = jobs().at(currentRow);

    // Can't delete the currently running job
    if (job == m_activeJob)
    {
        emit newLog(i18n("Cannot delete currently running job '%1'.", job->getName()));
        return false;
    }
    else if (job == nullptr || (activeJob() == nullptr && schedulerState() != SCHEDULER_IDLE))
    {
        // Don't allow delete--worried that we're about to schedule job that's being deleted.
        emit newLog(i18n("Cannot delete job. Scheduler state: %1",
                         getSchedulerStatusString(schedulerState(), true)));
        return false;
    }

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is being deleted.").arg(job->getName()).arg(currentRow + 1);

    // if it was a lead job, re-arrange its follower (if necessary)
    if (job->isLead() && job->followerJobs().count() > 0)
    {
        // search the new lead
        SchedulerJob *newLead = findLead(currentRow - 1);
        // if it does not exist, do not allow job deletion
        if (newLead == nullptr)
        {
            return false;
        }
        else
        {
            // set the new lead and add the follower jobs to the new lead
            for (auto follower : job->followerJobs())
            {
                follower->setLeadJob(newLead);
                newLead->followerJobs().append(follower);
            }
        }
    }
    /* Remove the job object */
    mutlableJobs().removeOne(job);

    delete (job);

    // Reduce the current position if the last element has been deleted
    if (currentPosition() >= jobs().count())
        setCurrentPosition(jobs().count() - 1);

    setDirty(true);
    // success
    return true;
}

void SchedulerModuleState::refreshFollowerLists()
{
    // clear the follower lists
    for (auto job : m_jobs)
        job->followerJobs().clear();

    // iterate over all jobs and append the follower to its lead
    for (auto job : m_jobs)
    {
        SchedulerJob *lead = job->leadJob();
        if (job->isLead() == false && lead != nullptr)
        {
            lead->followerJobs().append(job);
            lead->updateSharedFollowerAttributes();
        }
    }
}

SchedulerJob *SchedulerModuleState::findLead(int position, bool upward)
{
    auto start = std::min(position, static_cast<int>(jobs().count()));

    if (upward)
    {
        for (int i = start; i >= 0; i--)
            if (jobs().at(i)->isLead())
                return jobs().at(i);
    }
    else
    {
        for (int i = start; i < jobs().count(); i++)
            if (jobs().at(i)->isLead())
                return jobs().at(i);
    }

    // nothing found
    return nullptr;
}

void SchedulerModuleState::enablePreemptiveShutdown(const QDateTime &wakeupTime)
{
    m_preemptiveShutdownWakeupTime = wakeupTime;
}

void SchedulerModuleState::disablePreemptiveShutdown()
{
    m_preemptiveShutdownWakeupTime = QDateTime();
}

const QDateTime &SchedulerModuleState::preemptiveShutdownWakeupTime() const
{
    return m_preemptiveShutdownWakeupTime;
}

bool SchedulerModuleState::preemptiveShutdown() const
{
    return m_preemptiveShutdownWakeupTime.isValid();
}

void SchedulerModuleState::setEkosState(EkosState state)
{
    if (m_ekosState != state)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "EKOS state changed from" << m_ekosState << "to" << state;
        m_ekosState = state;
        emit ekosStateChanged(state);
    }
}

bool SchedulerModuleState::increaseEkosConnectFailureCount()
{
    return (++m_ekosConnectFailureCount <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseParkingCapFailureCount()
{
    return (++m_parkingCapFailureCount <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseParkingMountFailureCount()
{
    return (++m_parkingMountFailureCount <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseParkingDomeFailureCount()
{
    return (++m_parkingDomeFailureCount <= MAX_FAILURE_ATTEMPTS);
}

void SchedulerModuleState::resetFailureCounters()
{
    resetIndiConnectFailureCount();
    resetEkosConnectFailureCount();
    resetFocusFailureCount();
    resetGuideFailureCount();
    resetAlignFailureCount();
    resetCaptureFailureCount();
}

bool SchedulerModuleState::increaseIndiConnectFailureCount()
{
    return (++m_indiConnectFailureCount <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseCaptureFailureCount()
{
    return (++m_captureFailureCount <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseFocusFailureCount(const QString &trainname)
{
    return (++m_focusFailureCount[trainname] <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseAllFocusFailureCounts()
{
    bool result = true;

    // if one of the counters is beyond the threshold, we return false.
    for (QMap<QString, bool>::const_iterator it = m_autofocusCompleted.cbegin(); it != m_autofocusCompleted.cend(); it++)
        result &= increaseFocusFailureCount(it.key());

    return result;
}

bool SchedulerModuleState::autofocusCompleted(const QString &trainname) const
{
    if (!trainname.isEmpty())
        return m_autofocusCompleted[trainname];
    else
        return autofocusCompleted();
}

void SchedulerModuleState::setAutofocusCompleted(const QString &trainname, bool value)
{
    if (!trainname.isEmpty())
        m_autofocusCompleted[trainname] = value;
    else
        m_autofocusCompleted.clear();
}

bool SchedulerModuleState::autofocusCompleted() const
{
    if (m_autofocusCompleted.isEmpty())
        return false;

    for (QMap<QString, bool>::const_iterator it = m_autofocusCompleted.cbegin(); it != m_autofocusCompleted.cend(); it++)
    {
        if (it.value() == false)
            return false;
    }
    // all are completed
    return true;
}

bool SchedulerModuleState::increaseGuideFailureCount()
{
    return (++m_guideFailureCount <= MAX_FAILURE_ATTEMPTS);
}

bool SchedulerModuleState::increaseAlignFailureCount()
{
    return (++m_alignFailureCount <= MAX_FAILURE_ATTEMPTS);
}

void SchedulerModuleState::setIndiState(INDIState state)
{
    if (m_indiState != state)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "INDI state changed from" << m_indiState << "to" << state;
        m_indiState = state;
        emit indiStateChanged(state);
    }
}

qint64 SchedulerModuleState::getCurrentOperationMsec() const
{
    if (!currentOperationTimeStarted) return 0;
    return currentOperationTime.msecsTo(KStarsData::Instance()->ut());
}

void SchedulerModuleState::startCurrentOperationTimer()
{
    currentOperationTimeStarted = true;
    currentOperationTime = KStarsData::Instance()->ut();
}

void SchedulerModuleState::cancelGuidingTimer()
{
    m_restartGuidingInterval = -1;
    m_restartGuidingTime = KStarsDateTime();
}

bool SchedulerModuleState::isGuidingTimerActive()
{
    return (m_restartGuidingInterval > 0 &&
            m_restartGuidingTime.msecsTo(KStarsData::Instance()->ut()) >= 0);
}

void SchedulerModuleState::startGuidingTimer(int milliseconds)
{
    m_restartGuidingInterval = milliseconds;
    m_restartGuidingTime = KStarsData::Instance()->ut();
}

// Allows for unit testing of static Scheduler methods,
// as can't call KStarsData::Instance() during unit testing.
KStarsDateTime *SchedulerModuleState::storedLocalTime = nullptr;
KStarsDateTime SchedulerModuleState::getLocalTime()
{
    if (hasLocalTime())
        return *storedLocalTime;
    return KStarsData::Instance()->geo()->UTtoLT(KStarsData::Instance()->clock()->utc());
}

void SchedulerModuleState::calculateDawnDusk(const QDateTime &when, QDateTime &nDawn, QDateTime &nDusk)
{
    QDateTime startup = when;

    if (!startup.isValid())
        startup = getLocalTime();

    // Our local midnight - the KStarsDateTime date+time constructor is safe for local times
    // Exact midnight seems unreliable--offset it by a minute.
    KStarsDateTime midnight(startup.date(), QTime(0, 1), Qt::LocalTime);

    QDateTime dawn = startup, dusk = startup;

    // Loop dawn and dusk calculation until the events found are the next events
    for ( ; dawn <= startup || dusk <= startup ; midnight = midnight.addDays(1))
    {
        // KSAlmanac computes the closest dawn and dusk events from the local sidereal time corresponding to the midnight argument

#if 0
        KSAlmanac const ksal(midnight, getGeo());
        // If dawn is in the past compared to this observation, fetch the next dawn
        if (dawn <= startup)
            dawn = getGeo()->UTtoLT(ksal.getDate().addSecs((ksal.getDawnAstronomicalTwilight() * 24.0 + Options::dawnOffset()) *
                                    3600.0));
        // If dusk is in the past compared to this observation, fetch the next dusk
        if (dusk <= startup)
            dusk = getGeo()->UTtoLT(ksal.getDate().addSecs((ksal.getDuskAstronomicalTwilight() * 24.0 + Options::duskOffset()) *
                                    3600.0));
#else
        // Creating these almanac instances seems expensive.
        static QMap<QString, KSAlmanac const * > almanacMap;
        const QString key = QString("%1 %2 %3").arg(midnight.toString()).arg(getGeo()->lat()->Degrees()).arg(
                                getGeo()->lng()->Degrees());
        KSAlmanac const * ksal = almanacMap.value(key, nullptr);
        if (ksal == nullptr)
        {
            if (almanacMap.size() > 5)
            {
                // don't allow this to grow too large.
                qDeleteAll(almanacMap);
                almanacMap.clear();
            }
            ksal = new KSAlmanac(midnight, getGeo());
            almanacMap[key] = ksal;
        }

        // If dawn is in the past compared to this observation, fetch the next dawn
        if (dawn <= startup)
            dawn = getGeo()->UTtoLT(ksal->getDate().addSecs((ksal->getDawnAstronomicalTwilight() * 24.0 + Options::dawnOffset()) *
                                    3600.0));

        // If dusk is in the past compared to this observation, fetch the next dusk
        if (dusk <= startup)
            dusk = getGeo()->UTtoLT(ksal->getDate().addSecs((ksal->getDuskAstronomicalTwilight() * 24.0 + Options::duskOffset()) *
                                    3600.0));
#endif
    }

    // Now we have the next events:
    // - if dawn comes first, observation runs during the night
    // - if dusk comes first, observation runs during the day
    nDawn = dawn;
    nDusk = dusk;
}

void SchedulerModuleState::calculateDawnDusk()
{
    calculateDawnDusk(QDateTime(), m_Dawn, m_Dusk);

    m_PreDawnDateTime = m_Dawn.addSecs(-60.0 * abs(Options::preDawnTime()));
    emit updateNightTime();
}

const GeoLocation *SchedulerModuleState::getGeo()
{
    if (hasGeo())
        return storedGeo;
    return KStarsData::Instance()->geo();
}

bool SchedulerModuleState::hasGeo()
{
    return storedGeo != nullptr;
}

void SchedulerModuleState::setupNextIteration(SchedulerTimerState nextState)
{
    setupNextIteration(nextState, m_UpdatePeriodMs);
}

void SchedulerModuleState::setupNextIteration(SchedulerTimerState nextState, int milliseconds)
{
    if (iterationSetup())
    {
        qCDebug(KSTARS_EKOS_SCHEDULER)
                << QString("Multiple setupNextIteration calls: current %1 %2, previous %3 %4")
                .arg(nextState).arg(milliseconds).arg(timerState()).arg(timerInterval());
    }
    setTimerState(nextState);
    // check if setup is called from a thread outside of the iteration timer thread
    if (iterationTimer().isActive())
    {
        // restart the timer to ensure the correct startup delay
        int remaining = iterationTimer().remainingTime();
        iterationTimer().stop();
        setTimerInterval(std::max(0, milliseconds - remaining));
        iterationTimer().start(timerInterval());
    }
    else
    {
        // setup called from inside the iteration timer thread
        setTimerInterval(milliseconds);
    }
    setIterationSetup(true);
}

uint SchedulerModuleState::maxFailureAttempts()
{
    return MAX_FAILURE_ATTEMPTS;
}

void SchedulerModuleState::clearLog()
{
    logText().clear();
    emit newLog(QString());
}

bool SchedulerModuleState::checkRepeatSequence()
{
    return (!Options::rememberJobProgress() && Options::schedulerRepeatEverything() &&
            (Options::schedulerExecutionSequencesLimit() == 0
             || sequenceExecutionCounter()) < Options::schedulerExecutionSequencesLimit());
}
} // Ekos namespace
