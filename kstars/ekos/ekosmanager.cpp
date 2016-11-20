/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikartech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QComboBox>

#include <KConfigDialog>
#include <KMessageBox>
#include <KActionCollection>
#include <KNotifications/KNotification>

#include <config-kstars.h>
#include <basedevice.h>

#include "ekosmanager.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "auxiliary/ksuserdb.h"
#include "fitsviewer/fitsviewer.h"
#include "skymap.h"

#include "capture/sequencejob.h"
#include "auxiliary/darklibrary.h"

#include "profileeditor.h"
#include "profileinfo.h"
#include "auxiliary/QProgressIndicator.h"

#include "indi/clientmanager.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/indilistener.h"
#include "indi/guimanager.h"
#include "indi/indiwebmanager.h"

#include "ekosadaptor.h"

#define MAX_REMOTE_INDI_TIMEOUT 15000
#define MAX_LOCAL_INDI_TIMEOUT 5000

EkosManager::EkosManager(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX

   if(Options::independentWindowEkos())
        setWindowFlags(Qt::Window);
    else{
       setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
       connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
    }

#endif
    setupUi(this);

    new EkosAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos",  this);

    setWindowIcon(QIcon::fromTheme("kstars_ekos", QIcon(":/icons/breeze/default/kstars_ekos.svg")));

    nDevices=0;
    //nConnectedDevices=0;
    useGuideHead    =false;
    useST4          =false;
    isStarted       = false;
    remoteManagerStart=false;
    localMode       = true;

    indiConnectionStatus = EKOS_STATUS_IDLE;
    ekosStartingStatus   = EKOS_STATUS_IDLE;

    profileModel = new QStandardItemModel(0, 4);
    profileModel->setHorizontalHeaderLabels(QStringList() << "id" << "name" << "host" << "port");

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
    alignProcess   = NULL;
    mountProcess   = NULL;
    domeProcess    = NULL;
    schedulerProcess = NULL;
    weatherProcess = NULL;
    dustCapProcess = NULL;

    ekosOption     = NULL;

    focusStarPixmap=guideStarPixmap=NULL;

    mountPI=capturePI=focusPI=guidePI=NULL;

    captureProgress->setValue(0);
    sequenceProgress->setValue(0);    
    sequenceProgress->setDecimals(0);
    sequenceProgress->setFormat("%v");
    imageProgress->setValue(0);
    imageProgress->setDecimals(1);
    imageProgress->setFormat("%v");
    imageProgress->setBarStyle(QRoundProgressBar::StyleLine);
    countdownTimer.setInterval(1000);
    connect(&countdownTimer, SIGNAL(timeout()), this, SLOT(updateCaptureCountDown()));

    toolsWidget->setIconSize(QSize(48,48));
    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    // Enable scheduler Tab
    toolsWidget->setTabEnabled(1, false);

    // Start/Stop INDI Server
    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    // Connect/Disconnect INDI devices
    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));
    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    // INDI Control Panel
    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));
    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));

    // Clear Ekos Log
    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    // Summary
    previewPixmap = new QPixmap(QPixmap(":/images/noimage.png"));    

    // Profiles
    connect(addProfileB, SIGNAL(clicked()), this, SLOT(addProfile()));
    connect(editProfileB, SIGNAL(clicked()), this, SLOT(editProfile()));
    connect(deleteProfileB, SIGNAL(clicked()), this, SLOT(deleteProfile()));
    connect(profileCombo, SIGNAL(activated(QString)), this, SLOT(saveDefaultProfile(QString)));

    addProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    editProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    deleteProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // Set Profile icons
    addProfileB->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/breeze/default/list-add.svg")));
    addProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    editProfileB->setIcon(QIcon::fromTheme("document-edit", QIcon(":/icons/breeze/default/document-edit.svg")));
    editProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    deleteProfileB->setIcon(QIcon::fromTheme("list-remove", QIcon(":/icons/breeze/default/list-remove.svg")));
    deleteProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // Load all drivers
    loadDrivers();

    // Load add driver profiles
    loadProfiles();

    // INDI Control Panel and Ekos Options
    controlPanelB->setIcon(QIcon::fromTheme("kstars_indi", QIcon(":/icons/indi.png")));
    optionsB->setIcon(QIcon::fromTheme("configure", QIcon(":/icons/ekos_setup.png")));

    // Setup Tab
    toolsWidget->tabBar()->setTabIcon(0, QIcon(":/icons/ekos_setup.png"));
    toolsWidget->tabBar()->setTabToolTip(0, i18n("Setup"));

    // Initialize Ekos Scheduler Module
    schedulerProcess = new Ekos::Scheduler();
    toolsWidget->addTab( schedulerProcess, QIcon(":/icons/ekos_scheduler.png"), "");
    toolsWidget->tabBar()->setTabToolTip(1, i18n("Scheduler"));
    connect(schedulerProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
    connect(schedulerProcess, SIGNAL(newTarget(QString)), mountTarget, SLOT(setText(QString)));

    // Temporary fix. Not sure how to resize Ekos Dialog to fit contents of the various tabs in the QScrollArea which are added
    // dynamically. I used setMinimumSize() but it doesn't appear to make any difference.
    // Also set Layout policy to SetMinAndMaxSize as well. Any idea how to fix this?
    // FIXME
    //resize(1000,750);
}

void EkosManager::changeAlwaysOnTop(Qt::ApplicationState state)
{
    if(isVisible()){
        if (state == Qt::ApplicationActive)
            setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        else
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
    }
}

EkosManager::~EkosManager()
{
    delete captureProcess;
    delete focusProcess;
    delete guideProcess;
    delete alignProcess;
    delete domeProcess;
    delete weatherProcess;
    delete mountProcess;
    delete schedulerProcess;
    delete dustCapProcess;
    delete profileModel;

    delete previewPixmap;
    delete focusStarPixmap;    
    delete guideStarPixmap;

    qDeleteAll(profiles);
}

void EkosManager::closeEvent(QCloseEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_ekos" );
    a->setChecked(false);
}

void EkosManager::hideEvent(QHideEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_ekos" );
    a->setChecked(false);
}

void EkosManager::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_ekos" );
    a->setChecked(true);
}

