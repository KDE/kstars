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

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <QRadioButton>
#include <QFile>
#include <QTextStream>
#include <QTreeWidget>
#include <QIcon>
#include <QDialog>

#include <KMenu>
#include <KMessageBox>
#include <KPushButton>
#include <KLineEdit>
#include <KProcess>
#include <KAction>
#include <KActionCollection>
#include <KIconLoader>

#include <kstandarddirs.h>
#include <k3serversocket.h>

#include "indimenu.h"
#include "ui_indihostconf.h"
#include "devicemanager.h"
#include "indidevice.h"
//#include "indi/libs/indicom.h"
#include "Options.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <config-kstars.h>

#define  MAX_RETRIES 3
#define  ERRMSG_SIZE 1024

DeviceManagerUI::DeviceManagerUI(QWidget *parent) : QFrame(parent)
{

    setupUi(this);

    localTreeWidget->setSortingEnabled(false);
    localTreeWidget->setRootIsDecorated(true);

    clientTreeWidget->setSortingEnabled(false);

    runningPix = KIcon( "system-run" );
    stopPix    = KIcon( "dialog-cancel" );
    // jpetso: I don't know what the above icon does, depending on the usage
    // it might also be "process-stop". please check back.
    localMode  = KIcon( "computer" );
    serverMode = KIcon( "network-server" );

    connected           = KIcon( "network-connect" );
    disconnected        = KIcon( "network-disconnect" );

}

INDIDriver::INDIDriver( KStars *_ks )
        : KDialog( _ks ),  ksw( _ks )
{

    currentPort = Options::serverPortStart().toInt()-1;
    lastGroup = NULL;
    lastDevice = NULL;

    ui = new DeviceManagerUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Device Manager" ) );
    setButtons( KDialog::Close );

    foreach ( INDIHostsInfo * host, ksw->data()->INDIHostsList )
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->clientTreeWidget, lastGroup);
        lastGroup = item;
        item->setIcon(0, ui->disconnected);
        item->setText(1, host->name);
        item->setText(2, host->portnumber);
    }

    lastGroup = NULL;

    QObject::connect(ui->addB, SIGNAL(clicked()), this, SLOT(addINDIHost()));
    QObject::connect(ui->modifyB, SIGNAL(clicked()), this, SLOT(modifyINDIHost()));
QObject::connect(ui->removeB, SIGNAL(clicked()), this, SLOT(removeINDIHost()));
  
  
    //QObject::connect(ksw->indiMenu(), SIGNAL(driverDisconnected(int)), this, SLOT(shutdownHost(int)));
      QObject::connect(ui->connectHostB, SIGNAL(clicked()), this, SLOT(activateHostConnection()));
      QObject::connect(ui->disconnectHostB, SIGNAL(clicked()), this, SLOT(activateHostDisconnection()));
      QObject::connect(ui->runServiceB, SIGNAL(clicked()), this, SLOT(activateRunService()));
      QObject::connect(ui->stopServiceB, SIGNAL(clicked()), this, SLOT(activateStopService()));
    QObject::connect(ui->localTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateLocalTab()));
    QObject::connect(ui->clientTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateClientTab()));
      QObject::connect(ui->localTreeWidget, SIGNAL(expanded(const QModelIndex &)), this, SLOT(resizeDeviceColumn()));
  
      readXMLDriver();
  }
  
void INDIDriver::enableDevice(INDI_D *indi_device)
  {
   if (indi_device == NULL)
	return;
  
   if (indi_device->deviceManager->mode == DeviceManager::M_CLIENT)
   {
   	foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
    	{
	        if (host->deviceManager == indi_device->deviceManager && host->isConnected == false)
	        {
	    		foreach (QTreeWidgetItem *item, ui->clientTreeWidget->findItems(host->name, Qt::MatchExactly, 1))
				item->setIcon(0, ui->connected);
	
             		host->isConnected = true;
			updateClientTab();
			updateMenuActions();
             		return;
        	}
    	}
    }
    else
      {
	foreach (IDevice *device, devices)
    	{
		if (device->unique_label == indi_device->label)
		{
			foreach (QTreeWidgetItem *item, ui->localTreeWidget->findItems(device->tree_label, Qt::MatchExactly |  Qt::MatchRecursive))
			{
				item->setIcon(1, ui->runningPix);
        			item->setText(4, QString::number(indi_device->deviceManager->port));
			}
			
			updateLocalTab();
			updateMenuActions();
			return;
		}
	}
      }
  
}
  
