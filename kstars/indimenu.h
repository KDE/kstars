/*  INDI frontend for KStars
    Copyright (C) 2003 Elwood C. Downey

    Adapted to KStars by Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIMENU_H
#define INDIMENU_H

#include <kdialogbase.h>
#include <unistd.h>
#include <vector>

#include "indi/lilxml.h"

class INDI_L;
class INDI_P;
class INDI_G;
class INDI_D;

class KLed;
class KLineEdit;
class KComboBox;
class KDoubleSpinBox;
class KPushButton;

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QLineEdit;
class QString;
class QTextEdit;
class QListView;
class QSocketNotifier;
class QTabWidget;
class QSpacerItem;
class QGridLayout;
class QButtonGroup;
class QCheckBox;
class QScrollView;
class QVBox;

class KStars;
class DeviceManager;

class INDIMenu : public KDialogBase
{
  Q_OBJECT
 public:
   INDIMenu(QWidget * parent = 0 , const char *name = 0);
   ~INDIMenu();

   /*****************************************************************
   * GUI stuff
   ******************************************************************/
   QVBoxLayout	*mainLayout;
   QTabWidget	*deviceContainer;
   QTextEdit 	*msgST_w;
   QWidget	*tab;
   QPushButton  *clear;
   QPushButton  *savelog;

   KStars *ksw;

   XMLAtt *   findAtt     (XMLEle *ep  , const char *name , char errmsg[]);
   XMLEle *   findEle     (XMLEle *ep  , INDI_P *pp, const char *child, char errmsg[]);

   std::vector<DeviceManager *> mgr;

   void updateStatus();
   bool removeDevice(char *devName);
   void removeDeviceMgr(int mgrID);
   void getCustomLabel(const char *deviceName, char *new_label);

   int mgrCounter;
   bool processServer();
   int processClient(QString hostname, QString portnumber);
   INDI_D * findDevice(QString deviceName);
   INDI_D * findDeviceByLabel(char *label);

   signals:

   void driverDisconnected(int mgrID);

};

#endif
