/*  INDI Device
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QDebug>
#include <basedevice.h>

#include "driverinfo.h"
#include "deviceinfo.h"

#include "servermanager.h"


DriverInfo::DriverInfo(const QString &inName)
{
    uniqueLabel.clear();
    name = inName;

    driverSource  = PRIMARY_XML;

      // Initially off
    serverState = false;
    clientState = false;

    serverManager = NULL;

    hostname      = "localhost";
    port          = "-1";
    userPort      = "-1";

    type = KSTARS_UNKNOWN;

}

DriverInfo::~DriverInfo()
{
    qDeleteAll(devices);
}

void DriverInfo::clear()
{
    //managed = false;
    serverState = false;
    clientState = false;
    serverManager = NULL;

    uniqueLabel.clear();
    qDeleteAll(devices);
    devices.clear();
}

QString DriverInfo::getServerBuffer()
{
    if (serverManager != NULL)
           return serverManager->getLogBuffer();

    return QString();
}

void DriverInfo::setServerState(bool inState)
{
    if (inState == serverState)
        return;

    serverState = inState;

    if (serverState == false)
        serverManager = NULL;

    emit deviceStateChanged(this);
}

void DriverInfo::setClientState(bool inState)
{
    //qDebug() << "Request to change " << name << " client status to " << (inState ? "True" : "False") << endl;

    if (inState == clientState)
        return;


    clientState = inState;

    if (clientState == false)
        clientManager = NULL;

    //qDebug() << "Client state for this device changed, calling device state changed signal " << endl;

    emit deviceStateChanged(this);
}

void DriverInfo::setUserPort(const QString &inUserPort)
{
    if (inUserPort.isEmpty() == false)
        userPort = inUserPort;
    else
        userPort = "-1";
}

void DriverInfo::addDevice(DeviceInfo *idv)
{
    devices.append(idv);
}

DeviceInfo* DriverInfo::getDevice(const QString &deviceName)
{
    foreach(DeviceInfo *idv, devices)
    {
        if (idv->getBaseDevice()->getDeviceName() == deviceName)
            return idv;
    }

    return NULL;

}
QVariantMap DriverInfo::getAuxInfo() const
{
    return auxInfo;
}

void DriverInfo::setAuxInfo(const QVariantMap &value)
{
    auxInfo = value;
}


#include "driverinfo.moc"
