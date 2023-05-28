/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Handle INDI Standard properties.
*/

#include "indilistener.h"

#include "clientmanager.h"
#include "deviceinfo.h"
#include "kstars.h"
#include "Options.h"

#include "auxiliary/ksnotification.h"

#include <knotification.h>

#include <basedevice.h>
#include <indi_debug.h>

INDIListener *INDIListener::_INDIListener = nullptr;

INDIListener *INDIListener::Instance()
{
    if (_INDIListener == nullptr)
    {
        _INDIListener = new INDIListener(KStars::Instance());
        qRegisterMetaType<INDI::BaseDevice>("INDI::BaseDevice");
    }

    return _INDIListener;
}

bool INDIListener::findDevice(const QString &name, QSharedPointer<ISD::GenericDevice> &device)
{
    auto all = devices();
    auto it = std::find_if(all.begin(), all.end(), [name](auto & oneDevice)
    {
        return oneDevice->getDeviceName() == name;
    });
    if (it == all.end())
        return false;

    device = *it;
    return true;
}

INDIListener::INDIListener(QObject *parent) : QObject(parent) {}

bool INDIListener::getDevice(const QString &name, QSharedPointer<ISD::GenericDevice> &device) const
{
    for (auto &oneDevice : m_Devices)
    {
        if (oneDevice->getDeviceName() == name)
        {
            device = oneDevice;
            return true;
        }
    }
    return false;
}

void INDIListener::addClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI)
            << "INDIListener: Adding a new client manager to INDI listener..";

    clients.append(cm);

    connect(cm, &ClientManager::newINDIDevice, this, &INDIListener::processDevice);
    connect(cm, &ClientManager::removeINDIDevice, this, &INDIListener::removeDevice);

    connect(cm, &ClientManager::newINDIProperty, this, &INDIListener::registerProperty);
    connect(cm, &ClientManager::updateINDIProperty, this, &INDIListener::updateProperty);
    connect(cm, &ClientManager::removeINDIProperty, this, &INDIListener::removeProperty);

    connect(cm, &ClientManager::newINDIMessage, this,
            &INDIListener::processMessage);
    connect(cm, &ClientManager::newINDIUniversalMessage, this,
            &INDIListener::processUniversalMessage);
}

void INDIListener::removeClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing client manager for server"
                         << cm->getHost() << "@" << cm->getPort();

    cm->disconnect(this);
    clients.removeOne(cm);

    auto managedDrivers = cm->getManagedDrivers();
    for (auto &oneDriverInfo : managedDrivers)
        cm->removeManagedDriver(oneDriverInfo);
}

void INDIListener::processDevice(DeviceInfo *dv)
{
    ClientManager *cm = qobject_cast<ClientManager *>(sender());
    Q_ASSERT_X(cm, __FUNCTION__, "Client manager is not valid.");

    qCDebug(KSTARS_INDI) << "INDIListener: New device" << dv->getDeviceName();

    QSharedPointer<ISD::GenericDevice> gd;
    gd.reset(new ISD::GenericDevice(*dv, cm, this));

    auto isConnected = gd->isConnected();
    // If already connected, we need to process all the properties as well
    // Register all existing properties
    for (auto &oneProperty : dv->getBaseDevice().getProperties())
    {
        gd->registerProperty(oneProperty);
        if (isConnected)
        {
            switch (oneProperty.getType())
            {
                case INDI_SWITCH:
                    gd->processSwitch(oneProperty.getSwitch());
                    break;
                case INDI_NUMBER:
                    gd->processNumber(oneProperty.getNumber());
                    break;
                case INDI_TEXT:
                    gd->processText(oneProperty.getText());
                    break;
                case INDI_LIGHT:
                    gd->processLight(oneProperty.getLight());
                    break;
                default:
                    break;
            }
        }
    }

    m_Devices.append(std::move(gd));
    emit newDevice(gd);

}

void INDIListener::removeDevice(const QString &deviceName)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing device" << deviceName;

    for (auto oneDevice : qAsConst(m_Devices))
    {
        if (oneDevice->getDeviceName() == deviceName)
        {
            // Remove from list first
            m_Devices.removeOne(oneDevice);
            // Then emit a signal to inform subscribers that this device is removed.
            emit deviceRemoved(oneDevice);
            // Delete this device later.
            oneDevice->deleteLater();
            return;
        }
    }
}

void INDIListener::registerProperty(INDI::Property prop)
{
    if (!prop.getRegistered())
        return;

    qCDebug(KSTARS_INDI) << "<" << prop.getDeviceName() << ">: <" << prop.getName()
                         << ">";

    for (auto &oneDevice : m_Devices)
    {
        if (oneDevice->getDeviceName() == prop.getDeviceName())
        {
            oneDevice->registerProperty(prop);
            return;
        }
    }
}

void INDIListener::removeProperty(INDI::Property prop)
{
    for (auto &oneDevice : m_Devices)
    {
        if (oneDevice->getDeviceName() == prop.getDeviceName())
        {
            oneDevice->removeProperty(prop);
            return;
        }
    }
}

void INDIListener::updateProperty(INDI::Property prop)
{
    for (auto &oneDevice : m_Devices)
    {
        if (oneDevice->getDeviceName() == prop.getDeviceName())
        {
            oneDevice->updateProperty(prop);
            break;
        }
    }
}

void INDIListener::processMessage(INDI::BaseDevice dp, int messageID)
{
    for (auto &oneDevice : m_Devices)
    {
        if (oneDevice->getDeviceName() == dp.getDeviceName())
        {
            oneDevice->processMessage(messageID);
            break;
        }
    }
}

void INDIListener::processUniversalMessage(const QString &message)
{
    QString displayMessage = message;
    // Remove timestamp info as it is not suitable for message box
    int colonIndex = displayMessage.indexOf(": ");
    if (colonIndex > 0)
        displayMessage = displayMessage.mid(colonIndex + 2);

    // Special case for Alignment since it is not tied to a device
    if (displayMessage.startsWith("[ALIGNMENT]"))
    {
        qCDebug(KSTARS_INDI) << "AlignmentSubSystem:" << displayMessage;
        return;
    }

    if (Options::messageNotificationINDI())
    {
        KSNotification::event(QLatin1String("IndiServerMessage"), displayMessage,
                              KSNotification::INDI, KSNotification::Warn);
    }
    else
    {
        KSNotification::transient(displayMessage, i18n("INDI Server Message"));
    }
}
