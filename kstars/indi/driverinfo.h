/*  INDI Driver Info
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#pragma once

#include "indicommon.h"

#include <QObject>
#include <QVariantMap>

class ClientManager;
class DeviceInfo;
class ServerManager;

/**
 * @class DriverInfo
 * DriverInfo holds all metadata associated with a particular INDI driver.
 * An INDI drivers holds both static and dynamic information. Example for static information:
 * <ul>
 * <li>Device name.</li>
 * <li>Device label.</li>
 * <li>Driver version.</li>
 * <li>Device Family: Telescope, CCD, Focuser...etc</li>
 *
 * Dynamic information include associated Server and Client managers, port in use, associated devices...etc.
 * Most INDI drivers operate only one device, but some driver can present multiple devices simultaneously.
 *
 * Driver information are obtained from multiple sources:
 * <ol>
 * <li>INDI driver XML file: All INDI driver install an XML file (usually to /usr/share/indi) that contains information
 * on the driver and device or devices it is capabale of running.</li>
 * <li>Client DriverInfos: Users can add a new host/port combo in the Device Manager in order to connect to
 * to local or remote INDI servers.</li>
 * <li>Generated DriverInfos: DriverInfo can be created programatically to connect to local or remote INDI server with unknown
 * number of actual drivers/devices at the server side.
 * </ol>
 *
 * @author Jasem Mutlaq
 */
class DriverInfo : public QObject
{
    Q_OBJECT

  public:
    explicit DriverInfo(const QString &inName);
    explicit DriverInfo(DriverInfo *di);
    ~DriverInfo();

    DriverInfo *clone();

    void clear();
    QString getServerBuffer();

    bool isEmpty() { return devices.isEmpty(); }

    const QString &getName() { return name; }
    void setName(const QString &newName) { name = newName; }

    void setTreeLabel(const QString &inTreeLabel) { treeLabel = inTreeLabel; }
    const QString &getTreeLabel() { return treeLabel; }

    void setUniqueLabel(const QString &inUniqueLabel);
    const QString &getUniqueLabel() { return uniqueLabel; }

    void setDriver(const QString &newDriver) { driver = newDriver; }
    const QString &getDriver() { return driver; }

    void setVersion(const QString &newVersion) { version = newVersion; }
    const QString &getVersion() { return version; }

    void setType(DeviceFamily newType) { type = newType; }
    DeviceFamily getType() { return type; }

    void setDriverSource(DriverSource newDriverSource) { driverSource = newDriverSource; }
    DriverSource getDriverSource() { return driverSource; }

    void setServerManager(ServerManager *newServerManager) { serverManager = newServerManager; }
    ServerManager *getServerManager() { return serverManager; }

    void setClientManager(ClientManager *newClientManager) { clientManager = newClientManager; }
    ClientManager *getClientManager() { return clientManager; }

    void setUserPort(const QString &inUserPort);
    const QString &getUserPort() { return userPort; }

    void setSkeletonFile(const QString &inSkeleton) { skelFile = inSkeleton; }
    const QString &getSkeletonFile() { return skelFile; }

    void setServerState(bool inState);
    bool getServerState() { return serverState; }

    void setClientState(bool inState);
    bool getClientState() { return clientState; }

    void setHostParameters(const QString &inHost, const QString &inPort)
    {
        hostname = inHost;
        port     = inPort;
    }
    void setPort(const QString &inPort) { port = inPort; }
    void setHost(const QString &inHost) { hostname = inHost; }
    const QString &getHost() { return hostname; }
    const QString &getPort() { return port; }

    //void setBaseDevice(INDI::BaseDevice *idv) { baseDevice = idv;}
    //INDI::BaseDevice* getBaseDevice() { return baseDevice; }

    void addDevice(DeviceInfo *idv);
    void removeDevice(DeviceInfo *idv);
    DeviceInfo *getDevice(const QString &deviceName);
    QList<DeviceInfo *> getDevices() { return devices; }

    QVariantMap getAuxInfo() const;
    void setAuxInfo(const QVariantMap &value);
    void addAuxInfo(const QString &key, const QVariant &value);

  private:
    /// Actual device name as defined by INDI server
    QString name;
    /// How it appears in the GUI initially as read from source
    QString treeLabel;
    /// How it appears in INDI Menu in case tree_label above is taken by another device
    QString uniqueLabel;
    /// Exec for the driver
    QString driver;
    /// Version of the driver (optional)
    QString version;
    /// INDI server port as the user wants it.
    QString userPort;
    /// Skeleton file, if any;
    QString skelFile;
    /// INDI Host port
    QString port;
    /// INDI Host hostname
    QString hostname;
    /// Device type (Telescope, CCD..etc), if known (optional)
    DeviceFamily type { KSTARS_UNKNOWN };
    /// Is the driver in the server running?
    bool serverState { false };
    /// Is the client connected to the server running the desired driver?
    bool clientState { false };
    /// How did we read the driver information? From XML file? From 3rd party file? ..etc.
    DriverSource driverSource { PRIMARY_XML };
    /// Who is managing this device?
    ServerManager *serverManager { nullptr };
    /// Any GUI client handling this device?
    ClientManager *clientManager { nullptr };
    /// Any additional properties in key, value pairs
    QVariantMap auxInfo;
    QList<DeviceInfo *> devices;

  signals:
    void deviceStateChanged(DriverInfo *);
};
