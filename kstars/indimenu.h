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

#include "indielement.h"

class INDI_E;
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
   QString	currentLabel;

   KStars *ksw;

   QPtrList<DeviceManager> mgr;

   void updateStatus();
   //bool removeDevice(QString devName);
   void removeDeviceMgr(int mgrID);
   void setCustomLabel(QString deviceName);

   int mgrCounter;
   bool processServer();
   int processClient(QString hostname, QString portnumber);
   INDI_D * findDevice(QString deviceName);
   INDI_D * findDeviceByLabel(QString label);


   public slots:
   void discoverDevice();
   void announceDevice();

   signals:
   void driverDisconnected(int mgrID);
   void newDevice();

};

#endif
