/*  INDI frontend for KStars
    Copyright (C) 2003 Elwood C. Downey
    		       Jasem Mutlaq

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
#include "indidevice.h"

#include <stdlib.h>

#include <qlineedit.h>
#include <qtextedit.h>
#include <qframe.h>
#include <qtabwidget.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qsocketnotifier.h>
#include <qdatetime.h>
#include <qvbox.h>

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
INDIMenu::INDIMenu(QWidget *parent, const char *name ) : KDialogBase(KDialogBase::Plain, i18n("INDI Control Panel"), 0, KDialogBase::Default, parent, name)
{

 ksw = (KStars *) parent;

 mgrCounter = 0;

 mainLayout = new QVBoxLayout(plainPage(), 5, 5);

 deviceContainer = new QTabWidget(plainPage(), "deviceContainer");
 msgST_w	 = new QTextEdit(plainPage(), "genericMsgForm");
 msgST_w->setReadOnly(true);
 msgST_w->setMaximumHeight(100);

 mainLayout->addWidget(deviceContainer);
 mainLayout->addWidget(msgST_w);

 resize( 640, 480);
}

INDIMenu::~INDIMenu()
{
  for (uint i=0; i < mgr.size(); i++)
    delete (mgr[i]);
}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropiate action
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
       KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu"));
       return;
    }
   }
   else if (mgr.size() == 0)
   {
      KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu"));
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

    for (uint i=0; i < drivers->devices.size(); i++)
    {
      // Devices ready to run but not yet managed
      if (drivers->devices[i]->state && drivers->devices[i]->managed == false)
      {
        dev = new DeviceManager(this, mgrCounter);
    	if  (dev->indiConnect("localhost", QString("%1").arg(drivers->devices[i]->indiPort)))
	{
	        drivers->devices[i]->mgrID   = mgrCounter;
	        drivers->devices[i]->managed = true;
      		mgr.push_back(dev);
		mgrCounter++;

	}
    	else
	{
      		delete (dev);
		return false;
	}
      }
      // Devices running and they need to be shutdown
      else if (!drivers->devices[i]->state && drivers->devices[i]->managed == true)
      {
           removeDeviceMgr(drivers->devices[i]->mgrID);
	   drivers->devices[i]->managed = false;
	   return true;

      }
    }

  return true;

  }

int INDIMenu::processClient(QString hostname, QString portnumber)
{

  DeviceManager *dev;

  kdDebug() << "in process Client" << endl;
  dev = new DeviceManager(this, mgrCounter);
  if (dev->indiConnect(hostname, portnumber))
      mgr.push_back(dev);
  else
  {
     delete (dev);
     return (-1);
  }

 mgrCounter++;
 return (mgrCounter - 1);
}

bool INDIMenu::removeDevice(char *devName)
{

  char errmsg[1024];

  for (uint i=0; i < mgr.size(); i++)
    if (!mgr[i]->removeDevice(devName, errmsg))
    {
      if (mgr[i]->indi_dev.size() == 0)
        delete mgr[i];

      mgr.erase(mgr.begin() + i);
      return true;
    }

  kdDebug() << errmsg;

  return false;

}

void INDIMenu::removeDeviceMgr(int mgrID)
{
 char errmsg[1024];

 for (uint i=0; i < mgr.size(); i++)
  {
       if (mgrID == mgr[i]->mgrID)
       {

      for (uint j=0; j < mgr[i]->indi_dev.size(); j++)
         mgr[i]->removeDevice(mgr[i]->indi_dev[j]->name, errmsg);
       delete mgr[i];
       mgr.erase(mgr.begin() + i);

       emit driverDisconnected(mgrID);
       }
  }

}

/* search element for attribute.
 * return XMLAtt if find, else NULL with helpful info in errmsg.
 */
XMLAtt * INDIMenu::findAtt (XMLEle *ep, const char *name, char errmsg[])
{
	XMLAtt *ap = findXMLAtt (ep, name);
	if (ap)
	    return (ap);
	if (errmsg)
	    sprintf (errmsg, "INDI: <%s> missing attribute '%s'", ep->tag,name);
	return NULL;
}

/* search element for given child. pp is just to build a better errmsg.
 * return XMLEle if find, else NULL with helpful info in errmsg.
 */
XMLEle * INDIMenu::findEle (XMLEle *ep, INDI_P *pp, const char *child, char errmsg[])
{
	XMLEle *cp = findXMLEle (ep, child);
	if (cp)
	    return (cp);
	if (errmsg)
	    sprintf (errmsg, "INDI: <%s %s %s> missing child '%s'", ep->tag,
						pp->pg->dp->name, pp->name, child);
	return (NULL);
}



#include "indimenu.moc"
