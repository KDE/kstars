/*  INDI Client Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <cstdlib>
#include <unistd.h>

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

    foreach(DriverInfo *dv, managedDrivers)
    {
        QString dvName = dv->getName().split(" ").first();
        if (dvName.isEmpty())
            dvName = dv->getName();
        if (dv->getUniqueLabel() == dp->getDeviceName() ||
                QString(dp->getDeviceName()).startsWith(dvName, Qt::CaseInsensitive) || dv->getDriverSource() == HOST_SOURCE)
        {
            dv->setUniqueLabel(dp->getDeviceName());

            DeviceInfo *devInfo = new DeviceInfo(dv, dp);
            dv->addDevice(devInfo);
            emit newINDIDevice(devInfo);
            return;
        }
    }

}

void ClientManager::newProperty(INDI::Property *prop)
{
    //IDLog("Received new property %s for device %s\n", prop->getName(), prop->getgetDeviceName());

    if (!strcmp (prop->getName(), "EQUATORIAL_EOD_COORD"))
        emit newTelescope();
    else if (!strcmp (prop->getName(), "CCD_EXPOSURE"))
        emit newCCD();

    emit newINDIProperty(prop);
}

void ClientManager::removeProperty(INDI::Property *prop)
{
    emit removeINDIProperty(prop);
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
    managedDrivers.removeOne(dv);

    dv->setClientState(false);

    foreach(DeviceInfo *di, dv->getDevices())
        emit INDIDeviceRemoved(di);

    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getDriverSource() == HOST_SOURCE)
        {
            managedDrivers.removeOne(dv);
            delete (dv);
        }
    }
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

#include "clientmanager.moc"