void EkosManager::resizeEvent(QResizeEvent *)
{
    previewImage->setPixmap(previewPixmap->scaled(previewImage->width(), previewImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (focusStarPixmap)
        focusStarImage->setPixmap(focusStarPixmap->scaled(focusStarImage->width(), focusStarImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    //if (focusProfilePixmap)
        //focusProfileImage->setPixmap(focusProfilePixmap->scaled(focusProfileImage->width(), focusProfileImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (guideStarPixmap)
        guideStarImage->setPixmap(guideStarPixmap->scaled(guideStarImage->width(), guideStarImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    //if (guideProfilePixmap)
        //guideProfileImage->setPixmap(guideProfilePixmap->scaled(guideProfileImage->width(), guideProfileImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

}

void EkosManager::loadProfiles()
{
    qDeleteAll(profiles);
    profiles.clear();
    KStarsData::Instance()->userdb()->GetAllProfiles(profiles);

    profileModel->clear();

    foreach(ProfileInfo *pi, profiles)
    {
        QList<QStandardItem*> info;

        info << new QStandardItem(pi->id) << new QStandardItem(pi->name) << new QStandardItem(pi->host) << new QStandardItem(pi->port);
        profileModel->appendRow(info);
    }

    profileModel->sort(0);
    profileCombo->setModel(profileModel);
    profileCombo->setModelColumn(1);

    // Load last used profile from options
    int index = profileCombo->findText(Options::profile());
    // If not found, set it to first item
    if (index == -1)
        index=0;
    profileCombo->setCurrentIndex(index);
}

void EkosManager::loadDrivers()
{
    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        if (dv->getDriverSource() != HOST_SOURCE)
            driversList[dv->getTreeLabel()] = dv;
    }
}

void EkosManager::reset()
{
    nDevices=0;
    //nConnectedDevices=0;
    nRemoteDevices=0;

    useGuideHead           = false;
    useST4                 = false;

    removeTabs();

    genericDevices.clear();
    managedDevices.clear();

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
    domeProcess    = NULL;
    alignProcess   = NULL;
    mountProcess   = NULL;
    weatherProcess = NULL;
    dustCapProcess = NULL;

    ekosStartingStatus  = EKOS_STATUS_IDLE;
    indiConnectionStatus= EKOS_STATUS_IDLE;

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
    processINDIB->setEnabled(true);

    mountGroup->setEnabled(false);
    focusGroup->setEnabled(false);
    captureGroup->setEnabled(false);
    guideGroup->setEnabled(false);
    sequenceLabel->setText(i18n("Sequence"));
    sequenceProgress->setValue(0);
    captureProgress->setValue(0);
    overallRemainingTime->setText("--:--:--");
    sequenceRemainingTime->setText("--:--:--");
    imageRemainingTime->setText("--:--:--");
    mountStatus->setText(i18n("Idle"));
    captureStatus->setText(i18n("Idle"));
    focusStatus->setText(i18n("Idle"));
    guideStatus->setText(i18n("Idle"));
    if (capturePI)
        capturePI->stopAnimation();
    if (mountPI)
        mountPI->stopAnimation();
    if (focusPI)
        focusPI->stopAnimation();
    if (guidePI)
        guidePI->stopAnimation();

    isStarted = false;
    processINDIB->setText(i18n("Start INDI"));
}

void EkosManager::processINDI()
{
    if (isStarted == false)
        start();
    else
        stop();
}

bool EkosManager::stop()
{
    cleanDevices();

    profileGroup->setEnabled(true);

    return true;
}

bool EkosManager::start()
{
    if (localMode)
        qDeleteAll(managedDrivers);
    managedDrivers.clear();

    // If clock was paused, unpaused it and sync time
    if (KStarsData::Instance()->clock()->isActive() == false)
    {
        KStarsData::Instance()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );
        KStarsData::Instance()->clock()->start();
    }

    reset();

    ProfileInfo *currentProfile = getCurrentProfile();
    localMode = currentProfile->isLocal();

    // Load profile location if one exists
    updateProfileLocation(currentProfile);

    bool haveCCD=false, haveGuider=false;

    if (localMode)
    {
        DriverInfo *drv = NULL;

        drv = driversList.value(currentProfile->mount());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->ccd());
        if (drv != NULL)
        {
            managedDrivers.append(drv->clone());
            haveCCD = true;
        }

        drv = driversList.value(currentProfile->guider());
        if (drv != NULL)
        {
            haveGuider = true;

            // If the guider and ccd are the same driver, we have two cases:
            // #1 Drivers that only support ONE device per driver (such as sbig)
            // #2 Drivers that supports multiples devices per driver (such as sx)
            // For #1, we modify guider_di to make a unique label for the other device with postfix "Guide"
            // For #2, we set guider_di to NULL and we prompt the user to select which device is primary ccd and which is guider
            // since this is the only way to find out in real time.
            if (haveCCD && currentProfile->guider() == currentProfile->ccd())
            {
                if (drv->getAuxInfo().value("mdpd", false).toBool() == true)
                    drv = NULL;
                else
                {
                    drv->setUniqueLabel(drv->getTreeLabel() + " Guide");
                }
            }

            if (drv)
                managedDrivers.append(drv->clone());
        }

        drv = driversList.value(currentProfile->ao());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->filter());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->focuser());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->dome());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->weather());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux1());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux2());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux3());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux4());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        if (haveCCD == false && haveGuider == false)
        {
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            managedDrivers.clear();
            return false;
        }

        nDevices = managedDrivers.count();
    }
    else
    {
        DriverInfo *remote_indi = new DriverInfo(QString("Ekos Remote Host"));

        remote_indi->setHostParameters(currentProfile->host, QString::number(currentProfile->port));

        remote_indi->setDriverSource(GENERATED_SOURCE);

        managedDrivers.append(remote_indi);

        haveCCD    = currentProfile->drivers.contains("CCD");
        haveGuider = currentProfile->drivers.contains("Guider");

        if (haveCCD == false && haveGuider == false)
        {
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices=0;
            return false;
        }

        nDevices = currentProfile->drivers.count();

        nRemoteDevices=0;
    }

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this, SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this, SLOT(setFocuser(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDome(ISD::GDInterface*)), this, SLOT(setDome(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newWeather(ISD::GDInterface*)), this, SLOT(setWeather(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDustCap(ISD::GDInterface*)), this, SLOT(setDustCap(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newLightBox(ISD::GDInterface*)), this, SLOT(setLightBox(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newST4(ISD::ST4*)), this, SLOT(setST4(ISD::ST4*)));
    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this, SLOT(removeDevice(ISD::GDInterface*)), Qt::DirectConnection);

    if (localMode)
    {
        if (isRunning("indiserver"))
        {
            if (KMessageBox::Yes == (KMessageBox::questionYesNo(0, i18n("Ekos detected an instance of INDI server running. Do you wish to shut down the existing instance before starting a new one?"),
                                                                i18n("INDI Server"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "ekos_shutdown_existing_indiserver")))
            {
                //TODO is there a better way to do this.
                QProcess p;
                p.start("pkill indiserver");
                p.waitForFinished();
            }
        }

        appendLogText(i18n("Starting INDI services..."));

        if (DriverManager::Instance()->startDevices(managedDrivers) == false)
        {
            INDIListener::Instance()->disconnect(this);
            qDeleteAll(managedDrivers);
            managedDrivers.clear();
            ekosStartingStatus = EKOS_STATUS_ERROR;
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this, SLOT(processServerTermination(QString, QString)));

        ekosStartingStatus = EKOS_STATUS_PENDING;

        appendLogText(i18n("INDI services started on port %1. Please connect devices.", managedDrivers.first()->getPort()));

        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));
    }
    else
    {
        // If we need to use INDI Web Manager
        if (currentProfile->INDIWebManagerPort > 0)
        {
            remoteManagerStart = false;
            if (INDI::WebManager::isOnline(currentProfile))
            {
                if (INDI::WebManager::areDriversRunning(currentProfile) == false)
                {
                    INDI::WebManager::stopProfile(currentProfile);

                    if (INDI::WebManager::startProfile(currentProfile) == false)
                    {
                        appendLogText(i18n("Failed to start profile on remote INDI Web Manager."));
                        return false;
                    }

                    appendLogText(i18n("Starting profile on remote INDI Web Manager..."));
                    remoteManagerStart = true;
                }
            }
            else
                appendLogText(i18n("Warning: INDI Web Manager is not online."));
        }

        appendLogText(i18n("Connecting to remote INDI server at %1 on port %2 ...", currentProfile->host, currentProfile->port));
        qApp->processEvents();

        QApplication::setOverrideCursor(Qt::WaitCursor);

        if (DriverManager::Instance()->connectRemoteHost(managedDrivers.first()) == false)
        {
            appendLogText(i18n("Failed to connect to remote INDI server!"));
            INDIListener::Instance()->disconnect(this);
            qDeleteAll(managedDrivers);
            managedDrivers.clear();
            ekosStartingStatus = EKOS_STATUS_ERROR;
            QApplication::restoreOverrideCursor();
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this, SLOT(processServerTermination(QString, QString)));

        QApplication::restoreOverrideCursor();
        ekosStartingStatus = EKOS_STATUS_PENDING;

        appendLogText(i18n("INDI services started. Connection to remote INDI server is successful. Waiting for devices..."));

        QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));
    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    profileGroup->setEnabled(false);

    isStarted = true;
    processINDIB->setText(i18n("Stop INDI"));

    return true;
}

