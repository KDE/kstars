/*  INDI frontend for KStars
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)
    		       Elwood C. Downey

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    JM Changelog:
    2003-04-28 Used indimenu.c as a template. C --> C++, Xm --> KDE/Qt
    2003-05-01 Added tab for devices and a group feature
    2003-05-02 Added scrolling area. Most things are rewritten
    2003-05-05 Adding INDI Conf
    2003-05-06 Drivers XML reader
    2003-05-07 Device manager integration
    2003-05-21 Full client support

 */

#include "indimenu.h"
#include "indiproperty.h"
#include "indigroup.h"
#include "indidevice.h"
#include "devicemanager.h"

#include <stdlib.h>

#include <tqlineedit.h>
#include <tqtextedit.h>
#include <tqframe.h>
#include <tqtabwidget.h>
#include <tqcheckbox.h>
#include <tqlabel.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tqsocketnotifier.h>
#include <tqdatetime.h>
#include <tqvbox.h>
#include <tqtimer.h>

#include <kled.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>
#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <klistview.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <knuminput.h>
#include <kdialogbase.h>

#include "kstars.h"
#include "indidriver.h"

/*******************************************************************
** INDI Menu: Handles communication to server and fetching basic XML
** data.
*******************************************************************/
INDIMenu::INDIMenu(TQWidget *parent, const char *name ) : KDialogBase(KDialogBase::Tabbed, i18n("INDI Control Panel"), 0, KDialogBase::Default, parent, name, false)
{

 ksw = (KStars *) parent;

 mgrCounter = 0;
 
 mgr.setAutoDelete(true);

 currentLabel = "";

 resize( 640, 480);
}

INDIMenu::~INDIMenu()
{
  mgr.clear();
}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropriate action
*********************************************************************/
void INDIMenu::updatetqStatus()
{
   INDIDriver *drivers = ksw->getINDIDriver();

   // Local/Server
   processServer();

   if (drivers)
   {
    if (drivers->activeDriverCount() == 0)
    {
       KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu."));
       return;
    }
   }
   else if (mgr.count() == 0)
   {
      KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu."));
      return;
   }

  show();

}

bool INDIMenu::processServer()
{

INDIDriver *drivers = ksw->getINDIDriver();
DeviceManager *dev;

    if (drivers == NULL)
     return false;

    for (unsigned int i=0; i < drivers->devices.size(); i++)
    {
      // Devices ready to run but not yet managed
      if (drivers->devices[i]->state && drivers->devices[i]->managed == false && drivers->devices[i]->mode == IDevice::M_LOCAL)
      {
        dev = new DeviceManager(this, mgrCounter);
    	if  (dev->indiConnect("localhost", TQString("%1").arg(drivers->devices[i]->indiPort)))
	{
	        drivers->devices[i]->mgrID   = mgrCounter;
	        drivers->devices[i]->managed = true;
      		mgr.append(dev);
		connect(dev, TQT_SIGNAL(newDevice()), drivers, TQT_SLOT(updateMenuActions()));
                connect(dev, TQT_SIGNAL(newDevice()), this, TQT_SLOT(discoverDevice()));

		mgrCounter++;

	}
    	else
	{
      		delete (dev);
		return false;
	}
      }
      // Devices running and they need to be shutdown
      else if (!drivers->devices[i]->state && drivers->devices[i]->managed == true && drivers->devices[i]->mode == IDevice::M_LOCAL)
      {
           drivers->devices[i]->managed = false;
           removeDeviceMgr(drivers->devices[i]->mgrID);
	   return true;

      }
    }

  return true;

  }

int INDIMenu::processClient(TQString hostname, TQString portnumber)
{

  DeviceManager *dev;
  INDIDriver *drivers = ksw->getINDIDriver();

  dev = new DeviceManager(this, mgrCounter);
  if (dev->indiConnect(hostname, portnumber))
  {
      mgr.append(dev);
      if (drivers)
	{
      	connect(dev, TQT_SIGNAL(newDevice()), drivers, TQT_SLOT(updateMenuActions()));
        connect(dev, TQT_SIGNAL(newDevice()), this, TQT_SLOT(discoverDevice()));
	}
  }
  else
  {
     delete (dev);
     return (-1);
  }

 mgrCounter++;
 return (mgrCounter - 1);
}

void INDIMenu::removeDeviceMgr(int mgrID)
{

 for (unsigned int i=0; i < mgr.count(); i++)
 {
   if (mgrID == mgr.at(i)->mgrID)
        mgr.remove(i);

      emit driverDisconnected(mgrID);
 }
}

INDI_D * INDIMenu::findDevice(TQString deviceName)
{
  for (unsigned int i=0; i < mgr.count(); i++)
  {
      for (unsigned int j=0; j < mgr.at(i)->indi_dev.count(); j++)
        if (mgr.at(i)->indi_dev.at(j)->name == deviceName)
       		return mgr.at(i)->indi_dev.at(j);
 }
  return NULL;
}

INDI_D * INDIMenu::findDeviceByLabel(TQString label)
{
  for (unsigned int i=0; i < mgr.count(); i++)
  {
      for (unsigned int j=0; j < mgr.at(i)->indi_dev.count(); j++)
          if (mgr.at(i)->indi_dev.at(j)->label == label)
       		return mgr.at(i)->indi_dev.at(j);
  }
  
  return NULL;
}


void INDIMenu::setCustomLabel(TQString deviceName)
{

 int nset=0;

for (unsigned int i=0; i < mgr.count(); i++)
   for (unsigned int j=0; j < mgr.at(i)->indi_dev.count(); j++)
         if (mgr.at(i)->indi_dev.at(j)->label.find(deviceName) >= 0)
	 	nset++;

if (nset)
 currentLabel = deviceName + TQString(" %1").arg(nset+1);
else
 currentLabel = deviceName;

}

void INDIMenu::discoverDevice()
{
  TQTimer::singleShot( 1000, this, TQT_SLOT(announceDevice()) );
}

void INDIMenu::announceDevice()
{
  newDevice();
}



#include "indimenu.moc"
