/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "delayaction.h"

#include <QJsonObject>

namespace Ekos
{

DelayAction::DelayAction(QObject *parent)
    : TaskAction(parent)
{
    m_type = DELAY;
    m_delayTimer = new QTimer(this);
    m_delayTimer->setSingleShot(true);

    connect(m_delayTimer, &QTimer::timeout, this, &DelayAction::handleDelayComplete);
}

bool DelayAction::start()
{
    m_currentRetry = 0;

    setStatus(RUNNING);
    emit started();

    int durationMs = getDurationMs();

    QString unitStr;
    switch (m_unit)
    {
        case SECONDS:
            unitStr = "seconds";
            break;
        case MINUTES:
            unitStr = "minutes";
            break;
        case HOURS:
            unitStr = "hours";
            break;
    }

    emit progress(QString("Delaying for %1 %2").arg(m_duration).arg(unitStr));

    // Start the delay timer
    m_delayTimer->start(durationMs);

    return true;
}

bool DelayAction::abort()
{
    if (m_delayTimer && m_delayTimer->isActive())
        m_delayTimer->stop();

    setStatus(ABORTED);
    return true;
}

bool DelayAction::isAlreadyDone()
{
    // Delays are never already done - they always need to execute
    return false;
}

int DelayAction::getDurationMs() const
{
    switch (m_unit)
    {
        case SECONDS:
            return m_duration * 1000;
        case MINUTES:
            return m_duration * 60 * 1000;
        case HOURS:
            return m_duration * 60 * 60 * 1000;
        default:
            return 0;
    }
}

void DelayAction::handleDelayComplete()
{
    emit progress("Delay completed");
    setStatus(COMPLETED);
    emit completed();
}

QJsonObject DelayAction::toJson() const
{
    QJsonObject json = TaskAction::toJson();
    json["duration"] = m_duration;
    json["unit"] = static_cast<int>(m_unit);
    return json;
}

DelayAction* DelayAction::fromJson(const QJsonObject &json, QObject *parent)
{
    DelayAction *action = new DelayAction(parent);

    action->setDuration(json["duration"].toInt());
    action->setUnit(static_cast<TimeUnit>(json["unit"].toInt()));

    return action;
}

} // namespace Ekos
