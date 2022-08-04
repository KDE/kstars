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

        //        connect(_INDIListener, &INDIListener::newTelescope,
        //                [&]()
        //        {
        //            KStars::Instance()->slotSetTelescopeEnabled(true);
        //        });

        //        connect(_INDIListener, &INDIListener::newDome,
        //                [&]()
        //        {
        //            KStars::Instance()->slotSetDomeEnabled(true);
        //        });
    }

    return _INDIListener;
}

INDIListener::INDIListener(QObject *parent) : QObject(parent) {}

INDIListener::~INDIListener()
{
    qDeleteAll(devices);
}

ISD::GenericDevice *INDIListener::getDevice(const QString &name) const
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == name)
            return oneDevice;
    }
    return nullptr;
}

void INDIListener::addClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI)
            << "INDIListener: Adding a new client manager to INDI listener..";

    clients.append(cm);

    connect(cm, &ClientManager::newINDIDevice, this, &INDIListener::processDevice);
    connect(cm, &ClientManager::newINDIProperty, this, &INDIListener::registerProperty);

    connect(cm, &ClientManager::removeINDIDevice, this, &INDIListener::removeDevice);
    connect(cm, &ClientManager::removeINDIProperty, this, &INDIListener::removeProperty);

    connect(cm, &ClientManager::newINDISwitch, this, &INDIListener::processSwitch);
    connect(cm, &ClientManager::newINDIText, this, &INDIListener::processText);
    connect(cm, &ClientManager::newINDINumber, this, &INDIListener::processNumber);
    connect(cm, &ClientManager::newINDILight, this, &INDIListener::processLight);
    connect(cm, &ClientManager::newINDIBLOB, this, &INDIListener::processBLOB);
    connect(cm, &ClientManager::newINDIUniversalMessage, this,
            &INDIListener::processUniversalMessage);
}

void INDIListener::removeClient(ClientManager *cm)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing client manager for server"
                         << cm->getHost() << "@" << cm->getPort();

    QList<ISD::GenericDevice *>::iterator it = devices.begin();
    clients.removeOne(cm);

    while (it != devices.end())
    {
        DriverInfo *dv  = (*it)->getDriverInfo();
        bool hostSource = (dv->getDriverSource() == HOST_SOURCE) ||
                          (dv->getDriverSource() == GENERATED_SOURCE);

        if (cm->isDriverManaged(dv))
        {
            cm->removeManagedDriver(dv);
            cm->disconnect(this);
            if (hostSource)
                return;
        }
        else
            ++it;
    }
}

void INDIListener::processDevice(DeviceInfo *dv)
{
    ClientManager *cm = qobject_cast<ClientManager *>(sender());
    Q_ASSERT_X(cm, __FUNCTION__, "Client manager is not valid.");

    qCDebug(KSTARS_INDI) << "INDIListener: New device" << dv->getDeviceName();

    auto gd = new ISD::GenericDevice(*dv, cm);
    devices.append(gd);

    emit newDevice(gd);

    // If already connected, we need to process all the properties as well
    auto isConnected = gd->isConnected();
    // Register all existing properties
    for (auto &oneProperty : dv->getBaseDevice()->getProperties())
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
}

void INDIListener::removeDevice(const QString &deviceName)
{
    qCDebug(KSTARS_INDI) << "INDIListener: Removing device" << deviceName;

    for (auto oneDevice : qAsConst(devices))
    {
        if (oneDevice->getDeviceName() == deviceName)
        {
            // Remove from list first
            devices.removeOne(oneDevice);
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

    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == prop.getDeviceName())
        {
            oneDevice->registerProperty(prop);
            return;
        }
    }
}

void INDIListener::removeProperty(const QString &device, const QString &name)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == device)
        {
            oneDevice->removeProperty(name);
            return;
        }
    }
}

void INDIListener::processSwitch(ISwitchVectorProperty *svp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == svp->device)
        {
            oneDevice->processSwitch(svp);
            break;
        }
    }
}

void INDIListener::processNumber(INumberVectorProperty *nvp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == nvp->device)
        {
            oneDevice->processNumber(nvp);
            break;
        }
    }
}

void INDIListener::processText(ITextVectorProperty *tvp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == tvp->device)
        {
            oneDevice->processText(tvp);
            break;
        }
    }
}

void INDIListener::processLight(ILightVectorProperty *lvp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == lvp->device)
        {
            oneDevice->processLight(lvp);
            break;
        }
    }
}

void INDIListener::processBLOB(IBLOB *bp)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == bp->bvp->device)
        {
            oneDevice->processBLOB(bp);
            break;
        }
    }
}

void INDIListener::processMessage(INDI::BaseDevice *dp, int messageID)
{
    for (auto &oneDevice : devices)
    {
        if (oneDevice->getDeviceName() == dp->getDeviceName())
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
                              KSNotification::EVENT_WARN);
    }
    else
    {
        KSNotification::transient(displayMessage, i18n("INDI Server Message"));
    }
}
