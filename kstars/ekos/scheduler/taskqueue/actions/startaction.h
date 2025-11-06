/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "taskaction.h"
#include <QTimer>
#include <QDateTime>

namespace Ekos
{

/**
 * @class StartAction
 * @brief Action to control when a task begins execution
 *
 * StartAction determines the start time for a task. It can execute
 * immediately (ASAP) or wait until a scheduled datetime.
 */
class StartAction : public TaskAction
{
        Q_OBJECT

    public:
        enum ScheduleType
        {
            ASAP,           // Start as soon as possible
            SCHEDULED,      // Start at specific datetime
            CONDITIONAL     // Start when condition met (future extension)
        };
        Q_ENUM(ScheduleType)

        explicit StartAction(QObject *parent = nullptr);
        ~StartAction() override = default;

        // Core execution interface
        bool start() override;
        bool abort() override;
        bool isAlreadyDone() override;

        // Configuration
        ScheduleType scheduleType() const
        {
            return m_scheduleType;
        }
        void setScheduleType(ScheduleType type)
        {
            m_scheduleType = type;
        }

        QDateTime scheduledTime() const
        {
            return m_scheduledTime;
        }
        void setScheduledTime(const QDateTime &datetime)
        {
            m_scheduledTime = datetime;
        }

        // Serialization
        QJsonObject toJson() const override;
        static StartAction* fromJson(const QJsonObject &json, QObject *parent = nullptr);

    private slots:
        void handleScheduledTimeReached();
        void checkScheduledTime();

    private:
        /**
         * @brief Get time remaining until scheduled start (in ms)
         * @return Milliseconds until start, or 0 if time has passed
         */
        qint64 getTimeUntilStart() const;

        ScheduleType m_scheduleType = ASAP;
        QDateTime m_scheduledTime;

        QTimer *m_waitTimer = nullptr;
        QTimer *m_checkTimer = nullptr;
};

} // namespace Ekos
