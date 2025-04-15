/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

    m_Manufacturer = "Others";
    hostname = "localhost";
    port     = -1;
    userPort = -1;
}

DriverInfo::DriverInfo(DriverInfo *driver)
{
    name          = driver->getName();
    label         = driver->getLabel();
    uniqueLabel   = driver->getUniqueLabel();
    exec          = driver->getExecutable();
    version       = driver->getVersion();
    m_Manufacturer = driver->manufacturer();
    userPort      = driver->getUserPort();
    skelFile      = driver->getSkeletonFile();
    port          = driver->getPort();
    hostname      = driver->getHost();
    remotePort    = driver->getRemotePort();
    remoteHostname = driver->getRemoteHost();
    type          = driver->getType();
    serverState   = driver->getServerState();
    clientState   = driver->getClientState();
    driverSource  = driver->getDriverSource();
    serverManager = driver->getServerManager();
    clientManager = driver->getClientManager();
    auxInfo       = driver->getAuxInfo();
    devices       = driver->getDevices();
}

QSharedPointer<DriverInfo> DriverInfo::clone(bool resetClone)
{
    QSharedPointer<DriverInfo> clone(new DriverInfo(this));
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

    emit deviceStateChanged();
}

void DriverInfo::setClientState(bool inState)
{
    if (inState == clientState)
        return;

    clientState = inState;

    if (clientState == false)
        clientManager = nullptr;

    emit deviceStateChanged();
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
        if (idv->getDeviceName() == deviceName)
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

QString DriverInfo::manufacturer() const
{
    return m_Manufacturer;
}

void DriverInfo::setManufacturer(const QString &Manufacturer)
{
    m_Manufacturer = Manufacturer;
}

void DriverInfo::setUniqueLabel(const QString &inUniqueLabel)
{
    // N.B. We NEVER set unique label for multiple devices per driver "driver"
    if (auxInfo.value("mdpd", false).toBool() == true || driverSource >= HOST_SOURCE)
        return;

    uniqueLabel = inUniqueLabel;
}

QJsonObject DriverInfo::startupShutdownRule() const
{
    return m_StartupShutdownRule;
}

void DriverInfo::setStartupShutdownRule(const QJsonObject &value)
{
    m_StartupShutdownRule = value;
}
