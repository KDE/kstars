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

#include "indidriver.h"

#include "kstars.h"
#include "ksutils.h"

#include <kiconloader.h>
#include <klistview.h>
#include <kpopupmenu.h>
#include <kprocess.h>
#include <kmessagebox.h>



/*
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
INDIDriver::INDIDriver(QWidget *parent, const char *name ) : KDialogBase( parent, name, true, i18n("Driver Control Panel"), KDialogBase::Ok|KDialogBase::Cancel, KDialogBase::Ok)
{

    lastPort = 7263;
    lastGroup = NULL;
    lastDevice = NULL;
    silentRemove = false;

    ksw = (KStars *) parent;

    FormLayout =  makeVBoxMainWidget();

    deviceContainer = new KListView(FormLayout);
    deviceContainer->addColumn( i18n( "Devices" ) );
    deviceContainer->addColumn( i18n( "Status" ) );
    deviceContainer->addColumn( i18n( "Version" ) );
    deviceContainer->setSorting(-1);

    KIconLoader *icons = KGlobal::iconLoader();
    runningPix = icons->loadIcon( "exec", KIcon::Small);
    stopPix    = icons->loadIcon( "button_cancel", KIcon::Small);

    popMenu = new KPopupMenu(deviceContainer);
    popMenu->insertItem( runningPix, i18n("Run Service") , 0);
    popMenu->insertItem( stopPix, i18n("Stop Service"), 1);

    topItem = new QListViewItem(deviceContainer);
    topItem->setPixmap( 0, icons->loadIcon( "gear", KIcon::Small) );

    topItem->setOpen(true);

QObject::connect(deviceContainer, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )), this, SLOT(processRightButton( QListViewItem *, const QPoint &, int )));

QObject::connect(popMenu, SIGNAL(activated(int)), this, SLOT(processDeviceStatus(int)));


    readXMLDriver();

    resize( 400, 300);

}

void INDIDriver::processRightButton( QListViewItem *item, const QPoint &p, int column)
{

  column = 0;

  if (item && item->childCount() == 0)
  	popMenu->popup(p);
}

void INDIDriver::processDeviceStatus(int id)
{

   for (uint i=0; i < devices.size(); i++)
     if (deviceContainer->selectedItem()->text(0) == QString(devices[i]->name))
     {
	devices[i]->state = id == 0 ? 1 : 0;
	if (devices[i]->state)
	{
	  deviceContainer->selectedItem()->setPixmap(1, runningPix);
	  runDevice(devices[i]);
	}
        else
	{
	    silentRemove = true;
	    deviceContainer->selectedItem()->setPixmap(1, stopPix);
	    devices[i]->restart();
	}

	break;
     }
}

bool INDIDriver::runDevice(INDIDriver::IDevice *dev)
{
  dev->indiPort = getINDIPort();
  dev->proc = new KProcess;


  *dev->proc << "indiserver";
  *dev->proc << "-p" << QString("%1").arg(dev->indiPort) << dev->exec;

  dev->proc->start();

  return (dev->proc->isRunning());
}

void INDIDriver::removeDevice(INDIDriver::IDevice *dev)
{

  for (uint i=0 ; i < devices.size(); i++)
     if (!strcmp(dev->name, devices[i]->name))
     {
        kdDebug() << "INDI Driver: removing driver " << dev->name << endl;
	dev->restart();
     }
}

int INDIDriver::getINDIPort()
{

  //TODO might want to use something more reliable
  lastPort++;
  return lastPort;

 }

bool INDIDriver::readXMLDriver()
{
  QString indiFile("drivers.xml");
  QFile file;
  char errmsg[1024];

  if ( !KSUtils::openDataFile( file, indiFile ) )
  {
     KMessageBox::error(0, i18n("Unable to find device driver file 'drivers.xml'. Please locate the file or download it from http://edu.kde.org/kstars and place the file in one of the following locations:\n\n \t$(KDEDIR)/share/apps/kstars/%1 \n\t~/.kde/share/apps/kstars/%1"));

    return false;
 }

 char c;
 LilXML *xmlParser = newLilXML();
 XMLEle *root = NULL;

 while ( (c = (char) file.getch()) != -1)
 {
    root = readXMLEle(xmlParser, c, errmsg);

    if (root)
    {
      if (!buildDeviceGroup(root, errmsg))
         prXMLEle(stderr, root, 0);

      delXMLEle(root);
    }
    else if (errmsg[0])
    {
      kdDebug() << QString(errmsg);
      return false;
    }
  }

 return true;

}

bool INDIDriver::buildDeviceGroup(XMLEle *root, char errmsg[])
{

  XMLAtt *ap;

  // Get device grouping name
  ap = findXMLAtt(root, "group");

  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a group attribute", root->tag);
    return false;
  }


  QListViewItem *group = new QListViewItem(topItem, lastGroup);
  group->setText(0, QString(ap->valu));
  lastGroup = group;
  //group->setOpen(true);


  for (int i = 0; i < root->nel; i++)
	if (!buildDriverElement(root->el[i], group, errmsg))
	  return false;

  return true;
}

bool INDIDriver::buildDriverElement(XMLEle *root, QListViewItem *DGroup, char errmsg[])
{
  XMLAtt *ap;
  XMLEle *el;
  IDevice *dv;
  char name[128];
  char exec[128];
  char version[16];

  ap = findXMLAtt(root, "name");
  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a name attribute", root->tag);
    return false;
  }

  strcpy(name, ap->valu);

  el = findXMLEle(root, "Exec");

  if (!el)
   return false;

  strcpy(exec, el->pcdata);

  el = findXMLEle(root, "Version");

  if (!el)
   return false;

  strcpy(version ,el->pcdata);

  QListViewItem *device = new QListViewItem(DGroup, lastDevice);

  device->setText(0, QString(name));
  device->setPixmap(1, stopPix);
  device->setText(2, QString(version));

  lastDevice = device;

  dv = new IDevice(name, exec, version);
  devices.push_back(dv);


  // SLOTS/SIGNAL, pop menu, indi server logic

  return true;
}

INDIDriver::IDevice::IDevice(char *inName, char * inExec, char *inVersion)
{
  name = new char[128];
  exec = new char [128];
  version = new char[16];

  strcpy(name, inName);
  strcpy(exec, inExec);
  strcpy(version, inVersion);

  // Initially off
  state = 0;

  // No port initially
  indiPort = -1;

  // not yet managed by DeviceManager
  managed = false;

  proc = NULL;

}

INDIDriver::~INDIDriver()
{

  for (uint i=0; i < devices.size(); i++)
   delete (devices[i]);

}

INDIDriver::IDevice::~IDevice()
{
  if (proc)
     proc->kill();

}

int INDIDriver::activeDriverCount()
{
  int count = 0;

  for (uint i=0; i < devices.size(); i++)
    if (devices[i]->state)
      count++;

  return count;

}

void INDIDriver::IDevice::restart()
{

  state = 0;

  indiPort = -1;

  proc->kill();

  proc = NULL;

}
