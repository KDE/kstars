/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QSharedPointer>
#include <QMetaObject>

namespace ISD
{
class GenericDevice;
}

namespace Ekos
{

/**
 * @class TaskAction
 * @brief Base class for all task queue actions
 *
 * TaskAction represents a single operation within a task. Actions execute sequentially
 * within a task and can be of different types: SET, EVALUATE, DELAY, START, or SCRIPT.
 */
class TaskAction : public QObject
{
        Q_OBJECT

    public:
        /** @brief Action types supported by the task queue system */
        enum Type
        {
            SET,        ///< Set an INDI property value
            EVALUATE,   ///< Check if a condition is met (with polling)
            DELAY,      ///< Wait for a specified time period
            START,      ///< Schedule when the task should begin
            SCRIPT      ///< Execute an external script
        };
        Q_ENUM(Type)

        /** @brief Action execution status */
        enum Status
        {
            PENDING,    ///< Action has not started yet
            RUNNING,    ///< Action is currently executing
            COMPLETED,  ///< Action completed successfully
            FAILED,     ///< Action failed after all retries
            ABORTED     ///< Action was manually aborted
        };
        Q_ENUM(Status)

        /** @brief Actions to take when an action fails */
        enum FailureAction
        {
            ABORT_QUEUE,        ///< Stop entire queue execution
            CONTINUE,           ///< Log error but continue to next action
            SKIP_TO_NEXT_TASK   ///< Skip remaining actions in current task
        };
        Q_ENUM(FailureAction)

        explicit TaskAction(QObject *parent = nullptr);
        virtual ~TaskAction();

        // Core execution interface - must be implemented by subclasses
        /**
         * @brief Start executing the action
         * @return true if action started successfully, false otherwise
         */
        virtual bool start() = 0;

        /**
         * @brief Abort the currently running action
         * @return true if abort was successful, false otherwise
         */
        virtual bool abort() = 0;

        /**
         * @brief Check if action is already completed (idempotency check)
         * @return true if action doesn't need to be executed, false otherwise
         */
        virtual bool isAlreadyDone()
        {
            return false;
        }

        // Type and status
        Type type() const
        {
            return m_type;
        }
        Status status() const
        {
            return m_status;
        }
        QString statusString() const;
        QString typeString() const;

        // Configuration
        QString device() const
        {
            return m_device;
        }
        void setDevice(const QString &device)
        {
            m_device = device;
        }

        QString property() const
        {
            return m_property;
        }
        void setProperty(const QString &property)
        {
            m_property = property;
        }

        int timeout() const
        {
            return m_timeout;
        }
        void setTimeout(int seconds)
        {
            m_timeout = seconds;
        }

        int retries() const
        {
            return m_retries;
        }
        void setRetries(int count)
        {
            m_retries = count;
        }

        FailureAction failureAction() const
        {
            return m_failureAction;
        }
        void setFailureAction(FailureAction action)
        {
            m_failureAction = action;
        }

        // Error information
        QString errorMessage() const
        {
            return m_errorMessage;
        }
        int currentRetry() const
        {
            return m_currentRetry;
        }

        // Retry management
        void resetRetryCounter()
        {
            m_currentRetry = 0;
        }

        // Device pointer management
        /**
         * @brief Clear device pointer to prevent crashes during cleanup
         * This should be called before destroying actions when devices might be disconnecting
         */
        void clearDevicePointer()
        {
            if (m_deviceConnection)
            {
                disconnect(m_deviceConnection);
                m_deviceConnection = QMetaObject::Connection();
            }
            if (m_devicePtr)
                m_devicePtr.clear();
        }

        // Serialization
        virtual QJsonObject toJson() const;
        static TaskAction* fromJson(const QJsonObject &json, QObject *parent = nullptr);

    signals:
        /** @brief Emitted when action execution starts */
        void started();

        /** @brief Emitted when action completes successfully */
        void completed();

        /** @brief Emitted when action fails */
        void failed(const QString &error);

        /** @brief Emitted to report progress during long-running actions */
        void progress(const QString &message);

        /** @brief Emitted when status changes */
        void statusChanged(Status newStatus);

    protected:
        /**
         * @brief Set the current status and emit statusChanged signal
         * @param status New status
         */
        void setStatus(Status status);

        /**
         * @brief Set error message for failed actions
         * @param message Error description
         */
        void setErrorMessage(const QString &message)
        {
            m_errorMessage = message;
        }

        /**
         * @brief Increment retry counter
         * @return true if more retries are available, false if retry limit reached
         */
        bool incrementRetry();

        /**
         * @brief Get INDI device by name using INDIListener
         * @return Shared pointer to device, or null if not found
         */
        QSharedPointer<ISD::GenericDevice> getDevice() const;

        Type m_type;
        Status m_status = PENDING;
        QString m_device;
        QString m_property;
        int m_timeout = 30;
        int m_retries = 2;
        int m_currentRetry = 0;
        FailureAction m_failureAction = ABORT_QUEUE;
        QString m_errorMessage;

        // Connection to device for cleanup
        QMetaObject::Connection m_deviceConnection;
        // Keep device reference during execution
        QSharedPointer<ISD::GenericDevice> m_devicePtr;
};

} // namespace Ekos
