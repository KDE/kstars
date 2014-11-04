/***************************************************************************
                          INDI Driver
                             -------------------
    begin                : Wed May 7th 2003
    copyright            : (C) 2001 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef INDIDRIVER_H_
#define INDIDRIVER_H_

#include <QFrame>
#include <QHash>
#include <qstringlist.h>
#include <kdialog.h>
#include <unistd.h>

#include <lilxml.h>
#include "ui_devmanager.h"

class QTreeWidgetItem;
class QIcon;

class INDI_D;
class KStars;
class DeviceManager;

struct INDIHostsInfo
{
    QString name;
    QString hostname;
    QString portnumber;
    bool isConnected;
    DeviceManager *deviceManager;
};

class IDevice : public QObject
{
    Q_OBJECT

public:
    IDevice(const QString &inName, const QString &inLabel, const QString &inDriver, const QString &inVersion);
      ~IDevice();
  
    enum DeviceStatus { DEV_START, DEV_TERMINATE };
    enum XMLSource { PRIMARY_XML, THIRD_PARTY_XML, EM_XML };

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
    int type;
  
      /* Telescope specific attributes */
      double focal_length;
      double aperture;
  
    void clear();
    QString getServerBuffer();

};

class DeviceManagerUI : public QFrame, public Ui::devManager
{
    Q_OBJECT

public:
    DeviceManagerUI(QWidget *parent=0);

    QIcon runningPix;
    QIcon stopPix;
    QIcon connected;
    QIcon disconnected;
    QIcon localMode;
    QIcon serverMode;

public slots:
    void makePortEditable(QTreeWidgetItem* selectedItem, int column);

};

class INDIDriver : public QDialog
{

    Q_OBJECT

public:

    enum { LOCAL_NAME_COLUMN=0, LOCAL_STATUS_COLUMN, LOCAL_MODE_COLUMN, LOCAL_VERSION_COLUMN, LOCAL_PORT_COLUMN };
    enum { HOST_STATUS_COLUMN=0, HOST_NAME_COLUMN, HOST_PORT_COLUMN };

    INDIDriver(KStars *ks);
    ~INDIDriver();

    bool readXMLDrivers();
    void processXMLDriver(QString & driverName);
    bool buildDeviceGroup  (XMLEle *root, char errmsg[]);
    bool buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, int groupType, char errmsg[]);

    KStars *ksw;
    DeviceManagerUI *ui;
    QList<IDevice *> devices;
    QTreeWidgetItem *lastGroup;
    QTreeWidgetItem *lastDevice;
  
    QHash<QString, QString> driversList;
    int currentPort;
    IDevice::XMLSource xmlSource;
  
    int getINDIPort(int customPort);
    bool isDeviceRunning(const QString &deviceLabel);
  
    void saveHosts();
  
    void processLocalTree(IDevice::DeviceStatus dev_request);
    void processRemoteTree(IDevice::DeviceStatus dev_request);
    IDevice * findDeviceByLabel(const QString &label);
  
    

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

signals:
    void newDevice();
    void newTelescope();
    void newCCD();
};

#endif
