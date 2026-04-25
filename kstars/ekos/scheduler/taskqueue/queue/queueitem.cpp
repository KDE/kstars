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
    delete m_task.data();
}

void QueueItem::setTask(Task *task)
{
    if (m_task.data() == task)
        return;

    if (m_task)
    {
        Task *oldTask = m_task.data();
        disconnect(oldTask, nullptr, this, nullptr);
        delete oldTask;
    }

    m_task = task;

    if (m_task)
    {
        Task *newTask = m_task.data();
        newTask->setParent(this);

        // Connect task signals
        connect(newTask, &Task::statusChanged,
                this, &QueueItem::onTaskStatusChanged);
        connect(newTask, &Task::progress,
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

        Q_EMIT statusChanged(status);
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

    Q_EMIT progressUpdated(m_progress, m_progressMessage);
}

void QueueItem::reset()
{
    // Reset item status and progress
    m_status = PENDING;
    m_progress = 0;
    m_progressMessage.clear();
    m_errorMessage.clear();

    // Clear execution timestamps
    m_startTime = QDateTime();
    m_endTime = QDateTime();
    m_scheduledTime = QDateTime();

    // Reset the underlying task and all its actions
    if (m_task)
    {
        m_task->reset();
    }
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
    // Validate the replacement task first so failed loads leave this item unchanged.
    const QJsonValue taskValue = json["task"];
    if (!taskValue.isObject())
        return false;

    Task *task = new Task;
    if (!task->loadFromJson(taskValue.toObject()))
    {
        delete task;
        return false;
    }

    m_id = json["id"].toString();
    // Reset status to PENDING when loading - tasks should be re-runnable
    m_status = PENDING;
    m_createdTime = QDateTime::fromString(json["created_time"].toString(), Qt::ISODate);

    // Clear execution timestamps since we're starting fresh
    m_scheduledTime = QDateTime();
    m_startTime = QDateTime();
    m_endTime = QDateTime();

    // Reset progress since we're starting fresh
    m_progress = 0;
    m_progressMessage.clear();
    m_errorMessage.clear();

    setTask(task);

    return true;
}

} // namespace Ekos
