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

class TQLabel;
class TQHBoxLayout;
class TQVBoxLayout;
class TQFrame;
class TQLineEdit;
class TQString;
class TQTextEdit;
class TQListView;
class TQSocketNotifier;
class TQTabWidget;
class TQSpacerItem;
class TQGridLayout;
class TQButtonGroup;
class TQCheckBox;
class TQScrollView;
class TQVBox;

class KStars;
class DeviceManager;

class INDIMenu : public KDialogBase
{
  Q_OBJECT
 public:
   INDIMenu(TQWidget * parent = 0 , const char *name = 0);
   ~INDIMenu();

   /*****************************************************************
   * GUI stuff
   ******************************************************************/
   QVBoxLayout	*mainLayout;
   QTabWidget	*deviceContainer;
   TQTextEdit 	*msgST_w;
   QWidget	*tab;
   TQPushButton  *clear;
   QString	currentLabel;

   KStars *ksw;

   TQPtrList<DeviceManager> mgr;

   void updateStatus();
   //bool removeDevice(TQString devName);
   void removeDeviceMgr(int mgrID);
   void setCustomLabel(TQString deviceName);

   int mgrCounter;
   bool processServer();
   int processClient(TQString hostname, TQString portnumber);
   INDI_D * findDevice(TQString deviceName);
   INDI_D * findDeviceByLabel(TQString label);


   public slots:
   void discoverDevice();
   void announceDevice();

   signals:
   void driverDisconnected(int mgrID);
   void newDevice();

};

#endif
