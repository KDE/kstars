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
#include "devicemanager.h"
#include "indidevice.h"
#include "indi/indicom.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <qfile.h>
#include <qvaluelist.h>
#include <qcstring.h>
#include <qradiobutton.h>
#include <qtextedit.h>

#include <kiconloader.h>
#include <klistview.h>
#include <kpopupmenu.h>
#include <kprocess.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <kstandarddirs.h>
#include <kaction.h>

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
    localMode  = icons->loadIcon( "network_local", KIcon::Small);
    serverMode = icons->loadIcon( "network", KIcon::Small);

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
	return;
     }
 }
 
  for (uint i=0; i < devices.size(); i++)
  {
    if (devices[i]->mgrID == mgrID)
    {
      affectedItem = localListView->findItem(devices[i]->label, 0);
      if (!affectedItem) return;
      affectedItem->setPixmap(1, stopPix);
      affectedItem->setPixmap(2, NULL);
      affectedItem->setText(4, QString(""));
      runServiceB->setEnabled(true);
      stopServiceB->setEnabled(false);
      devices[i]->managed = false;
      devices[i]->restart();
      return;
	
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
	
	serverLogText->clear();
	
	for ( QStringList::Iterator it = devices[i]->serverBuffer.begin(); it != devices[i]->serverBuffer.end(); ++it )
	   serverLogText->insert(*it);
	
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
	devices[i]->state = (id == 0) ? 1 : 0;
	if (devices[i]->state)
	{

	  ksw->getINDIMenu()->setCustomLabel(devices[i]->label);
	  devices[i]->label = ksw->getINDIMenu()->currentLabel;

	  devices[i]->serverBuffer.clear();
	  
	  if (!runDevice(devices[i]))
	  {
	   devices[i]->restart();
	   return;
	  }

	  if (devices[i]->mode == IDevice::M_LOCAL)
	  {
	       //Allow time for the INDI server to listen
	  	usleep(50000);

	  	if (!ksw->getINDIMenu()->processServer())
	  	{
	   		devices[i]->restart();
	   		return;
	  	}
	  }

	  localListView->selectedItem()->setPixmap(1, runningPix);
	  localListView->selectedItem()->setText(4, QString("%1").arg(devices[i]->indiPort));
	  runServiceB->setEnabled(false);
	  stopServiceB->setEnabled(true);
	  
	  return;
	}
	
	  if (devices[i]->mode == IDevice::M_LOCAL)
	  	ksw->getINDIMenu()->processServer();
		
	  localListView->selectedItem()->setPixmap(1, stopPix);
	  localListView->selectedItem()->setPixmap(2, NULL);
	  localListView->selectedItem()->setText(4, QString(""));
	  runServiceB->setEnabled(true);
	  stopServiceB->setEnabled(false);
	  devices[i]->restart();
	  updateMenuActions();
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
	  updateMenuActions();
	}
	
	

       }
    }
}

void INDIDriver::updateMenuActions()
{
  // We iterate over devices, we enable INDI Control Panel if we have any active device
  // We enable capture image sequence if we have any imaging device
  
  KAction *tmpAction;
  INDIMenu *devMenu = ksw->getINDIMenu();
  bool activeDevice = false;
  bool activeImaging = false;
  INDI_P *imgProp = NULL;
  
  if (devMenu == NULL)
     return;
  
  if (devMenu->mgr.count() > 0)
   activeDevice = true;
   
  for (uint i=0; i < devMenu->mgr.count(); i++)
  {
	for (uint j=0; j < devMenu->mgr.at(i)->indi_dev.count(); j++)
	{
  		        imgProp = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("CCD_EXPOSE_DURATION");
			if (imgProp && devMenu->mgr.at(i)->indi_dev.at(j)->isOn())
			{
			  activeImaging = true;
			  break;
			}
	}
  }
   
  tmpAction = ksw->actionCollection()->action("indi_control_panel");
  if (!tmpAction)
  	kdDebug() << "Warning: indi_control_panel action not found" << endl;
  else
  	tmpAction->setEnabled(activeDevice);
  
  tmpAction = ksw->actionCollection()->action("capture_sequence");
  if (!tmpAction)
  	kdDebug() << "Warning: capture_sequence action not found" << endl;
  else
  	tmpAction->setEnabled(activeImaging);
  
}

