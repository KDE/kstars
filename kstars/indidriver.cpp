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
#include "indimenu.h"
#include "indihostconf.h"

#include "kstars.h"
#include "ksutils.h"

#include <qvaluelist.h>
#include <qcstring.h>

#include <kiconloader.h>
#include <klistview.h>
#include <kpopupmenu.h>
#include <kprocess.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <klineedit.h>

#include <kextsock.h>
#include <unistd.h>

/*
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
INDIDriver::INDIDriver(QWidget *parent) : devManager( parent )

{

    lastPort = 7263;
    lastGroup = NULL;
    lastDevice = NULL;

    ksw = (KStars *) parent;

    //FormLayout =  makeVBoxMainWidget();

    localListView->setSorting(-1);
    clientListView->setSorting(-1);

    KIconLoader *icons = KGlobal::iconLoader();
    runningPix = icons->loadIcon( "exec", KIcon::Small);
    stopPix    = icons->loadIcon( "button_cancel", KIcon::Small);

    LocalpopMenu = new KPopupMenu(localListView);
    LocalpopMenu->insertItem( runningPix, i18n("Run Service") , 0);
    LocalpopMenu->insertItem( stopPix, i18n("Stop Service"), 1);

    localListView->setRootIsDecorated(true);

  connected           = icons->loadIcon( "connect_established", KIcon::Small);
  disconnected        = icons->loadIcon( "connect_no", KIcon::Small);
  establishConnection = icons->loadIcon( "connect_creating", KIcon::Small);

  ClientpopMenu = new KPopupMenu(clientListView);
  ClientpopMenu->insertItem( establishConnection, i18n("Connect") , 0);
  ClientpopMenu->insertItem( disconnected, i18n("Disconnect"), 1);


  for (uint i = 0; i < ksw->data()->INDIHostsList.count(); i++)
  {
  	QListViewItem *item = new QListViewItem(clientListView, lastGroup);
	lastGroup = item;
	item->setPixmap(0, disconnected);
        item->setText(1, ksw->data()->INDIHostsList.at(i)->hostname);
	item->setText(2, ksw->data()->INDIHostsList.at(i)->portnumber);

  }

  lastGroup = NULL;

  QObject::connect(addB, SIGNAL(clicked()), this, SLOT(addINDIHost()));
  QObject::connect(modifyB, SIGNAL(clicked()), this, SLOT(modifyINDIHost()));
  QObject::connect(removeB, SIGNAL(clicked()), this, SLOT(removeINDIHost()));

  QObject::connect(clientListView, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )), this, SLOT(ClientprocessRightButton( QListViewItem *, const QPoint &, int )));

QObject::connect(ClientpopMenu, SIGNAL(activated(int)), this, SLOT(processHostStatus(int)));

QObject::connect(localListView, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )), this, SLOT(LocalprocessRightButton( QListViewItem *, const QPoint &, int )));

QObject::connect(LocalpopMenu, SIGNAL(activated(int)), this, SLOT(processDeviceStatus(int)));

QObject::connect(ksw->getINDIMenu(), SIGNAL(driverDisconnected(int)), this, SLOT(shutdownHost(int)));

   readXMLDriver();

   resize( 500, 300);

}

void INDIDriver::shutdownHost(int mgrID)
{
  QListViewItem *affectedItem;

for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
{
     if (ksw->data()->INDIHostsList.at(i)->mgrID == mgrID)
     {
        affectedItem = clientListView->itemAtIndex(i);
	ksw->data()->INDIHostsList.at(i)->mgrID = -1;
	ksw->data()->INDIHostsList.at(i)->isConnected = false;
        affectedItem->setPixmap(0, disconnected);
	break;
     }
 }
}

void INDIDriver::ClientprocessRightButton( QListViewItem *item, const QPoint &p, int column)
{

  column = 0;

  if (item && item->childCount() == 0)
  	ClientpopMenu->popup(p);
}

void INDIDriver::LocalprocessRightButton( QListViewItem *item, const QPoint &p, int column)
{

  column = 0;

  if (item && item->childCount() == 0)
  	LocalpopMenu->popup(p);
}

void INDIDriver::processDeviceStatus(int id)
{

   for (uint i=0; i < devices.size(); i++)
     if (localListView->selectedItem()->text(0) == devices[i]->label)
     {
	devices[i]->state = id == 0 ? 1 : 0;
	if (devices[i]->state)
	{

	  ksw->getINDIMenu()->setCustomLabel(devices[i]->label);
	  devices[i]->label = ksw->getINDIMenu()->currentLabel;

	  if (!runDevice(devices[i]))
	  {
	   devices[i]->restart();
	   return;
	  }

	  //Allow time for the INDI server to listen
	  usleep(50000);

	  if (!ksw->getINDIMenu()->processServer())
	  {
	   devices[i]->restart();
	   return;
	  }

	  localListView->selectedItem()->setPixmap(1, runningPix);

	  localListView->selectedItem()->setText(3, QString("%1").arg(devices[i]->indiPort));
	  return;
	}

	  ksw->getINDIMenu()->processServer();
	  localListView->selectedItem()->setPixmap(1, stopPix);
	  localListView->selectedItem()->setText(3, QString(""));
	  devices[i]->restart();
	return;
     }
}

bool INDIDriver::runDevice(INDIDriver::IDevice *dev)
{
  dev->indiPort = getINDIPort();

  if (dev->indiPort < 0)
  {
   KMessageBox::error(0, i18n("Cannot start INDI server : port error"));
   return false;
  }

  dev->proc = new KProcess;

  *dev->proc << "indiserver";
  *dev->proc << "-v" << "-r" << "0" << "-p" << QString("%1").arg(dev->indiPort) << dev->exec;



  //connect(dev->proc, SIGNAL(receivedStderr (KProcess *, char *, int)), this, SLOT(processstd(KProcess *, char*, int)));

  //dev->proc->start(KProcess::NotifyOnExit, KProcess::Stderr);
  dev->proc->start();


  return (dev->proc->isRunning());
}

void INDIDriver::processstd(KProcess *proc, char* buffer, int buflen)
{
  //fprintf(stderr, "Receving the following error from child \n\n %s\n", buffer);

  /*typedef QValueList<QCString> strlist;

  strlist list(proc->args());

  strlist::iterator it;

  for (it = list.begin(); it != list.end(); ++it)
  {
    QString thing((*it));
    fprintf(stderr, "%s ", thing.ascii());
  }*/
}

