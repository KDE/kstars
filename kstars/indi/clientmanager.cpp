/*  INDI Client Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "clientmanager.h"

#include "deviceinfo.h"
#include "drivermanager.h"
#include "guimanager.h"
#include "indilistener.h"
#include "Options.h"
#include "servermanager.h"

#include <indi_debug.h>

bool ClientManager::isDriverManaged(DriverInfo *di)
{
    foreach (DriverInfo *dv, managedDrivers)
    {
        if (dv == di)
            return true;
    }

    return false;
}

void ClientManager::newDevice(INDI::BaseDevice *dp)
{
    //setBLOBMode(B_ALSO, dp->getDeviceName());
    // JM 2018.09.27: ClientManager will no longer handle BLOB, just messages.
    // We relay the BLOB handling to BLOB Manager to better manage concurrent connections with large data
    setBLOBMode(B_NEVER, dp->getDeviceName());

    DriverInfo *deviceDriver = nullptr;

    if (QString(dp->getDeviceName()).isEmpty())
    {
        qCWarning(KSTARS_INDI) << "Received invalid device with empty name! Ignoring the device...";
        return;
    }

    qCDebug(KSTARS_INDI) << "Received new device" << dp->getDeviceName();

    // First iteration find unique matches
    foreach (DriverInfo *dv, managedDrivers)
    {
        if (dv->getUniqueLabel() == QString(dp->getDeviceName()))
        {
            deviceDriver = dv;
            break;
        }
    }

    // Second iteration find partial matches
    if (deviceDriver == nullptr)
    {
        foreach (DriverInfo *dv, managedDrivers)
        {
            QString dvName = dv->getName();
            dvName         = dv->getName().split(' ').first();
            if (dvName.isEmpty())
                dvName = dv->getName();
            if (/*dv->getUniqueLabel() == dp->getDeviceName() ||*/
                QString(dp->getDeviceName()).startsWith(dvName, Qt::CaseInsensitive) ||
                ((dv->getDriverSource() == HOST_SOURCE || dv->getDriverSource() == GENERATED_SOURCE)))
            {
                deviceDriver = dv;
                break;
            }
        }
    }

    if (deviceDriver == nullptr)
        return;

    deviceDriver->setUniqueLabel(dp->getDeviceName());

    DeviceInfo *devInfo = new DeviceInfo(deviceDriver, dp);
    deviceDriver->addDevice(devInfo);
    emit newINDIDevice(devInfo);
}

void ClientManager::newProperty(INDI::Property *pprop)
{
    INDI::Property prop(*pprop);

    // Do not emit the signal if the server is disconnected or disconnecting (deadlock between signals)
    if (!isServerConnected())
    {
        IDLog("Received new property %s for disconnected device %s, discarding\n", prop.getName(), prop.getDeviceName());
        return;
    }

    //IDLog("Received new property %s for device %s\n", prop->getName(), prop->getgetDeviceName());
    emit newINDIProperty(prop);

    // Only handle RW and RO BLOB properties
    if (prop.getType() == INDI_BLOB && prop.getPermission() != IP_WO)
    {
        QPointer<BlobManager> bm = new BlobManager(getHost(), getPort(), prop.getBaseDevice()->getDeviceName(), prop.getName());
        connect(bm.data(), &BlobManager::newINDIBLOB, this, &ClientManager::newINDIBLOB);
        connect(bm.data(), &BlobManager::connected, this, [prop, this]()
        {
            if (prop && prop.getRegistered())
                emit newBLOBManager(prop->getBaseDevice()->getDeviceName(), prop);
        });
        blobManagers.append(bm);
    }
}

void ClientManager::removeProperty(INDI::Property *prop)
{
    const QString name = prop->getName();
    const QString device = prop->getDeviceName();
    emit removeINDIProperty(device, name);

    // If BLOB property is removed, remove its corresponding property if one exists.
    if (blobManagers.empty() == false && prop->getType() == INDI_BLOB && prop->getPermission() != IP_WO)
    {
        for (QPointer<BlobManager> bm : blobManagers)
        {
            const QString bProperty = bm.data()->property("property").toString();
            const QString bDevice = bm.data()->property("device").toString();
            if (bDevice == device && bProperty == name)
            {
                blobManagers.removeOne(bm);
                bm.data()->disconnectServer();
                bm->deleteLater();
                break;
            }
        }
    }
}

void ClientManager::disconnectAll()
{
    disconnectServer();
    for (auto &oneManager : blobManagers)
        oneManager->disconnectServer();
}

