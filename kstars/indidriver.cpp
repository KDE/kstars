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
        item->setText(1, ksw->data()->INDIHostsList.at(i)->name);
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

QObject::connect(connectHostB, SIGNAL(clicked()), this, SLOT(activateHostConnection()));
QObject::connect(disconnectHostB, SIGNAL(clicked()), this, SLOT(activateHostDisconnection()));

QObject::connect(runServiceB, SIGNAL(clicked()), this, SLOT(activateRunService()));
QObject::connect(stopServiceB, SIGNAL(clicked()), this, SLOT(activateStopService()));

QObject::connect(localListView, SIGNAL(selectionChanged()), this, SLOT(updateLocalButtons()));
QObject::connect(clientListView, SIGNAL(selectionChanged()), this, SLOT(updateClientButtons()));

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
	connectHostB->setEnabled(true);
        disconnectHostB->setEnabled(false);
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

void INDIDriver::activateRunService()
{
  processDeviceStatus(0);
}

void INDIDriver::activateStopService()
{
  processDeviceStatus(1);
}

void INDIDriver::activateHostConnection()
{
  processHostStatus(0);
}

void INDIDriver::activateHostDisconnection()
{
  processHostStatus(1);
}
    
void INDIDriver::updateLocalButtons()
{
  
  if (localListView->selectedItem() == NULL)
   return;
 
  for (uint i=0; i < devices.size(); i++)
     if (localListView->selectedItem()->text(0) == devices[i]->label)
     {
	runServiceB->setEnabled(devices[i]->state == 0);
	stopServiceB->setEnabled(devices[i]->state == 1);
	break;
     }

}

void INDIDriver::updateClientButtons()
{
 INDIHostsInfo *hostInfo;
 if (clientListView->currentItem() == NULL)
  return;


for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
   {
     hostInfo = ksw->data()->INDIHostsList.at(i);
     if (clientListView->currentItem()->text(1) == hostInfo->name && clientListView->currentItem()->text(2) == hostInfo->portnumber)
     {
       connectHostB->setEnabled(!hostInfo->isConnected);
       disconnectHostB->setEnabled(hostInfo->isConnected);
       break;
     }
    }
   
}

    
void INDIDriver::processDeviceStatus(int id)
{
  if (localListView->selectedItem() == NULL)
    return;

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
	  runServiceB->setEnabled(false);
	  stopServiceB->setEnabled(true);
	  return;
	}

	  ksw->getINDIMenu()->processServer();
	  localListView->selectedItem()->setPixmap(1, stopPix);
	  localListView->selectedItem()->setText(3, QString(""));
	  runServiceB->setEnabled(true);
	  stopServiceB->setEnabled(false);
	  devices[i]->restart();
	return;
     }
}

void INDIDriver::processHostStatus(int id)
{
   int mgrID;
   bool toConnect = (id == 0);
   QListViewItem *currentItem = clientListView->selectedItem();
   if (!currentItem) return;
   INDIHostsInfo *hostInfo;

   //kdDebug() << "The INDI host size is " << ksw->data()->INDIHostsList.count() << endl;
   //for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
     //kdDebug() << "The name is " << ksw->data()->INDIHostsList.at(i)->name << endl;


   for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
   {
     hostInfo = ksw->data()->INDIHostsList.at(i);
     if (currentItem->text(1) == hostInfo->name && currentItem->text(2) == hostInfo->portnumber)
     {
        // Nothing changed, return
        if (hostInfo->isConnected == toConnect)
	 return;

	// connect to host
	if (toConnect)
	{
	   // if connection successful
          if ( (mgrID = ksw->getINDIMenu()->processClient(hostInfo->hostname, hostInfo->portnumber)) >= 0)
	  {
	    currentItem->setPixmap(0, connected);
	    hostInfo->isConnected = true;
	    hostInfo->mgrID = mgrID;
	    connectHostB->setEnabled(false);
	    disconnectHostB->setEnabled(true);
	  }
	}
	else
	{
	  ksw->getINDIMenu()->removeDeviceMgr(hostInfo->mgrID);
	  hostInfo->mgrID = mgrID = -1;
	  hostInfo->isConnected = false;
	  currentItem->setPixmap(0, disconnected);
	  connectHostB->setEnabled(true);
	  disconnectHostB->setEnabled(false);
	}


     }
    }
}

bool INDIDriver::runDevice(INDIDriver::IDevice *dev)
{
  dev->indiPort = getINDIPort();

  if (dev->indiPort < 0)
  {
   KMessageBox::error(0, i18n("Cannot start INDI server : port error."));
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

 delLilXML(xmlParser);
 return true;

}

bool INDIDriver::buildDeviceGroup(XMLEle *root, char errmsg[])
{

  XMLAtt *ap;
  XMLEle *ep;
  QString groupName;
  int groupType;

  // Get device grouping name
  ap = findXMLAtt(root, "group");

  // avoid overflow
  if (strlen(tagXMLEle(root)) > 1024)
   return false;

  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a group attribute", tagXMLEle(root));
    return false;
  }
  
  groupName = valuXMLAtt(ap);
  
  if (groupName.find("Telescopes") != -1)
    groupType = KSTARS_TELESCOPE;
  else if (groupName.find("CCDs") != -1)
    groupType = KSTARS_CCD;
  else if (groupName.find("Focusers") != -1)
    groupType = KSTARS_FOCUSER;
  else if (groupName.find("Domes") != -1)
    groupType = KSTARS_DOME;
  else if (groupName.find("GPS") != -1)
    groupType = KSTARS_GPS;


  //KListViewItem *group = new KListViewItem(topItem, lastGroup);
  QListViewItem *group = new QListViewItem(localListView, lastGroup);
  group->setText(0, groupName);
  lastGroup = group;
  //group->setOpen(true);


  for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
  /*for (int i = 0; i < root->nel; i++)*/
	if (!buildDriverElement(ep, group, groupType, errmsg))
	  return false;

  return true;
}