void EkosManager::checkINDITimeout()
{
    // Don't check anything unless we're still pending
    if (ekosStartingStatus != EKOS_STATUS_PENDING)
        return;

    if (nDevices <= 0)
    {
        ekosStartingStatus = EKOS_STATUS_SUCCESS;
        return;
    }

    if (localMode)
    {
        QStringList remainingDevices;
        foreach(DriverInfo *drv, managedDrivers)
        {
            if (drv->getDevices().count() == 0)
                remainingDevices << QString("+ %1").arg(drv->getUniqueLabel().isEmpty() == false ? drv->getUniqueLabel() : drv->getName());
        }

        if (remainingDevices.count() == 1)
        {
            appendLogText(i18n("Unable to establish:\n%1\nPlease ensure the device is connected and powered on.", remainingDevices.at(0)));
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            appendLogText(i18n("Unable to establish the following devices:\n%1\nPlease ensure each device is connected and powered on.", remainingDevices.join("\n")));
            KNotification::beep(i18n("Ekos startup error"));
        }
    }
    else
    {
        ProfileInfo *currentProfile = getCurrentProfile();

        QStringList remainingDevices;

        foreach(QString driver, currentProfile->drivers.values())
        {
            bool driverFound=false;

            foreach(ISD::GDInterface *device, genericDevices)
            {
                if (device->getBaseDevice()->getDriverName() == driver)
                {
                    driverFound = true;
                    break;
                }
            }

            if (driverFound == false)
                remainingDevices << QString("+ %1").arg(driver);
        }

        if (remainingDevices.count() == 1)
        {
            appendLogText(i18n("Unable to establish remote device:\n%1\nPlease ensure remote device name corresponds to actual device name.", remainingDevices.at(0)));
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            appendLogText(i18n("Unable to establish remote devices:\n%1\nPlease ensure remote device name corresponds to actual device name.", remainingDevices.join("\n")));
            KNotification::beep(i18n("Ekos startup error"));
        }
    }

    ekosStartingStatus = EKOS_STATUS_ERROR;
}

void EkosManager::connectDevices()
{
    indiConnectionStatus = EKOS_STATUS_PENDING;

    foreach(ISD::GDInterface *device, genericDevices)
        device->Connect();

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText(i18n("Connecting INDI devices..."));
}

void EkosManager::disconnectDevices()
{
    foreach(ISD::GDInterface *device, genericDevices)
        device->Disconnect();

    appendLogText(i18n("Disconnecting INDI devices..."));

}

void EkosManager::processServerTermination(const QString &host, const QString &port)
{
    ProfileInfo *pi = getCurrentProfile();

    if ( (localMode && managedDrivers.first()->getPort() == port) || (pi->host == host && pi->port == port.toInt()))
    {
        cleanDevices(false);
    }
}

void EkosManager::cleanDevices(bool stopDrivers)
{
    if (ekosStartingStatus == EKOS_STATUS_IDLE)
        return;

    INDIListener::Instance()->disconnect(this);
    DriverManager::Instance()->disconnect(this);

    if (managedDrivers.isEmpty() == false)
    {
        if (localMode)
        {
            if (stopDrivers)
                DriverManager::Instance()->stopDevices(managedDrivers);
        }
        else
        {
            if (stopDrivers)
                DriverManager::Instance()->disconnectRemoteHost(managedDrivers.first());

            ProfileInfo *pi = getCurrentProfile();
            if (remoteManagerStart && pi->INDIWebManagerPort != -1)
            {
                INDI::WebManager::stopProfile(pi);
                remoteManagerStart = false;
            }
        }
    }

    reset();

    profileGroup->setEnabled(true);

    appendLogText(i18n("INDI services stopped."));
}

void EkosManager::processNewDevice(ISD::GDInterface *devInterface)
{
    if (Options::verboseLogging())
        qDebug() << "Ekos received a new device: " << devInterface->getDeviceName();

    // Always reset INDI Connection status if we receive a new device
    indiConnectionStatus = EKOS_STATUS_IDLE;

    genericDevices.append(devInterface);

    nDevices--;

    connect(devInterface, SIGNAL(Connected()), this, SLOT(deviceConnected()));
    connect(devInterface, SIGNAL(Disconnected()), this, SLOT(deviceDisconnected()));
    connect(devInterface, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));

    if (nDevices <= 0)
    {
        ekosStartingStatus = EKOS_STATUS_SUCCESS;

        if (localMode == false && nDevices == 0)
            appendLogText(i18n("Remote devices established. Please connect devices."));

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
    }
}

