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
    setBLOBMode(B_ALSO, dp->getDeviceName());

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

void ClientManager::newProperty(INDI::Property *prop)
{
    //IDLog("Received new property %s for device %s\n", prop->getName(), prop->getgetDeviceName());
    emit newINDIProperty(prop);
}

void ClientManager::removeProperty(INDI::Property *prop)
{
    emit removeINDIProperty(prop);
}

void ClientManager::removeDevice(INDI::BaseDevice *dp)
{
    foreach (DriverInfo *driverInfo, managedDrivers)
    {
        foreach (DeviceInfo *deviceInfo, driverInfo->getDevices())
        {
            if (deviceInfo->getBaseDevice() == dp)
            {
                //GUIManager::Instance()->removeDevice(deviceInfo);
                //INDIListener::Instance()->removeDevice(deviceInfo);

                qCDebug(KSTARS_INDI) << "Removing device" << dp->getDeviceName();

                emit removeINDIDevice(deviceInfo);

                driverInfo->removeDevice(deviceInfo);

                if (driverInfo->isEmpty())
                    managedDrivers.removeOne(driverInfo);

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

    foreach (DeviceInfo *di, dv->getDevices())
    {
        //emit removeINDIDevice(di);
        INDIListener::Instance()->removeDevice(di);
        GUIManager::Instance()->removeDevice(di);
        dv->removeDevice(di);
    }

    /*foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getDriverSource() == GENERATED_SOURCE)
        {
            managedDrivers.removeOne(dv);
            delete (dv);
            return;
        }
    }*/

    managedDrivers.removeOne(dv);
    if (dv->getDriverSource() == GENERATED_SOURCE)
        delete (dv);
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
