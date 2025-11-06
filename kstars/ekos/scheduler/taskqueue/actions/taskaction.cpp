/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "taskaction.h"
#include "setaction.h"
#include "evaluateaction.h"
#include "delayaction.h"
#include "startaction.h"
#include "scriptaction.h"

#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <QJsonObject>
#include <QMetaEnum>

namespace Ekos
{

TaskAction::TaskAction(QObject *parent)
    : QObject(parent)
{
}

TaskAction::~TaskAction()
{
    // Disconnect any active device connections
    if (m_deviceConnection)
    {
        disconnect(m_deviceConnection);
        m_deviceConnection = QMetaObject::Connection();
    }
    
}

void TaskAction::setStatus(Status status)
{
    if (m_status != status)
    {
        m_status = status;
        emit statusChanged(status);
    }
}

bool TaskAction::incrementRetry()
{
    m_currentRetry++;
    return m_currentRetry <= m_retries;
}

QSharedPointer<ISD::GenericDevice> TaskAction::getDevice() const
{
    QSharedPointer<ISD::GenericDevice> device;
    INDIListener::findDevice(m_device, device);
    return device;
}

QString TaskAction::statusString() const
{
    QMetaEnum metaEnum = QMetaEnum::fromType<TaskAction::Status>();
    return QString(metaEnum.valueToKey(m_status));
}

QString TaskAction::typeString() const
{
    QMetaEnum metaEnum = QMetaEnum::fromType<TaskAction::Type>();
    return QString(metaEnum.valueToKey(m_type));
}

QJsonObject TaskAction::toJson() const
{
    QJsonObject json;
    json["type"] = typeString();
    json["device"] = m_device;
    json["property"] = m_property;
    json["timeout"] = m_timeout;
    json["retries"] = m_retries;
    json["failure_action"] = static_cast<int>(m_failureAction);
    return json;
}

TaskAction* TaskAction::fromJson(const QJsonObject &json, QObject *parent)
{
    QString type = json["type"].toString();
    TaskAction *action = nullptr;

    if (type == "SET")
    {
        action = SetAction::fromJson(json, parent);
    }
    else if (type == "EVALUATE")
    {
        action = EvaluateAction::fromJson(json, parent);
    }
    else if (type == "DELAY")
    {
        action = DelayAction::fromJson(json, parent);
    }
    else if (type == "START")
    {
        action = StartAction::fromJson(json, parent);
    }
    else if (type == "SCRIPT")
    {
        action = ScriptAction::fromJson(json, parent);
    }

    if (action)
    {
        action->setTimeout(json["timeout"].toInt(30));
        action->setRetries(json["retries"].toInt(2));
        action->setFailureAction(static_cast<FailureAction>(json["failure_action"].toInt(ABORT_QUEUE)));
    }

    return action;
}

} // namespace Ekos
