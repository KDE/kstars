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

#include <tqstringlist.h>
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
  TQString name;
  TQString hostname;
  TQString portnumber;
  bool isConnected;
  int mgrID;
};

class IDevice : public QObject
{
     Q_OBJECT
     
     public:
        IDevice(TQString inLabel, TQString inDriver, TQString inVersion);
	~IDevice();

      enum ServeMODE { M_LOCAL, M_SERVER };
      TQString label;
      TQString driver;
      TQString version;
      TQStringList serverBuffer;
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
    
class INDIDriver : public devManager
{

   Q_OBJECT

   public:

   INDIDriver(TQWidget * parent = 0);
   ~INDIDriver();

    KListView* deviceContainer;

    bool readXMLDriver();

    bool buildDriversList( XMLEle *root, char errmsg[]);
    bool buildDeviceGroup  (XMLEle *root, char errmsg[]);
    bool buildDriverElement(XMLEle *root, TQListViewItem *DGroup, int groupType, char errmsg[]);

    TQListViewItem *lastGroup;
    TQListViewItem *lastDevice;

    TQStringList driversList;

    TQPixmap runningPix;
    TQPixmap stopPix;
    TQPixmap connected;
    TQPixmap disconnected;
    TQPixmap establishConnection;
    TQPixmap localMode;
    TQPixmap serverMode;

    KPopupMenu *ClientpopMenu;
    KPopupMenu *LocalpopMenu;

    int lastPort;

    bool runDevice(IDevice *dev);
    void removeDevice(IDevice *dev);
    void removeDevice(TQString deviceLabel);
    void saveDevicesToDisk();
    int getINDIPort();
    int activeDriverCount();
    bool isDeviceRunning(TQString deviceLabel);

    void saveHosts();

    std::vector <IDevice *> devices;

     KStars *ksw;


public slots:
    void updateMenuActions();
    void ClientprocessRightButton( TQListViewItem *, const TQPoint &, int );
    void LocalprocessRightButton( TQListViewItem *, const TQPoint &, int );
    void processDevicetqStatus(int);
    void processHostqStatus(int);
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