void INDIDriver::disableDevice(INDI_D *indi_device)
{
    if (indi_device == NULL)
	return;

   if (indi_device->deviceManager->mode == DeviceManager::M_CLIENT)
   {
   	foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
    	{
	        if (host->deviceManager == indi_device->deviceManager && host->isConnected == true)
	        {
	    		foreach (QTreeWidgetItem *item, ui->clientTreeWidget->findItems(host->name, Qt::MatchExactly,1))
				item->setIcon(0, ui->disconnected);
	
			host->deviceManager = NULL;
		        host->isConnected = false;
            
          		updateClientTab();
             		return;
        	}
    	}
    }
    else
      {
	foreach (IDevice *device, devices)
    	{
		if (device->unique_label == indi_device->label)
		{
			foreach (QTreeWidgetItem *item, ui->localTreeWidget->findItems(device->tree_label, Qt::MatchExactly |  Qt::MatchRecursive))
			{
				item->setIcon(1, ui->stopPix);
            			item->setIcon(2, QIcon());
            			item->setText(4, QString());
			}
			
			device->clear();
			updateLocalTab();
			
			return;
		}
	}
      }
  }
  
void INDIDriver::activateRunService()
  {
    processLocalTree(IDevice::DEV_START);
  }
  
void INDIDriver::activateStopService()
  {
    processLocalTree(IDevice::DEV_TERMINATE);
  }
  
void INDIDriver::activateHostConnection()
  {
    processRemoteTree(IDevice::DEV_START);
  }
  
void INDIDriver::activateHostDisconnection()
  {
    processRemoteTree(IDevice::DEV_TERMINATE);
  }
  
void INDIDriver::updateLocalTab()
  {
  
      if (ui->localTreeWidget->currentItem() == NULL)
          return;
  
    foreach (IDevice *device, devices)
      {
  
        if (ui->localTreeWidget->currentItem()->text(0) == device->tree_label)
          {
            ui->runServiceB->setEnabled(device->state == IDevice::DEV_TERMINATE);
            ui->stopServiceB->setEnabled(device->state == IDevice::DEV_START);
  
              ui->serverLogText->clear();
            ui->serverLogText->append(device->getServerBuffer());
  
              return;
          }
	}
}

void INDIDriver::updateClientTab()
{
     if (ui->clientTreeWidget->currentItem() == NULL)
        return;

    foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
    {
        if (ui->clientTreeWidget->currentItem()->text(1) == host->name && ui->clientTreeWidget->currentItem()->text(2) == host->portnumber)
        {
            ui->connectHostB->setEnabled(!host->isConnected);
            ui->disconnectHostB->setEnabled(host->isConnected);
            break;
        }
    }

}

void INDIDriver::processLocalTree(IDevice::DeviceStatus dev_request)
{
   QList<IDevice *> processed_devices;
   DeviceManager *deviceManager=NULL;

   foreach(QTreeWidgetItem *item, ui->localTreeWidget->selectedItems())
   {
      foreach (IDevice *device, devices)
      {
		//device->state = (dev_request == IDevice::DEV_TERMINATE) ? IDevice::DEV_START : IDevice::DEV_TERMINATE;
		if (item->text(0) == device->tree_label && device->state != dev_request)

			processed_devices.append(device);
      }
   }

   if (processed_devices.empty()) return;

   if (dev_request == IDevice::DEV_START)
   {
	int port = getINDIPort();

    	if (port < 0)
        {
	        KMessageBox::error(0, i18n("Cannot start INDI server: port error."));
        	return;
    	}
   
	//deviceManager = ksw->indiMenu()->startDeviceManager(processed_devices, ui->localR->isChecked() ? DeviceManager::M_LOCAL : DeviceManager::M_SERVER, ((uint) port));
        deviceManager = ksw->indiMenu()->initDeviceManager("localhost", ((uint) port), ui->localR->isChecked() ? DeviceManager::M_LOCAL : DeviceManager::M_SERVER);

	if (deviceManager == NULL)
	{
		kWarning() << "Warning: device manager has not been established properly";
		return;
	}

	deviceManager->appendManagedDevices(processed_devices);
	deviceManager->startServer();
	connect(deviceManager, SIGNAL(newServerInput()), this, SLOT(updateLocalTab()));
   }
   else
	ksw->indiMenu()->stopDeviceManager(processed_devices);

}