void INDIDriver::removeDevice(INDIDriver::IDevice *dev)
{

  for (unsigned int i=0 ; i < devices.size(); i++)
     if (dev->label == devices[i]->label)
     	devices[i]->restart();
}

void INDIDriver::removeDevice(QString deviceLabel)
{
  for (unsigned int i=0 ; i < devices.size(); i++)
     if (deviceLabel == devices[i]->label)
     	devices[i]->restart();

}

bool INDIDriver::isDeviceRunning(QString deviceLabel)
{

    for (unsigned int i=0 ; i < devices.size(); i++)
     if (deviceLabel == devices[i]->label)
     {
       if (!devices[i]->proc)
        return false;
       else return (devices[i]->proc->isRunning());
     }

    return false;

}


int INDIDriver::getINDIPort()
{

  lastPort+=5;

  KExtendedSocket ks(QString::null, lastPort, KExtendedSocket::passiveSocket | KExtendedSocket::noResolve);

  for (uint i=0 ; i < 10; i++)
  {
    if (ks.listen() < 0)
    {
      lastPort+=5;
      ks.setPort(lastPort);
    }
    else
     return lastPort;
  }

   return -1;
}

bool INDIDriver::readXMLDriver()
{
  QString indiFile("drivers.xml");
  QFile file;
  char errmsg[1024];

  if ( !KSUtils::openDataFile( file, indiFile ) )
  {
     KMessageBox::error(0, i18n("Unable to find device driver file 'drivers.xml'. Please locate the file and place it in one of the following locations:\n\n \t$(KDEDIR)/share/apps/kstars/%1 \n\t~/.kde/share/apps/kstars/%1"));

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

  // avoid overflow
  if (strlen(root->tag) > 1024)
   return false;

  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a group attribute", root->tag);
    return false;
  }


  //KListViewItem *group = new KListViewItem(topItem, lastGroup);
  QListViewItem *group = new QListViewItem(localListView, lastGroup);
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
  char *label;
  char *driver;
  char *exec;
  char *version;

  ap = findXMLAtt(root, "label");
  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a label attribute", root->tag);
    return false;
  }

  label = new char[strlen(ap->valu) + 1];
  strcpy(label, ap->valu);

  ap = findXMLAtt(root, "driver");
  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a driver attribute", root->tag);
    return false;
  }

  driver = new char[strlen(ap->valu) + 1];
  strcpy(driver, ap->valu);

  el = findXMLEle(root, "Exec");

  if (!el)
   return false;

  exec = new char[strlen(ap->valu) + 1];
  strcpy(exec, el->pcdata);

  el = findXMLEle(root, "Version");

  if (!el)
   return false;

  version = new char[strlen(ap->valu) + 1];
  strcpy(version ,el->pcdata);

  QListViewItem *device = new QListViewItem(DGroup, lastDevice);

  device->setText(0, QString(label));
  device->setPixmap(1, stopPix);
  device->setText(2, QString(version));

  lastDevice = device;

  dv = new IDevice(QString(label), QString(driver), QString(exec), QString(version));
  devices.push_back(dv);

  // SLOTS/SIGNAL, pop menu, indi server logic
  return true;
}