void EkosManager::deviceConnected()
{
    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    processINDIB->setEnabled(false);

    if (Options::verboseLogging())
    {
        ISD::GDInterface *device = (ISD::GDInterface *) sender();
        qDebug() << "Ekos: " << device->getDeviceName() << "is connected.";
        //qDebug() << "Managed Devices: " << managedDrivers.count() << " Remote Devices: " << nRemoteDevices;
        //qDebug() << "Connected Devices: " << nConnectedDevices << " nDevices: " << nDevices;
    }

    //ProfileInfo *pi = getCurrentProfile();
    //if (nConnectedDevices == managedDrivers.count() || (nDevices <=0 && nConnectedDevices == nRemoteDevices))

    int nConnectedDevices=0;
    foreach(ISD::GDInterface *device, genericDevices)
    {
        if (device->isConnected())
            nConnectedDevices++;
    }

    qDebug() << "Ekos: " << nConnectedDevices << " devices connected out of " << genericDevices.count();

    //if (nConnectedDevices >= pi->drivers.count())
    if (nConnectedDevices >= genericDevices.count())
    {
        indiConnectionStatus = EKOS_STATUS_SUCCESS;
        qDebug() << "Ekos: All INDI devices are now connected.";
    }
    else
        indiConnectionStatus = EKOS_STATUS_PENDING;



    ISD::GDInterface *dev = static_cast<ISD::GDInterface *> (sender());

    if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        if (mountProcess)
        {
            mountProcess->setEnabled(true);
            if (alignProcess)
                alignProcess->setEnabled(true);
        }
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess)
            captureProcess->setEnabled(true);
        if (focusProcess)
            focusProcess->setEnabled(true);
        if (alignProcess)
        {
            if (mountProcess && mountProcess->isEnabled())
                alignProcess->setEnabled(true);
            else
                alignProcess->setEnabled(false);
        }
        if (guideProcess)
            guideProcess->setEnabled(true);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess)
            focusProcess->setEnabled(true);
    }

    if (Options::neverLoadConfig())
        return;

    INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

    foreach(ISD::GDInterface *device, genericDevices)
    {
        if (device == dev)
        {
            ISwitchVectorProperty *configProp = device->getBaseDevice()->getSwitch("CONFIG_PROCESS");
            if (configProp && configProp->s == IPS_IDLE)
                device->setConfig(tConfig);
            break;
        }
    }
}

void EkosManager::deviceDisconnected()
{
    ISD::GDInterface *dev = static_cast<ISD::GDInterface *> (sender());

    if (dev)
    {
        if (dev->getState("CONNECTION") == IPS_ALERT)
            indiConnectionStatus = EKOS_STATUS_ERROR;
        else if (dev->getState("CONNECTION") == IPS_BUSY)
            indiConnectionStatus = EKOS_STATUS_PENDING;
        else
            indiConnectionStatus = EKOS_STATUS_IDLE;

        if (Options::verboseLogging())
        {
            qDebug() << "Ekos: " << dev->getDeviceName() << " is disconnected.";
            //qDebug() << "Connected Devices: " << nConnectedDevices << " nDevices: " << nDevices;
        }
    }
    else
        indiConnectionStatus = EKOS_STATUS_IDLE;

    //if (indiConnectionStatus == EKOS_STATUS_IDLE)
        //nConnectedDevices--;

    //if (nConnectedDevices < 0)
        //nConnectedDevices = 0;

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);
    processINDIB->setEnabled(true);

    if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        if (mountProcess)
            mountProcess->setEnabled(false);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess)
            captureProcess->setEnabled(false);
        if (focusProcess)
            focusProcess->setEnabled(false);
        if (alignProcess)
            alignProcess->setEnabled(false);
        if (guideProcess)
            guideProcess->setEnabled(false);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess)
            focusProcess->setEnabled(false);
    }
}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    //mount = scopeDevice;

    managedDevices[KSTARS_TELESCOPE] = scopeDevice;

    appendLogText(i18n("%1 is online.", scopeDevice->getDeviceName()));

    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));

    initMount();

    mountProcess->setTelescope(scopeDevice);

    if (guideProcess)
        guideProcess->setTelescope(scopeDevice);

    if (alignProcess)
        alignProcess->setTelescope(scopeDevice);
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{
    managedDevices.insertMulti(KSTARS_CCD, ccdDevice);

    initCapture();

    captureProcess->addCCD(ccdDevice);

    ProfileInfo *pi = getCurrentProfile();
    QString primaryCCD, guiderCCD;

    // Only look for primary & guider CCDs if we can tell a difference between them
    // otherwise rely on saved options
    if (pi->ccd() != pi->guider())
    {
        foreach(ISD::GDInterface *device, findDevices(KSTARS_CCD))
        {
            if (QString(device->getDeviceName()).startsWith(pi->ccd(), Qt::CaseInsensitive))
                primaryCCD = QString(device->getDeviceName());
            else if (QString(device->getDeviceName()).startsWith(pi->guider(), Qt::CaseInsensitive))
                guiderCCD = QString(device->getDeviceName());
        }
    }

    bool rc=false;
    if (Options::defaultCaptureCCD().isEmpty() == false)
        rc = captureProcess->setCCD(Options::defaultCaptureCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        captureProcess->setCCD(primaryCCD);

    initFocus();

    focusProcess->addCCD(ccdDevice);

    rc=false;
    if (Options::defaultFocusCCD().isEmpty() == false)
        rc = focusProcess->setCCD(Options::defaultFocusCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        focusProcess->setCCD(primaryCCD);

    initAlign();

    alignProcess->addCCD(ccdDevice);

    rc=false;
    if (Options::defaultAlignCCD().isEmpty() == false)
        rc = alignProcess->setCCD(Options::defaultAlignCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        alignProcess->setCCD(primaryCCD);

    initGuide();

    guideProcess->addCCD(ccdDevice);

    rc=false;
    if (Options::defaultGuideCCD().isEmpty() == false)
        rc = guideProcess->setCCD(Options::defaultGuideCCD());
    if (rc == false && guiderCCD.isEmpty() == false)
        guideProcess->setCCD(guiderCCD);

    appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    if (managedDevices.contains(KSTARS_TELESCOPE))
    {
        alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
        captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
        guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
    }

}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{

    managedDevices.insertMulti(KSTARS_FILTER, filterDevice);

    appendLogText(i18n("%1 filter is online.", filterDevice->getDeviceName()));

    initCapture();

    connect(filterDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));
    connect(filterDevice, SIGNAL(textUpdated(ITextVectorProperty*)), this, SLOT(processNewText(ITextVectorProperty*)));

    captureProcess->addFilter(filterDevice);

    initFocus();

    focusProcess->addFilter(filterDevice);
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    managedDevices.insertMulti(KSTARS_FILTER, focuserDevice);

    initCapture();

    initFocus();

    focusProcess->addFocuser(focuserDevice);

    appendLogText(i18n("%1 is online.", focuserDevice->getDeviceName()));
}

void EkosManager::setDome(ISD::GDInterface *domeDevice)
{
    managedDevices[KSTARS_DOME] = domeDevice;

    initDome();

    domeProcess->setDome(domeDevice);

    if (captureProcess)
        captureProcess->setDome(domeDevice);

    appendLogText(i18n("%1 is online.", domeDevice->getDeviceName()));
}

void EkosManager::setWeather(ISD::GDInterface *weatherDevice)
{
    managedDevices[KSTARS_WEATHER] = weatherDevice;

    initWeather();

    weatherProcess->setWeather(weatherDevice);

    appendLogText(i18n("%1 is online.", weatherDevice->getDeviceName()));
}

void EkosManager::setDustCap(ISD::GDInterface *dustCapDevice)
{
    managedDevices.insertMulti(KSTARS_AUXILIARY, dustCapDevice);

    initDustCap();

    dustCapProcess->setDustCap(dustCapDevice);

    appendLogText(i18n("%1 is online.", dustCapDevice->getDeviceName()));

    if (captureProcess)
        captureProcess->setDustCap(dustCapDevice);
}

void EkosManager::setLightBox(ISD::GDInterface *lightBoxDevice)
{
    managedDevices.insertMulti(KSTARS_AUXILIARY, lightBoxDevice);

    if (captureProcess)
        captureProcess->setLightBox(lightBoxDevice);
}

void EkosManager::removeDevice(ISD::GDInterface* devInterface)
{
    switch (devInterface->getType())
    {
    case KSTARS_CCD:
        removeTabs();

        break;

    case KSTARS_FOCUSER:
        break;

    default:
        break;
    }

    appendLogText(i18n("%1 is offline.", devInterface->getDeviceName()));

    foreach(ISD::GDInterface *device, managedDevices.values())
    {
        if (device == devInterface)
        {
            managedDevices.remove(managedDevices.key(device));

            if (managedDevices.count() == 0)
                cleanDevices();

            break;
        }
    }
}

void EkosManager::processNewText(ITextVectorProperty *tvp)
{
    if (!strcmp(tvp->name, "FILTER_NAME"))
    {
        if (captureProcess)
            captureProcess->checkFilter();

        if (focusProcess)
            focusProcess->checkFilter();
    }
}

void EkosManager::processNewNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (guideProcess)
        {
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            guideProcess->syncTelescopeInfo();
        }

        if (alignProcess)
        {
            alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            alignProcess->syncTelescopeInfo();
        }

        if (mountProcess)
        {
            mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            mountProcess->syncTelescopeInfo();
        }

        return;

    }

    if (!strcmp(nvp->name, "CCD_INFO") || !strcmp(nvp->name, "GUIDER_INFO")  || !strcmp(nvp->name, "CCD_FRAME") || !strcmp(nvp->name, "GUIDER_FRAME"))
    {
        if (focusProcess)
            focusProcess->syncCCDInfo();

        if (guideProcess)
            guideProcess->syncCCDInfo();

        if (alignProcess)
            alignProcess->syncCCDInfo();

        return;
    }

    if (!strcmp(nvp->name, "FILTER_SLOT"))
    {
        if (captureProcess)
            captureProcess->checkFilter();

        if (focusProcess)
            focusProcess->checkFilter();
    }

}

