/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guimanager.h"

#include "clientmanager.h"
#include "deviceinfo.h"
#include "indidevice.h"
#include "kstars.h"
#include "Options.h"
#include "fitsviewer/fitsviewer.h"
#include "ksnotification.h"

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

void GUIManager::release()
{
    delete _GUIManager;
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
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    mainTabWidget = new QTabWidget(this);

    mainLayout->addWidget(mainTabWidget);

    setWindowIcon(QIcon::fromTheme("kstars_indi"));

    setWindowTitle(i18nc("@title:window", "INDI Control Panel"));
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

GUIManager::~GUIManager()
{
    for (auto oneClient : qAsConst(clients))
        oneClient->disconnect(this);
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
        KSNotification::error(i18n("No INDI devices currently running. To run devices, please select devices from the "
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
    for (auto oneGUIDevice : guidevices)
    {
        if (oneGUIDevice->name() == deviceName)
            return oneGUIDevice;
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
    connect(cm, &ClientManager::newINDIDevice, this, &GUIManager::buildDevice, Qt::BlockingQueuedConnection);
    connect(cm, &ClientManager::removeINDIDevice, this, &GUIManager::removeDevice, Qt::BlockingQueuedConnection);
}

void GUIManager::removeClient(ClientManager *cm)
{
    clients.removeOne(cm);

    QMutableListIterator<INDI_D*> it(guidevices);
    while (it.hasNext())
    {
        INDI_D *gdv = it.next();

        if (gdv->getClientManager() == cm)
        {
            for (int i = 0; i < mainTabWidget->count(); i++)
            {
                if (mainTabWidget->tabText(i).remove('&') == QString(gdv->name()))
                {
                    mainTabWidget->removeTab(i);
                    break;
                }
            }

            it.remove();
            gdv->deleteLater();
        }
    }

    if (clients.size() == 0)
        hide();
}

void GUIManager::removeDevice(const QString &name)
{
    INDI_D *dp = findGUIDevice(name);

    if (dp == nullptr)
        return;

    if (dp->getClientManager())
        dp->getClientManager()->disconnect(dp);
    dp->disconnect();

    // Hack to give mainTabWidget sometime to remove its item as these calls are coming from a different thread
    // the clientmanager thread. Sometimes removeTab() requires sometime to be removed properly and hence the wait.
    if (mainTabWidget->count() != guidevices.count())
        QThread::msleep(100);

    for (int i = 0; i < mainTabWidget->count(); i++)
    {
        if (mainTabWidget->tabText(i).remove('&') == name)
        {
            mainTabWidget->removeTab(i);
            break;
        }
    }

    guidevices.removeOne(dp);
    dp->deleteLater();

    if (guidevices.isEmpty())
    {
        QAction *showINDIPanel = KStars::Instance()->actionCollection()->action("show_control_panel");
        showINDIPanel->setEnabled(false);
    }
}

void GUIManager::buildDevice(DeviceInfo *di)
{
    ClientManager *cm = qobject_cast<ClientManager *>(sender());
    Q_ASSERT_X(cm, __FUNCTION__, "Client manager is not valid.");

    INDI_D *gdm = new INDI_D(mainTabWidget, di->getBaseDevice(), cm);

    connect(cm, &ClientManager::newINDIProperty, gdm, &INDI_D::buildProperty);
    connect(cm, &ClientManager::removeINDIProperty, gdm, &INDI_D::removeProperty);
    connect(cm, &ClientManager::newINDISwitch, gdm, &INDI_D::updateSwitchGUI);
    connect(cm, &ClientManager::newINDIText, gdm, &INDI_D::updateTextGUI);
    connect(cm, &ClientManager::newINDINumber, gdm, &INDI_D::updateNumberGUI);
    connect(cm, &ClientManager::newINDILight, gdm, &INDI_D::updateLightGUI);
    connect(cm, &ClientManager::newINDIBLOB, gdm, &INDI_D::updateBLOBGUI);

    connect(cm, &ClientManager::newINDIMessage, gdm, &INDI_D::updateMessageLog);

    QString deviceName = di->getDeviceName();
    int index = mainTabWidget->count();
    for(int i = 0; i < mainTabWidget->count(); i++)
    {
        if(mainTabWidget->tabText(i) > deviceName)
        {
            index = i;
            break;
        }
    }

    mainTabWidget->insertTab(index, gdm, di->getDeviceName());
    guidevices.insert(index, gdm);

    updateStatus(false);
}