void INDIDriver::processRemoteTree(IDevice::DeviceStatus dev_request)
{
      DeviceManager* deviceManager=NULL;
      QTreeWidgetItem *currentItem = ui->clientTreeWidget->currentItem();
      if (!currentItem) return;
     bool toConnect = (dev_request == IDevice::DEV_START);

    foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
        //hostInfo = ksw->data()->INDIHostsList.at(i);
        if (currentItem->text(1) == host->name && currentItem->text(2) == host->portnumber)
        {
            // Nothing changed, return
            if (host->isConnected == toConnect)
                return;

            // connect to host
            if (toConnect)
            {
		deviceManager = ksw->indiMenu()->initDeviceManager(host->hostname, host->portnumber.toUInt(), DeviceManager::M_CLIENT);
  
	        if (deviceManager == NULL)
		{
			// msgbox after freeze
			kWarning() << "Warning: device manager has not been established properly";
		        return;
		}
  
		host->deviceManager = deviceManager;
		deviceManager->connectToServer();
	     }
             else
             	ksw->indiMenu()->removeDeviceManager(host->deviceManager);
  
		return;
	  }
}

void INDIDriver::newDeviceDiscovered()
{

    emit newDevice();

    updateMenuActions();
}

void INDIDriver::newTelescopeDiscovered()
{

    emit newTelescope();

}

void INDIDriver::newCCDDiscovered()
{

    emit newCCD();

}

void INDIDriver::resizeDeviceColumn()
{
  ui->localTreeWidget->resizeColumnToContents(0);
}

void INDIDriver::updateMenuActions()
{


    // We iterate over devices, we enable INDI Control Panel if we have any active device
    // We enable capture image sequence if we have any imaging device

    QAction *tmpAction = NULL;
    INDIMenu *devMenu = ksw->indiMenu();
    bool activeDevice = false;
    bool activeImaging = false;
    INDI_P *imgProp = NULL;

    if (devMenu == NULL)
        return;

    if (devMenu->managers.count() > 0)
        activeDevice = true;

    foreach(DeviceManager *dev_managers, devMenu->managers)
    {
        foreach (INDI_D *device, dev_managers->indi_dev)
        {

            imgProp = device->findProp("CCD_EXPOSURE");
            if (imgProp && device->isOn())
            {
                activeImaging = true;
                break;
            }
        }
    }

    tmpAction = ksw->actionCollection()->action("capture_sequence");

    if (tmpAction != NULL)
        tmpAction->setEnabled(activeImaging);

    tmpAction = NULL;
    tmpAction = ksw->actionCollection()->action("indi_cpl");
    if (tmpAction != NULL)
        tmpAction->setEnabled(activeDevice);
#if 0
    /* FIXME The following seems to cause a crash in KStars when we use
       the telescope wizard to automatically search for scopes. I can't
       find any correlation! */

    // Troubled Code START
    tmpAction = ksw->actionCollection()->action("indi_cpl");
    if (!tmpAction)
        kDebug() << "Warning: indi_cpl action not found";
    else
        tmpAction->setEnabled(activeDevice);
    // Troubled Code END
#endif


}

#if 0
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
    *dev->proc << "-v" << "-p" << QString::number(dev->indiPort) << dev->driver;

    // Check Mode
    dev->mode = ui->localR->isChecked() ? IDevice::M_LOCAL : IDevice::M_SERVER;

    if (dev->mode == IDevice::M_LOCAL)
        ui->localTreeWidget->currentItem()->setIcon(2, ui->localMode);
    else
        ui->localTreeWidget->currentItem()->setIcon(2, ui->serverMode);

    connect(dev->proc, SIGNAL(readyReadStandardError  ()),  dev, SLOT(processstd()));

    dev->proc->setOutputChannelMode(KProcess::SeparateChannels);
    dev->proc->setReadChannel(QProcess::StandardError);
    dev->proc->start();

    //dev->proc->start();

    return (dev->proc->waitForStarted());
}

