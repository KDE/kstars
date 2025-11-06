/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "evaluateaction.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <indiproperty.h>
#include <basedevice.h>

#include <QJsonObject>
#include <QtMath>

namespace Ekos
{

EvaluateAction::EvaluateAction(QObject *parent)
    : TaskAction(parent)
{
    m_type = EVALUATE;
    m_evaluationTimer = new QTimer(this);
    m_timeoutTimer = new QTimer(this);

    connect(m_evaluationTimer, &QTimer::timeout, this, &EvaluateAction::checkCondition);
    connect(m_timeoutTimer, &QTimer::timeout, this, &EvaluateAction::handleTimeout);
}

bool EvaluateAction::start()
{
    m_currentRetry = 0;

    // Get device
    m_devicePtr = getDevice();
    if (!m_devicePtr)
    {
        setErrorMessage(QString("Device %1 not found").arg(m_device));
        setStatus(FAILED);
        emit failed(m_errorMessage);
        return false;
    }

    if (!m_devicePtr->isConnected())
    {
        setErrorMessage(QString("Device %1 not connected").arg(m_device));
        setStatus(FAILED);
        emit failed(m_errorMessage);
        return false;
    }

    // Check idempotency - is condition already met?
    if (isAlreadyDone())
    {
        QString msg = QString("%1.%2.%3 condition already met, skipping")
                      .arg(m_device).arg(m_property).arg(m_element);
        emit progress(msg);
        setStatus(COMPLETED);
        emit completed();
        return true;
    }

    setStatus(RUNNING);
    emit started();

    QString conditionStr;
    switch (m_condition)
    {
        case EQUALS:
            conditionStr = "equals";
            break;
        case NOT_EQUALS:
            conditionStr = "not equals";
            break;
        case GREATER_THAN:
            conditionStr = "greater than";
            break;
        case LESS_THAN:
            conditionStr = "less than";
            break;
        case GREATER_EQUAL:
            conditionStr = "greater or equal";
            break;
        case LESS_EQUAL:
            conditionStr = "less or equal";
            break;
        case WITHIN_RANGE:
            conditionStr = QString("within %1 of").arg(m_margin);
            break;
        case CONTAINS:
            conditionStr = "contains";
            break;
        case STARTS_WITH:
            conditionStr = "starts with";
            break;
    }

    emit progress(QString("Evaluating %1.%2.%3 %4 %5")
                  .arg(m_device)
                  .arg(m_property)
                  .arg(m_element)
                  .arg(conditionStr)
                  .arg(m_target.toString()));

    // Connect to property updates for reactive monitoring
    m_deviceConnection = connect(m_devicePtr.get(),
                                 &ISD::GenericDevice::propertyUpdated,
                                 this, &EvaluateAction::onPropertyUpdated);

    // Start polling for condition (backup to reactive)
    m_timeoutTimer->start(m_timeout * 1000);
    m_evaluationTimer->start(m_pollInterval);

    return true;
}

bool EvaluateAction::abort()
{
    if (m_evaluationTimer)
        m_evaluationTimer->stop();
    if (m_timeoutTimer)
        m_timeoutTimer->stop();

    // Clear device pointer to prevent crashes during cleanup
    if (m_deviceConnection)
        disconnect(m_deviceConnection);
    m_devicePtr.clear();

    setStatus(ABORTED);
    return true;
}

bool EvaluateAction::isAlreadyDone()
{
    // Special handling for STATE properties when a predecessor was skipped
    if (m_propertyType == STATE && m_acceptIdleOnSkippedPredecessor)
    {
        QVariant currentValue = getCurrentValue();
        if (currentValue.isValid() && currentValue.toString().compare("Idle", Qt::CaseInsensitive) == 0)
        {
            // If current state is Idle and we are configured to accept it, then condition is met.
            return true;
        }
    }
    // Fallback to general evaluation
    return evaluateCondition();
}

QVariant EvaluateAction::getCurrentValue()
{
    auto device = getDevice();
    if (!device || !device->isConnected())
        return QVariant();

    // For STATE property type, get the property state
    if (m_propertyType == STATE)
    {
        INDI::Property prop = device->getProperty(m_property.toUtf8());
        if (!prop.isValid())
            return QVariant();

        IPState state = prop.getState();
        switch (state)
        {
            case IPS_IDLE:
                return QVariant("Idle");
            case IPS_OK:
                return QVariant("OK");
            case IPS_BUSY:
                return QVariant("Busy");
            case IPS_ALERT:
                return QVariant("Alert");
            default:
                return QVariant();
        }
    }

    // For other types, get the property value
    INDI::Property prop = device->getProperty(m_property.toUtf8());
    if (!prop.isValid())
        return QVariant();

    switch (prop.getType())
    {
        case INDI_NUMBER:
        {
            auto np = prop.getNumber();
            auto element = np->findWidgetByName(m_element.toUtf8().constData());
            if (element)
                return QVariant(element->getValue());
            break;
        }
        case INDI_TEXT:
        {
            auto tp = prop.getText();
            auto element = tp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
                return QVariant(QString::fromUtf8(element->getText()));
            break;
        }
        case INDI_SWITCH:
        {
            auto sp = prop.getSwitch();
            auto element = sp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
                return QVariant(element->getState() == ISS_ON);
            break;
        }
        case INDI_LIGHT:
        {
            auto lp = prop.getLight();
            auto element = lp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                // Convert light state to boolean
                IPState lightState = element->getState();
                return QVariant(lightState == IPS_OK || lightState == IPS_BUSY);
            }
            break;
        }
        default:
            break;
    }

