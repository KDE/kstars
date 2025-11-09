/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tasktemplate.h"
#include "actions/taskaction.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QJsonObject>

namespace Ekos
{

/**
 * @class Task
 * @brief Runtime instance of a task created from a template
 *
 * Task represents an executable instance created from a TaskTemplate. It:
 * - Has a specific device assignment
 * - Contains concrete parameter values (placeholders resolved)
 * - Contains instantiated TaskAction objects ready to execute
 * - Tracks runtime state and execution progress
 */
class Task : public QObject
{
        Q_OBJECT

    public:
        enum Status
        {
            PENDING,      ///< Task waiting to execute
            RUNNING,      ///< Task currently executing
            COMPLETED,    ///< Task completed successfully
            FAILED,       ///< Task failed
            ABORTED       ///< Task manually aborted
        };
        Q_ENUM(Status)

        explicit Task(QObject *parent = nullptr);
        ~Task() override;

        /**
         * @brief Create task from template with device and parameters
         * @param tmpl Template to instantiate from
         * @param deviceName Name of INDI device to use
         * @param parameters Map of parameter values
         * @return true if task was created successfully
         */
        bool instantiateFromTemplate(TaskTemplate *tmpl,
                                     const QString &deviceName,
                                     const QMap<QString, QVariant> &parameters);

        // Metadata
        QString name() const
        {
            return m_name;
        }
        void setName(const QString &name)
        {
            m_name = name;
        }

        QString templateId() const
        {
            return m_templateId;
        }

        QString device() const
        {
            return m_device;
        }

        void setDevice(const QString &deviceName);

        QString category() const
        {
            return m_category;
        }

        // Status
        Status status() const
        {
            return m_status;
        }
        void setStatus(Status status);

        QString errorMessage() const
        {
            return m_errorMessage;
        }
        void setErrorMessage(const QString &msg)
        {
            m_errorMessage = msg;
        }

        // Actions
        const QList<TaskAction *> &actions() const
        {
            return m_actions;
        }

        int actionCount() const
        {
            return m_actions.size();
        }

        TaskAction *currentAction() const
        {
            return m_currentAction;
        }

        int currentActionIndex() const;

        // Parameters
        const QMap<QString, QVariant> &parameters() const
        {
            return m_parameters;
        }

        // Device mapping failure action
        TaskAction::FailureAction deviceMappingFailureAction() const
        {
            return m_deviceMappingFailureAction;
        }
        void setDeviceMappingFailureAction(TaskAction::FailureAction action)
        {
            m_deviceMappingFailureAction = action;
        }

        // Serialization (for queue persistence)
        QJsonObject toJson() const;
        bool loadFromJson(const QJsonObject &json);

    signals:
        void statusChanged(Status status);
        void actionStarted(TaskAction *action, int index);
        void actionCompleted(TaskAction *action, int index);
        void actionFailed(TaskAction *action, int index, const QString &error);
        void progress(const QString &message);

    private:
        /**
         * @brief Substitute parameter placeholders in action definition
         * @param actionDef Action definition JSON with placeholders
         * @return Action definition with resolved values
         */
        QJsonObject substituteParameters(const QJsonObject &actionDef) const;

        /**
         * @brief Substitute a single value that may contain placeholder
         * @param value Value that may be string with ${param_name}
         * @return Resolved value
         */
        QVariant substituteValue(const QVariant &value) const;

        // Metadata
        QString m_name;
        QString m_templateId;
        QString m_device;
        QString m_category;

        // Runtime state
        Status m_status = PENDING;
        QString m_errorMessage;
        TaskAction *m_currentAction = nullptr;

        // Parameters and actions
        QMap<QString, QVariant> m_parameters;
        QList<TaskAction *> m_actions;

        // Device mapping failure action (defaults to abort for backward compatibility)
        TaskAction::FailureAction m_deviceMappingFailureAction = TaskAction::ABORT_QUEUE;
};

} // namespace Ekos