void INDIDriver::.arg(q(IDevice *dev)
{

    //for (unsigned int i=0 ; i < devices.size(); i++)
    foreach (IDevice *device, devices)
    if (dev->label == device->label)
        device->restart();
}

void INDIDriver::.arg(q(const QString &deviceLabel)
{
    //for (unsigned int i=0 ; i < devices.size(); i++)
    foreach (IDevice *device, devices)
    if (deviceLabel == device->label)
        device->restart();

}
#endif

void INDIDriver::saveDevicesToDisk()
{

    QFile file;
    QString elementData;

    file.setFileName( KStandardDirs::locateLocal( "appdata", "drivers.xml" ) ); //determine filename in local user KDE directory tree.

    if ( !file.open( QIODevice::WriteOnly))
    {
        QString message = i18n( "unable to write to file 'drivers.xml'\nAny changes to INDI device drivers will not be saved." );
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
        return;
    }

    QTextStream outstream(&file);

    // Let's write drivers first
    outstream << "<ScopeDrivers>" << endl;
    for (int i=0; i < driversList.count(); i++)
        outstream << "       <driver>" << driversList[i] << "</driver>" << endl;
    outstream << "</ScopeDrivers>" << endl;

    // Next we write devices, in the following order:
    // Telescopes, CCDs, Filter Wheels, Video, Dome, GPS

    // #1 Telescopes
    outstream << "<devGroup group='Telescopes'>" << endl;
    //for (unsigned i=0; i < devices.size(); i++)
    foreach (IDevice *dev, devices)
    {
        if (dev->deviceType == KSTARS_TELESCOPE)
        {
            outstream << QString("<device label='%1' focal_length='%2' aperture='%3'>").arg(dev->tree_label).arg(dev->focal_length > 0 ? dev->focal_length : -1).arg(dev->aperture > 0 ? dev->aperture : -1) << endl;

            outstream << "       <driver>" << dev->driver << "</driver>" << endl;
            outstream << "       <version>" << dev->version << "</version>" << endl;
            outstream << "</device>" << endl;
        }
    }
    outstream << "</devGroup>" << endl;

#ifdef HAVE_CFITSIO_H
    // #2 CCDs
    outstream << "<devGroup group='CCDs'>" << endl;
    //for (unsigned i=0; i < devices.size(); i++)
    foreach (IDevice *dev, devices)
    {
        if (dev->deviceType == KSTARS_CCD)
        {
            outstream << QString("<device label='%1'>").arg(dev->tree_label) << endl;
            outstream << "       <driver>" << dev->driver << "</driver>" << endl;
            outstream << "       <version>" << dev->version << "</version>" << endl;
            outstream << "</device>" << endl;
        }
    }
    outstream << "</devGroup>" << endl;
#endif

    // #3 Filter wheels
    outstream << "<devGroup group='Filter Wheels'>" << endl;

    foreach (IDevice *dev, devices)
    //for (unsigned i=0; i < devices.size(); i++)
    {
        if (dev->deviceType == KSTARS_FILTER)
        {
            outstream << QString("<device label='%1'>").arg(dev->tree_label) << endl;
            outstream << "       <driver>" << dev->driver << "</driver>" << endl;
            outstream << "       <version>" << dev->version << "</version>" << endl;
            outstream << "</device>" << endl;
        }
    }
    outstream << "</devGroup>" << endl;

#ifdef HAVE_CFITSIO_H
    // #4 Video
    outstream << "<devGroup group='Video'>" << endl;

    foreach (IDevice *dev, devices)
    //for (unsigned i=0; i < devices.size(); i++)
    {
        if (dev->deviceType == KSTARS_VIDEO)
        {
            outstream << QString("<device label='%1'>").arg(dev->tree_label) << endl;
            outstream << "       <driver>" << dev->driver << "</driver>" << endl;
            outstream << "       <version>" << dev->version << "</version>" << endl;
            outstream << "</device>" << endl;
        }
    }
    outstream << "</devGroup>" << endl;
#endif

    file.close();

}

bool INDIDriver::isDeviceRunning(const QString &deviceLabel)
{
    foreach (IDevice *dev, devices)
    if (deviceLabel == dev->tree_label)
    {
        if (dev->state == IDevice::DEV_START)
		return true;
	else
		return false;
    }

    return false;
}

int INDIDriver::getINDIPort()
{

    int lastPort  = Options::serverPortEnd().toInt();;
    currentPort++;

    // recycle
    if (currentPort > lastPort) currentPort = Options::serverPortStart().toInt();

    KNetwork::KServerSocket ss;
    ss.setFamily(KNetwork::KResolver::InetFamily);
    for(; currentPort <= lastPort; currentPort++)
    {
        ss.setAddress( QString::number( currentPort ) );
        bool success = ss.listen();
        if(success && ss.error() == KNetwork::KSocketBase::NoError )
        {
            ss.close();
            return currentPort;
        }
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
        KMessageBox::error(0, i18n("Unable to find device driver file 'drivers.xml'. Please locate the file and place it in one of the following locations:\n\n \t$(KDEDIR)/share/apps/kstars/ \n\t~/.kde/share/apps/kstars/"));

        return false;
    }

    char c;
    LilXML *xmlParser = newLilXML();
    XMLEle *root = NULL;

    //while ( (c = (signed char) file.getch()) != -1)
    while ( file.getChar(&c))
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
            kDebug() << QString(errmsg);
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

    if (groupName.indexOf("Telescopes") != -1)
        groupType = KSTARS_TELESCOPE;
    else if (groupName.indexOf("CCDs") != -1)
        groupType = KSTARS_CCD;
    else if (groupName.indexOf("Filter") != -1)
        groupType = KSTARS_FILTER;
    else if (groupName.indexOf("Video") != -1)
        groupType = KSTARS_VIDEO;
    else if (groupName.indexOf("Focusers") != -1)
        groupType = KSTARS_FOCUSER;
    else if (groupName.indexOf("Domes") != -1)
        groupType = KSTARS_DOME;
    else if (groupName.indexOf("GPS") != -1)
        groupType = KSTARS_GPS;

#ifndef HAVE_CFITSIO_H
    // We do not create these groups if we don't have CFITSIO support
    if (groupType == KSTARS_CCD || groupType == KSTARS_VIDEO)
        return true;
#endif

    QTreeWidgetItem *group = new QTreeWidgetItem(ui->localTreeWidget, lastGroup);
    group->setText(0, groupName);
    lastGroup = group;

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        if (!buildDriverElement(ep, group, groupType, errmsg))
            return false;

    return true;
}

bool INDIDriver::buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, int groupType, char errmsg[])
{
    XMLAtt *ap;
    XMLEle *el;
    IDevice *dv;
    QString label;
    QString driver;
    QString version;
    QString name;
    double focal_length (-1), aperture (-1);

    ap = findXMLAtt(root, "label");
    if (!ap)
    {
        snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a label attribute", tagXMLEle(root));
        return false;
    }

    label = valuXMLAtt(ap);

    // Let's look for telescope-specific attributes: focal length and aperture
    ap = findXMLAtt(root, "focal_length");
    if (ap)
        focal_length = QString(valuXMLAtt(ap)).toDouble();

    ap = findXMLAtt(root, "aperture");
    if (ap)
        aperture = QString(valuXMLAtt(ap)).toDouble();


    el = findXMLEle(root, "name");

    if (el)
	    name = pcdataXMLEle(el);
    else
	    name = label;

    el = findXMLEle(root, "driver");

    if (!el)
        return false;

    driver = pcdataXMLEle(el);

    el = findXMLEle(root, "version");

    if (!el)
        return false;

  version = pcdataXMLEle(el);

    QTreeWidgetItem *device = new QTreeWidgetItem(DGroup, lastDevice);

    device->setText(0, QString(label));
    device->setIcon(1, ui->stopPix);
    device->setText(3, QString(version));

    lastDevice = device;

    dv = new IDevice(name, label, driver, version);
    dv->deviceType = groupType;
    //connect(dv, SIGNAL(newServerInput()), this, SLOT(updateLocalTab()));
    if (focal_length > 0)
        dv->focal_length = focal_length;
    if (aperture > 0)
        dv->aperture = aperture;

    devices.append(dv);

    // SLOTS/SIGNAL, pop menu, indi server logic
    return true;
}

void INDIDriver::addINDIHost()
{
    QDialog hostConfDialog;
    Ui::INDIHostConf hostConf;
    hostConf.setupUi(&hostConfDialog);
    hostConfDialog.setWindowTitle(i18n("Add Host"));
    bool portOk = false;

    if (hostConfDialog.exec() == QDialog::Accepted)
    {
        INDIHostsInfo *hostItem = new INDIHostsInfo;
        hostItem->name        	= hostConf.nameIN->text();
        hostItem->hostname    	= hostConf.hostname->text();
        hostItem->portnumber  	= hostConf.portnumber->text();
        hostItem->isConnected 	= false;
        hostItem->deviceManager	= NULL;

        hostItem->portnumber.toInt(&portOk);

        if (portOk == false)
        {
            KMessageBox::error(0, i18n("Error: the port number is invalid."));
            delete hostItem;
            return;
        }

        //search for duplicates
        //for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
        foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
        if (hostItem->name   == host->name &&  hostItem->portnumber == host->portnumber)
        {
            KMessageBox::error(0, i18n("Host: %1 Port: %2 already exists.", hostItem->name, hostItem->portnumber));
            delete hostItem;
            return;
        }

        ksw->data()->INDIHostsList.append(hostItem);

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->clientTreeWidget);
        item->setIcon(0, ui->disconnected);
        item->setText(1, hostConf.nameIN->text());
        item->setText(2, hostConf.portnumber->text());

    }

    saveHosts();
}



