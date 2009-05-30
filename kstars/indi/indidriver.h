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
class KProcess;

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

    QString tree_label;
    QString unique_label;
    QString driver_class;
    QString driver;
    QString version;
    DeviceStatus state;
    bool primary_xml;

    DeviceManager *deviceManager;
    int deviceType;
  
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

};

class INDIDriver : public KDialog
{

    Q_OBJECT

public:

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
  
    QStringList driversList;
    int currentPort;
    bool primary_xml;
  
    void saveDevicesToDisk();
    int getINDIPort();
    bool isDeviceRunning(const QString &deviceLabel);
  
    void saveHosts();
  
    void processLocalTree(IDevice::DeviceStatus dev_request);
    void processRemoteTree(IDevice::DeviceStatus dev_request);
  
    

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
    //TODO void shutdownHost(int managersID);
    void activateRunService();
    void activateStopService();
    void activateHostConnection();
    void activateHostDisconnection();
    void newDeviceDiscovered();
    void newTelescopeDiscovered();
    void newCCDDiscovered();

signals:
    void newDevice();
    void newTelescope();
    void newCCD();
};

#endif
