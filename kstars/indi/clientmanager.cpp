/*  INDI Client Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <cstdlib>

#include <QDebug>
#include <QProcess>
#include <QTime>
#include <QTemporaryFile>
#include <QDataStream>
#include <QTimer>

#include <basedevice.h>

#include "clientmanager.h"
#include "drivermanager.h"
#include "servermanager.h"
#include "driverinfo.h"
#include "deviceinfo.h"
#include "indilistener.h"
#include "guimanager.h"

#include "Options.h"

ClientManager::ClientManager()
{

sManager = NULL;

}

ClientManager::~ClientManager()
{

}

bool ClientManager::isDriverManaged(DriverInfo *di)
{
    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv == di)
            return true;
    }

    return false;
}

void ClientManager::newDevice(INDI::BaseDevice *dp)
{
    setBLOBMode(B_ALSO, dp->getDeviceName());

    DriverInfo *deviceDriver=NULL;

    if (QString(dp->getDeviceName()).isEmpty())
    {
        qWarning() << "Received invalid device with empty name! Ignoring the device...";
        return;
    }

    if (Options::verboseLogging())
        qDebug() << "Received new device " << dp->getDeviceName();

    // First iteration find unique matches
    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getUniqueLabel() == QString(dp->getDeviceName()))
        {
            deviceDriver = dv;
            break;
        }

    }

    // Second iteration find partial matches
    if (deviceDriver == NULL)
    {
        foreach(DriverInfo *dv, managedDrivers)
        {
            QString dvName = dv->getName();
            dvName = dv->getName().split(" ").first();
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

    if (deviceDriver == NULL)
        return;

    deviceDriver->setUniqueLabel(dp->getDeviceName());

    DeviceInfo *devInfo = new DeviceInfo(deviceDriver, dp);
    deviceDriver->addDevice(devInfo);
    emit newINDIDevice(devInfo);
    return;

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
    foreach(DriverInfo *driverInfo, managedDrivers)
    {
        foreach(DeviceInfo *deviceInfo, driverInfo->getDevices())
        {
            if (deviceInfo->getBaseDevice() == dp)
            {
                //GUIManager::Instance()->removeDevice(deviceInfo);
                //INDIListener::Instance()->removeDevice(deviceInfo);

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

void ClientManager::newNumber(INumberVectorProperty * nvp)
{
    emit newINDINumber(nvp);
}

void ClientManager::newText(ITextVectorProperty * tvp)
{
    emit newINDIText(tvp);
}

void ClientManager::newLight(ILightVectorProperty * lvp)
{
    emit newINDILight(lvp);
}

void ClientManager::newMessage(INDI::BaseDevice *dp, int messageID)
{
    emit newINDIMessage(dp, messageID);
}


void ClientManager::appendManagedDriver(DriverInfo *dv)
{
    managedDrivers.append(dv);

    dv->setClientManager(this);

    sManager = dv->getServerManager();
}

void ClientManager::removeManagedDriver(DriverInfo *dv)
{    
    dv->setClientState(false);

    foreach(DeviceInfo *di, dv->getDevices())
    {
        emit removeINDIDevice(di);
        dv->removeDevice(di);
    }

    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getDriverSource() == GENERATED_SOURCE)
        {
            managedDrivers.removeOne(dv);
            delete (dv);
            return;
        }
    }

    managedDrivers.removeOne(dv);
}


void ClientManager::serverConnected()
{
    foreach (DriverInfo *device, managedDrivers)
    {
        device->setClientState(true);
        if (sManager)
            device->setHostParameters(sManager->getHost(), sManager->getPort());
    }
}

void ClientManager::serverDisconnected(int exit_code)
{
    foreach (DriverInfo *device, managedDrivers)
    {
        device->setClientState(false);

        device->clear();
    }

    if (exit_code < 0)
        emit connectionFailure(this);
}
QList<DriverInfo *> ClientManager::getManagedDrivers() const
{
    return managedDrivers;
}

DriverInfo * ClientManager::findDriverInfoByName(const QString &name)
{
    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getName() == name)
            return dv;
    }

    return NULL;

}

DriverInfo * ClientManager::findDriverInfoByLabel(const QString &label)
{
    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getTreeLabel() == label)
            return dv;
    }

    return NULL;

}


