/*  INDI Device
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "driverinfo.h"

#include "deviceinfo.h"
#include "servermanager.h"

#include <basedevice.h>

#include <QDebug>

DriverInfo::DriverInfo(const QString &inName)
{
    uniqueLabel.clear();
    name = inName;

    hostname = "localhost";
    port     = "-1";
    userPort = "-1";
}

DriverInfo::DriverInfo(DriverInfo *di)
{
    name          = di->getName();
    label         = di->getLabel();
    uniqueLabel   = di->getUniqueLabel();
    exec          = di->getExecutable();
    version       = di->getVersion();
    userPort      = di->getUserPort();
    skelFile      = di->getSkeletonFile();
    port          = di->getPort();
    hostname      = di->getHost();
    remotePort    = di->getRemotePort();
    remoteHostname= di->getRemoteHost();
    type          = di->getType();
    serverState   = di->getServerState();
    clientState   = di->getClientState();
    driverSource  = di->getDriverSource();
    serverManager = di->getServerManager();
    clientManager = di->getClientManager();
    auxInfo       = di->getAuxInfo();
    devices       = di->getDevices();
}

DriverInfo *DriverInfo::clone(bool resetClone)
{
    DriverInfo * clone = new DriverInfo(this);
    if (resetClone)
    {
        clone->reset();
        clone->resetDevices();
    }
    return clone;
}

DriverInfo::~DriverInfo()
{
    qDeleteAll(devices);
}

void DriverInfo::reset()
{
    serverState   = false;
    clientState   = false;    
    serverManager = nullptr;
    clientManager = nullptr;
}

QString DriverInfo::getServerBuffer() const
{
    if (serverManager != nullptr)
        return serverManager->getLogBuffer();

    return QString();
}

void DriverInfo::setServerState(bool inState)
{
    if (inState == serverState)
        return;

    serverState = inState;

    if (serverState == false)
        serverManager = nullptr;

    emit deviceStateChanged(this);
}

void DriverInfo::setClientState(bool inState)
{
    //qDebug() << "Request to change " << name << " client status to " << (inState ? "True" : "False") << endl;

    if (inState == clientState)
        return;

    clientState = inState;

    if (clientState == false)
        clientManager = nullptr;

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

void DriverInfo::removeDevice(DeviceInfo *idv)
{
    devices.removeOne(idv);
    delete (idv);
}

DeviceInfo *DriverInfo::getDevice(const QString &deviceName) const
{
    foreach (DeviceInfo *idv, devices)
    {
        if (idv->getBaseDevice()->getDeviceName() == deviceName)
            return idv;
    }

    return nullptr;
}
QVariantMap DriverInfo::getAuxInfo() const
{
    return auxInfo;
}

void DriverInfo::setAuxInfo(const QVariantMap &value)
{
    auxInfo = value;
}

void DriverInfo::addAuxInfo(const QString &key, const QVariant &value)
{
    auxInfo[key] = value;
}

void DriverInfo::setUniqueLabel(const QString &inUniqueLabel)
{
    // N.B. We NEVER set unique label for multiple devices per driver "driver"
    if (auxInfo.value("mdpd", false).toBool() == true || driverSource >= HOST_SOURCE)
        return;

    uniqueLabel = inUniqueLabel;
}
