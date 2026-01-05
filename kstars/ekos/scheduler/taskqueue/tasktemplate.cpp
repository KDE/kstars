/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tasktemplate.h"

#include <QJsonArray>
#include <QJsonValue>

namespace Ekos
{

TaskTemplate::TaskTemplate(QObject *parent)
    : QObject(parent)
{
}

bool TaskTemplate::loadFromJson(const QJsonObject &json)
{
    // Load metadata
    m_id = json["id"].toString();
    m_name = json["name"].toString();
    m_description = json["description"].toString();
    m_category = json["category"].toString();
    m_version = json["version"].toString("1.0");
    m_parentTemplate = json["parent_template"].toString();
    m_isSystemTemplate = !json.contains("parent_template");

    // Load supported interfaces
    m_supportedInterfaces.clear();
    QJsonArray interfacesArray = json["supported_interfaces"].toArray();
    for (const QJsonValue &val : interfacesArray)
    {
        m_supportedInterfaces.append(val.toInt());
    }

    // Load parameters
    m_parameters.clear();
    QJsonArray paramsArray = json["parameters"].toArray();
    for (const QJsonValue &val : paramsArray)
    {
        QJsonObject paramObj = val.toObject();
        Parameter param;
        param.name = paramObj["name"].toString();
        param.type = paramObj["type"].toString();
        param.defaultValue = paramObj["default"].toVariant();
        param.minValue = paramObj["min"].toVariant();
        param.maxValue = paramObj["max"].toVariant();
        param.step = paramObj["step"].toVariant();
        param.unit = paramObj["unit"].toString();
        param.description = paramObj["description"].toString();

        m_parameters.append(param);
    }

    // Load action definitions (keep as JSON for flexibility)
    m_actionDefinitions = json["actions"].toArray();

    // Validate required fields
    if (m_id.isEmpty() || m_name.isEmpty() || m_category.isEmpty())
    {
        return false;
    }

    if (m_actionDefinitions.isEmpty())
    {
        return false;
    }

    return true;
}

QJsonObject TaskTemplate::toJson() const
{
    QJsonObject json;

    // Metadata
    json["id"] = m_id;
    json["name"] = m_name;
    json["description"] = m_description;
    json["category"] = m_category;
    json["version"] = m_version;

    if (!m_parentTemplate.isEmpty())
    {
        json["parent_template"] = m_parentTemplate;
    }

    // Supported interfaces
    QJsonArray interfacesArray;
    for (uint32_t interface : m_supportedInterfaces)
    {
        interfacesArray.append(static_cast<int>(interface));
    }
    json["supported_interfaces"] = interfacesArray;

    // Parameters
    QJsonArray paramsArray;
    for (const Parameter &param : m_parameters)
    {
        QJsonObject paramObj;
        paramObj["name"] = param.name;
        paramObj["type"] = param.type;
        paramObj["default"] = QJsonValue::fromVariant(param.defaultValue);

        if (param.minValue.isValid())
            paramObj["min"] = QJsonValue::fromVariant(param.minValue);
        if (param.maxValue.isValid())
            paramObj["max"] = QJsonValue::fromVariant(param.maxValue);
        if (param.step.isValid())
            paramObj["step"] = QJsonValue::fromVariant(param.step);
        if (!param.unit.isEmpty())
            paramObj["unit"] = param.unit;
        if (!param.description.isEmpty())
            paramObj["description"] = param.description;

        paramsArray.append(paramObj);
    }
    json["parameters"] = paramsArray;

    // Action definitions
    json["actions"] = m_actionDefinitions;

    return json;
}

bool TaskTemplate::supportsDeviceInterface(uint32_t deviceInterface) const
{
    for (uint32_t templateInterface : m_supportedInterfaces)
    {
        if (deviceInterface & templateInterface)
        {
            return true;
        }
    }
    return false;
}

TaskTemplate::Parameter TaskTemplate::getParameter(const QString &name) const
{
    for (const Parameter &param : m_parameters)
    {
        if (param.name == name)
        {
            return param;
        }
    }
    return Parameter();  // Return default-constructed parameter if not found
}

bool TaskTemplate::hasParameter(const QString &name) const
{
    for (const Parameter &param : m_parameters)
    {
        if (param.name == name)
        {
            return true;
        }
    }
    return false;
}

bool TaskTemplate::validateParameters(const QMap<QString, QVariant> &paramValues,
                                      QString &errorMsg) const
{
    // Check that all required parameters are provided
    for (const Parameter &param : m_parameters)
    {
        if (!paramValues.contains(param.name))
        {
            errorMsg = QString("Missing required parameter: %1").arg(param.name);
            return false;
        }

        QVariant value = paramValues[param.name];

        // Validate number constraints
        if (param.type == "number")
        {
            bool ok;
            double numValue = value.toDouble(&ok);
            if (!ok)
            {
                errorMsg = QString("Parameter %1 must be a number").arg(param.name);
                return false;
            }

            if (param.minValue.isValid())
            {
                double minVal = param.minValue.toDouble();
                if (numValue < minVal)
                {
                    errorMsg = QString("Parameter %1 must be >= %2").arg(param.name).arg(minVal);
                    return false;
                }
            }

            if (param.maxValue.isValid())
            {
                double maxVal = param.maxValue.toDouble();
                if (numValue > maxVal)
                {
                    errorMsg = QString("Parameter %1 must be <= %2").arg(param.name).arg(maxVal);
                    return false;
                }
            }
        }
        // Validate text constraints (could add length checks, etc.)
        else if (param.type == "text")
        {
            if (!value.canConvert<QString>())
            {
                errorMsg = QString("Parameter %1 must be text").arg(param.name);
                return false;
            }
        }
        // Validate boolean
        else if (param.type == "boolean")
        {
            if (!value.canConvert<bool>())
            {
                errorMsg = QString("Parameter %1 must be boolean").arg(param.name);
                return false;
            }
        }
    }

    return true;
}

} // namespace Ekos
