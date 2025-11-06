/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "task.h"
#include "actions/taskaction.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace Ekos
{

Task::Task(QObject *parent)
    : QObject(parent)
{
}

Task::~Task()
{
    // First, clear device pointers from all actions to prevent crashes
    // when devices are being destroyed by INDI during disconnection
    for (TaskAction *action : m_actions)
    {
        if (action)
        {
            // Clear device pointer before qDeleteAll triggers destructors
            action->clearDevicePointer();
        }
    }
    
    // Now safe to delete all actions
    qDeleteAll(m_actions);
    m_actions.clear();
}

bool Task::instantiateFromTemplate(TaskTemplate *tmpl,
                                   const QString &deviceName,
                                   const QMap<QString, QVariant> &parameters)
{
    if (!tmpl)
        return false;

    // Validate parameters
    QString errorMsg;
    if (!tmpl->validateParameters(parameters, errorMsg))
    {
        m_errorMessage = errorMsg;
        return false;
    }

    // Store metadata
    m_templateId = tmpl->id();
    m_device = deviceName;
    m_category = tmpl->category();
    m_parameters = parameters;

    // Generate default name
    m_name = QString("%1 - %2").arg(tmpl->name()).arg(deviceName);

    // Clear any existing actions
    qDeleteAll(m_actions);
    m_actions.clear();

    // Instantiate actions from template
    QJsonArray actionDefs = tmpl->actionDefinitions();
    for (const QJsonValue &val : actionDefs)
    {
        QJsonObject actionDef = val.toObject();

        // Substitute parameters in action definition
        QJsonObject resolvedDef = substituteParameters(actionDef);

        // Set device name
        resolvedDef["device"] = deviceName;

        // Create action from JSON
        TaskAction *action = TaskAction::fromJson(resolvedDef, this);
        if (!action)
        {
            m_errorMessage = QString("Failed to create action from template");
            qDeleteAll(m_actions);
            m_actions.clear();
            return false;
        }

        m_actions.append(action);
    }

    if (m_actions.isEmpty())
    {
        m_errorMessage = "No actions created from template";
        return false;
    }

    return true;
}

void Task::setDevice(const QString &deviceName)
{
    m_device = deviceName;
    
    // Update device name for all actions
    for (TaskAction *action : m_actions)
    {
        if (action)
        {
            action->setDevice(deviceName);
        }
    }
    
    // Update task name to reflect new device
    if (!m_templateId.isEmpty())
    {
        m_name = QString("%1 - %2").arg(m_name.section(" - ", 0, 0)).arg(deviceName);
    }
}

void Task::setStatus(Status status)
{
    if (m_status != status)
    {
        m_status = status;
        emit statusChanged(status);
    }
}

int Task::currentActionIndex() const
{
    return m_actions.indexOf(m_currentAction);
}

QJsonObject Task::substituteParameters(const QJsonObject &actionDef) const
{
    QJsonObject result;

    // Iterate through all keys in the action definition
    for (const QString &key : actionDef.keys())
    {
        QJsonValue value = actionDef[key];

        // Recursively handle nested objects
        if (value.isObject())
        {
            result[key] = substituteParameters(value.toObject());
        }
        // Handle arrays
        else if (value.isArray())
        {
            QJsonArray array = value.toArray();
            QJsonArray resultArray;
            for (const QJsonValue &item : array)
            {
                if (item.isObject())
                {
                    resultArray.append(substituteParameters(item.toObject()));
                }
                else
                {
                    QVariant resolved = substituteValue(item.toVariant());
                    resultArray.append(QJsonValue::fromVariant(resolved));
                }
            }
            result[key] = resultArray;
        }
        // Handle primitive values
        else
        {
            QVariant resolved = substituteValue(value.toVariant());
            result[key] = QJsonValue::fromVariant(resolved);
        }
    }

    return result;
}

QVariant Task::substituteValue(const QVariant &value) const
{
    // Only substitute strings
    if (value.type() != QVariant::String)
    {
        return value;
    }

    QString str = value.toString();

    // Pattern to match ${parameter_name}
    static QRegularExpression regex("\\$\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");

    QString result = str;
    QRegularExpressionMatchIterator it = regex.globalMatch(str);

    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        QString placeholder = match.captured(0);  // Full match: ${param_name}
        QString paramName = match.captured(1);     // Just the name: param_name

        if (m_parameters.contains(paramName))
        {
            QVariant paramValue = m_parameters[paramName];

            // If the entire string is just the placeholder, return the actual type
            if (result == placeholder)
            {
                return paramValue;
            }

            // Otherwise, do string substitution
            result.replace(placeholder, paramValue.toString());
        }
    }

    // If we did any substitutions and the result can be converted to number, return as number
    if (result != str)
    {
        bool ok;
        double numValue = result.toDouble(&ok);
        if (ok)
        {
            return numValue;
        }
    }

    return result;
}

QJsonObject Task::toJson() const
{
    QJsonObject json;

    // Metadata
    json["name"] = m_name;
    json["template_id"] = m_templateId;
    json["device"] = m_device;
    json["category"] = m_category;
    json["status"] = static_cast<int>(m_status);

    if (!m_errorMessage.isEmpty())
    {
        json["error_message"] = m_errorMessage;
    }

    // Parameters
    QJsonObject paramsObj;
    for (auto it = m_parameters.constBegin(); it != m_parameters.constEnd(); ++it)
    {
        paramsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    json["parameters"] = paramsObj;

    // Actions
    QJsonArray actionsArray;
    for (TaskAction *action : m_actions)
    {
        actionsArray.append(action->toJson());
    }
    json["actions"] = actionsArray;

    return json;
}

bool Task::loadFromJson(const QJsonObject &json)
{
    // Load metadata
    m_name = json["name"].toString();
    m_templateId = json["template_id"].toString();
    m_device = json["device"].toString();
    m_category = json["category"].toString();
    m_status = static_cast<Status>(json["status"].toInt());
    m_errorMessage = json["error_message"].toString();

    // Load parameters
    m_parameters.clear();
    QJsonObject paramsObj = json["parameters"].toObject();
    for (const QString &key : paramsObj.keys())
    {
        m_parameters[key] = paramsObj[key].toVariant();
    }

    // Load actions
    qDeleteAll(m_actions);
    m_actions.clear();

    QJsonArray actionsArray = json["actions"].toArray();
    for (const QJsonValue &val : actionsArray)
    {
        TaskAction *action = TaskAction::fromJson(val.toObject(), this);
        if (action)
        {
            m_actions.append(action);
        }
    }

    return !m_actions.isEmpty();
}

} // namespace Ekos