void INDIDriver::modifyINDIHost()
{

    QDialog hostConfDialog;
    Ui::INDIHostConf hostConf;
    hostConf.setupUi(&hostConfDialog);
    hostConfDialog.setWindowTitle(i18n("Modify Host"));

    QTreeWidgetItem *currentItem = ui->clientTreeWidget->currentItem();

    if (currentItem == NULL)
        return;

    foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
    {
        if (currentItem->text(1) == host->name && currentItem->text(2) == host->portnumber)
        {
            hostConf.nameIN->setText(host->name);
            hostConf.hostname->setText(host->hostname);
            hostConf.portnumber->setText(host->portnumber);

            if (hostConfDialog.exec() == QDialog::Accepted)
            {
                //INDIHostsInfo *hostItem = new INDIHostsInfo;
                host->name       = hostConf.nameIN->text();
                host->hostname   = hostConf.hostname->text();
                host->portnumber = hostConf.portnumber->text();

                currentItem->setText(1, hostConf.nameIN->text());
                currentItem->setText(2, hostConf.portnumber->text());

                //ksw->data()->INDIHostsList.replace(i, hostItem);

                saveHosts();
                return;
            }
        }
    }

}

void INDIDriver::removeINDIHost()
{

    if (ui->clientTreeWidget->currentItem() == NULL)
        return;

    for (int i=0; i < ksw->data()->INDIHostsList.count(); i++)
        if (ui->clientTreeWidget->currentItem()->text(1) == ksw->data()->INDIHostsList[i]->name &&
                ui->clientTreeWidget->currentItem()->text(2) == ksw->data()->INDIHostsList[i]->portnumber)
        {
            if (ksw->data()->INDIHostsList[i]->isConnected)
            {
                KMessageBox::error( 0, i18n("You need to disconnect the client before removing it."));
                return;
            }

            if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove the %1 client?", ui->clientTreeWidget->currentItem()->text(1)), i18n("Delete Confirmation"),KStandardGuiItem::del())!=KMessageBox::Continue)
                return;

            delete ksw->data()->INDIHostsList.takeAt(i);
            delete (ui->clientTreeWidget->currentItem());
            break;
        }



    saveHosts();


}

