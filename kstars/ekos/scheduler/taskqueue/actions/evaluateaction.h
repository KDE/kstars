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
 * @class EvaluateAction
 * @brief Action to evaluate an INDI property condition
 *
 * EvaluateAction polls an INDI property or state until a specified condition
 * is met. It supports various condition types and property types (number, text,
 * switch, light, state).
 */
class EvaluateAction : public TaskAction
{
        Q_OBJECT

    public:
        enum ConditionType
        {
            EQUALS,
            NOT_EQUALS,
            GREATER_THAN,
            LESS_THAN,
            GREATER_EQUAL,
            LESS_EQUAL,
            WITHIN_RANGE,
            CONTAINS,
            STARTS_WITH
        };
        Q_ENUM(ConditionType)

        enum PropertyType
        {
            NUMBER,
            TEXT,
            SWITCH,
            LIGHT,
            STATE
        };
        Q_ENUM(PropertyType)

        explicit EvaluateAction(QObject *parent = nullptr);
        ~EvaluateAction() override = default;

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

        PropertyType propertyType() const
        {
            return m_propertyType;
        }
        void setPropertyType(PropertyType type)
        {
            m_propertyType = type;
        }

        ConditionType condition() const
        {
            return m_condition;
        }
        void setCondition(ConditionType condition)
        {
            m_condition = condition;
        }

        QVariant target() const
        {
            return m_target;
        }
        void setTarget(const QVariant &target)
        {
            m_target = target;
        }

        double margin() const
        {
            return m_margin;
        }
        void setMargin(double margin)
        {
            m_margin = margin;
        }

        void setAcceptIdleOnSkippedPredecessor(bool accept)
        {
            m_acceptIdleOnSkippedPredecessor = accept;
        }

        // Serialization
        QJsonObject toJson() const override;
        static EvaluateAction* fromJson(const QJsonObject &json, QObject *parent = nullptr);

    private slots:
        void checkCondition();
        void handleTimeout();

        /**
         * @brief Handle property updates from device
         * @param prop Updated property
         */
        void onPropertyUpdated(INDI::Property prop);

    private:

        /**
         * @brief Evaluate the condition based on current property value
         * @return true if condition is met
         */
        bool evaluateCondition();

        /**
         * @brief Get current property value or state
         * @return Current value, or invalid QVariant on error
         */
        QVariant getCurrentValue();

        /**
         * @brief Evaluate condition for numeric properties
         * @param current Current numeric value
         * @return true if condition is met
         */
        bool evaluateNumericCondition(double current);

        /**
         * @brief Evaluate condition for text properties
         * @param current Current text value
         * @return true if condition is met
         */
        bool evaluateTextCondition(const QString &current);

        /**
         * @brief Evaluate condition for boolean properties (switch/light)
         * @param current Current boolean value
         * @return true if condition is met
         */
        bool evaluateBooleanCondition(bool current);

        /**
         * @brief Evaluate condition for state properties
         * @param current Current state string
         * @return true if condition is met
         */
        bool evaluateStateCondition(const QString &current);

        QString m_element;
        PropertyType m_propertyType = NUMBER;
        ConditionType m_condition = EQUALS;
        QVariant m_target;
        double m_margin = 0.0;  // For WITHIN_RANGE conditions

        QTimer *m_evaluationTimer = nullptr;
        QTimer *m_timeoutTimer = nullptr;

        // For tracking state during execution
        int m_pollInterval = 1000; // Poll every 1 second
        bool m_acceptIdleOnSkippedPredecessor = false;
};

} // namespace Ekos
