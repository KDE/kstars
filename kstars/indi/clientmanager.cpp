/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientmanager.h"

#include "deviceinfo.h"
#include "drivermanager.h"
#include "guimanager.h"
#include "indilistener.h"
#include "Options.h"
#include "servermanager.h"

#include <indi_debug.h>
#include <QTimer>

ClientManager::ClientManager()
{
    connect(this, &ClientManager::newINDIProperty, this, &ClientManager::processNewProperty, Qt::UniqueConnection);
    connect(this, &ClientManager::removeBLOBManager, this, &ClientManager::processRemoveBLOBManager, Qt::UniqueConnection);
}

bool ClientManager::isDriverManaged(const QSharedPointer<DriverInfo> &driver)
{
    return std::any_of(m_ManagedDrivers.begin(), m_ManagedDrivers.end(), [driver](const auto & oneDriver)
    {
        return driver == oneDriver;
    });
}

void ClientManager::newDevice(INDI::BaseDevice dp)
{
    //setBLOBMode(B_ALSO, dp->getDeviceName());
    // JM 2018.09.27: ClientManager will no longer handle BLOB, just messages.
    // We relay the BLOB handling to BLOB Manager to better manage concurrent connections with large data
    setBLOBMode(B_NEVER, dp.getDeviceName());

    if (QString(dp.getDeviceName()).isEmpty())
    {
        qCWarning(KSTARS_INDI) << "Received invalid device with empty name! Ignoring the device...";
        return;
    }

    qCDebug(KSTARS_INDI) << "Received new device" << dp.getDeviceName();

    // First iteration find unique matches
    for (auto &oneDriverInfo : m_ManagedDrivers)
    {
        if (oneDriverInfo->getUniqueLabel() == QString(dp.getDeviceName()))
        {
            oneDriverInfo->setUniqueLabel(dp.getDeviceName());
            DeviceInfo *devInfo = new DeviceInfo(oneDriverInfo, dp);
            oneDriverInfo->addDevice(devInfo);
            emit newINDIDevice(devInfo);
            return;
        }
    }

    // Second iteration find partial matches

    for (auto &oneDriverInfo : m_ManagedDrivers)
    {
        auto dvName = oneDriverInfo->getName().split(' ').first();
        if (dvName.isEmpty())
            dvName = oneDriverInfo->getName();
        if (/*dv->getUniqueLabel() == dp->getDeviceName() ||*/
            QString(dp.getDeviceName()).startsWith(dvName, Qt::CaseInsensitive) ||
            ((oneDriverInfo->getDriverSource() == HOST_SOURCE || oneDriverInfo->getDriverSource() == GENERATED_SOURCE)))
        {
            oneDriverInfo->setUniqueLabel(dp.getDeviceName());
            DeviceInfo *devInfo = new DeviceInfo(oneDriverInfo, dp);
            oneDriverInfo->addDevice(devInfo);
            emit newINDIDevice(devInfo);
            return;
        }
    }
}

void ClientManager::newProperty(INDI::Property property)
{
    // Do not emit the signal if the server is disconnected or disconnecting (deadlock between signals)
    if (!isServerConnected())
    {
        IDLog("Received new property %s for disconnected device %s, discarding\n", property.getName(), property.getDeviceName());
        return;
    }

    //IDLog("Received new property %s for device %s\n", prop->getName(), prop->getgetDeviceName());
    emit newINDIProperty(property);
}

void ClientManager::updateProperty(INDI::Property property)
{
    emit updateINDIProperty(property);
}

void ClientManager::removeProperty(INDI::Property prop)
{
    const QString name = prop.getName();
    const QString device = prop.getDeviceName();
    emit removeINDIProperty(prop);

    // If BLOB property is removed, remove its corresponding property if one exists.
    if (blobManagers.empty() == false && prop.getType() == INDI_BLOB && prop.getPermission() != IP_WO)
        emit removeBLOBManager(device, name);
}

void ClientManager::processRemoveBLOBManager(const QString &device, const QString &property)
{
    auto manager = std::find_if(blobManagers.begin(), blobManagers.end(), [device, property](auto & oneManager)
    {
        const auto bProperty = oneManager->property("property").toString();
        const auto bDevice = oneManager->property("device").toString();
        return (device == bDevice && property == bProperty);
    });

    if (manager != blobManagers.end())
    {
        (*manager)->disconnectServer();
        (*manager)->deleteLater();
        blobManagers.removeOne(*manager);
    }
}

void ClientManager::processNewProperty(INDI::Property prop)
{
    // Only handle RW and RO BLOB properties
    if (prop.getType() == INDI_BLOB && prop.getPermission() != IP_WO)
    {
        BlobManager *bm = new BlobManager(this, getHost(), getPort(), prop.getDeviceName(), prop.getName());
        connect(bm, &BlobManager::propertyUpdated, this, &ClientManager::updateINDIProperty);
        connect(bm, &BlobManager::connected, this, [prop, this]()
        {
            if (prop && prop.getRegistered())
                emit newBLOBManager(prop.getDeviceName(), prop);
        });
        blobManagers.append(bm);
    }
}

void ClientManager::disconnectAll()
{
    disconnectServer();
    for (auto &oneManager : blobManagers)
        oneManager->disconnectServer();
}

