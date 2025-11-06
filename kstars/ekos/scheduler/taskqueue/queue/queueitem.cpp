/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "queueitem.h"

#include <QJsonObject>
#include <QMetaEnum>
#include <QUuid>

namespace Ekos
{

QueueItem::QueueItem(QObject *parent)
    : QObject(parent)
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_createdTime = QDateTime::currentDateTime();
}

QueueItem::QueueItem(Task *task, QObject *parent)
    : QueueItem(parent)
{
    setTask(task);
}

QueueItem::~QueueItem()
{
    // Task is owned by this QueueItem
    delete m_task;
}

void QueueItem::setTask(Task *task)
{
    if (m_task)
    {
        disconnect(m_task, nullptr, this, nullptr);
        delete m_task;
    }

    m_task = task;

    if (m_task)
    {
        m_task->setParent(this);

        // Connect task signals
        connect(m_task, &Task::statusChanged,
                this, &QueueItem::onTaskStatusChanged);
        connect(m_task, &Task::progress,
                this, &QueueItem::onTaskProgress);
    }
}

QString QueueItem::name() const
{
    return m_task ? m_task->name() : QString();
}

QString QueueItem::device() const
{
    return m_task ? m_task->device() : QString();
}

QString QueueItem::category() const
{
    return m_task ? m_task->category() : QString();
}

void QueueItem::setStatus(Status status)
{
    if (m_status != status)
    {
        m_status = status;

        // Update timestamps
        if (status == RUNNING)
        {
            m_startTime = QDateTime::currentDateTime();
        }
        else if (status == COMPLETED || status == FAILED || status == ABORTED)
        {
            m_endTime = QDateTime::currentDateTime();
        }

        emit statusChanged(status);
    }
}

QString QueueItem::statusString() const
{
    QMetaEnum metaEnum = QMetaEnum::fromType<QueueItem::Status>();
    return QString(metaEnum.valueToKey(m_status));
}

void QueueItem::onTaskStatusChanged(Task::Status taskStatus)
{
    // Map task status to queue item status
    switch (taskStatus)
    {
        case Task::PENDING:
            // Don't change queue item status
            break;
        case Task::RUNNING:
            setStatus(RUNNING);
            break;
        case Task::COMPLETED:
            setStatus(COMPLETED);
            setProgress(100);
            break;
        case Task::FAILED:
            setStatus(FAILED);
            if (m_task)
            {
                setErrorMessage(m_task->errorMessage());
            }
            break;
        case Task::ABORTED:
            setStatus(ABORTED);
            break;
    }
}

void QueueItem::onTaskProgress(const QString &message)
{
    m_progressMessage = message;

    // Calculate progress based on action completion
    if (m_task && m_task->actionCount() > 0)
    {
        int currentIndex = m_task->currentActionIndex();
        if (currentIndex >= 0)
        {
            m_progress = (currentIndex * 100) / m_task->actionCount();
        }
    }

    emit progressUpdated(m_progress, m_progressMessage);
}

QJsonObject QueueItem::toJson() const
{
    QJsonObject json;

    json["id"] = m_id;
    json["status"] = static_cast<int>(m_status);
    json["created_time"] = m_createdTime.toString(Qt::ISODate);

    if (m_scheduledTime.isValid())
    {
        json["scheduled_time"] = m_scheduledTime.toString(Qt::ISODate);
    }

    if (m_startTime.isValid())
    {
        json["start_time"] = m_startTime.toString(Qt::ISODate);
    }

    if (m_endTime.isValid())
    {
        json["end_time"] = m_endTime.toString(Qt::ISODate);
    }

    json["progress"] = m_progress;

    if (!m_progressMessage.isEmpty())
    {
        json["progress_message"] = m_progressMessage;
    }

    if (!m_errorMessage.isEmpty())
    {
        json["error_message"] = m_errorMessage;
    }

    // Serialize task
    if (m_task)
    {
        json["task"] = m_task->toJson();
    }

    return json;
}

bool QueueItem::loadFromJson(const QJsonObject &json)
{
    m_id = json["id"].toString();
    m_status = static_cast<Status>(json["status"].toInt());
    m_createdTime = QDateTime::fromString(json["created_time"].toString(), Qt::ISODate);

    if (json.contains("scheduled_time"))
    {
        m_scheduledTime = QDateTime::fromString(json["scheduled_time"].toString(), Qt::ISODate);
    }

    if (json.contains("start_time"))
    {
        m_startTime = QDateTime::fromString(json["start_time"].toString(), Qt::ISODate);
    }

    if (json.contains("end_time"))
    {
        m_endTime = QDateTime::fromString(json["end_time"].toString(), Qt::ISODate);
    }

    m_progress = json["progress"].toInt();
    m_progressMessage = json["progress_message"].toString();
    m_errorMessage = json["error_message"].toString();

    // Load task
    if (json.contains("task"))
    {
        Task *task = new Task(this);
        if (task->loadFromJson(json["task"].toObject()))
        {
            setTask(task);
        }
        else
        {
            delete task;
            return false;
        }
    }

    return true;
}

} // namespace Ekos
