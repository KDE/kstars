/*  INDI Driver Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#ifndef DriverManager_H_
#define DriverManager_H_

#include <QFrame>
#include <QHash>
#include <QStringList>
#include <QDialog>

#include <lilxml.h>
#include "ui_drivermanager.h"
#include "indidbus.h"
#include "indicommon.h"

class QTreeWidgetItem;
class QIcon;

class DriverManager;
class ServerManager;
class ClientManager;
class DriverInfo;

class DriverManagerUI : public QFrame, public Ui::DriverManager
{
    Q_OBJECT

public:
    DriverManagerUI(QWidget *parent=0);

    QIcon runningPix;
    QIcon stopPix;
    QIcon connected;
    QIcon disconnected;
    QIcon localMode;
    QIcon serverMode;

public slots:
    void makePortEditable(QTreeWidgetItem* selectedItem, int column);

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

    enum { LOCAL_NAME_COLUMN=0, LOCAL_STATUS_COLUMN, LOCAL_MODE_COLUMN, LOCAL_VERSION_COLUMN, LOCAL_PORT_COLUMN };
    enum { HOST_STATUS_COLUMN=0, HOST_NAME_COLUMN, HOST_PORT_COLUMN };


    bool readXMLDrivers();
    bool readINDIHosts();
    void processXMLDriver(QString & driverName);
    bool buildDeviceGroup  (XMLEle *root, char errmsg[]);
    bool buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, DeviceFamily groupType, char errmsg[]);

    QTreeWidgetItem *lastGroup;
    QTreeWidgetItem *lastDevice;

    int currentPort;

    //DriverInfo::XMLSource xmlSource;
    DriverSource driverSource;

    int getINDIPort(int customPort);
    bool isDeviceRunning(const QString &deviceLabel);

    void saveHosts();

    void processLocalTree(bool dState);
    void processRemoteTree(bool dState);

    DriverInfo * findDriverByName(const QString &name);
    DriverInfo * findDriverByLabel(const QString &label);
    DriverInfo * findDriverByExec(const QString &exec);

    ClientManager *getClientManager(DriverInfo *dv);

    const QList<DriverInfo *> & getDrivers() { return driversList; }

    const QStringList & getDriversStringList() { return driversStringList; }

    void getUniqueHosts(QList<DriverInfo*> & dList, QList < QList<DriverInfo *> > & uHosts);

    void addDriver(DriverInfo *di) { driversList.append(di) ; }
    void removeDriver(DriverInfo *di) { driversList.removeOne(di) ; }

    bool startDevices(QList<DriverInfo*> & dList);
    void stopDevices(const QList<DriverInfo*> & dList);

    bool connectRemoteHost(DriverInfo *dv);
    bool disconnectRemoteHost(DriverInfo *dv);

    QString getUniqueDeviceLabel(const QString &label);

    void clearServers();

protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);

private:
    DriverManager();

    static DriverManager * _DriverManager;

    ServerMode connectionMode;
    DriverManagerUI *ui;

    QList<DriverInfo *> driversList;
    QList<ServerManager *> servers;
    QList<ClientManager *> clients;
    QStringList driversStringList;

    INDIDBus indiDBUS;

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
    void processServerTermination(ServerManager* server);

    void processDeviceStatus(DriverInfo *dv);

signals:
    void clientTerminated(ClientManager *);
    void serverTerminated(const QString & host, const QString & port);

    /*
signals:
    void newDevice();
    void newTelescope();
    void newCCD();
    */
};

#endif
