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

#include <QLineEdit>
#include <QTextEdit>
#include <QFrame>
#include <QTabWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QSocketNotifier>
#include <QDateTime>
#include <QTimer>

#include <kled.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>
#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <k3listview.h>
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
INDIMenu::INDIMenu(QWidget *parent, const char * name ) : QWidget(parent, name, Qt::Window)
/*KDialogBase(KDialogBase::Tabbed, i18n("INDI Control Panel"), 0, KDialogBase::Default, parent, name, false)*/
{

 ksw = (KStars *) parent;

 mgrCounter = 0;
 
// mgr.setAutoDelete(true);

 mainLayout    = new QVBoxLayout(this, 10, 10);

 mainTabWidget = new QTabWidget(this);

 mainLayout->addWidget(mainTabWidget);

 currentLabel = QString();

 setWindowTitle(i18n("INDI Control Panel"));
 setAttribute(Qt::WA_ShowModal, false);
 //setModal(false);

 resize( 640, 480);
}

INDIMenu::~INDIMenu()
{
  while ( ! mgr.isEmpty() ) delete mgr.takeFirst();
}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropriate action
*********************************************************************/
void INDIMenu::updateStatus()
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
   else if (mgr.size() == 0)
   {
      KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu."));
      return;
   }

  //deviceContainer->show();
  show();

}

bool INDIMenu::processServer()
{

INDIDriver *drivers = ksw->getINDIDriver();
DeviceManager *dev;

    if (drivers == NULL)
     return false;

    //for (unsigned int i=0; i < drivers->devices.size(); i++)
    foreach (IDevice *device, drivers->devices)
    {
      // Devices ready to run but not yet managed
      if (device->state && device->managed == false && device->mode == IDevice::M_LOCAL)
      {
        dev = new DeviceManager(this, mgrCounter);
    	if  (dev->indiConnect("localhost", QString("%1").arg(device->indiPort)))
	{
	        device->mgrID   = mgrCounter;
	        device->managed = true;
      		mgr.append(dev);
		connect(dev, SIGNAL(newDevice()), drivers, SLOT(updateMenuActions()));
                connect(dev, SIGNAL(newDevice()), this, SLOT(discoverDevice()));

		mgrCounter++;

	}
    	else
	{
      		delete (dev);
		return false;
	}
      }
      // Devices running and they need to be shutdown
      else if (!device->state && device->managed == true && device->mode == IDevice::M_LOCAL)
      {
           device->managed = false;
           removeDeviceMgr(device->mgrID);
	   return true;

      }
    }

  return true;

  }

int INDIMenu::processClient(const QString &hostname, const QString &portnumber)
{

  DeviceManager *dev;
  INDIDriver *drivers = ksw->getINDIDriver();

  dev = new DeviceManager(this, mgrCounter);
  if (dev->indiConnect(hostname, portnumber))
  {
      mgr.append(dev);
      if (drivers)
	{
      	connect(dev, SIGNAL(newDevice()), drivers, SLOT(updateMenuActions()));
        connect(dev, SIGNAL(newDevice()), this, SLOT(discoverDevice()));
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

  for (int i=0; i < mgr.size(); i++)
  {
    if (mgrID == mgr.at(i)->mgrID)
	{
      		delete mgr.takeAt(i);
	        emit driverDisconnected(mgrID);
	}
  }

  //FIXME try to hide unnecessary left over, only leave the msg box
  // don't use KDialogBase.. just a regular QWidget will suffice
     //if (mgr.empty())
	//deviceContainer->hide();
}

INDI_D * INDIMenu::findDevice(const QString &deviceName)
{
  for (int i=0; i < mgr.size(); i++)
    for (int j=0; j < mgr[i]->indi_dev.size(); j++)
      if (mgr[i]->indi_dev[j]->name == deviceName)
        return mgr[i]->indi_dev[j];

  return NULL;
}

INDI_D * INDIMenu::findDeviceByLabel(const QString &label)
{
  for (int i=0; i < mgr.size(); i++)
    for (int j=0; j < mgr[i]->indi_dev.size(); j++)
      if (mgr[i]->indi_dev[j]->label == label)
        return mgr[i]->indi_dev[j];

  return NULL;
}


void INDIMenu::setCustomLabel(const QString &deviceName)
{
  int nset=0;

  for (int i=0; i < mgr.size(); i++)
    for (int j=0; j < mgr[i]->indi_dev.size(); j++)
      if (mgr[i]->indi_dev[j]->label.find(deviceName) >= 0)
        nset++;

  if (nset)
    currentLabel = deviceName + QString(" %1").arg(nset+1);
  else
    currentLabel = deviceName;
}

void INDIMenu::discoverDevice()
{
  QTimer::singleShot( 1000, this, SLOT(announceDevice()) );
}

void INDIMenu::announceDevice()
{
  newDevice();
}



#include "indimenu.moc"
