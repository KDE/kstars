/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "setaction.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/clientmanager.h"
#include <ekos_scheduler_debug.h>

#include <indiproperty.h>
#include <basedevice.h>

#include <QJsonObject>

namespace Ekos
{

SetAction::SetAction(QObject *parent)
    : TaskAction(parent)
{
    m_type = SET;
    m_completionTimer = new QTimer(this);
    m_timeoutTimer = new QTimer(this);

    connect(m_completionTimer, &QTimer::timeout, this, &SetAction::checkCompletion);
    connect(m_timeoutTimer, &QTimer::timeout, this, &SetAction::handleTimeout);
}

bool SetAction::start()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "SET action starting:" << m_device << "." << m_property << "." << m_element
                                  << "=" << m_value.toString();

    // Check idempotency - is action already done?
    if (isAlreadyDone())
    {
        QString msg = QString("%1.%2.%3 already at target value, skipping")
                      .arg(m_device).arg(m_property).arg(m_element);
        qCInfo(KSTARS_EKOS_SCHEDULER) << msg;
        emit progress(msg);
        setStatus(COMPLETED);
        emit completed();
        return true;
    }

    setStatus(RUNNING);
    emit started();
    emit progress(QString("Setting %1.%2.%3 to %4")
                  .arg(m_device)
                  .arg(m_property)
                  .arg(m_element)
                  .arg(m_value.toString()));

    // Send the SET command
    if (!sendSetCommand())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Failed to send SET command";
        if (incrementRetry())
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SET action" << m_currentRetry << "/" << m_retries;
            emit progress(QString("Retry %1/%2").arg(m_currentRetry).arg(m_retries));
            return start();  // Retry
        }
        else
        {
            setErrorMessage("Failed to send SET command after all retries");
            setStatus(FAILED);
            emit failed(m_errorMessage);
            return false;
        }
    }

    qCInfo(KSTARS_EKOS_SCHEDULER) << "SET command sent successfully";

    // If we need to wait for completion, start monitoring
    if (m_waitForCompletion)
    {
        // Connect to property updates for reactive monitoring
        if (m_devicePtr)
        {
            m_deviceConnection = connect(m_devicePtr.get(),
                                         &ISD::GenericDevice::propertyUpdated,
                                         this, &SetAction::onPropertyUpdated);
        }

        m_timeoutTimer->start(m_timeout * 1000);
        m_completionTimer->start(m_pollInterval);
    }
    else
    {
        // Command sent, assume success immediately
        setStatus(COMPLETED);
        emit completed();
    }

    return true;
}

bool SetAction::abort()
{
    if (m_completionTimer)
        m_completionTimer->stop();
    if (m_timeoutTimer)
        m_timeoutTimer->stop();
    
    // Clear device pointer to prevent crashes during cleanup
    if (m_deviceConnection)
        disconnect(m_deviceConnection);
    m_devicePtr.clear();

    setStatus(ABORTED);
    return true;
}

bool SetAction::isAlreadyDone()
{
    // Check if the property already has the target value
    return checkCurrentValue();
}

bool SetAction::sendSetCommand()
{
    // Get device from INDIListener
    m_devicePtr = getDevice();
    if (!m_devicePtr)
    {
        setErrorMessage(QString("Device %1 not found").arg(m_device));
        return false;
    }

    // Check device is connected
    if (!m_devicePtr->isConnected())
    {
        setErrorMessage(QString("Device %1 not connected").arg(m_device));
        return false;
    }

    // Get property
    INDI::Property prop = m_devicePtr->getProperty(m_property.toUtf8());
    if (!prop.isValid())
    {
        setErrorMessage(QString("Property %1 not found on device %2").arg(m_property).arg(m_device));
        return false;
    }

    // Set value based on property type
    bool success = false;
    switch (prop.getType())
    {
        case INDI_NUMBER:
        {
            auto np = prop.getNumber();
            auto element = np->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                element->setValue(m_value.toDouble());
                m_devicePtr->getClientManager()->sendNewProperty(prop);
                success = true;
            }
            else
            {
                setErrorMessage(QString("Element %1 not found in property %2").arg(m_element).arg(m_property));
            }
            break;
        }
        case INDI_SWITCH:
        {
            auto sp = prop.getSwitch();
            auto element = sp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                if (sp->getRule() != ISR_NOFMANY)
                    sp->reset();
                element->setState(m_value.toBool() ? ISS_ON : ISS_OFF);
                m_devicePtr->getClientManager()->sendNewProperty(prop);
                success = true;
            }
            else
            {
                setErrorMessage(QString("Element %1 not found in property %2").arg(m_element).arg(m_property));
            }
            break;
        }
        case INDI_TEXT:
        {
            auto tp = prop.getText();
            auto element = tp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                element->setText(m_value.toString().toUtf8().constData());
                m_devicePtr->getClientManager()->sendNewProperty(prop);
                success = true;
            }
            else
            {
                setErrorMessage(QString("Element %1 not found in property %2").arg(m_element).arg(m_property));
            }
            break;
        }
        default:
            setErrorMessage(QString("Unsupported property type for %1").arg(m_property));
            break;
    }

    return success;
}