bool INDIDriver::buildDriverElement(XMLEle *root, QListViewItem *DGroup, int groupType, char errmsg[])
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
    sprintf(errmsg, "Tag %s does not have a label attribute", tagXMLEle(root));
    return false;
  }

  label = new char[strlen(valuXMLAtt(ap)) + 1];
  strcpy(label, valuXMLAtt(ap));

  ap = findXMLAtt(root, "driver");
  if (!ap)
  {
    sprintf(errmsg, "Tag %s does not have a driver attribute", tagXMLEle(root));
    return false;
  }

  driver = new char[strlen(valuXMLAtt(ap)) + 1];
  strcpy(driver, valuXMLAtt(ap));

  el = findXMLEle(root, "Exec");

  if (!el)
   return false;

  exec = new char[strlen(valuXMLAtt(ap)) + 1];
  strcpy(exec, pcdataXMLEle(el));

  el = findXMLEle(root, "Version");

  if (!el)
   return false;

  version = new char[strlen(valuXMLAtt(ap)) + 1];
  strcpy(version , pcdataXMLEle(el));

  QListViewItem *device = new QListViewItem(DGroup, lastDevice);

  device->setText(0, QString(label));
  device->setPixmap(1, stopPix);
  device->setText(2, QString(version));

  lastDevice = device;

  dv = new IDevice(QString(label), QString(driver), QString(exec), QString(version));
  dv->deviceType = groupType;
  
  devices.push_back(dv);

  delete [] label;
  delete [] driver;
  delete [] exec;
  delete [] version;

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


void INDIDriver::addINDIHost()
{
  INDIHostConf hostConf(this);
  hostConf.setCaption(i18n("Add Host"));
  bool portOk = false;

  if (hostConf.exec() == QDialog::Accepted)
  {
    INDIHostsInfo *hostItem = new INDIHostsInfo;
    hostItem->name        = hostConf.nameIN->text();
    hostItem->hostname    = hostConf.hostname->text();
    hostItem->portnumber  = hostConf.portnumber->text();
    hostItem->isConnected = false;
    hostItem->mgrID       = -1;

    hostItem->portnumber.toInt(&portOk);

    if (portOk == false)
    {
     KMessageBox::error(0, i18n("Error: the port number is invalid."));
     return;
    }

    //search for duplicates
    for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
     if (hostItem->name   == ksw->data()->INDIHostsList.at(i)->name &&
         hostItem->portnumber == ksw->data()->INDIHostsList.at(i)->portnumber)
     {
       KMessageBox::error(0, i18n("Host: %1 Port: %2 already exists.").arg(hostItem->name).arg(hostItem->portnumber));
       return;
     }

    ksw->data()->INDIHostsList.append(hostItem);

    QListViewItem *item = new QListViewItem(clientListView);
    item->setPixmap(0, disconnected);
    item->setText(1, hostConf.nameIN->text());
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

  for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
     if (currentItem->text(1) == ksw->data()->INDIHostsList.at(i)->name &&
         currentItem->text(2) == ksw->data()->INDIHostsList.at(i)->portnumber)
     {

        hostConf.nameIN->setText(ksw->data()->INDIHostsList.at(i)->name);
  	hostConf.hostname->setText(ksw->data()->INDIHostsList.at(i)->hostname);
  	hostConf.portnumber->setText(ksw->data()->INDIHostsList.at(i)->portnumber);

  	if (hostConf.exec() == QDialog::Accepted)
  	{
    	INDIHostsInfo *hostItem = new INDIHostsInfo;
	hostItem->name       = hostConf.nameIN->text();
    	hostItem->hostname   = hostConf.hostname->text();
    	hostItem->portnumber = hostConf.portnumber->text();

    	currentItem->setText(1, hostConf.nameIN->text());
    	currentItem->setText(2, hostConf.portnumber->text());

    	ksw->data()->INDIHostsList.replace(clientListView->itemIndex(currentItem), hostItem);

    	saveHosts();
  	}

     }
}

void INDIDriver::removeINDIHost()
{

 if (clientListView->currentItem() == NULL)
  return;

 for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
     if (clientListView->currentItem()->text(1) == ksw->data()->INDIHostsList.at(i)->name &&
         clientListView->currentItem()->text(2) == ksw->data()->INDIHostsList.at(i)->portnumber)
   {
        if (ksw->data()->INDIHostsList.at(i)->isConnected)
        {
           KMessageBox::error( 0, i18n("You need to disconnect the client before removing it."));
           return;
        }

        if (KMessageBox::questionYesNoCancel( 0, i18n("Are you sure you want to remove the %1 client?").arg(clientListView->currentItem()->text(1)), i18n("Delete Confirmation"))!=KMessageBox::Yes)
           return;
	   
 	ksw->data()->INDIHostsList.remove(i);
	clientListView->takeItem(clientListView->currentItem());
	break;
   }

 

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

 hostData  = "<INDIHost name='";
 hostData += ksw->data()->INDIHostsList.at(i)->name;
 hostData += "' hostname='";
 hostData += ksw->data()->INDIHostsList.at(i)->hostname;
 hostData += "' port='";
 hostData += ksw->data()->INDIHostsList.at(i)->portnumber;
 hostData += "' />\n";

 outstream << hostData;

 }

  file.close();

}

#include "indidriver.moc"