INDIDriver::IDevice::IDevice(QString inLabel, QString inDriver, QString inExec, QString inVersion)
{
  label = inLabel;;
  driver = inDriver;;
  exec = inExec;
  version = inVersion;

  // Initially off
  state = 0;

  // No port initially
  indiPort = -1;

  // not yet managed by DeviceManager
  managed = false;

  mgrID = -1;

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

  for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
    if (ksw->data()->INDIHostsList.at(i)->isConnected)
      count++;


  return count;

}

void INDIDriver::IDevice::restart()
{

  mgrID = -1;

  state = 0;

  indiPort = -1;

  if (proc)
  	proc->kill();

  proc = NULL;

}

void INDIDriver::processHostStatus(int id)
{
   int mgrID;
   bool toConnect = (id == 0);
   QListViewItem *currentItem = clientListView->selectedItem();
   INDIHostsInfo *hostInfo;

   for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
   {
     hostInfo = ksw->data()->INDIHostsList.at(i);
     if (currentItem->text(1) == hostInfo->hostname && currentItem->text(2) == hostInfo->portnumber)
     {
        // Nothing changed, return
        if (hostInfo->isConnected == toConnect)
	 return;

	// connect to host
	if (toConnect)
	{
	   // if connection successful
          if ( (mgrID = ksw->getINDIMenu()->processClient(currentItem->text(1), currentItem->text(2))) >= 0)
	  {
	    currentItem->setPixmap(0, connected);
	    hostInfo->isConnected = true;
	    hostInfo->mgrID = mgrID;
	  }
	}
	else
	{
	  ksw->getINDIMenu()->removeDeviceMgr(hostInfo->mgrID);
	  hostInfo->mgrID = mgrID = -1;
	  hostInfo->isConnected = false;
	  currentItem->setPixmap(0, disconnected);
	}


     }
    }
}

void INDIDriver::addINDIHost()
{
  INDIHostConf hostConf(this);
  hostConf.setCaption(i18n("Add Host"));

  if (hostConf.exec() == QDialog::Accepted)
  {
    INDIHostsInfo *hostItem = new INDIHostsInfo;
    hostItem->hostname   = hostConf.hostname->text();
    hostItem->portnumber = hostConf.portnumber->text();

    //search for duplicates
    for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
     if (hostItem->hostname   == ksw->data()->INDIHostsList.at(i)->hostname &&
         hostItem->portnumber == ksw->data()->INDIHostsList.at(i)->portnumber)
     {
       KMessageBox::error(0, i18n("Host: ") + hostItem->hostname + " Port: " + hostItem->portnumber + i18n(" already exist."));
       return;
     }

    ksw->data()->INDIHostsList.append(hostItem);

    QListViewItem *item = new QListViewItem(clientListView);
    item->setPixmap(0, disconnected);
    item->setText(1, hostConf.hostname->text());
    item->setText(2, hostConf.portnumber->text());

  }

    saveHosts();
}



void INDIDriver::modifyINDIHost()
{

  INDIHostConf hostConf(this);
  hostConf.setCaption(i18n("Modify Host"));

  QListViewItem *currentItem = clientListView->currentItem();

  if (currentItem == NULL)
   return;

  hostConf.hostname->setText(currentItem->text(1));
  hostConf.portnumber->setText(currentItem->text(2));

  if (hostConf.exec() == QDialog::Accepted)
  {
    INDIHostsInfo *hostItem = new INDIHostsInfo;
    hostItem->hostname   = hostConf.hostname->text();
    hostItem->portnumber = hostConf.portnumber->text();

    currentItem->setText(1, hostConf.hostname->text());
    currentItem->setText(2, hostConf.portnumber->text());

    ksw->data()->INDIHostsList.replace(clientListView->itemIndex(currentItem), hostItem);

    saveHosts();
  }

}

void INDIDriver::removeINDIHost()
{

 if (clientListView->currentItem() == NULL)
  return;

 if (KMessageBox::questionYesNoCancel( 0, i18n("Are you sure you want to remove the host?"), i18n("Delete confirmation..."))!=KMessageBox::Yes)
   return;

 ksw->data()->INDIHostsList.remove(clientListView->itemIndex(clientListView->currentItem()));
 clientListView->takeItem(clientListView->currentItem());

 saveHosts();

}

void INDIDriver::saveHosts()
{

 QFile file;
 QString hostData;

 file.setName( locateLocal( "appdata", "indihosts.xml" ) ); //determine filename in local user KDE directory tree.

 if ( !file.open( IO_WriteOnly))
 {
  QString message = i18n( "unable to write to file 'indihosts.xml'\nAny changes to INDI hosts configurations will not be saved." );
 KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
  return;
 }

 QTextStream outstream(&file);

 for (uint i= 0; i < ksw->data()->INDIHostsList.count(); i++)
 {

 hostData  = "<INDIHost hostname='";
 hostData += ksw->data()->INDIHostsList.at(i)->hostname;
 hostData += "' port='";
 hostData += ksw->data()->INDIHostsList.at(i)->portnumber;
 hostData += "' />\n";

 outstream << hostData;

 }

  file.close();

}

#include "indidriver.moc"
