/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indicommon.h"
#include "customdrivers.h"
#include "ui_drivermanager.h"

#include <QDialog>
#include <QFrame>
#include <QIcon>
#include <QString>
#include <QPointer>
#include <QJsonArray>

#include <lilxml.h>

class QStringList;
class QTreeWidgetItem;

class DriverManager;
class ServerManager;
class ClientManager;
class DriverInfo;

class DriverManagerUI : public QFrame, public Ui::DriverManager
{
        Q_OBJECT

    public:
        explicit DriverManagerUI(QWidget *parent = nullptr);

    public slots:
        void makePortEditable(QTreeWidgetItem *selectedItem, int column);

    public:
        QIcon runningPix;
        QIcon stopPix;
        QIcon connected;
        QIcon disconnected;
        QIcon localMode;
        QIcon serverMode;
};

/**
 * @brief DriverManager is the primary class to handle all operations related to starting and stopping INDI drivers.
 *
 * INDI drivers can be local or remote drivers. For remote hosts, driver information is not known and devices are built
 * as they arrive dynamically. The class parses INDI primary devices XML file (drivers.xml) and any 3rd party INDI Driver
 * XML file to build a tree of devices grouped by driver family type.
 *
 * When starting local drivers, DriverManager also establishes an INDI server with the requested drivers and then connect to
 * the local server to receive the devices dynamically.
 *
 * The class also handles INDI hosts which can be added in order to connect to a local or remote INDI server.
 *
 * @author Jasem Mutlaq
 */
class DriverManager : public QDialog
{
        Q_OBJECT

    public:
        static DriverManager *Instance();

        enum
        {
            LOCAL_NAME_COLUMN = 0,
            LOCAL_STATUS_COLUMN,
            LOCAL_MODE_COLUMN,
            LOCAL_VERSION_COLUMN,
            LOCAL_PORT_COLUMN
        };
        enum
        {
            HOST_STATUS_COLUMN = 0,
            HOST_NAME_COLUMN,
            HOST_PORT_COLUMN
        };

        bool readXMLDrivers();
        bool readINDIHosts();
        void processXMLDriver(const QString &driverName);
        bool buildDeviceGroup(XMLEle *root, char errmsg[]);
        bool buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, DeviceFamily groupType, char errmsg[]);

        int getINDIPort(int customPort);
        bool isDeviceRunning(const QString &deviceLabel);

        void saveHosts();

        void processLocalTree(bool dState);
        void processRemoteTree(bool dState);

        DriverInfo *findDriverByName(const QString &name);
        DriverInfo *findDriverByLabel(const QString &label);
        DriverInfo *findDriverByExec(const QString &exec);

        ClientManager *getClientManager(DriverInfo *dv);

        const QList<DriverInfo *> &getDrivers() const
        {
            return driversList;
        }
        const QList<QVariantMap> &getCustomDrivers() const
        {
            return m_CustomDrivers->customDrivers();
        }
        QJsonArray getDriverList() const;

        const QStringList &getDriversStringList()
        {
            return driversStringList;
        }

        /**
         * @brief getUniqueHosts Given a list of DriverInfos, extract all the host:port information from all the drivers.
         * and then consolidate each groups of drivers that belong to the same server & port to a specific list
         * e.g. If we have driver1 (localhost:7624), driver2(192.168.1.90:7624), driver3(localhost:7624) then this would create
         * two lists. First list contains [driver1,driver3] and second list contains [driver2] making each list _unique_ in terms of host params.
         * @param dList list of driver to examine
         * @param uHosts List of unique hosts, each with a group of drivers that belong to it.
         */
        void getUniqueHosts(QList<DriverInfo *> &dList, QList<QList<DriverInfo *>> &uHosts);

        void addDriver(DriverInfo *di)
        {
            driversList.append(di);
        }
        void removeDriver(DriverInfo *di)
        {
            driversList.removeOne(di);
        }

        bool startDevices(QList<DriverInfo *> &dList);
        void stopDevices(const QList<DriverInfo *> &dList);
        void stopAllDevices()
        {
            stopDevices(driversList);
        }
        bool restartDriver(DriverInfo *dv);

        bool connectRemoteHost(DriverInfo *dv);
        bool disconnectRemoteHost(DriverInfo *dv);

        QString getUniqueDeviceLabel(const QString &label);

        void clearServers();

    private:
        DriverManager(QWidget *parent);
        ~DriverManager();

        bool checkDriverAvailability(const QString &driver);

        static DriverManager *_DriverManager;

        ServerMode connectionMode { SERVER_CLIENT };
        QTreeWidgetItem *lastGroup { nullptr };
        int currentPort;
        //DriverInfo::XMLSource xmlSource;
        DriverSource driverSource;
        DriverManagerUI *ui { nullptr };
        QList<DriverInfo *> driversList;
        QList<ServerManager *> servers;
        QList<ClientManager *> clients;
        QStringList driversStringList;
        QPointer<CustomDrivers> m_CustomDrivers;

    public slots:
        //void enableDevice(INDI_D *device);
        //void disableDevice(INDI_D *device);

        void resizeDeviceColumn();
        void updateLocalTab();
        void updateClientTab();

        void updateMenuActions();

        void addINDIHost();
        void modifyINDIHost();
        void removeINDIHost();
        void activateRunService();
        void activateStopService();
        void activateHostConnection();
        void activateHostDisconnection();

        void updateCustomDrivers();

        void processClientTermination(ClientManager *client);
        void processServerTermination(ServerManager *server);

        void processDeviceStatus(DriverInfo *dv);

        void showCustomDrivers()
        {
            m_CustomDrivers->show();
        }

    signals:
        void clientTerminated(ClientManager *);
        void serverTerminated(const QString &host, const QString &port);
        void serverStarted(const QString &host, const QString &port);

        /*
            signals:
            void newDevice();
            void newTelescope();
            void newCCD();
            */
};
