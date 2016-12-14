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
#include <QSplitter>
#include <QLocale>
#include <KMessageBox>
#include <QDebug>
#include <QComboBox>
#include <QDialog>
#include <QTabWidget>

#include <KLed>
#include <KActionCollection>

#include <basedevice.h>

#include "fitsviewer/fitsviewer.h"

#include "kstars.h"
#include "indidevice.h"
#include "guimanager.h"
#include "driverinfo.h"
#include "deviceinfo.h"

#include "Options.h"

extern const char *libindi_strings_context;

GUIManager * GUIManager::_GUIManager = NULL;

GUIManager * GUIManager::Instance()
{
    if (_GUIManager == NULL)
        _GUIManager = new GUIManager(Options::independentWindowINDI() ? NULL : KStars::Instance());

    return _GUIManager;
}

GUIManager::GUIManager(QWidget *parent) : QWidget(parent, Qt::Window)
{    
#ifdef Q_OS_OSX
    if(Options::independentWindowINDI())
         setWindowFlags(Qt::Window);
     else{
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
     }
#endif

    mainLayout    = new QVBoxLayout(this);
    mainLayout->setMargin(10);
    mainLayout->setSpacing(10);

    mainTabWidget = new QTabWidget(this);

    mainLayout->addWidget(mainTabWidget);

    setWindowIcon(QIcon::fromTheme("kstars_indi", QIcon(":/icons/breeze/default/kstars_indi.svg")));

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

void GUIManager::changeAlwaysOnTop(Qt::ApplicationState state)
{
    if(isVisible()){
        if (state == Qt::ApplicationActive)
            setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        else
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
    }
}

void GUIManager::closeEvent(QCloseEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a = KStars::Instance()->actionCollection()->action( "show_control_panel" );
        a->setChecked(false);
    }

}

void GUIManager::hideEvent(QHideEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a = KStars::Instance()->actionCollection()->action( "show_control_panel" );
        a->setChecked(false);
    }
}

void GUIManager::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_control_panel" );
    a->setEnabled(true);
    a->setChecked(true);
}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropriate action
*********************************************************************/
void GUIManager::updateStatus()
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_control_panel" );

    //if (clients.size() == 0)
    if (guidevices.count() == 0)
    {
        KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the Device Manager in the devices menu."));
        a->setChecked(false);
        return;
    }

   a->setChecked(true);

    raise();
    activateWindow();
    showNormal();


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

    Qt::ConnectionType type = Qt::BlockingQueuedConnection;

    #ifdef USE_QT5_INDI
    type = Qt::DirectConnection;
    #endif

    connect(cm, SIGNAL(newINDIDevice(DeviceInfo*)), this, SLOT(buildDevice(DeviceInfo*)), type);
    connect(cm, SIGNAL(removeINDIDevice(DeviceInfo*)), this, SLOT(removeDevice(DeviceInfo*)), type);
}

void GUIManager::removeClient(ClientManager *cm)
{
    clients.removeOne(cm);

    foreach(INDI_D *gdv, guidevices)
    {
        if (gdv->getClientManager() == cm)
        {
            for (int i=0; i < mainTabWidget->count(); i++)
            {
                if (mainTabWidget->tabText(i).remove("&") == QString(gdv->getBaseDevice()->getDeviceName()))
                {
                    mainTabWidget->removeTab(i);
                    break;
                }
            }

            guidevices.removeOne(gdv);
            delete (gdv);
            //break;
        }
    }

    if (clients.size() == 0)
        hide();
}

void GUIManager::removeDevice(DeviceInfo *di)
{   
    QString deviceName = di->getBaseDevice()->getDeviceName();
    INDI_D *dp = findGUIDevice(deviceName);

    if (dp == NULL)
        return;

    ClientManager *cm = di->getDriverInfo()->getClientManager();
    if (cm)
        cm->disconnect(dp);

    // Hack to give mainTabWidget sometime to remove its item as these calls are coming from a different thread
    // the clientmanager thread. Sometimes removeTab() requires sometime to be removed properly and hence the wait.
    if (mainTabWidget->count() != guidevices.count())
        QThread::msleep(100);

    for (int i=0; i < mainTabWidget->count(); i++)
    {
        if (mainTabWidget->tabText(i).remove("&") == QString(deviceName))
        {
            mainTabWidget->removeTab(i);
            break;
        }
    }

    guidevices.removeOne(dp);
    delete (dp);

    if (guidevices.isEmpty())
    {
        QAction *a = KStars::Instance()->actionCollection()->action( "show_control_panel" );
        a->setEnabled(false);
    }

}

void GUIManager::buildDevice(DeviceInfo *di)
{
    //qDebug() << "In build Device for device with tree label " << di->getTreeLabel() << endl;
    ClientManager *cm = di->getDriverInfo()->getClientManager();

    if (cm == NULL)
    {
        qCritical() << "ClientManager is null in build device!" << endl;
        return;
    }

    INDI_D *gdm = new INDI_D(this, di->getBaseDevice(), cm);

    Qt::ConnectionType type = Qt::BlockingQueuedConnection;

    #ifdef USE_QT5_INDI
    type = Qt::DirectConnection;
    #endif

    connect(cm, SIGNAL(newINDIProperty(INDI::Property*)), gdm, SLOT(buildProperty(INDI::Property*)), type);
    connect(cm, SIGNAL(removeINDIProperty(INDI::Property*)), gdm, SLOT(removeProperty(INDI::Property*)), type);

    connect(cm, SIGNAL(newINDISwitch(ISwitchVectorProperty*)), gdm, SLOT(updateSwitchGUI(ISwitchVectorProperty*)));
    connect(cm, SIGNAL(newINDIText(ITextVectorProperty*)), gdm, SLOT(updateTextGUI(ITextVectorProperty*)));
    connect(cm, SIGNAL(newINDINumber(INumberVectorProperty*)), gdm, SLOT(updateNumberGUI(INumberVectorProperty*)));
    connect(cm, SIGNAL(newINDILight(ILightVectorProperty*)), gdm, SLOT(updateLightGUI(ILightVectorProperty*)));
    connect(cm, SIGNAL(newINDIBLOB(IBLOB*)), gdm, SLOT(updateBLOBGUI(IBLOB*)));

    connect(cm, SIGNAL(newINDIMessage(INDI::BaseDevice*, int)), gdm, SLOT(updateMessageLog(INDI::BaseDevice*, int)));

    mainTabWidget->addTab(gdm->getDeviceBox(), di->getBaseDevice()->getDeviceName());

    guidevices.append(gdm);

    updateStatus();
}
