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
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QSocketNotifier>
#include <QDateTime>
#include <QTimer>

#include <KLed>
#include <KLineEdit>
#include <KPushButton>
#include <KLocale>
#include <KMessageBox>
#include <KDebug>
#include <KComboBox>
#include <KNumInput>
#include <KDialog>
#include <KTabWidget>

#include "kstars.h"
#include "indidriver.h"

/*******************************************************************
** INDI Menu: Handles communication to server and fetching basic XML
** data.
*******************************************************************/
INDIMenu::INDIMenu(QWidget *parent) : QWidget(parent, Qt::Window)
{

    ksw = (KStars *) parent;

    mainLayout    = new QVBoxLayout(this);
    mainLayout->setMargin(10);
    mainLayout->setSpacing(10);

    mainTabWidget = new KTabWidget(this);

    mainLayout->addWidget(mainTabWidget);

    setWindowTitle(i18n("INDI Control Panel"));
    setAttribute(Qt::WA_ShowModal, false);

    clearB      = new QPushButton(i18n("Clear"));
    closeB      = new QPushButton(i18n("Close"));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->insertStretch(0);
    buttonLayout->addWidget(clearB, 0, Qt::AlignRight);
    buttonLayout->addWidget(closeB, 0, Qt::AlignRight);
    
    mainLayout->addLayout(buttonLayout);

    connect(closeB, SIGNAL(clicked()), this, SLOT(close()));
    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    resize( 640, 480);
}

INDIMenu::~INDIMenu()
{
    while ( ! managers.isEmpty() ) delete managers.takeFirst();
}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropriate action
*********************************************************************/
void INDIMenu::updateStatus()
{
    if (managers.size() == 0)
    {
        KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu."));
        return;
    }

    show();
}

DeviceManager* INDIMenu::initDeviceManager(QString inHost, uint inPort, DeviceManager::ManagerMode inMode)
  {
    DeviceManager *deviceManager;
    
    deviceManager = new DeviceManager(this, inHost, inPort, inMode);
    managers.append(deviceManager);
  
    connect(deviceManager, SIGNAL(newDevice(INDI_D *)), ksw->indiDriver(), SLOT(enableDevice(INDI_D *)));
    connect(deviceManager, SIGNAL(deviceManagerError(DeviceManager *)), this, SLOT(removeDeviceManager(DeviceManager*)), Qt::QueuedConnection);
  
    return deviceManager;
}
  
void INDIMenu::stopDeviceManager(QList<IDevice *> &processed_devices)
{

	foreach(IDevice *device, processed_devices)
	{
		if (device->deviceManager != NULL)
			removeDeviceManager(device->deviceManager);
	}
}

void INDIMenu::removeDeviceManager(DeviceManager *deviceManager)
{
    if (deviceManager == NULL)
    {
	kWarning() << "Warning: trying to remove a null device manager detected.";
	return;
    }

    for (int i=0; i < managers.size(); i++)
    {
        if (deviceManager == managers.at(i))
        {
		foreach(INDI_D *device, deviceManager->indi_dev)
			ksw->indiDriver()->disableDevice(device);

            delete managers.takeAt(i);
	    break;
        }
    }
  
    ksw->indiDriver()->updateMenuActions();

    if (managers.size() == 0)
	close();
}

INDI_D * INDIMenu::findDevice(const QString &deviceName)
{
    for (int i=0; i < managers.size(); i++)
        for (int j=0; j < managers[i]->indi_dev.size(); j++)
            if (managers[i]->indi_dev[j]->name == deviceName)
                return managers[i]->indi_dev[j];

    return NULL;
}

INDI_D * INDIMenu::findDeviceByLabel(const QString &label)
{
    for (int i=0; i < managers.size(); i++)
        for (int j=0; j < managers[i]->indi_dev.size(); j++)
            if (managers[i]->indi_dev[j]->label == label)
                return managers[i]->indi_dev[j];

    return NULL;
}


QString INDIMenu::getUniqueDeviceLabel(const QString &deviceName)
{
      int nset=0;
  
      for (int i=0; i < managers.size(); i++)
    {
          for (int j=0; j < managers[i]->indi_dev.size(); j++)
              if (managers[i]->indi_dev[j]->label.indexOf(deviceName) >= 0)
                  nset++;
    }
  
      if (nset)
        return (deviceName + QString(" %1").arg(nset+1));
      else
        return (deviceName);

}

void INDIMenu::clearLog()
{
  INDI_D *dev = findDeviceByLabel(mainTabWidget->tabText(mainTabWidget->currentIndex()).remove(QChar('&')));

  if (dev)
	dev->msgST_w->clear();

}


#include "indimenu.moc"
