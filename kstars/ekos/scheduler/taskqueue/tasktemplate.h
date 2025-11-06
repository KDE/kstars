/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

namespace Ekos
{

/**
 * @class TaskTemplate
 * @brief Immutable template for creating tasks
 *
 * TaskTemplate represents a reusable blueprint for creating tasks. It defines:
 * - Metadata (id, name, description, category)
 * - Supported INDI device interfaces
 * - Configurable parameters with defaults and constraints
 * - Action definitions with parameter placeholders
 *
 * Templates are loaded from JSON and can be system-provided or user-created.
 */
class TaskTemplate : public QObject
{
        Q_OBJECT

    public:
        /**
         * @brief Parameter definition for template customization
         */
        struct Parameter
        {
            QString name;           ///< Parameter name (used in placeholders)
            QString type;           ///< Data type: "number", "text", "boolean"
            QVariant defaultValue;  ///< Default value
            QVariant minValue;      ///< Minimum value (for number type)
            QVariant maxValue;      ///< Maximum value (for number type)
            QVariant step;          ///< Step value (for number type)
            QString unit;           ///< Display unit (e.g., "°C", "seconds")
            QString description;    ///< User-friendly description

            Parameter() = default;
            Parameter(const QString &n, const QString &t, const QVariant &def)
                : name(n), type(t), defaultValue(def) {}
        };

        explicit TaskTemplate(QObject *parent = nullptr);
        ~TaskTemplate() override = default;

        // Load from JSON
        bool loadFromJson(const QJsonObject &json);
        QJsonObject toJson() const;

        // Metadata accessors
        QString id() const
        {
            return m_id;
        }
        void setId(const QString &id)
        {
            m_id = id;
        }

        QString name() const
        {
            return m_name;
        }
        void setName(const QString &name)
        {
            m_name = name;
        }

        QString description() const
        {
            return m_description;
        }
        void setDescription(const QString &desc)
        {
            m_description = desc;
        }

        QString category() const
        {
            return m_category;
        }
        void setCategory(const QString &category)
        {
            m_category = category;
        }

        QString version() const
        {
            return m_version;
        }
        void setVersion(const QString &version)
        {
            m_version = version;
        }

        QString parentTemplate() const
        {
            return m_parentTemplate;
        }
        void setParentTemplate(const QString &parent)
        {
            m_parentTemplate = parent;
        }

        bool isSystemTemplate() const
        {
            return m_isSystemTemplate;
        }
        void setSystemTemplate(bool system)
        {
            m_isSystemTemplate = system;
        }

        // Device interface support
        QList<uint32_t> supportedInterfaces() const
        {
            return m_supportedInterfaces;
        }
        void setSupportedInterfaces(const QList<uint32_t> &interfaces)
        {
            m_supportedInterfaces = interfaces;
        }

        /**
         * @brief Check if template supports a device with given interface
         * @param deviceInterface Device's driver interface flags
         * @return true if any of the template's interfaces match the device
         */
        bool supportsDeviceInterface(uint32_t deviceInterface) const;

        // Parameter management
        const QList<Parameter> &parameters() const
        {
            return m_parameters;
        }
        void setParameters(const QList<Parameter> &params)
        {
            m_parameters = params;
        }
        void addParameter(const Parameter &param)
        {
            m_parameters.append(param);
        }

        /**
         * @brief Get parameter by name
         * @param name Parameter name
         * @return Parameter if found, otherwise default Parameter
         */
        Parameter getParameter(const QString &name) const;

        /**
         * @brief Check if parameter exists
         * @param name Parameter name
         * @return true if parameter exists
         */
        bool hasParameter(const QString &name) const;

        // Action definitions
        QJsonArray actionDefinitions() const
        {
            return m_actionDefinitions;
        }
        void setActionDefinitions(const QJsonArray &actions)
        {
            m_actionDefinitions = actions;
        }

        /**
         * @brief Validate parameter values against constraints
         * @param paramValues Map of parameter name to value
         * @param errorMsg Output error message if validation fails
         * @return true if all values are valid
         */
        bool validateParameters(const QMap<QString, QVariant> &paramValues,
                                QString &errorMsg) const;

    private:
        // Metadata
        QString m_id;
        QString m_name;
        QString m_description;
        QString m_category;
        QString m_version;
        QString m_parentTemplate;  // For user templates derived from system templates
        bool m_isSystemTemplate = true;

        // Device support
        QList<uint32_t> m_supportedInterfaces;

        // Parameters
        QList<Parameter> m_parameters;

        // Action definitions (stored as JSON for flexibility)
        QJsonArray m_actionDefinitions;
};

} // namespace Ekos
