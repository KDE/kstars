/*
    SPDX-FileCopyrightText: 2001 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_devmanager.h"

#include <QFrame>
#include <QHash>
#include <QIcon>
#include <QList>

#include <lilxml.h>

#include <unistd.h>

class QTreeWidgetItem;

class DeviceManager;
class INDI_D;
class KStars;

struct INDIHostsInfo
{
    QString name;
    QString hostname;
    QString portnumber;
    bool isConnected { false };
    DeviceManager *deviceManager { nullptr };
};

class IDevice : public QObject
{
    Q_OBJECT

  public:
    enum DeviceStatus
    {
        DEV_START,
        DEV_TERMINATE
    };
    enum XMLSource
    {
        PRIMARY_XML,
        THIRD_PARTY_XML,
        EM_XML
    };

    IDevice(const QString &inName, const QString &inLabel, const QString &inDriver, const QString &inVersion);
    ~IDevice();

    void clear();
    QString getServerBuffer();

    QString tree_label;
    QString unique_label;
    QString name;
    QString driver;
    QString version;
    QString id;
    QString port;
    DeviceStatus state;
    XMLSource xmlSource;

    DeviceManager *deviceManager;
    int type { 0 };

    /* Telescope specific attributes */
    double focal_length { 0 };
    double aperture { 0 };
};

class DeviceManagerUI : public QFrame, public Ui::devManager
{
    Q_OBJECT

  public:
    explicit DeviceManagerUI(QWidget *parent = nullptr);

    QIcon runningPix;
    QIcon stopPix;
    QIcon connected;
    QIcon disconnected;
    QIcon localMode;
    QIcon serverMode;

  public slots:
    void makePortEditable(QTreeWidgetItem *selectedItem, int column);
};

class INDIDriver : public QDialog
{
    Q_OBJECT

  public:
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

    explicit INDIDriver(KStars *ks);
    ~INDIDriver();

    bool readXMLDrivers();
    void processXMLDriver(QString &driverName);
    bool buildDeviceGroup(XMLEle *root, char errmsg[]);
    bool buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, int groupType, char errmsg[]);

    int getINDIPort(int customPort);
    bool isDeviceRunning(const QString &deviceLabel);

    void saveHosts();

    void processLocalTree(IDevice::DeviceStatus dev_request);
    void processRemoteTree(IDevice::DeviceStatus dev_request);
    IDevice *findDeviceByLabel(const QString &label);

  public slots:
    void enableDevice(INDI_D *device);
    void disableDevice(INDI_D *device);

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
    void newTelescopeDiscovered();
    void newCCDDiscovered();
    void updateCustomDrivers();

  public:
    KStars *ksw { nullptr };
    DeviceManagerUI *ui { nullptr };
    QList<IDevice *> devices;
    QTreeWidgetItem *lastGroup { nullptr };
    QTreeWidgetItem *lastDevice { nullptr };
    QHash<QString, QString> driversList;
    int currentPort { 0 };
    IDevice::XMLSource xmlSource;

  signals:
    void newDevice();
    void newTelescope();
    void newCCD();
};