void INDIDriver::saveHosts()
{

    QFile file;
    QString hostData;

    file.setFileName( KStandardDirs::locateLocal( "appdata", "indihosts.xml" ) ); //determine filename in local user KDE directory tree.

    if ( !file.open( QIODevice::WriteOnly))
    {
        QString message = i18n( "unable to write to file 'indihosts.xml'\nAny changes to INDI hosts configurations will not be saved." );
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
        return;
    }

    QTextStream outstream(&file);

    //for (uint i= 0; i < ksw->data()->INDIHostsList.count(); i++)
    foreach (INDIHostsInfo * host, ksw->data()->INDIHostsList)
    {

        hostData  = "<INDIHost name='";
        hostData += host->name;
        hostData += "' hostname='";
        hostData += host->hostname;
        hostData += "' port='";
        hostData += host->portnumber;
        hostData += "' />\n";

        outstream << hostData;

    }

    file.close();

}

INDIDriver::~INDIDriver()
{

    //for (uint i=0; i < devices.size(); i++)
    //delete (devices[i]);
    while (!devices.isEmpty())
        delete devices.takeFirst();

}

IDevice::IDevice(const QString &inName, const QString &inLabel, const QString &inDriver, const QString &inVersion)
{
    tree_label	 = inLabel;;
    unique_label.clear();
    driver_class = inName;
    driver	 = inDriver;
    version	 = inVersion;
  
      // Initially off
    state = IDevice::DEV_TERMINATE;
  
      // No port initially
    //indiPort = -1;
  
      // not yet managed by DeviceManager
    //managed = false;
  
    deviceManager = NULL;
  
      focal_length = -1;
      aperture = -1;
  
    //proc = NULL;
  
}
  
IDevice::~IDevice()
{

}
  
void IDevice::clear()
{
    //managed = false;
    state = IDevice::DEV_TERMINATE;
    deviceManager = NULL;
  
    unique_label.clear();
}
  
QString IDevice::getServerBuffer()
{
	if (deviceManager != NULL)
		return deviceManager->getServerBuffer();
  
	return QString();
}

#include "indidriver.moc"