void EkosManager::processNewProperty(INDI::Property* prop)
{
    if (!strcmp(prop->getName(), "CCD_INFO") || !strcmp(prop->getName(), "GUIDER_INFO"))
    {
        if (focusProcess)
            focusProcess->syncCCDInfo();

        if (guideProcess)
            guideProcess->syncCCDInfo();

        if (alignProcess)
            alignProcess->syncCCDInfo();

        return;
    }

    if (!strcmp(prop->getName(), "TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (guideProcess)
        {
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            guideProcess->syncTelescopeInfo();
        }

        if (alignProcess)
        {
            alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            alignProcess->syncTelescopeInfo();
        }

        if (mountProcess)
        {
            mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            mountProcess->syncTelescopeInfo();
        }

        return;
    }

    if (!strcmp(prop->getName(), "GUIDER_EXPOSURE"))
    {
        foreach(ISD::GDInterface *device, findDevices(KSTARS_CCD))
        {
            if (!strcmp(device->getDeviceName(), prop->getDeviceName()))
            {
                initCapture();
                initGuide();
                useGuideHead=true;
                captureProcess->addGuideHead(device);
                guideProcess->addGuideHead(device);

                bool rc = false;
                if (Options::defaultGuideCCD().isEmpty() == false)
                    rc = guideProcess->setCCD(Options::defaultGuideCCD());
                if (rc == false)
                    guideProcess->setCCD(QString(device->getDeviceName()) + QString(" Guider"));
                return;
            }
        }

        return;
    }

    if (!strcmp(prop->getName(), "CCD_FRAME_TYPE"))
    {
        if (captureProcess)
        {
            foreach(ISD::GDInterface *device, findDevices(KSTARS_CCD))
            {
                if (!strcmp(device->getDeviceName(), prop->getDeviceName()))
                {
                    captureProcess->syncFrameType(device);
                    return;
                }
            }
        }

        return;
    }

    if (!strcmp(prop->getName(), "TELESCOPE_PARK") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (captureProcess)
            captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        return;
    }

    if (!strcmp(prop->getName(), "FILTER_NAME"))
    {
        if (captureProcess)
            captureProcess->checkFilter();

        if (focusProcess)
            focusProcess->checkFilter();

        return;
    }

    if (strstr(prop->getName(), "FOCUS_POSITION"))
    {
        if (focusProcess)
            focusProcess->checkFocuser();
    }

}

QList<ISD::GDInterface*> EkosManager::findDevices(DeviceFamily type)
{
    QList<ISD::GDInterface*> deviceList;

    QMapIterator<DeviceFamily, ISD::GDInterface*> i(managedDevices);
    while (i.hasNext())
    {
        i.next();

        if (i.key() == type)
            deviceList.append(i.value());
    }

    return deviceList;
}

void EkosManager::processTabChange()
{
    QWidget *currentWidget = toolsWidget->currentWidget();

    //if (focusProcess && currentWidget != focusProcess)
         //focusProcess->resetFrame();

    if (alignProcess && alignProcess == currentWidget)
    {
        if (alignProcess->isEnabled() == false && captureProcess->isEnabled())
        {
            if (managedDevices[KSTARS_CCD]->isConnected() && managedDevices.contains(KSTARS_TELESCOPE) && alignProcess->isParserOK())
                alignProcess->setEnabled(true);
        }

        alignProcess->checkCCD();
    }
    else if (captureProcess && currentWidget == captureProcess)
    {
        captureProcess->checkCCD();
    }
    else if (focusProcess && currentWidget == focusProcess)
    {
        focusProcess->checkCCD();
    }
    else if (guideProcess && currentWidget == guideProcess)
    {
        guideProcess->checkCCD();
    }

    updateLog();

}

void EkosManager::updateLog()
{
    //if (enableLoggingCheck->isChecked() == false)
        //return;

    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
        ekosLogOut->setPlainText(logText.join("\n"));
    else if (currentWidget == alignProcess)
        ekosLogOut->setPlainText(alignProcess->getLogText());
    else if (currentWidget == captureProcess)
        ekosLogOut->setPlainText(captureProcess->getLogText());
    else if (currentWidget == focusProcess)
        ekosLogOut->setPlainText(focusProcess->getLogText());
    else if (currentWidget == guideProcess)
        ekosLogOut->setPlainText(guideProcess->getLogText());
    else if (currentWidget == mountProcess)
        ekosLogOut->setPlainText(mountProcess->getLogText());
    if (currentWidget == schedulerProcess)
        ekosLogOut->setPlainText(schedulerProcess->getLogText());


}

void EkosManager::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::verboseLogging())
        qDebug() << "Ekos: " << text;

    updateLog();
}

void EkosManager::clearLog()
{
    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
    {
        logText.clear();
        updateLog();
    }
    else if (currentWidget == alignProcess)
        alignProcess->clearLog();
    else if (currentWidget == captureProcess)
        captureProcess->clearLog();
    else if (currentWidget == focusProcess)
        focusProcess->clearLog();
    else if (currentWidget == guideProcess)
        guideProcess->clearLog();
    else if (currentWidget == mountProcess)
        mountProcess->clearLog();
    else if (currentWidget == schedulerProcess)
        schedulerProcess->clearLog();

}