void ClientManager::removeDevice(INDI::BaseDevice *dp)
{
    QString deviceName = dp->getDeviceName();

    QMutableListIterator<QPointer<BlobManager>> it(blobManagers);
    while (it.hasNext())
    {
        QPointer<BlobManager> &oneManager = it.next();
        if (oneManager->property("device").toString() == deviceName)
        {
            oneManager->disconnect();
            it.remove();
        }
    }

    for (auto driverInfo : managedDrivers)
    {
        for (auto deviceInfo : driverInfo->getDevices())
        {
            if (deviceInfo->getDeviceName() == deviceName)
            {
                qCDebug(KSTARS_INDI) << "Removing device" << deviceName;

                emit removeINDIDevice(deviceName);

                driverInfo->removeDevice(deviceInfo);

                if (driverInfo->isEmpty())
                {
                    managedDrivers.removeOne(driverInfo);
                    if (driverInfo->getDriverSource() == GENERATED_SOURCE)
                        driverInfo->deleteLater();
                }

                return;
            }
        }
    }
}

void ClientManager::newBLOB(IBLOB *bp)
{
    emit newINDIBLOB(bp);
}

void ClientManager::newSwitch(ISwitchVectorProperty *svp)
{
    emit newINDISwitch(svp);
}

void ClientManager::newNumber(INumberVectorProperty *nvp)
{
    emit newINDINumber(nvp);
}

void ClientManager::newText(ITextVectorProperty *tvp)
{
    emit newINDIText(tvp);
}

void ClientManager::newLight(ILightVectorProperty *lvp)
{
    emit newINDILight(lvp);
}

void ClientManager::newMessage(INDI::BaseDevice *dp, int messageID)
{
    emit newINDIMessage(dp, messageID);
}

#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 5
void ClientManager::newUniversalMessage(std::string message)
{
    emit newINDIUniversalMessage(QString::fromStdString(message));
}
#endif

void ClientManager::appendManagedDriver(DriverInfo *dv)
{
    qCDebug(KSTARS_INDI) << "Adding managed driver" << dv->getName();

    managedDrivers.append(dv);

    dv->setClientManager(this);

    sManager = dv->getServerManager();
}

void ClientManager::removeManagedDriver(DriverInfo *dv)
{
    qCDebug(KSTARS_INDI) << "Removing managed driver" << dv->getName();

    dv->setClientState(false);
    managedDrivers.removeOne(dv);

    for (auto di : dv->getDevices())
    {
        // #1 Remove from GUI Manager
        GUIManager::Instance()->removeDevice(di->getDeviceName());

        // #2 Remove from INDI Listener
        INDIListener::Instance()->removeDevice(di->getDeviceName());

        // #3 Remove device from Driver Info
        dv->removeDevice(di);
    }

    if (dv->getDriverSource() == GENERATED_SOURCE)
        dv->deleteLater();
}

void ClientManager::serverConnected()
{
    qCDebug(KSTARS_INDI) << "INDI server connected.";

    foreach (DriverInfo *device, managedDrivers)
    {
        device->setClientState(true);
        if (sManager)
            device->setHostParameters(sManager->getHost(), sManager->getPort());
    }
}

void ClientManager::serverDisconnected(int exit_code)
{
    qCDebug(KSTARS_INDI) << "INDI server disconnected. Exit code:" << exit_code;

    foreach (DriverInfo *device, managedDrivers)
    {
        device->setClientState(false);

        device->reset();
    }

    if (exit_code < 0)
        emit connectionFailure(this);
}
QList<DriverInfo *> ClientManager::getManagedDrivers() const
{
    return managedDrivers;
}

DriverInfo *ClientManager::findDriverInfoByName(const QString &name)
{
    foreach (DriverInfo *dv, managedDrivers)
    {
        if (dv->getName() == name)
            return dv;
    }

    return nullptr;
}

DriverInfo *ClientManager::findDriverInfoByLabel(const QString &label)
{
    foreach (DriverInfo *dv, managedDrivers)
    {
        if (dv->getLabel() == label)
            return dv;
    }

    return nullptr;
}

void ClientManager::setBLOBEnabled(bool enabled, const QString &device, const QString &property)
{
    for(QPointer<BlobManager> bm : blobManagers)
    {
        if (bm->property("device") == device && (property.isEmpty() || bm->property("property") == property))
        {
            bm->setEnabled(enabled);
            return;
        }
    }

    //    if (property.isEmpty())
    //        setBLOBMode(enabled ? B_ONLY : B_NEVER, device.toLatin1().constData());
    //    else
    //        setBLOBMode(enabled ? B_ONLY : B_NEVER, device.toLatin1().constData(), property.toLatin1().constData());
}

bool ClientManager::isBLOBEnabled(const QString &device, const QString &property)
{
    for(QPointer<BlobManager> bm : blobManagers)
    {
        if (bm->property("device") == device && bm->property("property") == property)
            return bm->property("enabled").toBool();
        //        if (bm->property("device") == device && bm->property("property") == property)
        //        {
        //            return (getBLOBMode(device.toLatin1().constData(), property.toLatin1().constData()) != B_NEVER);
        //        }
    }

    return false;
}
