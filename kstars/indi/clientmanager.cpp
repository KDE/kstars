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
        if (dv->getUniqueLabel() == dp->getDeviceName() || dv->getDriverSource() == HOST_SOURCE)
        {
            if (dv->getDriverSource() == HOST_SOURCE)
            {
                DriverInfo *drv = new DriverInfo(dv->getName());

                drv->setDriverSource(dv->getDriverSource());

                drv->setServerState(dv->getServerState());

                drv->setClientState(dv->getClientState());

                drv->setClientManager(this);

                drv->setServerManager(dv->getServerManager());

                drv->setHostParameters(dv->getHost(), dv->getPort());

                drv->setUniqueLabel(DriverManager::Instance()->getUniqueDeviceLabel(dp->getDeviceName()));

                drv->setBaseDevice(dp);

                managedDrivers.append(drv);

                emit newINDIDevice(drv);
                return;
            }
            else
            {
                dv->setBaseDevice(dp);
                emit newINDIDevice(dv);
                return;
            }
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

    emit INDIDeviceRemoved(dv);

    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv->getDriverSource() == HOST_SOURCE)
        {
            managedDrivers.removeOne(dv);
            emit INDIDeviceRemoved(dv);
            delete (dv);
        }
    }
}


void ClientManager::serverConnected()
{
    foreach (DriverInfo *device, managedDrivers)
    {
        //qDebug() << "Setting device " << device->getName() << " client state to true" << endl;
        device->setClientState(true);
        if (sManager)
            device->setHostParameters(sManager->getHost(), sManager->getPort());
    }
}

void ClientManager::serverDisconnected(int exit_code)
{
    //bool disconnectionError = false;
    //ServerManager *sManager= NULL;

    //qDebug() << "Server disconnected!!!" << endl;

    foreach (DriverInfo *device, managedDrivers)
    {
        device->setClientState(false);

       /*sManager = device->getServerManager();
        // If the server is still running, it means we lost connection to it.
        if (sManager || device->getDriverSource() == HOST_SOURCE)
            disconnectionError = true;*/

        device->clear();
    }

    if (exit_code < 0)
        emit connectionFailure(this);
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