void ClientManager::removeDevice(INDI::BaseDevice dp)
{
    QString deviceName = dp.getDeviceName();

    QMutableListIterator<BlobManager*> it(blobManagers);
    while (it.hasNext())
    {
        auto &oneManager = it.next();
        if (oneManager->property("device").toString() == deviceName)
        {
            oneManager->disconnect();
            it.remove();
        }
    }

    for (auto &driverInfo : m_ManagedDrivers)
    {
        for (auto &deviceInfo : driverInfo->getDevices())
        {
            if (deviceInfo->getDeviceName() == deviceName)
            {
                qCDebug(KSTARS_INDI) << "Removing device" << deviceName;

                emit removeINDIDevice(deviceName);

                driverInfo->removeDevice(deviceInfo);

                if (driverInfo->isEmpty())
                {
                    driverInfo->setClientState(false);
                    m_ManagedDrivers.removeOne(driverInfo);
                }

                return;
            }
        }
    }
}

void ClientManager::newMessage(INDI::BaseDevice dp, int messageID)
{
    emit newINDIMessage(dp, messageID);
}

void ClientManager::newUniversalMessage(std::string message)
{
    emit newINDIUniversalMessage(QString::fromStdString(message));
}


void ClientManager::appendManagedDriver(const QSharedPointer<DriverInfo> &driver)
{
    qCDebug(KSTARS_INDI) << "Adding managed driver" << driver->getName();

    m_ManagedDrivers.append(driver);

    driver->setClientManager(this);

    sManager = driver->getServerManager();
}

void ClientManager::removeManagedDriver(const QSharedPointer<DriverInfo> &driver)
{
    if (m_ManagedDrivers.empty())
    {
        qCDebug(KSTARS_INDI) << "removeManagedDriver: no managed drivers!";
        return;
    }

    qCDebug(KSTARS_INDI) << "Removing managed driver" << driver->getName();

    driver->setClientState(false);
    m_ManagedDrivers.removeOne(driver);

    for (auto &di : driver->getDevices())
    {
        // #1 Remove from GUI Manager
        GUIManager::Instance()->removeDevice(di->getDeviceName());

        // #2 Remove from INDI Listener
        INDIListener::Instance()->removeDevice(di->getDeviceName());

        // #3 Remove device from Driver Info
        driver->removeDevice(di);
    }
}

void ClientManager::serverConnected()
{
    qCDebug(KSTARS_INDI) << "INDI server connected.";

    for (auto &oneDriverInfo : m_ManagedDrivers)
    {
        oneDriverInfo->setClientState(true);
        if (sManager)
            oneDriverInfo->setHostParameters(sManager->getHost(), sManager->getPort());
    }

    m_PendingConnection = false;
    m_ConnectionRetries = MAX_RETRIES;

    emit started();
}

void ClientManager::serverDisconnected(int exitCode)
{
    if (m_PendingConnection)
        qCDebug(KSTARS_INDI) << "INDI server connection refused.";
    else
        qCDebug(KSTARS_INDI) << "INDI server disconnected. Exit code:" << exitCode;

    for (auto &oneDriverInfo : m_ManagedDrivers)
    {
        oneDriverInfo->setClientState(false);
        oneDriverInfo->reset();
    }

    if (m_PendingConnection)
    {
        // Should we retry again?
        if (m_ConnectionRetries-- > 0)
        {
            // Connect again in 1 second.
            QTimer::singleShot(1000, this, [this]()
            {
                qCDebug(KSTARS_INDI) << "Retrying connection again...";
                if (connectServer() == false)
                    serverDisconnected(0);
                else
                    m_PendingConnection = false;
            });
        }
        // Nope cannot connect to server.
        else
        {
            m_PendingConnection = false;
            m_ConnectionRetries = MAX_RETRIES;
            emit failed(i18n("Failed to connect to INDI server %1:%2", getHost(), getPort()));
        }
    }
    // Did server disconnect abnormally?
    else if (exitCode < 0)
        emit terminated(i18n("Connection to INDI host at %1 on port %2 lost. Server disconnected: %3", getHost(), getPort(),
                             exitCode));
}

const QList<QSharedPointer<DriverInfo>> &ClientManager::getManagedDrivers() const
{
    return m_ManagedDrivers;
}

void ClientManager::establishConnection()
{
    qCDebug(KSTARS_INDI)
            << "INDI: Connecting to local INDI server on port " << getPort() << " ...";

    m_PendingConnection = true;
    m_ConnectionRetries = 2;

    if (connectServer() == false)
        serverDisconnected(0);
    else
        m_PendingConnection = false;
}

const QSharedPointer<DriverInfo> &ClientManager::findDriverInfoByName(const QString &name)
{
    auto pos = std::find_if(m_ManagedDrivers.begin(), m_ManagedDrivers.end(), [name](QSharedPointer<DriverInfo> oneDriverInfo)
    {
        return oneDriverInfo->getName() == name;
    });

    return *pos;
}

const QSharedPointer<DriverInfo> &ClientManager::findDriverInfoByLabel(const QString &label)
{
    auto pos = std::find_if(m_ManagedDrivers.begin(), m_ManagedDrivers.end(), [label](QSharedPointer<DriverInfo> oneDriverInfo)
    {
        return oneDriverInfo->getLabel() == label;
    });

    return *pos;
}

void ClientManager::setBLOBEnabled(bool enabled, const QString &device, const QString &property)
{
    for(auto &bm : blobManagers)
    {
        if (bm->property("device") == device && (property.isEmpty() || bm->property("property") == property))
        {
            bm->setEnabled(enabled);
            return;
        }
    }
}

bool ClientManager::isBLOBEnabled(const QString &device, const QString &property)
{
    for(auto &bm : blobManagers)
    {
        if (bm->property("device") == device && bm->property("property") == property)
            return bm->property("enabled").toBool();
    }

    return false;
}
