/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indicommon.h"

#include <QObject>
#include <QVariantMap>
#include <QJsonObject>

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
 * </ul>
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
 * number of actual drivers/devices at the server side.</li>
 * </ol>
 *
 * @author Jasem Mutlaq
 */
class DriverInfo : public QObject
{
        Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer)
        Q_OBJECT

    public:
        explicit DriverInfo(const QString &inName);
        explicit DriverInfo(DriverInfo *di);
        ~DriverInfo();

        DriverInfo *clone(bool resetClone = true);

        QJsonObject toJson() const
        {
            return
            {
                {"name", name},
                {"label", label},
                {"binary", exec},
                {"version", version},
                {"manufacturer", m_Manufacturer},
                {"skel", skelFile},
                {"family", DeviceFamilyLabels[type]},
            };
        }

        void reset();
        void resetDevices()
        {
            devices.clear();
        }
        QString getServerBuffer() const;

        bool isEmpty() const
        {
            return devices.isEmpty();
        }

        // Actual name of the driver
        // i.e. what getDefaultName() returns
        const QString &getName() const
        {
            return name;
        }
        void setName(const QString &newName)
        {
            name = newName;
        }

        // Driver executable
        void setExecutable(const QString &newDriver)
        {
            exec = newDriver;
        }
        const QString &getExecutable() const
        {
            return exec;
        }

        // Driver Label/Alias. We _rename_ the INDI driver in _runtime_ to the label
        // Internally INDI server changes the actual driver "name" above to the label value
        // It's a method of running multiple instances of the same driver with multiple names.
        void setLabel(const QString &inlabel)
        {
            label = inlabel;
        }
        const QString &getLabel() const
        {
            return label;
        }

        void setUniqueLabel(const QString &inUniqueLabel);
        const QString &getUniqueLabel() const
        {
            return uniqueLabel;
        }

        void setVersion(const QString &newVersion)
        {
            version = newVersion;
        }
        const QString &getVersion() const
        {
            return version;
        }

        void setType(DeviceFamily newType)
        {
            type = newType;
        }
        DeviceFamily getType() const
        {
            return type;
        }

        void setDriverSource(DriverSource newDriverSource)
        {
            driverSource = newDriverSource;
        }
        DriverSource getDriverSource() const
        {
            return driverSource;
        }

        void setServerManager(ServerManager *newServerManager)
        {
            serverManager = newServerManager;
        }
        ServerManager *getServerManager() const
        {
            return serverManager;
        }

        void setClientManager(ClientManager *newClientManager)
        {
            clientManager = newClientManager;
        }
        ClientManager *getClientManager() const
        {
            return clientManager;
        }

        void setUserPort(const QString &inUserPort);
        const QString &getUserPort() const
        {
            return userPort;
        }

        void setSkeletonFile(const QString &inSkeleton)
        {
            skelFile = inSkeleton;
        }
        const QString &getSkeletonFile() const
        {
            return skelFile;
        }

        void setServerState(bool inState);
        bool getServerState() const
        {
            return serverState;
        }

        void setClientState(bool inState);
        bool getClientState() const
        {
            return clientState;
        }

        void setHostParameters(const QString &inHost, const QString &inPort)
        {
            hostname = inHost;
            port     = inPort;
        }
        void setPort(const QString &inPort)
        {
            port = inPort;
        }
        void setHost(const QString &inHost)
        {
            hostname = inHost;
        }
        const QString &getHost() const
        {
            return hostname;
        }
        const QString &getPort() const
        {
            return port;
        }

        void setRemotePort(const QString &inPort)
        {
            remotePort = inPort;
        }
        void setRemoteHost(const QString &inHost)
        {
            remoteHostname = inHost;
        }
        const QString &getRemoteHost() const
        {
            return remoteHostname;
        }
        const QString &getRemotePort() const
        {
            return remotePort;
        }

        //void setBaseDevice(INDI::BaseDevice *idv) { baseDevice = idv;}
        //INDI::BaseDevice* getBaseDevice() { return baseDevice; }

        void addDevice(DeviceInfo *idv);
        void removeDevice(DeviceInfo *idv);
        DeviceInfo *getDevice(const QString &deviceName) const;
        QList<DeviceInfo *> getDevices() const
        {
            return devices;
        }

        QVariantMap getAuxInfo() const;
        void setAuxInfo(const QVariantMap &value);
        void addAuxInfo(const QString &key, const QVariant &value);

        QString manufacturer() const;
        void setManufacturer(const QString &Manufacturer);

    private:
        /// Actual device name as defined by INDI server
        QString name;
        /// How it appears in the GUI initially as read from source
        QString label;
        /// How it appears in INDI Menu in case tree_label above is taken by another device
        QString uniqueLabel;
        /// Exec for the driver
        QString exec;
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
        // INDI Remote Hostname (for remote drivers)
        QString remoteHostname;
        // INDI remote port (for remote drivers)
        QString remotePort;
        // Manufacturer
        QString m_Manufacturer;
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
