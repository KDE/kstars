/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "taskaction.h"
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

    private slots:
        void checkCompletion();
        void handleTimeout();

    private slots:
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

        QString m_element;
        QVariant m_value;
        bool m_waitForCompletion = true;

        QTimer *m_completionTimer = nullptr;
        QTimer *m_timeoutTimer = nullptr;

        // For tracking state during execution
        int m_pollInterval = 1000; // Poll every 1 second
};

} // namespace Ekos
