/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../task.h"

#include <QObject>
#include <QString>
#include <QDateTime>

namespace Ekos
{

/**
 * @class QueueItem
 * @brief Wraps a Task with queue-specific metadata
 *
 * QueueItem represents a task within the execution queue, adding:
 * - Queue position tracking
 * - Execution timestamps
 * - Scheduling information
 * - Status history
 */
class QueueItem : public QObject
{
        Q_OBJECT

    public:
        enum Status
        {
            PENDING,      ///< Waiting to execute
            SCHEDULED,    ///< Scheduled for future execution
            RUNNING,      ///< Currently executing
            COMPLETED,    ///< Finished successfully
            FAILED,       ///< Failed to complete
            ABORTED,      ///< Manually stopped
            SKIPPED       ///< Skipped due to conditions
        };
        Q_ENUM(Status)

        explicit QueueItem(QObject *parent = nullptr);
        explicit QueueItem(Task *task, QObject *parent = nullptr);
        ~QueueItem() override;

        // Task access
        Task *task() const
        {
            return m_task;
        }
        void setTask(Task *task);

        // Metadata
        QString id() const
        {
            return m_id;
        }
        void setId(const QString &id)
        {
            m_id = id;
        }

        QString name() const;
        QString device() const;
        QString category() const;

        // Status
        Status status() const
        {
            return m_status;
        }
        void setStatus(Status status);
        QString statusString() const;

        // Timestamps
        QDateTime createdTime() const
        {
            return m_createdTime;
        }
        QDateTime scheduledTime() const
        {
            return m_scheduledTime;
        }
        void setScheduledTime(const QDateTime &time)
        {
            m_scheduledTime = time;
        }

        QDateTime startTime() const
        {
            return m_startTime;
        }
        QDateTime endTime() const
        {
            return m_endTime;
        }

        // Progress
        int progress() const
        {
            return m_progress;
        }
        void setProgress(int percent)
        {
            m_progress = percent;
        }

        QString progressMessage() const
        {
            return m_progressMessage;
        }

        // Error handling
        QString errorMessage() const
        {
            return m_errorMessage;
        }
        void setErrorMessage(const QString &msg)
        {
            m_errorMessage = msg;
        }

        // Serialization
        QJsonObject toJson() const;
        bool loadFromJson(const QJsonObject &json);

    signals:
        void statusChanged(Status newStatus);
        void progressUpdated(int percent, const QString &message);

    private slots:
        void onTaskStatusChanged(Task::Status taskStatus);
        void onTaskProgress(const QString &message);

    private:
        QString m_id;
        Task *m_task = nullptr;
        Status m_status = PENDING;

        // Timestamps
        QDateTime m_createdTime;
        QDateTime m_scheduledTime;
        QDateTime m_startTime;
        QDateTime m_endTime;

        // Progress tracking
        int m_progress = 0;
        QString m_progressMessage;
        QString m_errorMessage;
};

} // namespace Ekos
