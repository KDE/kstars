/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "schedulermodulestate.h"
#include <ekos_scheduler_debug.h>
#include "schedulerprocess.h"
#include "schedulerjob.h"
#include "kstarsdata.h"

namespace Ekos
{
SchedulerModuleState::SchedulerModuleState() {}

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
} // Ekos namespace
