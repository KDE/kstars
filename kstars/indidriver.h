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

#ifndef INDIDRIVER_H
#define INDIDRIVER_H

#include <kdialogbase.h>
#include <unistd.h>
#include <vector>

#include "indi/lilxml.h"
#include "devmanager.h"

class KStars;


class KListView;
class KPopupMenu;
class KProcess;

struct INDIHostsInfo
{
  QString hostname;
  QString portnumber;
  bool isConnected;
  int mgrID;
};


class INDIDriver : public devManager
{

   Q_OBJECT

   public:

   INDIDriver(QWidget * parent = 0);
   ~INDIDriver();

    KListView* deviceContainer;

    bool readXMLDriver();

    bool buildDeviceGroup  (XMLEle *root, char errmsg[]);
    bool buildDriverElement(XMLEle *root, QListViewItem *DGroup, char errmsg[]);

    QListViewItem *lastGroup;
    QListViewItem *lastDevice;

    QPixmap runningPix;
    QPixmap stopPix;
    QPixmap connected;
    QPixmap disconnected;
    QPixmap establishConnection;

    KPopupMenu *ClientpopMenu;
    KPopupMenu *LocalpopMenu;

    int lastPort;

    class IDevice
    {
     public:
        IDevice(QString inLabel, QString inDriver, QString inExec, QString inVersion);
	~IDevice();

      QString label;
      QString driver;
      QString exec;
      QString version;
      int state;
      int indiPort;
      bool managed;
      int mgrID;
      KProcess *proc;

      void restart();
    };

    bool runDevice(INDIDriver::IDevice *dev);
    void removeDevice(INDIDriver::IDevice *dev);
    void removeDevice(QString deviceLabel);
    int getINDIPort();
    int activeDriverCount();
    bool isDeviceRunning(QString deviceLabel);

    void saveHosts();

   std::vector <IDevice *> devices;

   KStars *ksw;


public slots:
    void ClientprocessRightButton( QListViewItem *, const QPoint &, int );
    void LocalprocessRightButton( QListViewItem *, const QPoint &, int );
    void processDeviceStatus(int);
    void processHostStatus(int);
    void addINDIHost();
    void modifyINDIHost();
    void removeINDIHost();
    void shutdownHost(int mgrID);
    void processstd(KProcess *proc, char* buffer, int buflen);
};

#endif