bool SetAction::checkCurrentValue()
{
    auto device = getDevice();
    if (!device || !device->isConnected())
        return false;

    // Get property
    INDI::Property prop = device->getProperty(m_property.toUtf8());
    if (!prop.isValid())
        return false;

    // Check value based on property type
    switch (prop.getType())
    {
        case INDI_NUMBER:
        {
            auto np = prop.getNumber();
            auto element = np->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                double current = element->getValue();
                double target = m_value.toDouble();
                double diff = qAbs(current - target);
                return diff < 0.01;  // Tolerance for floating point comparison
            }
            break;
        }
        case INDI_SWITCH:
        {
            auto sp = prop.getSwitch();
            auto element = sp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                bool current = (element->getState() == ISS_ON);
                bool target = m_value.toBool();
                return current == target;
            }
            break;
        }
        case INDI_TEXT:
        {
            auto tp = prop.getText();
            auto element = tp->findWidgetByName(m_element.toUtf8().constData());
            if (element)
            {
                QString current = QString::fromUtf8(element->getText());
                QString target = m_value.toString();
                return current == target;
            }
            break;
        }
        default:
            break;
    }

    return false;
}

void SetAction::onPropertyUpdated(INDI::Property prop)
{
    // Only care about our target property
    if (QString::fromUtf8(prop.getName()) != m_property)
        return;

    // Check if property reached desired state and value
    IPState state = prop.getState();

    if (state == IPS_OK || state == IPS_IDLE)
    {
        // Verify the value is correct
        if (checkCurrentValue())
        {
            m_completionTimer->stop();
            m_timeoutTimer->stop();
            disconnect(m_deviceConnection);
            // Clear device pointer to prevent crashes during cleanup
            m_devicePtr.clear();

            emit progress(QString("%1.%2.%3 confirmed").arg(m_device).arg(m_property).arg(m_element));
            setStatus(COMPLETED);
            emit completed();
        }
    }
    else if (state == IPS_ALERT)
    {
        m_completionTimer->stop();
        m_timeoutTimer->stop();
        disconnect(m_deviceConnection);

        if (incrementRetry())
        {
            emit progress(QString("Property in Alert state, retry %1/%2")
                          .arg(m_currentRetry).arg(m_retries));
            start();
        }
        else
        {
            // Clear device pointer to prevent crashes during cleanup
            m_devicePtr.clear();
            setErrorMessage("Property entered Alert state");
            setStatus(FAILED);
            emit failed(m_errorMessage);
        }
    }
    // If Busy, continue waiting
}

void SetAction::checkCompletion()
{
    // Polling fallback - check property state
    if (!m_devicePtr || !m_devicePtr->isConnected())
    {
        handleTimeout();
        return;
    }

    INDI::Property prop = m_devicePtr->getProperty(m_property.toUtf8());
    if (!prop.isValid())
    {
        handleTimeout();
        return;
    }

    IPState state = prop.getState();

    if (state == IPS_OK || state == IPS_IDLE)
    {
        // Verify the value is correct
        if (checkCurrentValue())
        {
            m_completionTimer->stop();
            m_timeoutTimer->stop();
            disconnect(m_deviceConnection);
            // Clear device pointer to prevent crashes during cleanup
            m_devicePtr.clear();

            setStatus(COMPLETED);
            emit completed();
        }
    }
    else if (state == IPS_ALERT)
    {
        m_completionTimer->stop();
        m_timeoutTimer->stop();
        disconnect(m_deviceConnection);

        if (incrementRetry())
        {
            emit progress(QString("Property in Alert state, retry %1/%2")
                          .arg(m_currentRetry).arg(m_retries));
            start();
        }
        else
        {
            // Clear device pointer to prevent crashes during cleanup
            m_devicePtr.clear();
            setErrorMessage("Property entered Alert state");
            setStatus(FAILED);
            emit failed(m_errorMessage);
        }
    }
    // If Busy, continue polling
}

void SetAction::handleTimeout()
{
    m_completionTimer->stop();
    m_timeoutTimer->stop();
    if (m_deviceConnection)
        disconnect(m_deviceConnection);

    qCWarning(KSTARS_EKOS_SCHEDULER) << "Timeout waiting for" << m_property << "." << m_element;

    if (incrementRetry())
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Retrying SET action (timeout) -" << m_currentRetry << "/" << m_retries;
        emit progress(QString("Timeout, retry %1/%2").arg(m_currentRetry).arg(m_retries));
        start();
    }
    else
    {
        // Clear device pointer to prevent crashes during cleanup
        m_devicePtr.clear();
        qCWarning(KSTARS_EKOS_SCHEDULER) << "SET action failed after all retries";
        setErrorMessage(QString("Timeout waiting for %1.%2 to complete").arg(m_property).arg(m_element));
        setStatus(FAILED);
        emit failed(m_errorMessage);
    }
}

QJsonObject SetAction::toJson() const
{
    QJsonObject json = TaskAction::toJson();
    json["element"] = m_element;
    json["value"] = QJsonValue::fromVariant(m_value);
    json["wait_for_completion"] = m_waitForCompletion;
    return json;
}

SetAction* SetAction::fromJson(const QJsonObject &json, QObject *parent)
{
    SetAction *action = new SetAction(parent);

    action->setDevice(json["device"].toString());
    action->setProperty(json["property"].toString());
    action->setElement(json["element"].toString());
    action->setValue(json["value"].toVariant());
    action->setWaitForCompletion(json["wait_for_completion"].toBool(true));

    return action;
}

} // namespace Ekos
