/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "taskaction.h"
#include <QTimer>

namespace Ekos
{

/**
 * @class DelayAction
 * @brief Action to add a time-based delay
 *
 * DelayAction pauses execution for a specified duration. Useful for adding
 * delays between operations, allowing hardware to settle, or rate-limiting.
 */
class DelayAction : public TaskAction
{
        Q_OBJECT

    public:
        enum TimeUnit
        {
            SECONDS,
            MINUTES,
            HOURS
        };
        Q_ENUM(TimeUnit)

        explicit DelayAction(QObject *parent = nullptr);
        ~DelayAction() override = default;

        // Core execution interface
        bool start() override;
        bool abort() override;
        bool isAlreadyDone() override;

        // Configuration
        int duration() const
        {
            return m_duration;
        }
        void setDuration(int duration)
        {
            m_duration = duration;
        }

        TimeUnit unit() const
        {
            return m_unit;
        }
        void setUnit(TimeUnit unit)
        {
            m_unit = unit;
        }

        // Serialization
        QJsonObject toJson() const override;
        static DelayAction* fromJson(const QJsonObject &json, QObject *parent = nullptr);

    private slots:
        void handleDelayComplete();

    private:
        /**
         * @brief Get delay duration in milliseconds
         * @return Delay duration in ms
         */
        int getDurationMs() const;

        int m_duration = 0;
        TimeUnit m_unit = SECONDS;

        QTimer *m_delayTimer = nullptr;
};

} // namespace Ekos
