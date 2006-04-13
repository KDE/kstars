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
#include <kdialogbase.h>
#include <unistd.h>

#include "indi/lilxml.h"
#include "ui_devmanager.h"

class QTreeWidgetItem;
class QListWidgetItem;
class QIcon;

class KStars;

class K3ListView;
class KMenu;
class KProcess;

struct INDIHostsInfo
{
  QString name;
  QString hostname;
  QString portnumber;
  bool isConnected;
  int mgrID;
};

class IDevice : public QObject
{
     Q_OBJECT
     
     public:
        IDevice(const QString &inLabel, const QString &inDriver, const QString &inVersion);
	~IDevice();

      enum ServeMODE { M_LOCAL, M_SERVER };
      QString label;
      QString driver;
      QString version;
      QString serverBuffer;
      int state;
      int mode;
      int indiPort;
      bool managed;
      int mgrID;
      int deviceType;
      KProcess *proc;

      /* Telescope specific attributes */
      double focal_length;
      double aperture;

      void restart();
      
      public slots:
      void processstd(KProcess *proc, char* buffer, int buflen);
      
      signals:
      void newServerInput();
      
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
   QIcon establishConnection;
   QIcon localMode;
   QIcon serverMode;

};

class INDIDriver : public KDialogBase
{

   Q_OBJECT

   public:

   INDIDriver(QWidget * parent = 0);
   ~INDIDriver();

    //K3ListView* deviceContainer;

    bool readXMLDriver();

    bool buildDriversList( XMLEle *root, char errmsg[]);
    bool buildDeviceGroup  (XMLEle *root, char errmsg[]);
    bool buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, int groupType, char errmsg[]);

    KStars *ksw;
    DeviceManagerUI *ui;

    QTreeWidgetItem *lastGroup;
    QTreeWidgetItem *lastDevice;

    QStringList driversList;

    int currentPort;

    bool runDevice(IDevice *dev);
    void removeDevice(IDevice *dev);
    void removeDevice(const QString &deviceLabel);
    void saveDevicesToDisk();
    int getINDIPort();
    int activeDriverCount();
    bool isDeviceRunning(const QString &deviceLabel);

    void saveHosts();

    QList<IDevice *> devices;

public slots:
    void updateMenuActions();
    void processDeviceStatus(int);
    void processHostStatus(int);
    void addINDIHost();
    void modifyINDIHost();
    void removeINDIHost();
    void shutdownHost(int mgrID);
    void updateLocalButtons();
    void updateClientButtons();
    void activateRunService();
    void activateStopService();
    void activateHostConnection();
    void activateHostDisconnection();
};

#endif