void EkosManager::initCapture()
{
    if (captureProcess)
        return;

    captureProcess = new Ekos::Capture();
    int index = toolsWidget->addTab( captureProcess, QIcon(":/icons/ekos_ccd.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18nc("Charge-Coupled Device", "CCD"));
    connect(captureProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
    connect(captureProcess, SIGNAL(newStatus(Ekos::CaptureState)), this, SLOT(updateCaptureStatus(Ekos::CaptureState)));
    connect(captureProcess, SIGNAL(newImage(QImage*, Ekos::SequenceJob*)), this, SLOT(updateCaptureProgress(QImage*, Ekos::SequenceJob*)));
    connect(captureProcess, SIGNAL(newExposureProgress(Ekos::SequenceJob*)), this, SLOT(updateExposureProgress(Ekos::SequenceJob*)));
    captureGroup->setEnabled(true);
    sequenceProgress->setEnabled(true);
    captureProgress->setEnabled(true);
    imageProgress->setEnabled(true);

    capturePI = new QProgressIndicator(captureProcess);
    captureStatusLayout->addWidget(capturePI);

    foreach(ISD::GDInterface *device, findDevices(KSTARS_AUXILIARY))
    {
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
            captureProcess->setDustCap(device);
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::LIGHTBOX_INTERFACE)
            captureProcess->setLightBox(device);
    }

    if (focusProcess)
    {
        // Autofocus
        connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(newStatus(Ekos::FocusState)), captureProcess, SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
        connect(focusProcess, &Ekos::Focus::newHFR, captureProcess, &Ekos::Capture::setHFR, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), focusProcess, SLOT(resetFrame()), Qt::UniqueConnection);
    }

    if (alignProcess)
    {
        // Alignment flag       
        connect(alignProcess, SIGNAL(newStatus(Ekos::AlignState)), captureProcess, SLOT(setAlignStatus(Ekos::AlignState)), Qt::UniqueConnection);

        // Capture Status
        connect(captureProcess, SIGNAL(newStatus(Ekos::CaptureState)), alignProcess, SLOT(setCaptureStatus(Ekos::CaptureState)), Qt::UniqueConnection);

    }

    if (mountProcess)
    {
        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), mountProcess, SLOT(disableAltLimits()), Qt::UniqueConnection);
        connect(captureProcess, SIGNAL(meridianFlipCompleted()), mountProcess, SLOT(enableAltLimits()), Qt::UniqueConnection);
    }

    if (managedDevices.contains(KSTARS_DOME))
    {
        captureProcess->setDome(managedDevices[KSTARS_DOME]);
    }

}

void EkosManager::initAlign()
{
    if (alignProcess)
        return;

    alignProcess = new Ekos::Align();
    alignProcess->setEnabled(false);
    int index = toolsWidget->addTab( alignProcess, QIcon(":/icons/ekos_align.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Align"));
    connect(alignProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    if (captureProcess)
    {
        // Align Status
        connect(alignProcess, SIGNAL(newStatus(Ekos::AlignState)), captureProcess, SLOT(setAlignStatus(Ekos::AlignState)), Qt::UniqueConnection);
        // Capture Status
        connect(captureProcess, SIGNAL(newStatus(Ekos::CaptureState)), alignProcess, SLOT(setCaptureStatus(Ekos::CaptureState)), Qt::UniqueConnection);
    }

    if (focusProcess)
    {
        // Filter lock
        connect(focusProcess, SIGNAL(filterLockUpdated(ISD::GDInterface*,int)), alignProcess, SLOT(setLockedFilter(ISD::GDInterface*,int)), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(newStatus(Ekos::FocusState)) , alignProcess, SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
    }
}


void EkosManager::initFocus()
{
    if (focusProcess)
        return;

    focusProcess = new Ekos::Focus();
    int index = toolsWidget->addTab( focusProcess, QIcon(":/icons/ekos_focus.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Focus"));
    connect(focusProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
    connect(focusProcess, SIGNAL(newStatus(Ekos::FocusState)), this, SLOT(setFocusStatus(Ekos::FocusState)));
    connect(focusProcess, SIGNAL(newStarPixmap(QPixmap&)), this, SLOT(updateFocusStarPixmap(QPixmap&)));
    connect(focusProcess, SIGNAL(newProfilePixmap(QPixmap&)), this, SLOT(updateFocusProfilePixmap(QPixmap&)));

    focusGroup->setEnabled(true);

    focusPI = new QProgressIndicator(focusProcess);
    focusStatusLayout->addWidget(focusPI);

    if (captureProcess)
    {
        // Autofocus
        connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(newStatus(Ekos::FocusState)), captureProcess, SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
        connect(focusProcess, &Ekos::Focus::newHFR, captureProcess, &Ekos::Capture::setHFR, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), focusProcess, SLOT(resetFrame()), Qt::UniqueConnection);
    }

    if (guideProcess)
    {
        // Suspend
        connect(focusProcess, SIGNAL(suspendGuiding()), guideProcess, SLOT(suspend()), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(resumeGuiding()), guideProcess, SLOT(resume()), Qt::UniqueConnection);
    }

    if (alignProcess)
    {
        // Filter lock
        connect(focusProcess, SIGNAL(filterLockUpdated(ISD::GDInterface*,int)), alignProcess, SLOT(setLockedFilter(ISD::GDInterface*,int)), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(newStatus(Ekos::FocusState)), alignProcess, SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
    }

}

void EkosManager::initMount()
{
    if (mountProcess)
        return;

    mountProcess = new Ekos::Mount();
    int index = toolsWidget->addTab(mountProcess, QIcon(":/icons/ekos_mount.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Mount"));

    connect(mountProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
    connect(mountProcess, SIGNAL(newCoords(QString,QString,QString,QString)), this, SLOT(updateMountCoords(QString,QString,QString,QString)));
    connect(mountProcess, SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), this, SLOT(updateMountStatus(ISD::Telescope::TelescopeStatus)));
    connect(mountProcess, SIGNAL(newTarget(QString)), mountTarget, SLOT(setText(QString)));

    mountPI = new QProgressIndicator(mountProcess);
    mountStatusLayout->addWidget(mountPI);
    mountGroup->setEnabled(true);

    if (captureProcess)
    {
        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), mountProcess, SLOT(disableAltLimits()), Qt::UniqueConnection);
        connect(captureProcess, SIGNAL(meridianFlipCompleted()), mountProcess, SLOT(enableAltLimits()), Qt::UniqueConnection);
    }

    if (guideProcess)
    {
        connect(mountProcess, SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), guideProcess, SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
    }

}

void EkosManager::initGuide()
{
    if (guideProcess == NULL)
        guideProcess = new Ekos::Guide();

    //if ( (haveGuider || ccdCount > 1 || useGuideHead) && useST4 && toolsWidget->indexOf(guideProcess) == -1)
    if ( (findDevices(KSTARS_CCD).isEmpty() == false || useGuideHead) && useST4 && toolsWidget->indexOf(guideProcess) == -1)
    {

        //if (mount && mount->isConnected())
        if (managedDevices.contains(KSTARS_TELESCOPE) && managedDevices[KSTARS_TELESCOPE]->isConnected())
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        int index = toolsWidget->addTab( guideProcess, QIcon(":/icons/ekos_guide.png"), "");
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Guide"));
        connect(guideProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
        guideGroup->setEnabled(true);

        guidePI = new QProgressIndicator(guideProcess);
        guideStatusLayout->addWidget(guidePI);

        connect(guideProcess, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(updateGuideStatus(Ekos::GuideState)));
        connect(guideProcess, SIGNAL(newStarPixmap(QPixmap&)), this, SLOT(updateGuideStarPixmap(QPixmap&)));
        connect(guideProcess, SIGNAL(newProfilePixmap(QPixmap&)), this, SLOT(updateGuideProfilePixmap(QPixmap&)));
    }

    if (captureProcess)
    {
        guideProcess->disconnect(captureProcess);
        captureProcess->disconnect(guideProcess);

        // Guide Limits
        //connect(guideProcess, SIGNAL(guideReady()), captureProcess, SLOT(enableGuideLimits()));
        connect(guideProcess, SIGNAL(newStatus(Ekos::GuideState)), captureProcess, SLOT(setGuideStatus(Ekos::GuideState)));
        connect(guideProcess, SIGNAL(newAxisDelta(double,double)), captureProcess, SLOT(setGuideDeviation(double,double)));

        // Dithering
        connect(captureProcess, SIGNAL(newStatus(Ekos::CaptureState)), guideProcess, SLOT(setCaptureStatus(Ekos::CaptureState)), Qt::UniqueConnection);


        // Guide Head
        connect(captureProcess, SIGNAL(suspendGuiding()), guideProcess, SLOT(suspend()));
        connect(captureProcess, SIGNAL(resumeGuiding()), guideProcess, SLOT(resume()));
        connect(guideProcess, SIGNAL(guideChipUpdated(ISD::CCDChip*)), captureProcess, SLOT(setGuideChip(ISD::CCDChip*)));

        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), guideProcess, SLOT(abort()), Qt::UniqueConnection);
        connect(captureProcess, SIGNAL(meridianFlipCompleted()), guideProcess, SLOT(startAutoCalibrateGuide()), Qt::UniqueConnection);
    }

    if (mountProcess)
    {
        // Parking
        connect(mountProcess, SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), guideProcess, SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
    }

    if (focusProcess)
    {
        // Suspend
        connect(focusProcess, SIGNAL(suspendGuiding()), guideProcess, SLOT(suspend()), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(resumeGuiding()), guideProcess, SLOT(resume()), Qt::UniqueConnection);
    }

}

