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
 isClientOn = false;
 createGUI();

}

INDIMenu::~INDIMenu()
{
  for (uint i=0; i < mgr.size(); i++)
    delete (mgr[i]);
}

void INDIMenu::createGUI()
{

 mainLayout = new QVBoxLayout(plainPage(), 5, 5);

 deviceContainer = new QTabWidget(plainPage(), "deviceContainer");
 msgST_w	 = new QTextEdit(plainPage(), "genericMsgForm");
 msgST_w->setReadOnly(true);
 msgST_w->setMaximumHeight(100);

 mainLayout->addWidget(deviceContainer);
 mainLayout->addWidget(msgST_w);

 resize( 640, 480);

}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropiate action
*********************************************************************/
void INDIMenu::updateStatus()
{

  // just in case
  hide();

  INDIDriver *drivers = ksw->getINDIDriver();
  DeviceManager *dev;

  // Local/Server
  if (ksw->options()->isINDILocal)
  {

    if (drivers == NULL)
    {
      KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Driver Control Panel in the devices menu"));
      return;
    }


    for (uint i=0; i < drivers->devices.size(); i++)
    {
      // Devices ready to run but not yet managed
      if (drivers->devices[i]->state && drivers->devices[i]->managed == false)
      {

        kdDebug() << "Device manager will run " << drivers->devices[i]->name << " now" << endl;
	dev = new DeviceManager(this);
	//FIXME is it safe to assume localhost for local connections always?
    	if (dev->indiConnect("localhost", QString("%1").arg(drivers->devices[i]->indiPort)))
	{
      		mgr.push_back(dev);
		drivers->devices[i]->managed = true;
	}
    	else
      		delete (dev);
      }
      // Devices running and they need to be shutdown
      else if (!drivers->devices[i]->state && drivers->devices[i]->managed == true)
      {
         kdDebug() << "Device manager will shutdown " << drivers->devices[i]->name << " now" << endl;
           if (removeDevice(drivers->devices[i]->name))
	     drivers->devices[i]->managed = false;
	   else
	 kdDebug() << "Error removing device" << endl;

      }
    }

    if (drivers->activeDriverCount() == 0)
    {
       KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Driver Control Panel in the devices menu"));
      return;
    }
  }
  // Client
  else
  {

    //TODO
    // JM: The infrastructure that enable KStars to connect and control multiple
    // INDI servers is complete, but I still need to add the GUI part for this
    // For now, only create one client device manager and that's it
     KMessageBox::error(0, i18n("Client support is not complete yet"));
     /*
    if (!isClientOn)
    {
    	dev = new DeviceManager(this);
    	// only one server connection for now
    	if (dev->indiConnect(ksw->options()->INDIHost, ksw->options()->INDIPort))
	{
      		mgr.push_back(dev);
		isClientOn = true;
	}
    	else
	{
		delete (dev);
		return;
	}
    }*/
    return;

  }

  show();

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
