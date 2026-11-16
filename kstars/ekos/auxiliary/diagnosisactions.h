/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "diagnosismanager.h"
#include "indi/indiconcretedevice.h"

#include <KLocalizedString>

#include <QPointer>

#include <memory>

namespace Ekos
{
/**
 * @namespace Ekos::DiagnosisActions
 * @short Building blocks for the actions modules register with the DiagnosisManager.
 */
namespace DiagnosisActions
{

inline DiagnosisManager::ApplyResult failed(const QString &message)
{
    DiagnosisManager::ApplyResult result;
    result.outcome = DiagnosisManager::Outcome::Failed;
    result.message = message;
    return result;
}

inline DiagnosisManager::ApplyResult done(const QString &message = QString())
{
    DiagnosisManager::ApplyResult result;
    result.outcome = DiagnosisManager::Outcome::Done;
    result.message = message;
    return result;
}

/**
 * Connect a device and finish the item when it reports connected.
 */
inline DiagnosisManager::ApplyResult connectDevice(DiagnosisManager *manager, ISD::ConcreteDevice *device,
        const Diagnosis::Report &report, const Diagnosis::Item &item)
{
    if (device == nullptr)
        return failed(i18n("The device is not available."));
    if (device->isConnected())
        return done();

    QPointer<DiagnosisManager> guard(manager);
    const QString diagnosisId = report.id;
    const QString itemId = item.id;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(device, &ISD::ConcreteDevice::Connected, manager, [guard, diagnosisId, itemId,
                                   connection]()
    {
        QObject::disconnect(*connection);
        if (guard)
            guard->finishItem(diagnosisId, itemId, true);
    }, Qt::QueuedConnection);

    device->Connect();

    DiagnosisManager::ApplyResult result;
    result.outcome = DiagnosisManager::Outcome::Running;
    result.timeoutMs = 30000;
    return result;
}

}
}