void EkosManager::initDome()
{
    if (domeProcess)
        return;

    domeProcess = new Ekos::Dome();
}

void EkosManager::initWeather()
{
    if (weatherProcess)
        return;

    weatherProcess = new Ekos::Weather();
}

void EkosManager::initDustCap()
{
    if (dustCapProcess)
        return;

    dustCapProcess = new Ekos::DustCap();
}

void EkosManager::setST4(ISD::ST4 * st4Driver)
{
    appendLogText(i18n("Guider port from %1 is ready.", st4Driver->getDeviceName()));

    useST4=true;

    initGuide();

    guideProcess->addST4(st4Driver);

    if (Options::defaultST4Driver().isEmpty() == false)
        guideProcess->setST4(Options::defaultST4Driver());
    //if (ao && ao->getDeviceName() == st4Driver->getDeviceName())
    //if (managedDevices.contains(KSTARS_ADAPTIVE_OPTICS) && (st4Driver->getDeviceName() == managedDevices[KSTARS_ADAPTIVE_OPTICS]->getDeviceName()))
        //guideProcess->setAO(st4Driver);

}

void EkosManager::removeTabs()
{
    disconnect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    for (int i=2; i < toolsWidget->count(); i++)
        toolsWidget->removeTab(i);

    delete (alignProcess);
    alignProcess = NULL;

    //ccd = NULL;
    delete (captureProcess);
    captureProcess = NULL;


    //guider = NULL;
    delete (guideProcess);
    guideProcess = NULL;

    delete (mountProcess);
    mountProcess = NULL;

    //ao = NULL;

    //focuser = NULL;
    delete (focusProcess);
    focusProcess = NULL;

    //dome = NULL;
    delete (domeProcess);
    domeProcess = NULL;

    //weather = NULL;
    delete (weatherProcess);
    weatherProcess = NULL;

    //dustCap = NULL;
    delete (dustCapProcess);
    dustCapProcess = NULL;

    //lightBox = NULL;

    //aux1 = NULL;
    //aux2 = NULL;
    //aux3 = NULL;
    //aux4 = NULL;

    managedDevices.clear();

    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

}

bool EkosManager::isRunning(const QString &process)
{
    QProcess ps;
    ps.start("ps", QStringList() << "-o" << "comm" << "--no-headers" << "-C" << process);
    ps.waitForFinished();
    QString output = ps.readAllStandardOutput();
    return output.startsWith(process);
}

void EkosManager::addObjectToScheduler(SkyObject *object)
{
    if (schedulerProcess)
        schedulerProcess->addObject(object);
}

bool EkosManager::setProfile(const QString &profileName)
{
    int index = profileCombo->findText(profileName);

    if (index < 0)
        return false;

    profileCombo->setCurrentIndex(index);

    return true;
}

QStringList EkosManager::getProfiles()
{
    QStringList profiles;

    for (int i=0; i < profileCombo->count(); i++)
        profiles << profileCombo->itemText(i);

    return profiles;
}

void EkosManager::addProfile()
{
    ProfileEditor editor(this);

    if (editor.exec() == QDialog::Accepted)
    {
        qDeleteAll(profiles);
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(profileCombo->count()-1);
    }
}

void EkosManager::editProfile()
{
    ProfileEditor editor(this);

    ProfileInfo *currentProfile = getCurrentProfile();

    Q_ASSERT(currentProfile);

    editor.setPi(currentProfile);

    if (editor.exec() == QDialog::Accepted)
    {
        int currentIndex = profileCombo->currentIndex();
        qDeleteAll(profiles);
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(currentIndex);
    }

}

void EkosManager::deleteProfile()
{
    ProfileInfo *currentProfile = getCurrentProfile();

    if (currentProfile->name == "Simulators")
        return;

    if (KMessageBox::questionYesNo(this, i18n("Are you sure you want to delete the profile?"), i18n("Confirm Delete")) == KMessageBox::No)
        return;

    KStarsData::Instance()->userdb()->DeleteProfile(currentProfile);

    qDeleteAll(profiles);
    profiles.clear();
    loadProfiles();
}

ProfileInfo * EkosManager::getCurrentProfile()
{
    ProfileInfo *currentProfile = NULL;

    // Get current profile
    foreach(ProfileInfo *pi, profiles)
    {
        if (profileCombo->currentText() == pi->name)
        {
            currentProfile = pi;
            break;
        }

    }

    return currentProfile;
}

