/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "startaction.h"

#include <QJsonObject>
#include <QDateTime>

namespace Ekos
{

StartAction::StartAction(QObject *parent)
    : TaskAction(parent)
{
    m_type = START;
    m_waitTimer = new QTimer(this);
    m_waitTimer->setSingleShot(true);

    m_checkTimer = new QTimer(this);

    connect(m_waitTimer, &QTimer::timeout, this, &StartAction::handleScheduledTimeReached);
    connect(m_checkTimer, &QTimer::timeout, this, &StartAction::checkScheduledTime);
}

bool StartAction::start()
{
    m_currentRetry = 0;

    setStatus(RUNNING);
    emit started();

    if (m_scheduleType == ASAP)
    {
        // Start immediately
        emit progress("Starting immediately");
        setStatus(COMPLETED);
        emit completed();
        return true;
    }
    else if (m_scheduleType == SCHEDULED)
    {
        // Check if scheduled time is valid
        if (!m_scheduledTime.isValid())
        {
            setErrorMessage("Invalid scheduled time");
            setStatus(FAILED);
            emit failed(m_errorMessage);
            return false;
        }

        qint64 msUntilStart = getTimeUntilStart();

        if (msUntilStart <= 0)
        {
            // Scheduled time has already passed
            emit progress("Scheduled time has passed, starting now");
            setStatus(COMPLETED);
            emit completed();
            return true;
        }

        // Wait until scheduled time
        emit progress(QString("Waiting until %1").arg(m_scheduledTime.toString(Qt::ISODate)));

        // If wait time is very long, use a check timer to update progress
        if (msUntilStart > 60000)  // More than 1 minute
        {
            m_checkTimer->start(10000);  // Check every 10 seconds
        }

        m_waitTimer->start(static_cast<int>(msUntilStart));
        return true;
    }

    // Unknown schedule type
    setErrorMessage("Unknown schedule type");
    setStatus(FAILED);
    emit failed(m_errorMessage);
    return false;
}

bool StartAction::abort()
{
    if (m_waitTimer && m_waitTimer->isActive())
        m_waitTimer->stop();
    if (m_checkTimer && m_checkTimer->isActive())
        m_checkTimer->stop();

    setStatus(ABORTED);
    return true;
}

bool StartAction::isAlreadyDone()
{
    // START actions are never already done
    return false;
}

qint64 StartAction::getTimeUntilStart() const
{
    if (!m_scheduledTime.isValid())
        return 0;

    QDateTime now = QDateTime::currentDateTime();
    return now.msecsTo(m_scheduledTime);
}

void StartAction::handleScheduledTimeReached()
{
    if (m_checkTimer && m_checkTimer->isActive())
        m_checkTimer->stop();

    emit progress("Scheduled time reached");
    setStatus(COMPLETED);
    emit completed();
}

void StartAction::checkScheduledTime()
{
    qint64 msRemaining = getTimeUntilStart();

    if (msRemaining <= 0)
    {
        // Time has arrived
        handleScheduledTimeReached();
    }
    else
    {
        // Update progress message
        int minutesRemaining = static_cast<int>(msRemaining / 60000);
        if (minutesRemaining > 60)
        {
            int hoursRemaining = minutesRemaining / 60;
            emit progress(QString("Waiting - %1 hours remaining").arg(hoursRemaining));
        }
        else if (minutesRemaining > 0)
        {
            emit progress(QString("Waiting - %1 minutes remaining").arg(minutesRemaining));
        }
        else
        {
            int secondsRemaining = static_cast<int>(msRemaining / 1000);
            emit progress(QString("Waiting - %1 seconds remaining").arg(secondsRemaining));
        }
    }
}

QJsonObject StartAction::toJson() const
{
    QJsonObject json = TaskAction::toJson();
    json["schedule_type"] = static_cast<int>(m_scheduleType);
    if (m_scheduledTime.isValid())
    {
        json["scheduled_time"] = m_scheduledTime.toString(Qt::ISODate);
    }
    return json;
}

StartAction* StartAction::fromJson(const QJsonObject &json, QObject *parent)
{
    StartAction *action = new StartAction(parent);

    action->setScheduleType(static_cast<ScheduleType>(json["schedule_type"].toInt()));

    if (json.contains("scheduled_time"))
    {
        QDateTime dt = QDateTime::fromString(json["scheduled_time"].toString(), Qt::ISODate);
        action->setScheduledTime(dt);
    }

    return action;
}

} // namespace Ekos
