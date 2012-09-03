/*  INDI frontend for KStars
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

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
#include <QSplitter>

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

#include <libindi/basedevice.h>

#include "kstars.h"
#include "indidevice.h"
#include "guimanager.h"
#include "driverinfo.h"


GUIManager * GUIManager::_GUIManager = NULL;

GUIManager * GUIManager::Instance()
{
    if (_GUIManager == NULL)
        _GUIManager = new GUIManager(KStars::Instance());

    return _GUIManager;
}

GUIManager::GUIManager(QWidget *parent) : QWidget(parent, Qt::Window)
{

    //ksw = (KStars *) parent;

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

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropriate action
*********************************************************************/
void GUIManager::updateStatus()
{
    if (clients.size() == 0)
    {
        KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu."));
        return;
    }

    show();
}

INDI_D * GUIManager::findGUIDevice(const QString &deviceName)
{

    foreach(INDI_D *gdv, guidevices)
    {
        if (gdv->getBaseDevice()->getDeviceName() == deviceName)
            return gdv;
    }

    return NULL;
}


void GUIManager::clearLog()
{

  INDI_D *dev = findGUIDevice(mainTabWidget->tabText(mainTabWidget->currentIndex()).remove(QChar('&')));

  if (dev)
      dev->clearMessageLog();

}

void GUIManager::addClient(ClientManager *cm)
{
    clients.append(cm);

    connect(cm, SIGNAL(newINDIDevice(DriverInfo*)), this, SLOT(buildDevice(DriverInfo*)), Qt::BlockingQueuedConnection);

    connect(cm, SIGNAL(INDIDeviceRemoved(DriverInfo*)), this, SLOT(removeDevice(DriverInfo*)));

    //INDI_D *gdm = new INDI_D(this, cm);

    //connect(cm, SIGNAL(newINDIProperty(INDI::Property*)), gdm, SLOT(buildProperty(INDI::Property*)));

    //guidevices.append(gdm);

    updateStatus();
}

void GUIManager::removeClient(ClientManager *cm)
{
    clients.removeOne(cm);

   for (int i=0; i < guidevices.size(); i++)
            guidevices.at(i)->setTabID(i);

    foreach(INDI_D *gdv, guidevices)
    {
        //qDebug() << "Terminating client.... " << endl;
        if (gdv->getClientManager() == cm)
        {
            //qDebug() << "Will remove device " << gdv->getBaseDevice()->getDeviceName() << " with tab ID " << gdv->getTabID() << endl;
            mainTabWidget->removeTab(gdv->getTabID());
            guidevices.removeOne(gdv);
            delete (gdv);
            break;
        }
    }

    if (clients.size() == 0)
        hide();
}

void GUIManager::removeDevice(DriverInfo *di)
{
    INDI_D *dp=NULL;

    dp = findGUIDevice(di->getUniqueLabel());

    if (dp == NULL)
        return;

    mainTabWidget->removeTab(dp->getTabID());
    guidevices.removeOne(dp);
    delete (dp);

    // rearrange tab indexes
    for (int i=0; i < guidevices.size(); i++)
            guidevices.at(i)->setTabID(i);
}

void GUIManager::buildDevice(DriverInfo *di)
{
    //qDebug() << "In build Device for device with tree label " << di->getTreeLabel() << endl;
    int nset=0;
    ClientManager *cm = di->getClientManager();

    if (cm == NULL)
    {
        qDebug() << "Error clientManager is null in build device!" << endl;
        return;

    }

    INDI_D *gdm = new INDI_D(this, di->getBaseDevice(), cm);

    connect(cm, SIGNAL(newINDIProperty(INDI::Property*)), gdm, SLOT(buildProperty(INDI::Property*)));
    connect(cm, SIGNAL(removeINDIProperty(INDI::Property*)), gdm, SLOT(removeProperty(INDI::Property*)), Qt::BlockingQueuedConnection);
    connect(cm, SIGNAL(newINDISwitch(ISwitchVectorProperty*)), gdm, SLOT(updateSwitchGUI(ISwitchVectorProperty*)));
    connect(cm, SIGNAL(newINDIText(ITextVectorProperty*)), gdm, SLOT(updateTextGUI(ITextVectorProperty*)));
    connect(cm, SIGNAL(newINDINumber(INumberVectorProperty*)), gdm, SLOT(updateNumberGUI(INumberVectorProperty*)));
    connect(cm, SIGNAL(newINDILight(ILightVectorProperty*)), gdm, SLOT(updateLightGUI(ILightVectorProperty*)));
    connect(cm, SIGNAL(newINDIBLOB(IBLOB*)), gdm, SLOT(updateBLOBGUI(IBLOB*)));

    connect(cm, SIGNAL(newINDIMessage(INDI::BaseDevice*)), gdm, SLOT(updateMessageLog(INDI::BaseDevice*)));


    //qDebug() << "About to add to tab main widget with name " << di->getBaseDevice()->getDeviceName() << endl;
    //nset = mainTabWidget->addTab(gdm->getDeviceBox(), di->getBaseDevice()->getDeviceName());
    nset = mainTabWidget->addTab(gdm->getDeviceBox(), di->getUniqueLabel());

    gdm->setTabID(nset);

    guidevices.append(gdm);



}

#include "guimanager.moc"
