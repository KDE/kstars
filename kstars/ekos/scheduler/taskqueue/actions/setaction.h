/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "taskaction.h"
#include <QElapsedTimer>
#include <QTimer>
#include <indiproperty.h>

class QDBusInterface;

namespace Ekos
{

/**
 * @class SetAction
 * @brief Action to set an INDI property element to a specific value
 *
 * SetAction sends a command to set an INDI property and optionally waits for
 * the property state to become OK. It supports idempotency checking to avoid
 * redundant operations.
 */
class SetAction : public TaskAction
{
        Q_OBJECT

    public:
        explicit SetAction(QObject *parent = nullptr);
        ~SetAction() override = default;

        // Core execution interface
        bool start() override;
        bool abort() override;
        bool isAlreadyDone() override;

        // Configuration
        QString element() const
        {
            return m_element;
        }
        void setElement(const QString &element)
        {
            m_element = element;
        }

        QVariant value() const
        {
            return m_value;
        }
        void setValue(const QVariant &value)
        {
            m_value = value;
        }

        bool waitForCompletion() const
        {
            return m_waitForCompletion;
        }
        void setWaitForCompletion(bool wait)
        {
            m_waitForCompletion = wait;
        }

        // Serialization
        QJsonObject toJson() const override;
        static SetAction* fromJson(const QJsonObject &json, QObject *parent = nullptr);

        /** @brief How the property state seen after the SET command was sent is taken */
        enum class AckResult
        {
            Wait,       ///< No answer from the device yet, keep waiting
            Accepted,   ///< The device accepted the command
            Refused     ///< The device refused the command
        };

        /**
         * @brief Decide whether a property state is the device's answer to the SET command
         *
         * When KStars sends a property, the INDI client sets its own copy of the property to Busy before the device
         * answers. Property updates reach the action through queued signals that read this shared copy when they
         * run, so a Busy state seen shortly after the send can be the client's own state and not an answer. A
         * refusal (Alert) normally arrives within milliseconds, so Busy only counts as accepted once @p settleMs
         * have passed since the send. Ok and Idle can only come from the device, because the send left the
         * property at Busy.
         * @param state property state seen now
         * @param msSinceSend milliseconds since the SET command was sent
         * @param settleMs time after the send from which a Busy state counts as accepted
         * @return Refused for Alert, Accepted for Ok and Idle, Accepted for Busy from @p settleMs on, else Wait
         */
        static AckResult evaluateAnswer(IPState state, qint64 msSinceSend, qint64 settleMs);

        /** @brief Time after the send (ms) from which a Busy state counts as the device's answer */
        static constexpr qint64 ACK_SETTLE_MS = 2000;
        /** @brief Delay (ms) before the SET command is sent again after the device refused it */
        static constexpr int RETRY_DELAY_MS = 3000;

    private Q_SLOTS:
        void checkCompletion();
        void handleTimeout();

    private Q_SLOTS:
        /**
         * @brief Handle property updates from device
         * @param prop Updated property
         */
        void onPropertyUpdated(INDI::Property prop);

    private:
        /**
         * @brief Send the actual SET command via INDI
         * @return true if command was sent successfully
         */
        bool sendSetCommand();

        /**
         * @brief Check if the property already has the target value
         * @return true if property is already set correctly
         */
        bool checkCurrentValue();

        /**
         * @brief Complete, retry or keep waiting depending on the property state seen after the send
         * @param state property state seen now
         * @param settleMs time after the send from which a Busy state counts as accepted
         */
        void handleAnswer(IPState state, qint64 settleMs);

        QString m_element;
        QVariant m_value;
        bool m_waitForCompletion = true;

        QTimer *m_completionTimer = nullptr;
        QTimer *m_timeoutTimer = nullptr;
        // Sends the command again after the device refused it
        QTimer *m_retryTimer = nullptr;
        // Time since the SET command was sent
        QElapsedTimer m_sentTimer;

        // For tracking state during execution
        int m_pollInterval = 1000; // Poll every 1 second
};

} // namespace Ekos