bool INDIDriver::runDevice(IDevice *dev)
{
  dev->indiPort = getINDIPort();

  if (dev->indiPort < 0)
  {
   KMessageBox::error(0, i18n("Cannot start INDI server: port error."));
   return false;
  }

  dev->proc = new KProcess;
  
  *dev->proc << "indiserver";
  *dev->proc << "-v" << "-r" << "0" << "-p" << QString("%1").arg(dev->indiPort) << dev->driver;
  
  // Check Mode
  dev->mode = localR->isChecked() ? IDevice::M_LOCAL : IDevice::M_SERVER;
  
  if (dev->mode == IDevice::M_LOCAL)
    localListView->selectedItem()->setPixmap(2, localMode);
  else
    localListView->selectedItem()->setPixmap(2, serverMode);

  connect(dev->proc, SIGNAL(receivedStderr (KProcess *, char *, int)),  dev, SLOT(processstd(KProcess *, char*, int)));

  dev->proc->start(KProcess::NotifyOnExit, KProcess::Stderr);
  //dev->proc->start();
  
  return (dev->proc->isRunning());
}

void INDIDriver::removeDevice(IDevice *dev)
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

void INDIDriver::saveDevicesToDisk()
{

 QFile file;
 QString elementData;

 file.setName( locateLocal( "appdata", "drivers.xml" ) ); //determine filename in local user KDE directory tree.

 if ( !file.open( IO_WriteOnly))
 {
  QString message = i18n( "unable to write to file 'drivers.xml'\nAny changes to INDI device drivers will not be saved." );
 KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
  return;
 }

 QTextStream outstream(&file);

 // Let's write drivers first
 outstream << "<ScopeDrivers>" << endl;
 for (unsigned int i=0; i < driversList.count(); i++)
  outstream << "       <driver>" << driversList[i] << "</driver>" << endl;
 outstream << "</ScopeDrivers>" << endl;

 // Next we write devices, in the following order:
 // Telescopes, CCDs, Filter Wheels, Video, Dome, GPS

  // #1 Telescopes
  outstream << "<devGroup group='Telescopes'>" << endl;
  for (unsigned i=0; i < devices.size(); i++)
  {
     if (devices[i]->deviceType == KSTARS_TELESCOPE)
     {
	outstream << QString("<device label='%1' focal_length='%2' aperture='%3'>").arg(devices[i]->label).arg(devices[i]->focal_length > 0 ? devices[i]->focal_length : -1).arg(devices[i]->aperture > 0 ? devices[i]->aperture : -1) << endl;

	outstream << "       <driver>" << devices[i]->driver << "</driver>" << endl;
	outstream << "       <version>" << devices[i]->version << "</version>" << endl;
        outstream << "</device>" << endl;
      }
   }
   outstream << "</devGroup>" << endl;

   // #2 CCDs
   outstream << "<devGroup group='CCDs'>" << endl;
   for (unsigned i=0; i < devices.size(); i++)
   {
     if (devices[i]->deviceType == KSTARS_CCD)
     {
	outstream << QString("<device label='%1'>").arg(devices[i]->label) << endl;
	outstream << "       <driver>" << devices[i]->driver << "</driver>" << endl;
	outstream << "       <version>" << devices[i]->version << "</version>" << endl;
        outstream << "</device>" << endl;
     }
  }
  outstream << "</devGroup>" << endl;

  // #3 Filter wheels
  outstream << "<devGroup group='Filter Wheels'>" << endl;
   for (unsigned i=0; i < devices.size(); i++)
   {
     if (devices[i]->deviceType == KSTARS_FILTER)
     {
	outstream << QString("<device label='%1'>").arg(devices[i]->label) << endl;
	outstream << "       <driver>" << devices[i]->driver << "</driver>" << endl;
	outstream << "       <version>" << devices[i]->version << "</version>" << endl;
        outstream << "</device>" << endl;
     }
  }
  outstream << "</devGroup>" << endl;

   // #4 Video
   outstream << "<devGroup group='Video'>" << endl;
   for (unsigned i=0; i < devices.size(); i++)
   {
     if (devices[i]->deviceType == KSTARS_VIDEO)
     {
	outstream << QString("<device label='%1'>").arg(devices[i]->label) << endl;
	outstream << "       <driver>" << devices[i]->driver << "</driver>" << endl;
	outstream << "       <version>" << devices[i]->version << "</version>" << endl;
        outstream << "</device>" << endl;
     }
   }
   outstream << "</devGroup>" << endl;

   file.close();

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

 signed char c;
 LilXML *xmlParser = newLilXML();
 XMLEle *root = NULL;

 while ( (c = (signed char) file.getch()) != -1)
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

bool INDIDriver::buildDriversList( XMLEle *root, char* /*errmsg[]*/)
{

  XMLEle *ep;

  for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
      driversList << pcdataXMLEle(ep);

  return true;

}

bool INDIDriver::buildDeviceGroup(XMLEle *root, char errmsg[])
{

  XMLAtt *ap;
  XMLEle *ep;
  QString groupName;
  int groupType = KSTARS_TELESCOPE;

  if (!strcmp(tagXMLEle(root), "ScopeDrivers"))
    return buildDriversList(root, errmsg);
 
  // avoid overflow
  if (strlen(tagXMLEle(root)) > 1024)
   return false;

  // Get device grouping name
  ap = findXMLAtt(root, "group");

  if (!ap)
  {
    snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a group attribute", tagXMLEle(root));
    return false;
  }

  groupName = valuXMLAtt(ap);
  
  if (groupName.find("Telescopes") != -1)
    groupType = KSTARS_TELESCOPE;
  else if (groupName.find("CCDs") != -1)
    groupType = KSTARS_CCD;
  else if (groupName.find("Filter") != -1)
    groupType = KSTARS_FILTER;
  else if (groupName.find("Video") != -1)
    groupType = KSTARS_VIDEO;
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
  QString label;
  QString driver;
  QString version;
  double focal_length (-1), aperture (-1);

  ap = findXMLAtt(root, "label");
  if (!ap)
  {
    snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a label attribute", tagXMLEle(root));
    return false;
  }

  label = valuXMLAtt(ap);

  // Let's look for telescope-specfic attributes: focal length and aperture
  ap = findXMLAtt(root, "focal_length");
  if (ap)
   focal_length = QString(valuXMLAtt(ap)).toDouble();

  ap = findXMLAtt(root, "aperture");
  if (ap)
   aperture = QString(valuXMLAtt(ap)).toDouble();


  el = findXMLEle(root, "driver");

  if (!el)
   return false;

  driver = pcdataXMLEle(el);
  
  el = findXMLEle(root, "version");

  if (!el)
   return false;

  version = pcdataXMLEle(el);

  QListViewItem *device = new QListViewItem(DGroup, lastDevice);

  device->setText(0, QString(label));
  device->setPixmap(1, stopPix);
  device->setText(3, QString(version));

  lastDevice = device;

  dv = new IDevice(label, driver, version);
  dv->deviceType = groupType;
  connect(dv, SIGNAL(newServerInput()), this, SLOT(updateLocalButtons()));
  if (focal_length > 0)
   dv->focal_length = focal_length;
  if (aperture > 0)
   dv->aperture = aperture;
  
  devices.push_back(dv);

  // SLOTS/SIGNAL, pop menu, indi server logic
  return true;
}

int INDIDriver::activeDriverCount()
{
  int count = 0;

  for (uint i=0; i < devices.size(); i++)
    if (devices[i]->state && devices[i]->mode == IDevice::M_LOCAL)
      count++;

  for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
    if (ksw->data()->INDIHostsList.at(i)->isConnected)
      count++;


  return count;

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

        if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove the %1 client?").arg(clientListView->currentItem()->text(1)), i18n("Delete Confirmation"),KStdGuiItem::del())!=KMessageBox::Continue)
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
 KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
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

INDIDriver::~INDIDriver()
{

  for (uint i=0; i < devices.size(); i++)
   delete (devices[i]);

}

IDevice::IDevice(QString inLabel, QString inDriver, QString inVersion)
{
  label = inLabel;;
  driver = inDriver;;
  version = inVersion;

  // Initially off
  state = 0;

  // No port initially
  indiPort = -1;

  // not yet managed by DeviceManager
  managed = false;

  mgrID = -1;

  focal_length = -1;
  aperture = -1;

  proc = NULL;

}

void IDevice::processstd(KProcess* /*proc*/, char* buffer, int /*buflen*/)
{
  serverBuffer.append(buffer);
  emit newServerInput();
}


IDevice::~IDevice()
{
  if (proc)
      proc->kill();

}

void IDevice::restart()
{

  mgrID = -1;

  state = 0;

  indiPort = -1;

  if (proc)
  	proc->kill();

  proc = NULL;

}

#include "indidriver.moc"