void EkosManager::saveDefaultProfile(const QString &name)
{
    Options::setProfile(name);
}

void EkosManager::updateProfileLocation(ProfileInfo *pi)
{
    if (pi->city.isEmpty() == false)
    {
        bool cityFound = KStars::Instance()->setGeoLocation(pi->city, pi->province, pi->country);
        if (cityFound)
            appendLogText(i18n("Site location updated to %1.", KStarsData::Instance()->geo()->fullName()));
        else
            appendLogText(i18n("Failed to update site location to %1. City not found.", KStarsData::Instance()->geo()->fullName()));
    }
}

void EkosManager::updateMountStatus(ISD::Telescope::TelescopeStatus status)
{
    static ISD::Telescope::TelescopeStatus lastStatus = ISD::Telescope::MOUNT_IDLE;

    if (status == lastStatus)
        return;

    lastStatus = status;

    mountStatus->setText(ISD::Telescope::getStatusString(status));

    switch (status)
    {
    case ISD::Telescope::MOUNT_PARKING:
    case ISD::Telescope::MOUNT_SLEWING:
        mountPI->setColor(QColor( KStarsData::Instance()->colorScheme()->colorNamed("TargetColor" )));
        if (mountPI->isAnimated() == false)
            mountPI->startAnimation();
        break;
    case ISD::Telescope::MOUNT_TRACKING:
        mountPI->setColor(Qt::darkGreen);
        if (mountPI->isAnimated() == false)
            mountPI->startAnimation();
        break;

    default:
        if (mountPI->isAnimated())
            mountPI->stopAnimation();
    }
}

void EkosManager::updateMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt)
{
    raOUT->setText(ra);
    decOUT->setText(dec);
    azOUT->setText(az);
    altOUT->setText(alt);
}

void EkosManager::updateCaptureStatus(Ekos::CaptureState status)
{
    captureStatus->setText(Ekos::getCaptureStatusString(status));
    captureProgress->setValue(captureProcess->getProgressPercentage());

    overallCountDown.setHMS(0,0,0);
    overallCountDown = overallCountDown.addSecs(captureProcess->getOverallRemainingTime());

    sequenceCountDown.setHMS(0,0,0);
    sequenceCountDown = sequenceCountDown.addSecs(captureProcess->getActiveJobRemainingTime());

    if (status != Ekos::CAPTURE_ABORTED && status != Ekos::CAPTURE_COMPLETE)
    {
        if (capturePI->isAnimated() == false)
        {
            capturePI->startAnimation();
            countdownTimer.start();
        }
    }
    else
    {
        if (capturePI->isAnimated())
        {
            capturePI->stopAnimation();
            countdownTimer.stop();

            imageProgress->setValue(0);
            imageRemainingTime->setText("--:--:--");
            overallRemainingTime->setText("--:--:--");
            sequenceRemainingTime->setText("--:--:--");
        }
    }
}

void EkosManager::updateCaptureProgress(QImage *image, Ekos::SequenceJob *job)
{
    if (image)
    {
        delete (previewPixmap);
        previewPixmap = new QPixmap(QPixmap::fromImage(*image));
        previewImage->setPixmap(previewPixmap->scaled(previewImage->width(), previewImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (job->isPreview() == false)
    {
        // Image is set to NULL only on initial capture start up
        int completed   = (image == NULL) ? job->getCompleted() : job->getCompleted()+1;        

        sequenceLabel->setText(QString("Job # %1/%2 %3 (%4/%5)").arg(captureProcess->getActiveJobID()+1).arg(captureProcess->getJobCount()).arg(job->getFullPrefix()).arg(completed).arg(job->getCount()));
        sequenceProgress->setRange(0, job->getCount());
        sequenceProgress->setValue(completed);
    }
}

void EkosManager::updateExposureProgress(Ekos::SequenceJob *job)
{
    imageCountDown.setHMS(0,0,0);
    imageCountDown = imageCountDown.addSecs(job->getExposeLeft());
    if (imageCountDown.hour() == 23)
        imageCountDown.setHMS(0,0,0);

    imageProgress->setRange(0, job->getExposure());
    imageProgress->setValue(job->getExposeLeft());

    imageRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
}

void EkosManager::updateCaptureCountDown()
{        
    overallCountDown  = overallCountDown.addSecs(-1);
    if (overallCountDown.hour() == 23)
        overallCountDown.setHMS(0,0,0);

    sequenceCountDown = sequenceCountDown.addSecs(-1);
    if (sequenceCountDown.hour() == 23)
        sequenceCountDown.setHMS(0,0,0);

    overallRemainingTime->setText(overallCountDown.toString("hh:mm:ss"));
    sequenceRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));    
}

void EkosManager::updateFocusStarPixmap(QPixmap &starPixmap)
{
    delete(focusStarPixmap);
    focusStarPixmap = new QPixmap(starPixmap);

    focusStarImage->setPixmap(focusStarPixmap->scaled(focusStarImage->width(), focusStarImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EkosManager::updateFocusProfilePixmap(QPixmap &profilePixmap)
{        
    focusProfileImage->setPixmap(profilePixmap);
}

void EkosManager::setFocusStatus(Ekos::FocusState status)
{
    focusStatus->setText(Ekos::getFocusStatusString(status));

    if (status >= Ekos::FOCUS_PROGRESS)
    {
        if (focusPI->isAnimated() == false)
            focusPI->startAnimation();
    }
    else
    {
        if (focusPI->isAnimated())
            focusPI->stopAnimation();
    }
}

void EkosManager::updateGuideStatus(Ekos::GuideState status)
{
    guideStatus->setText(Ekos::getGuideStatusString(status));

    switch (status)
    {
    case Ekos::GUIDE_IDLE:
    case Ekos::GUIDE_CALIBRATION_ERROR:
    case Ekos::GUIDE_ABORTED:
    case Ekos::GUIDE_SUSPENDED:
    case Ekos::GUIDE_DITHERING_ERROR:
    case Ekos::GUIDE_CALIBRATION_SUCESS:
        if (guidePI->isAnimated())
            guidePI->stopAnimation();
        break;

    case Ekos::GUIDE_CALIBRATING:
    case Ekos::GUIDE_GUIDING:
    case Ekos::GUIDE_DITHERING:
    case Ekos::GUIDE_DITHERING_SUCCESS:
        if (guidePI->isAnimated() == false)
            guidePI->startAnimation();
        break;

    default:
        break;
    }
}

void EkosManager::updateGuideStarPixmap(QPixmap & starPix)
{
    delete (guideStarPixmap);
    guideStarPixmap = new QPixmap(starPix);

    guideStarImage->setPixmap(guideStarPixmap->scaled(guideStarImage->width(), guideStarImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EkosManager::updateGuideProfilePixmap(QPixmap & profilePix)
{
    guideProfileImage->setPixmap(profilePix);
}

void EkosManager::setTarget(SkyObject *o)
{
    mountTarget->setText(o->name());
}
