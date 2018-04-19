/*  INDI frontend for KStars
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guimanager.h"

#include "clientmanager.h"
#include "deviceinfo.h"
#include "indidevice.h"
#include "kstars.h"
#include "Options.h"
#include "fitsviewer/fitsviewer.h"

#include <KActionCollection>
#include <KMessageBox>

#include <QApplication>
#include <QSplitter>
#include <QTextEdit>
#include <QPushButton>
#include <QThread>
#include <QAction>

extern const char *libindi_strings_context;

GUIManager *GUIManager::_GUIManager = nullptr;

GUIManager *GUIManager::Instance()
{
    if (_GUIManager == nullptr)
        _GUIManager = new GUIManager(Options::independentWindowINDI() ? nullptr : KStars::Instance());

    return _GUIManager;
}

GUIManager::GUIManager(QWidget *parent) : QWidget(parent, Qt::Window)
{
#ifdef Q_OS_OSX
    if (Options::independentWindowINDI())
        setWindowFlags(Qt::Window);
    else
    {
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this,
                SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
    }
#endif

    mainLayout = new QVBoxLayout(this);
    mainLayout->setMargin(10);
    mainLayout->setSpacing(10);

    mainTabWidget = new QTabWidget(this);

    mainLayout->addWidget(mainTabWidget);

    setWindowIcon(QIcon::fromTheme("kstars_indi"));

    setWindowTitle(i18n("INDI Control Panel"));
    setAttribute(Qt::WA_ShowModal, false);

    clearB = new QPushButton(i18n("Clear"));
    closeB = new QPushButton(i18n("Close"));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->insertStretch(0);
    buttonLayout->addWidget(clearB, 0, Qt::AlignRight);
    buttonLayout->addWidget(closeB, 0, Qt::AlignRight);

    mainLayout->addLayout(buttonLayout);

    connect(closeB, SIGNAL(clicked()), this, SLOT(close()));
    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    resize(Options::iNDIWindowWidth(), Options::iNDIWindowHeight());
}

void GUIManager::changeAlwaysOnTop(Qt::ApplicationState state)
{
    if (isVisible())
    {
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
        QAction *showINDIPanel = KStars::Instance()->actionCollection()->action("show_control_panel");
        showINDIPanel->setChecked(false);
    }

    Options::setINDIWindowWidth(width());
    Options::setINDIWindowHeight(height());
}

void GUIManager::hideEvent(QHideEvent * /*event*/)
{
    KStars *ks = KStars::Instance();

    if (ks)
    {
        QAction *a = KStars::Instance()->actionCollection()->action("show_control_panel");
        a->setChecked(false);
    }
}

void GUIManager::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action("show_control_panel");
    a->setEnabled(true);
    a->setChecked(true);
}

/*********************************************************************
** Traverse the drivers list, check for updated drivers and take
** appropriate action
*********************************************************************/
void GUIManager::updateStatus(bool toggle_behavior)
{
    QAction *showINDIPanel = KStars::Instance()->actionCollection()->action("show_control_panel");

    if (guidevices.count() == 0)
    {
        KMessageBox::error(0, i18n("No INDI devices currently running. To run devices, please select devices from the "
                                   "Device Manager in the devices menu."));
        showINDIPanel->setChecked(false);
        showINDIPanel->setEnabled(false);
        return;
    }

    showINDIPanel->setChecked(true);

    if (isVisible() && isActiveWindow() && toggle_behavior)
    {
        hide();
    }
    else
    {
        raise();
        activateWindow();
        showNormal();
    }
}

INDI_D *GUIManager::findGUIDevice(const QString &deviceName)
{
    foreach (INDI_D *gdv, guidevices)
    {
        if (gdv->getBaseDevice()->getDeviceName() == deviceName)
            return gdv;
    }

    return nullptr;
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

    foreach (INDI_D *gdv, guidevices)
    {
        if (gdv->getClientManager() == cm)
        {
            for (int i = 0; i < mainTabWidget->count(); i++)
            {
                if (mainTabWidget->tabText(i).remove('&') == QString(gdv->getBaseDevice()->getDeviceName()))
                {
                    mainTabWidget->removeTab(i);
                    break;
                }
            }

            guidevices.removeOne(gdv);
            gdv->deleteLater();
            //break;
        }
    }

    if (clients.size() == 0)
        hide();
}

void GUIManager::removeDevice(DeviceInfo *di)
{
    QString deviceName = di->getBaseDevice()->getDeviceName();
    INDI_D *dp         = findGUIDevice(deviceName);

    if (dp == nullptr)
        return;

    ClientManager *cm = di->getDriverInfo()->getClientManager();
    if (cm)
        cm->disconnect(dp);

    // Hack to give mainTabWidget sometime to remove its item as these calls are coming from a different thread
    // the clientmanager thread. Sometimes removeTab() requires sometime to be removed properly and hence the wait.
    if (mainTabWidget->count() != guidevices.count())
        QThread::msleep(100);

    for (int i = 0; i < mainTabWidget->count(); i++)
    {
        if (mainTabWidget->tabText(i).remove('&') == QString(deviceName))
        {
            mainTabWidget->removeTab(i);
            break;
        }
    }

    guidevices.removeOne(dp);
    delete (dp);

    if (guidevices.isEmpty())
    {
        QAction *showINDIPanel = KStars::Instance()->actionCollection()->action("show_control_panel");
        showINDIPanel->setEnabled(false);
    }
}

void GUIManager::buildDevice(DeviceInfo *di)
{
    //qDebug() << "In build Device for device with tree label " << di->getTreeLabel() << endl;
    ClientManager *cm = di->getDriverInfo()->getClientManager();

    if (cm == nullptr)
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

    connect(cm, SIGNAL(newINDIMessage(INDI::BaseDevice*,int)), gdm, SLOT(updateMessageLog(INDI::BaseDevice*,int)));

    mainTabWidget->addTab(gdm->getDeviceBox(), di->getBaseDevice()->getDeviceName());

    guidevices.append(gdm);

    updateStatus(false);
}