    return QVariant();
}

bool EvaluateAction::evaluateCondition()
{
    QVariant currentValue = getCurrentValue();
    if (!currentValue.isValid())
        return false;

    switch (m_propertyType)
    {
        case NUMBER:
        {
            bool ok;
            double current = currentValue.toDouble(&ok);
            if (!ok)
                return false;
            return evaluateNumericCondition(current);
        }

        case TEXT:
        {
            QString current = currentValue.toString();
            return evaluateTextCondition(current);
        }

        case SWITCH:
        case LIGHT:
        {
            bool current = currentValue.toBool();
            return evaluateBooleanCondition(current);
        }

        case STATE:
        {
            QString current = currentValue.toString();
            return evaluateStateCondition(current);
        }
    }

    return false;
}

bool EvaluateAction::evaluateNumericCondition(double current)
{
    bool targetOk;
    double targetVal = m_target.toDouble(&targetOk);
    if (!targetOk)
        return false;

    switch (m_condition)
    {
        case EQUALS:
            return qAbs(current - targetVal) < 0.001;  // Floating point tolerance

        case NOT_EQUALS:
            return qAbs(current - targetVal) >= 0.001;

        case GREATER_THAN:
            return current > targetVal;

        case LESS_THAN:
            return current < targetVal;

        case GREATER_EQUAL:
            return current >= targetVal;

        case LESS_EQUAL:
            return current <= targetVal;

        case WITHIN_RANGE:
            return qAbs(current - targetVal) <= m_margin;

        default:
            return false;
    }
}

bool EvaluateAction::evaluateTextCondition(const QString &current)
{
    QString targetStr = m_target.toString();

    switch (m_condition)
    {
        case EQUALS:
            return current == targetStr;

        case NOT_EQUALS:
            return current != targetStr;

        case CONTAINS:
            return current.contains(targetStr, Qt::CaseInsensitive);

        case STARTS_WITH:
            return current.startsWith(targetStr, Qt::CaseInsensitive);

        default:
            return false;
    }
}

bool EvaluateAction::evaluateBooleanCondition(bool current)
{
    bool targetBool = m_target.toBool();

    switch (m_condition)
    {
        case EQUALS:
            return current == targetBool;

        case NOT_EQUALS:
            return current != targetBool;

        default:
            return false;
    }
}

bool EvaluateAction::evaluateStateCondition(const QString &current)
{
    QString targetState = m_target.toString();

    switch (m_condition)
    {
        case EQUALS:
            return current.compare(targetState, Qt::CaseInsensitive) == 0;

        case NOT_EQUALS:
            return current.compare(targetState, Qt::CaseInsensitive) != 0;

        default:
            return false;
    }
}

void EvaluateAction::onPropertyUpdated(INDI::Property prop)
{
    // Only care about our target property
    if (QString::fromUtf8(prop.getName()) != m_property)
        return;

    // Check if condition is now met
    if (evaluateCondition())
    {
        m_evaluationTimer->stop();
        m_timeoutTimer->stop();
        disconnect(m_deviceConnection);
        // Clear device pointer to prevent crashes during cleanup
        m_devicePtr.clear();

        emit progress(QString("%1.%2.%3 condition met").arg(m_device).arg(m_property).arg(m_element));
        setStatus(COMPLETED);
        emit completed();
    }
}

void EvaluateAction::checkCondition()
{
    if (evaluateCondition())
    {
        m_evaluationTimer->stop();
        m_timeoutTimer->stop();
        disconnect(m_deviceConnection);
        // Clear device pointer to prevent crashes during cleanup
        m_devicePtr.clear();

        emit progress(QString("%1.%2.%3 condition met").arg(m_device).arg(m_property).arg(m_element));
        setStatus(COMPLETED);
        emit completed();
    }
    // If condition not met, continue polling
}

void EvaluateAction::handleTimeout()
{
    m_evaluationTimer->stop();
    m_timeoutTimer->stop();
    if (m_deviceConnection)
        disconnect(m_deviceConnection);

    if (incrementRetry())
    {
        emit progress(QString("Timeout, retry %1/%2").arg(m_currentRetry).arg(m_retries));
        start();
    }
    else
    {
        // Clear device pointer to prevent crashes during cleanup
        m_devicePtr.clear();
        setErrorMessage(QString("Timeout evaluating %1.%2").arg(m_property).arg(m_element));
        setStatus(FAILED);
        emit failed(m_errorMessage);
    }
}

QJsonObject EvaluateAction::toJson() const
{
    QJsonObject json = TaskAction::toJson();
    json["element"] = m_element;
    json["property_type"] = static_cast<int>(m_propertyType);
    json["condition"] = static_cast<int>(m_condition);
    json["target"] = QJsonValue::fromVariant(m_target);
    json["margin"] = m_margin;
    return json;
}

EvaluateAction* EvaluateAction::fromJson(const QJsonObject &json, QObject *parent)
{
    EvaluateAction *action = new EvaluateAction(parent);

    action->setDevice(json["device"].toString());
    action->setProperty(json["property"].toString());
    action->setElement(json["element"].toString());
    action->setPropertyType(static_cast<PropertyType>(json["property_type"].toInt()));
    action->setCondition(static_cast<ConditionType>(json["condition"].toInt()));
    action->setTarget(json["target"].toVariant());
    action->setMargin(json["margin"].toDouble(0.0));

    return action;
}

} // namespace Ekos
