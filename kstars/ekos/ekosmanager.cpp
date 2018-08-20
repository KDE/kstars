/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikartech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "ekosmanager.h"

#include "ekosadaptor.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "opsekos.h"
#include "Options.h"
#include "profileeditor.h"
#include "profilewizard.h"
#include "skymap.h"
#include "auxiliary/darklibrary.h"
#include "auxiliary/QProgressIndicator.h"
#include "capture/sequencejob.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/guimanager.h"
#include "indi/indielement.h"
#include "indi/indilistener.h"
#include "indi/indiproperty.h"
#include "indi/indiwebmanager.h"


#include "ekoslive/ekosliveclient.h"
#include "ekoslive/message.h"
#include "ekoslive/media.h"

#include <basedevice.h>

#include <KConfigDialog>
#include <KMessageBox>
#include <KActionCollection>
#include <KNotifications/KNotification>

#include <QComboBox>

#include <ekos_debug.h>

#define MAX_REMOTE_INDI_TIMEOUT 15000
#define MAX_LOCAL_INDI_TIMEOUT  5000

EkosManager::EkosManager(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX

    if (Options::independentWindowEkos())
        setWindowFlags(Qt::Window);
    else
    {
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)), this,
                SLOT(changeAlwaysOnTop(Qt::ApplicationState)));
    }

#endif
    setupUi(this);

    new EkosAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos", this);

    setWindowIcon(QIcon::fromTheme("kstars_ekos"));

    profileModel.reset(new QStandardItemModel(0, 4));
    profileModel->setHorizontalHeaderLabels(QStringList() << "id"
                                            << "name"
                                            << "host"
                                            << "port");

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

    toolsWidget->setIconSize(QSize(48, 48));
    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    // Enable scheduler Tab
    toolsWidget->setTabEnabled(1, false);

    // Start/Stop INDI Server
    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    // Connect/Disconnect INDI devices
    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));
    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    ekosLiveB->setIcon(QIcon::fromTheme("folder-cloud"));
    ekosLiveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ekosLiveClient.reset(new EkosLive::Client(this));

    // INDI Control Panel
    //connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));
    connect(ekosLiveB, &QPushButton::clicked, [&]() {
        ekosLiveClient.get()->show();
        ekosLiveClient.get()->raise();
    });

    connect(this, &EkosManager::newEkosStartingStatus, ekosLiveClient.get()->message(), &EkosLive::Message::setEkosStatingStatus);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::connected, [&]() {ekosLiveB->setIcon(QIcon(":/icons/cloud-online.svg"));});
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::disconnected, [&]() {ekosLiveB->setIcon(QIcon::fromTheme("folder-cloud"));});
    connect(ekosLiveClient.get()->media(), &EkosLive::Media::newBoundingRect, ekosLiveClient.get()->message(), &EkosLive::Message::setBoundingRect);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::resetPolarView, ekosLiveClient.get()->media(), &EkosLive::Media::resetPolarView);

    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));
    // Save as above, but it appears in all modules
    connect(ekosOptionsB, SIGNAL(clicked()), SLOT(showEkosOptions()));

    // Clear Ekos Log
    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    // Logs
    KConfigDialog *dialog = new KConfigDialog(this, "logssettings", Options::self());
    opsLogs = new Ekos::OpsLogs();
    KPageWidgetItem *page = dialog->addPage(opsLogs, i18n("Logging"));
    page->setIcon(QIcon::fromTheme("configure"));
    connect(logsB, SIGNAL(clicked()), dialog, SLOT(show()));
    connect(dialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(updateDebugInterfaces()));
    connect(dialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(updateDebugInterfaces()));

    // Summary
    // previewPixmap = new QPixmap(QPixmap(":/images/noimage.png"));

    // Profiles
    connect(addProfileB, SIGNAL(clicked()), this, SLOT(addProfile()));
    connect(editProfileB, SIGNAL(clicked()), this, SLOT(editProfile()));
    connect(deleteProfileB, SIGNAL(clicked()), this, SLOT(deleteProfile()));
    connect(profileCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [=](const QString &text)
    {
        Options::setProfile(text);
        if (text == "Simulators")
        {
            editProfileB->setEnabled(false);
            deleteProfileB->setEnabled(false);
        }
        else
        {
            editProfileB->setEnabled(true);
            deleteProfileB->setEnabled(true);
        }
    });


    // Ekos Wizard
    connect(wizardProfileB, SIGNAL(clicked()), this, SLOT(wizardProfile()));

    addProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    editProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    deleteProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // Set Profile icons
    addProfileB->setIcon(QIcon::fromTheme("list-add"));
    addProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    editProfileB->setIcon(QIcon::fromTheme("document-edit"));
    editProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    deleteProfileB->setIcon(QIcon::fromTheme("list-remove"));
    deleteProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    wizardProfileB->setIcon(QIcon::fromTheme("tools-wizard"));
    wizardProfileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    customDriversB->setIcon(QIcon::fromTheme("roll"));
    customDriversB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(customDriversB, &QPushButton::clicked, DriverManager::Instance(), &DriverManager::showCustomDrivers);

    // Load all drivers
    loadDrivers();

    // Load add driver profiles
    loadProfiles();

    // INDI Control Panel and Ekos Options    
    optionsB->setIcon(QIcon::fromTheme("configure", QIcon(":/icons/ekos_setup.png")));
    optionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // Setup Tab
    toolsWidget->tabBar()->setTabIcon(0, QIcon(":/icons/ekos_setup.png"));
    toolsWidget->tabBar()->setTabToolTip(0, i18n("Setup"));

    // Initialize Ekos Scheduler Module
    schedulerProcess.reset(new Ekos::Scheduler());
    toolsWidget->addTab(schedulerProcess.get(), QIcon(":/icons/ekos_scheduler.png"), "");
    toolsWidget->tabBar()->setTabToolTip(1, i18n("Scheduler"));
    connect(schedulerProcess.get(), SIGNAL(newLog()), this, SLOT(updateLog()));
    //connect(schedulerProcess.get(), SIGNAL(newTarget(QString)), mountTarget, SLOT(setText(QString)));
    connect(schedulerProcess.get(), &Ekos::Scheduler::newTarget, [&](const QString &target) {
        mountTarget->setText(target);
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", target}}));
    });

    // Temporary fix. Not sure how to resize Ekos Dialog to fit contents of the various tabs in the QScrollArea which are added
    // dynamically. I used setMinimumSize() but it doesn't appear to make any difference.
    // Also set Layout policy to SetMinAndMaxSize as well. Any idea how to fix this?
    // FIXME
    //resize(1000,750);

    summaryPreview.reset(new FITSView(previewWidget, FITS_NORMAL));
    previewWidget->setContentsMargins(0, 0, 0, 0);
    summaryPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    summaryPreview->setBaseSize(previewWidget->size());
    summaryPreview->createFloatingToolBar();
    summaryPreview->setCursorMode(FITSView::dragCursor);
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(summaryPreview.get());
    previewWidget->setLayout(vlayout);

    connect(summaryPreview.get(), &FITSView::imageLoaded, [&]()
    {
        // UUID binds the cloud & preview frames by a common key
        QString uuid = QUuid::createUuid().toString();
        uuid = uuid.remove(QRegularExpression("[-{}]"));
        ekosLiveClient.get()->media()->sendPreviewImage(summaryPreview.get(), uuid);
        ekosLiveClient.get()->cloud()->sendPreviewImage(summaryPreview.get(), uuid);
    });

    if (Options::ekosLeftIcons())
    {
        toolsWidget->setTabPosition(QTabWidget::West);
        QTransform trans;
        trans.rotate(90);

        QIcon icon  = toolsWidget->tabIcon(0);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(0, icon);

        icon = toolsWidget->tabIcon(1);
        pix  = icon.pixmap(QSize(48, 48));
        icon = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(1, icon);
    }

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box

    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);


    resize(Options::ekosWindowWidth(), Options::ekosWindowHeight());
}

void EkosManager::changeAlwaysOnTop(Qt::ApplicationState state)
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

EkosManager::~EkosManager()
{
    toolsWidget->disconnect(this);
    //delete previewPixmap;
}

void EkosManager::closeEvent(QCloseEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action("show_ekos");
    a->setChecked(false);

    Options::setEkosWindowWidth(width());
    Options::setEkosWindowHeight(height());
}

void EkosManager::hideEvent(QHideEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action("show_ekos");
    a->setChecked(false);
}

void EkosManager::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action("show_ekos");
    a->setChecked(true);

    // Just show the profile wizard ONCE per session
    if (profileWizardLaunched == false && profiles.count() == 1)
    {
        profileWizardLaunched = true;
        wizardProfile();
    }
}

void EkosManager::resizeEvent(QResizeEvent *)
{
    //previewImage->setPixmap(previewPixmap->scaled(previewImage->width(), previewImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (focusStarPixmap.get() != nullptr)
        focusStarImage->setPixmap(focusStarPixmap->scaled(focusStarImage->width(), focusStarImage->height(),
                                                          Qt::KeepAspectRatio, Qt::SmoothTransformation));
    //if (focusProfilePixmap)
    //focusProfileImage->setPixmap(focusProfilePixmap->scaled(focusProfileImage->width(), focusProfileImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (guideStarPixmap.get() != nullptr)
        guideStarImage->setPixmap(guideStarPixmap->scaled(guideStarImage->width(), guideStarImage->height(),
                                                          Qt::KeepAspectRatio, Qt::SmoothTransformation));
    //if (guideProfilePixmap)
    //guideProfileImage->setPixmap(guideProfilePixmap->scaled(guideProfileImage->width(), guideProfileImage->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EkosManager::loadProfiles()
{
    profiles.clear();
    KStarsData::Instance()->userdb()->GetAllProfiles(profiles);

    profileModel->clear();

    for (auto& pi : profiles)
    {
        QList<QStandardItem *> info;

        info << new QStandardItem(pi->id) << new QStandardItem(pi->name) << new QStandardItem(pi->host)
             << new QStandardItem(pi->port);
        profileModel->appendRow(info);
    }

    profileModel->sort(0);
    profileCombo->blockSignals(true);
    profileCombo->setModel(profileModel.get());
    profileCombo->setModelColumn(1);
    profileCombo->blockSignals(false);

    // Load last used profile from options
    int index = profileCombo->findText(Options::profile());
    // If not found, set it to first item
    if (index == -1)
        index = 0;
    profileCombo->setCurrentIndex(index);
}

void EkosManager::loadDrivers()
{
    foreach (DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        if (dv->getDriverSource() != HOST_SOURCE)
            driversList[dv->getLabel()] = dv;
    }
}

void EkosManager::reset()
{
    qCDebug(KSTARS_EKOS) << "Resetting Ekos Manager...";

    // Filter Manager
    filterManager.reset(new Ekos::FilterManager());

    nDevices = 0;

    useGuideHead = false;
    useST4       = false;

    removeTabs();

    genericDevices.clear();
    managedDevices.clear();

    captureProcess.reset();
    focusProcess.reset();
    guideProcess.reset();
    domeProcess.reset();
    alignProcess.reset();
    mountProcess.reset();
    weatherProcess.reset();
    dustCapProcess.reset();

    ekosStartingStatus   = EKOS_STATUS_IDLE;
    emit newEkosStartingStatus(ekosStartingStatus);
    indiConnectionStatus = EKOS_STATUS_IDLE;

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    //controlPanelB->setEnabled(false);
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
        KStarsData::Instance()->changeDateTime(KStarsDateTime::currentDateTimeUtc());
        KStarsData::Instance()->clock()->start();
    }

    reset();

    currentProfile = getCurrentProfile();
    localMode      = currentProfile->isLocal();

    // Load profile location if one exists
    updateProfileLocation(currentProfile);

    bool haveCCD = false, haveGuider = false;

    if (currentProfile->guidertype == Ekos::Guide::GUIDE_PHD2)
    {
        Options::setPHD2Host(currentProfile->guiderhost);
        Options::setPHD2Port(currentProfile->guiderport);
    }
    else if (currentProfile->guidertype == Ekos::Guide::GUIDE_LINGUIDER)
    {
        Options::setLinGuiderHost(currentProfile->guiderhost);
        Options::setLinGuiderPort(currentProfile->guiderport);
    }

    if (localMode)
    {
        DriverInfo *drv = driversList.value(currentProfile->mount());

        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->ccd());
        if (drv != nullptr)
        {
            managedDrivers.append(drv->clone());
            haveCCD = true;
        }

        Options::setGuiderType(currentProfile->guidertype);

        drv = driversList.value(currentProfile->guider());
        if (drv != nullptr)
        {
            haveGuider = true;

            // If the guider and ccd are the same driver, we have two cases:
            // #1 Drivers that only support ONE device per driver (such as sbig)
            // #2 Drivers that supports multiples devices per driver (such as sx)
            // For #1, we modify guider_di to make a unique label for the other device with postfix "Guide"
            // For #2, we set guider_di to nullptr and we prompt the user to select which device is primary ccd and which is guider
            // since this is the only way to find out in real time.
            if (haveCCD && currentProfile->guider() == currentProfile->ccd())
            {
                if (drv->getAuxInfo().value("mdpd", false).toBool() == true)
                {
                    drv = nullptr;
                }
                else
                {
                    drv->setUniqueLabel(drv->getLabel() + " Guide");
                }
            }

            if (drv)
                managedDrivers.append(drv->clone());
        }

        drv = driversList.value(currentProfile->ao());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->filter());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->focuser());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->dome());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->weather());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux1());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux2());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux3());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux4());
        if (drv != nullptr)
            managedDrivers.append(drv->clone());

        // Add remote drivers if we have any
        if (currentProfile->remotedrivers.isEmpty() == false && currentProfile->remotedrivers.contains("@"))
        {
            for (auto remoteDriver : currentProfile->remotedrivers.split(","))
            {
                QString name, label, host("localhost"), port("7624");
                QStringList properties = remoteDriver.split(QRegExp("[@:]"));
                if (properties.length() > 1)
                {
                    name = properties[0];
                    host = properties[1];

                    if (properties.length() > 2)
                        port = properties[2];
                }

                DriverInfo *dv = new DriverInfo(name);
                dv->setRemoteHost(host);
                dv->setRemotePort(port);

                label = name;
                // Remove extra quotes
                label.remove("\"");
                dv->setLabel(label);
                dv->setUniqueLabel(label);
                managedDrivers.append(dv);
            }
        }


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

        Options::setGuiderType(currentProfile->guidertype);

        if (haveCCD == false && haveGuider == false)
        {
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices = 0;
            return false;
        }

        nDevices = currentProfile->drivers.count();
    }

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this,
            SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this,
            SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this,
            SLOT(setFocuser(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDome(ISD::GDInterface*)), this, SLOT(setDome(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newWeather(ISD::GDInterface*)), this,
            SLOT(setWeather(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDustCap(ISD::GDInterface*)), this,
            SLOT(setDustCap(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newLightBox(ISD::GDInterface*)), this,
            SLOT(setLightBox(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newST4(ISD::ST4*)), this, SLOT(setST4(ISD::ST4*)));
    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this,
            SLOT(removeDevice(ISD::GDInterface*)), Qt::DirectConnection);

#ifdef Q_OS_OSX
    if (localMode||currentProfile->host=="localhost")
    {
        if (isRunning("PTPCamera"))
        {
            if (KMessageBox::Yes ==
                    (KMessageBox::questionYesNo(0,
                                                i18n("Ekos detected that PTP Camera is running and may prevent a Canon or Nikon camera from connecting to Ekos. Do you want to quit PTP Camera now?"),
                                                i18n("PTP Camera"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                                "ekos_shutdown_PTPCamera")))
            {
                //TODO is there a better way to do this.
                QProcess p;
                p.start("killall PTPCamera");
                p.waitForFinished();
            }
        }
    }
#endif
    if (localMode)
    {
        if (isRunning("indiserver"))
        {
            if (KMessageBox::Yes ==
                    (KMessageBox::questionYesNo(nullptr,
                                                i18n("Ekos detected an instance of INDI server running. Do you wish to "
                                                     "shut down the existing instance before starting a new one?"),
                                                i18n("INDI Server"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                                "ekos_shutdown_existing_indiserver")))
            {
                DriverManager::Instance()->stopAllDevices();
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
            emit newEkosStartingStatus(ekosStartingStatus);
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this,
                SLOT(processServerTermination(QString,QString)));

        ekosStartingStatus = EKOS_STATUS_PENDING;
        emit newEkosStartingStatus(ekosStartingStatus);

        if (currentProfile->autoConnect)
            appendLogText(i18n("INDI services started on port %1.", managedDrivers.first()->getPort()));
        else
            appendLogText(
                        i18n("INDI services started on port %1. Please connect devices.", managedDrivers.first()->getPort()));

        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));
    }
    else
    {
        // If we need to use INDI Web Manager
        if (currentProfile->INDIWebManagerPort > 0)
        {
            appendLogText(i18n("Establishing communication with remote INDI Web Manager..."));
            remoteManagerStart = false;
            if (INDI::WebManager::isOnline(currentProfile))
            {
                INDI::WebManager::syncCustomDrivers(currentProfile);

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

        appendLogText(
                    i18n("Connecting to remote INDI server at %1 on port %2 ...", currentProfile->host, currentProfile->port));
        qApp->processEvents();

        QApplication::setOverrideCursor(Qt::WaitCursor);

        if (DriverManager::Instance()->connectRemoteHost(managedDrivers.first()) == false)
        {
            appendLogText(i18n("Failed to connect to remote INDI server."));
            INDIListener::Instance()->disconnect(this);
            qDeleteAll(managedDrivers);
            managedDrivers.clear();
            ekosStartingStatus = EKOS_STATUS_ERROR;
            emit newEkosStartingStatus(ekosStartingStatus);
            QApplication::restoreOverrideCursor();
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this,
                SLOT(processServerTermination(QString,QString)));

        QApplication::restoreOverrideCursor();
        ekosStartingStatus = EKOS_STATUS_PENDING;
        emit newEkosStartingStatus(ekosStartingStatus);

        appendLogText(
                    i18n("INDI services started. Connection to remote INDI server is successful. Waiting for devices..."));

        QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));
    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    //controlPanelB->setEnabled(false);

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
        emit newEkosStartingStatus(ekosStartingStatus);
        return;
    }

    if (localMode)
    {
        QStringList remainingDevices;
        foreach (DriverInfo *drv, managedDrivers)
        {
            if (drv->getDevices().count() == 0)
                remainingDevices << QString("+ %1").arg(
                                        drv->getUniqueLabel().isEmpty() == false ? drv->getUniqueLabel() : drv->getName());
        }

        if (remainingDevices.count() == 1)
        {
            appendLogText(i18n("Unable to establish:\n%1\nPlease ensure the device is connected and powered on.",
                               remainingDevices.at(0)));
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            appendLogText(i18n("Unable to establish the following devices:\n%1\nPlease ensure each device is connected "
                               "and powered on.",
                               remainingDevices.join("\n")));
            KNotification::beep(i18n("Ekos startup error"));
        }
    }
    else
    {
        QStringList remainingDevices;

        for (auto &driver : currentProfile->drivers.values())
        {
            bool driverFound = false;

            for (auto &device : genericDevices)
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
            appendLogText(i18n("Unable to establish remote device:\n%1\nPlease ensure remote device name corresponds "
                               "to actual device name.",
                               remainingDevices.at(0)));
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            appendLogText(i18n("Unable to establish remote devices:\n%1\nPlease ensure remote device name corresponds "
                               "to actual device name.",
                               remainingDevices.join("\n")));
            KNotification::beep(i18n("Ekos startup error"));
        }
    }

    ekosStartingStatus = EKOS_STATUS_ERROR;
}

void EkosManager::connectDevices()
{
    // Check if already connected
    int nConnected = 0;

    for (auto &device : genericDevices)
    {
        if (device->isConnected())
            nConnected++;
    }
    if (genericDevices.count() == nConnected)
    {
        indiConnectionStatus = EKOS_STATUS_SUCCESS;
        return;
    }

    indiConnectionStatus = EKOS_STATUS_PENDING;

    for (auto &device : genericDevices)
    {
        qCDebug(KSTARS_EKOS) << "Connecting " << device->getDeviceName();
        device->Connect();
    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText(i18n("Connecting INDI devices..."));
}

void EkosManager::disconnectDevices()
{
    for (auto &device : genericDevices)
    {
        qCDebug(KSTARS_EKOS) << "Disconnecting " << device->getDeviceName();
        device->Disconnect();
    }

    appendLogText(i18n("Disconnecting INDI devices..."));
}

void EkosManager::processServerTermination(const QString &host, const QString &port)
{
    if ((localMode && managedDrivers.first()->getPort() == port) ||
            (currentProfile->host == host && currentProfile->port == port.toInt()))
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

            if (remoteManagerStart && currentProfile->INDIWebManagerPort != -1)
            {
                INDI::WebManager::stopProfile(currentProfile);
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
    qCInfo(KSTARS_EKOS) << "Ekos received a new device: " << devInterface->getDeviceName();

    for(auto &device: genericDevices)
    {
        if (!strcmp(device->getDeviceName(), devInterface->getDeviceName()))
        {
            qCWarning(KSTARS_EKOS) << "Found duplicate device, ignoring...";
            return;
        }
    }

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
        emit newEkosStartingStatus(ekosStartingStatus);

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        //controlPanelB->setEnabled(true);

        if (localMode == false && nDevices == 0)
        {
            if (currentProfile->autoConnect)
                appendLogText(i18n("Remote devices established."));
            else
                appendLogText(i18n("Remote devices established. Please connect devices."));
        }
    }
}

void EkosManager::deviceConnected()
{
    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    processINDIB->setEnabled(false);

    if (Options::verboseLogging())
    {
        ISD::GDInterface *device = qobject_cast<ISD::GDInterface *>(sender());
        qCInfo(KSTARS_EKOS) << device->getDeviceName() << "is connected.";
    }

    int nConnectedDevices = 0;

    foreach (ISD::GDInterface *device, genericDevices)
    {
        if (device->isConnected())
            nConnectedDevices++;
    }

    qCDebug(KSTARS_EKOS) << nConnectedDevices << " devices connected out of " << genericDevices.count();

    //if (nConnectedDevices >= pi->drivers.count())
    if (nConnectedDevices >= genericDevices.count())
    {
        indiConnectionStatus = EKOS_STATUS_SUCCESS;
        qCInfo(KSTARS_EKOS)<< "All INDI devices are now connected.";
    }
    else
        indiConnectionStatus = EKOS_STATUS_PENDING;

    ISD::GDInterface *dev = static_cast<ISD::GDInterface *>(sender());

    if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        if (mountProcess.get() != nullptr)
        {
            mountProcess->setEnabled(true);
            if (alignProcess.get() != nullptr)
                alignProcess->setEnabled(true);
        }
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess.get() != nullptr)
            captureProcess->setEnabled(true);
        if (focusProcess.get() != nullptr)
            focusProcess->setEnabled(true);
        if (alignProcess.get() != nullptr)
        {
            if (mountProcess.get() && mountProcess->isEnabled())
                alignProcess->setEnabled(true);
            else
                alignProcess->setEnabled(false);
        }
        if (guideProcess.get() != nullptr)
            guideProcess->setEnabled(true);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess.get() != nullptr)
            focusProcess->setEnabled(true);
    }

    if (Options::neverLoadConfig())
        return;

    INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

    foreach (ISD::GDInterface *device, genericDevices)
    {
        if (device == dev)
        {
            connect(dev, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this, SLOT(watchDebugProperty(ISwitchVectorProperty*)));

            ISwitchVectorProperty *configProp = device->getBaseDevice()->getSwitch("CONFIG_PROCESS");
            if (configProp && configProp->s == IPS_IDLE)
                device->setConfig(tConfig);
            break;
        }
    }
}

void EkosManager::deviceDisconnected()
{
    ISD::GDInterface *dev = static_cast<ISD::GDInterface *>(sender());

    if (dev != nullptr)
    {
        if (dev->getState("CONNECTION") == IPS_ALERT)
            indiConnectionStatus = EKOS_STATUS_ERROR;
        else if (dev->getState("CONNECTION") == IPS_BUSY)
            indiConnectionStatus = EKOS_STATUS_PENDING;
        else
            indiConnectionStatus = EKOS_STATUS_IDLE;

        if (Options::verboseLogging())
            qCDebug(KSTARS_EKOS) << dev->getDeviceName() << " is disconnected.";

        appendLogText(i18n("%1 is disconnected.", dev->getDeviceName()));
    }
    else
        indiConnectionStatus = EKOS_STATUS_IDLE;

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);
    processINDIB->setEnabled(true);

    if (dev != nullptr && dev->getBaseDevice() &&
            (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
    {
        if (mountProcess.get() != nullptr)
            mountProcess->setEnabled(false);
    }
    // Do not disable modules on device connection loss, let them handle it
    /*
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess.get() != nullptr)
            captureProcess->setEnabled(false);
        if (focusProcess.get() != nullptr)
            focusProcess->setEnabled(false);
        if (alignProcess.get() != nullptr)
            alignProcess->setEnabled(false);
        if (guideProcess.get() != nullptr)
            guideProcess->setEnabled(false);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess.get() != nullptr)
            focusProcess->setEnabled(false);
    }*/
}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    //mount = scopeDevice;

    managedDevices[KSTARS_TELESCOPE] = scopeDevice;

    appendLogText(i18n("%1 is online.", scopeDevice->getDeviceName()));

    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
            SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    initMount();

    mountProcess->setTelescope(scopeDevice);

    double primaryScopeFL=0, primaryScopeAperture=0, guideScopeFL=0, guideScopeAperture=0;
    getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    // Save telescope info in mount driver
    mountProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);

    if (guideProcess.get() != nullptr)
    {
        guideProcess->setTelescope(scopeDevice);
        guideProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    }

    if (alignProcess.get() != nullptr)
    {
        alignProcess->setTelescope(scopeDevice);
        alignProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    }

    if (domeProcess.get() != nullptr)
        domeProcess->setTelescope(scopeDevice);

    ekosLiveClient->message()->sendMounts();
    ekosLiveClient->message()->sendScopes();
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{
    // No duplicates
    for (auto oneCCD : findDevices(KSTARS_CCD))
        if (oneCCD == ccdDevice)
            return;

    managedDevices.insertMulti(KSTARS_CCD, ccdDevice);

    initCapture();

    captureProcess->setEnabled(true);
    captureProcess->addCCD(ccdDevice);

    QString primaryCCD, guiderCCD;

    // Only look for primary & guider CCDs if we can tell a difference between them
    // otherwise rely on saved options
    if (currentProfile->ccd() != currentProfile->guider())
    {
        foreach (ISD::GDInterface *device, findDevices(KSTARS_CCD))
        {
            if (QString(device->getDeviceName()).startsWith(currentProfile->ccd(), Qt::CaseInsensitive))
                primaryCCD = QString(device->getDeviceName());
            else if (QString(device->getDeviceName()).startsWith(currentProfile->guider(), Qt::CaseInsensitive))
                guiderCCD = QString(device->getDeviceName());
        }
    }

    bool rc = false;
    if (Options::defaultCaptureCCD().isEmpty() == false)
        rc = captureProcess->setCCD(Options::defaultCaptureCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        captureProcess->setCCD(primaryCCD);

    initFocus();

    focusProcess->addCCD(ccdDevice);

    rc = false;
    if (Options::defaultFocusCCD().isEmpty() == false)
        rc = focusProcess->setCCD(Options::defaultFocusCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        focusProcess->setCCD(primaryCCD);

    initAlign();

    alignProcess->addCCD(ccdDevice);

    rc = false;
    if (Options::defaultAlignCCD().isEmpty() == false)
        rc = alignProcess->setCCD(Options::defaultAlignCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        alignProcess->setCCD(primaryCCD);

    initGuide();

    guideProcess->addCCD(ccdDevice);

    rc = false;
    if (Options::defaultGuideCCD().isEmpty() == false)
        rc = guideProcess->setCCD(Options::defaultGuideCCD());
    if (rc == false && guiderCCD.isEmpty() == false)
        guideProcess->setCCD(guiderCCD);

    appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
            SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    if (managedDevices.contains(KSTARS_TELESCOPE))
    {
        alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
        captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
        guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
    }
}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{
    // No duplicates
    for (auto oneFilter : findDevices(KSTARS_FILTER))
        if (oneFilter == filterDevice)
            return;

    managedDevices.insertMulti(KSTARS_FILTER, filterDevice);

    appendLogText(i18n("%1 filter is online.", filterDevice->getDeviceName()));

    initCapture();

    connect(filterDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
            SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);
    connect(filterDevice, SIGNAL(textUpdated(ITextVectorProperty*)), this,
            SLOT(processNewText(ITextVectorProperty*)), Qt::UniqueConnection);

    captureProcess->addFilter(filterDevice);

    initFocus();

    focusProcess->addFilter(filterDevice);

    initAlign();

    alignProcess->addFilter(filterDevice);

    if (Options::defaultAlignFW().isEmpty() == false)
        alignProcess->setFilter(Options::defaultAlignFW(), -1);    
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    // No duplicates
    for (auto oneFocuser : findDevices(KSTARS_FOCUSER))
        if (oneFocuser == focuserDevice)
            return;

    managedDevices.insertMulti(KSTARS_FOCUSER, focuserDevice);

    initCapture();

    initFocus();

    focusProcess->addFocuser(focuserDevice);

    if (Options::defaultFocusFocuser().isEmpty() == false)
        focusProcess->setFocuser(Options::defaultFocusFocuser());

    appendLogText(i18n("%1 focuser is online.", focuserDevice->getDeviceName()));
}

void EkosManager::setDome(ISD::GDInterface *domeDevice)
{
    managedDevices[KSTARS_DOME] = domeDevice;

    initDome();

    domeProcess->setDome(domeDevice);

    if (captureProcess.get() != nullptr)
        captureProcess->setDome(domeDevice);

    if (alignProcess.get() != nullptr)
        alignProcess->setDome(domeDevice);

    if (managedDevices.contains(KSTARS_TELESCOPE))
        domeProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

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

    if (captureProcess.get() != nullptr)
        captureProcess->setDustCap(dustCapDevice);
}

void EkosManager::setLightBox(ISD::GDInterface *lightBoxDevice)
{
    managedDevices.insertMulti(KSTARS_AUXILIARY, lightBoxDevice);

    if (captureProcess.get() != nullptr)
        captureProcess->setLightBox(lightBoxDevice);
}

void EkosManager::removeDevice(ISD::GDInterface *devInterface)
{
    switch (devInterface->getType())
    {
    case KSTARS_CCD:
        removeTabs();
        break;

    case KSTARS_TELESCOPE:
        if (mountProcess.get() != nullptr)
        {
            mountProcess.reset();
        }
        break;

    case KSTARS_FOCUSER:
        // TODO this should be done for all modules
        if (focusProcess.get() != nullptr)
            focusProcess.get()->removeDevice(devInterface);
        break;

    default:
        break;
    }


    appendLogText(i18n("%1 is offline.", devInterface->getDeviceName()));

    // #1 Remove from Generic Devices
    // Generic devices are ALL the devices we receive from INDI server
    // Whether Ekos cares about them (i.e. selected equipment) or extra devices we
    // do not care about
    foreach (ISD::GDInterface *genericDevice, genericDevices)
        if (!strcmp(genericDevice->getDeviceName(), devInterface->getDeviceName()))
        {
            genericDevices.removeOne(genericDevice);
            break;
        }

    // #2 Remove from Ekos Managed Device
    // Managed devices are devices selected by the user in the device profile
    foreach (ISD::GDInterface *device, managedDevices.values())
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
        ekosLiveClient.get()->message()->sendFilterWheels();
    }
}

void EkosManager::processNewNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (guideProcess.get() != nullptr)
        {
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            //guideProcess->syncTelescopeInfo();
        }

        if (alignProcess.get() != nullptr)
        {
            alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            //alignProcess->syncTelescopeInfo();
        }

        if (mountProcess.get() != nullptr)
        {
            mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            //mountProcess->syncTelescopeInfo();
        }        

        return;
    }

    if (!strcmp(nvp->name, "CCD_INFO") || !strcmp(nvp->name, "GUIDER_INFO") || !strcmp(nvp->name, "CCD_FRAME") ||
            !strcmp(nvp->name, "GUIDER_FRAME"))
    {
        if (focusProcess.get() != nullptr)
            focusProcess->syncCCDInfo();

        if (guideProcess.get() != nullptr)
            guideProcess->syncCCDInfo();

        if (alignProcess.get() != nullptr)
            alignProcess->syncCCDInfo();

        return;
    }

    /*
    if (!strcmp(nvp->name, "FILTER_SLOT"))
    {
        if (captureProcess.get() != nullptr)
            captureProcess->checkFilter();

        if (focusProcess.get() != nullptr)
            focusProcess->checkFilter();

        if (alignProcess.get() != nullptr)
            alignProcess->checkFilter();

    }
    */
}

void EkosManager::processNewProperty(INDI::Property *prop)
{
    ISD::GenericDevice *deviceInterface = qobject_cast<ISD::GenericDevice *>(sender());

    if (!strcmp(prop->getName(), "CONNECTION") && currentProfile->autoConnect)
    {
        deviceInterface->Connect();
        return;
    }

    // Check if we need to turn on DEBUG for logging purposes
    if (!strcmp(prop->getName(), "DEBUG"))
    {
        uint16_t interface = deviceInterface->getBaseDevice()->getDriverInterface();
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Check if we need to enable debug logging for the INDI drivers.
            ISwitchVectorProperty *debugSP = prop->getSwitch();
            debugSP->sp[0].s = ISS_ON;
            debugSP->sp[1].s = ISS_OFF;
            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(debugSP);
        }
    }

    // Handle debug levels for logging purposes
    if (!strcmp(prop->getName(), "DEBUG_LEVEL"))
    {
        uint16_t interface = deviceInterface->getBaseDevice()->getDriverInterface();
        // Check if the logging option for the specific device class is on and if the device interface matches it.
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Turn on everything
            ISwitchVectorProperty *debugLevel = prop->getSwitch();
            for (int i=0; i < debugLevel->nsp; i++)
                debugLevel->sp[i].s = ISS_ON;

            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(debugLevel);
        }
    }

    if (!strcmp(prop->getName(), "TELESCOPE_INFO") || !strcmp(prop->getName(), "TELESCOPE_SLEW_RATE")
            || !strcmp(prop->getName(), "TELESCOPE_PARK"))
    {
        ekosLiveClient.get()->message()->sendMounts();
        ekosLiveClient.get()->message()->sendScopes();
    }

    if (!strcmp(prop->getName(), "CCD_INFO") || !strcmp(prop->getName(), "CCD_TEMPERATURE") || !strcmp(prop->getName(), "CCD_ISO"))
    {
        ekosLiveClient.get()->message()->sendCameras();
        ekosLiveClient.get()->media()->registerCameras();
    }

    if (!strcmp(prop->getName(), "FILTER_SLOT"))
        ekosLiveClient.get()->message()->sendFilterWheels();

    if (!strcmp(prop->getName(), "CCD_INFO") || !strcmp(prop->getName(), "GUIDER_INFO"))
    {
        if (focusProcess.get() != nullptr)
            focusProcess->syncCCDInfo();

        if (guideProcess.get() != nullptr)
            guideProcess->syncCCDInfo();

        if (alignProcess.get() != nullptr)
            alignProcess->syncCCDInfo();

        return;
    }

    if (!strcmp(prop->getName(), "TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (guideProcess.get() != nullptr)
        {
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            //guideProcess->syncTelescopeInfo();
        }

        if (alignProcess.get() != nullptr)
        {
            alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            //alignProcess->syncTelescopeInfo();
        }

        if (mountProcess.get() != nullptr)
        {
            mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            //mountProcess->syncTelescopeInfo();
        }

        return;
    }

    if (!strcmp(prop->getName(), "GUIDER_EXPOSURE"))
    {
        foreach (ISD::GDInterface *device, findDevices(KSTARS_CCD))
        {
            if (!strcmp(device->getDeviceName(), prop->getDeviceName()))
            {
                initCapture();
                initGuide();
                useGuideHead = true;
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
        if (captureProcess.get() != nullptr)
        {
            foreach (ISD::GDInterface *device, findDevices(KSTARS_CCD))
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
        if (captureProcess.get() != nullptr)
            captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        if (mountProcess.get() != nullptr)
            mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        return;
    }

    /*
    if (!strcmp(prop->getName(), "FILTER_NAME"))
    {
        if (captureProcess.get() != nullptr)
            captureProcess->checkFilter();

        if (focusProcess.get() != nullptr)
            focusProcess->checkFilter();

        if (alignProcess.get() != nullptr)
            alignProcess->checkFilter();


        return;
    }
    */

    if (!strcmp(prop->getName(), "ASTROMETRY_SOLVER"))
    {
        foreach (ISD::GDInterface *device, genericDevices)
        {
            if (!strcmp(device->getDeviceName(), prop->getDeviceName()))
            {
                initAlign();
                alignProcess->setAstrometryDevice(device);
                break;
            }
        }
    }

    if (!strcmp(prop->getName(), "ABS_ROTATOR_ANGLE"))
    {
        managedDevices[KSTARS_ROTATOR] = deviceInterface;
        if (captureProcess.get() != nullptr)
            captureProcess->setRotator(deviceInterface);
        if (alignProcess.get() != nullptr)
            alignProcess->setRotator(deviceInterface);
    }

    if (!strcmp(prop->getName(), "GPS_REFRESH"))
    {
        managedDevices.insertMulti(KSTARS_AUXILIARY, deviceInterface);
        if (mountProcess.get() != nullptr)
            mountProcess->setGPS(deviceInterface);
    }

    if (focusProcess.get() != nullptr && strstr(prop->getName(), "FOCUS_"))
    {
        focusProcess->checkFocuser();
    }
}

QList<ISD::GDInterface *> EkosManager::findDevices(DeviceFamily type)
{
    QList<ISD::GDInterface *> deviceList;

    QMapIterator<DeviceFamily, ISD::GDInterface *> i(managedDevices);
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

    //if (focusProcess.get() != nullptr && currentWidget != focusProcess)
    //focusProcess->resetFrame();

    if (alignProcess.get() && alignProcess.get() == currentWidget)
    {
        if (alignProcess->isEnabled() == false && captureProcess->isEnabled())
        {
            if (managedDevices[KSTARS_CCD]->isConnected() && managedDevices.contains(KSTARS_TELESCOPE))
            {
                if (alignProcess->isParserOK())
                    alignProcess->setEnabled(true);
                //#ifdef Q_OS_WIN
                else
                {
                    // If current setting is remote astrometry and profile doesn't contain
                    // remote astrometry, then we switch to online solver. Otherwise, the whole align
                    // module remains disabled and the user cannot change re-enable it since he cannot select online solver
                    ProfileInfo *pi = getCurrentProfile();
                    if (Options::solverType() == Ekos::Align::SOLVER_REMOTE && pi->aux1() != "Astrometry" && pi->aux2() != "Astrometry"
                            && pi->aux3() != "Astrometry" && pi->aux4() != "Astrometry")
                    {

                        Options::setSolverType(Ekos::Align::SOLVER_ONLINE);
                        alignModule()->setSolverType(Ekos::Align::SOLVER_ONLINE);
                        alignProcess->setEnabled(true);
                    }
                }
                //#endif
            }
        }

        alignProcess->checkCCD();
    }
    else if (captureProcess.get() != nullptr && currentWidget == captureProcess.get())
    {
        captureProcess->checkCCD();
    }
    else if (focusProcess.get() != nullptr && currentWidget == focusProcess.get())
    {
        focusProcess->checkCCD();
    }
    else if (guideProcess.get() != nullptr && currentWidget == guideProcess.get())
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
    else if (currentWidget == alignProcess.get())
        ekosLogOut->setPlainText(alignProcess->getLogText());
    else if (currentWidget == captureProcess.get())
        ekosLogOut->setPlainText(captureProcess->getLogText());
    else if (currentWidget == focusProcess.get())
        ekosLogOut->setPlainText(focusProcess->getLogText());
    else if (currentWidget == guideProcess.get())
        ekosLogOut->setPlainText(guideProcess->getLogText());
    else if (currentWidget == mountProcess.get())
        ekosLogOut->setPlainText(mountProcess->getLogText());
    if (currentWidget == schedulerProcess.get())
        ekosLogOut->setPlainText(schedulerProcess->getLogText());

#ifdef Q_OS_OSX
    repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif

}

void EkosManager::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                            QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS) << text;

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
    else if (currentWidget == alignProcess.get())
        alignProcess->clearLog();
    else if (currentWidget == captureProcess.get())
        captureProcess->clearLog();
    else if (currentWidget == focusProcess.get())
        focusProcess->clearLog();
    else if (currentWidget == guideProcess.get())
        guideProcess->clearLog();
    else if (currentWidget == mountProcess.get())
        mountProcess->clearLog();
    else if (currentWidget == schedulerProcess.get())
        schedulerProcess->clearLog();
}

void EkosManager::initCapture()
{
    if (captureProcess.get() != nullptr)
        return;

    captureProcess.reset(new Ekos::Capture());
    captureProcess->setEnabled(false);
    int index = toolsWidget->addTab(captureProcess.get(), QIcon(":/icons/ekos_ccd.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18nc("Charge-Coupled Device", "CCD"));
    if (Options::ekosLeftIcons())
    {
        QTransform trans;
        trans.rotate(90);
        QIcon icon  = toolsWidget->tabIcon(index);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(index, icon);
    }
    connect(captureProcess.get(), SIGNAL(newLog()), this, SLOT(updateLog()));    
    connect(captureProcess.get(), SIGNAL(newStatus(Ekos::CaptureState)), this, SLOT(updateCaptureStatus(Ekos::CaptureState)));
    connect(captureProcess.get(), SIGNAL(newImage(QImage*,Ekos::SequenceJob*)), this,
            SLOT(updateCaptureProgress(QImage*,Ekos::SequenceJob*)));
    connect(captureProcess.get(), SIGNAL(newExposureProgress(Ekos::SequenceJob*)), this,
            SLOT(updateExposureProgress(Ekos::SequenceJob*)));
    connect(captureProcess.get(), &Ekos::Capture::sequenceChanged, ekosLiveClient.get()->message(), &EkosLive::Message::sendCaptureSequence);
    connect(captureProcess.get(), &Ekos::Capture::settingsUpdated, ekosLiveClient.get()->message(), &EkosLive::Message::sendCaptureSettings);
    captureGroup->setEnabled(true);
    sequenceProgress->setEnabled(true);
    captureProgress->setEnabled(true);
    imageProgress->setEnabled(true);

    captureProcess->setFilterManager(filterManager);

    if (!capturePI)
    {
        capturePI = new QProgressIndicator(captureProcess.get());
        captureStatusLayout->insertWidget(0, capturePI);
    }

    foreach (ISD::GDInterface *device, findDevices(KSTARS_AUXILIARY))
    {
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
            captureProcess->setDustCap(device);
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::LIGHTBOX_INTERFACE)
            captureProcess->setLightBox(device);
    }

    if (focusProcess.get() != nullptr)
    {
        // Autofocus
        connect(captureProcess.get(), SIGNAL(checkFocus(double)), focusProcess.get(), SLOT(checkFocus(double)),
                Qt::UniqueConnection);
        connect(focusProcess.get(), SIGNAL(newStatus(Ekos::FocusState)), captureProcess.get(),
                SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::newHFR, captureProcess.get(), &Ekos::Capture::setHFR, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess.get(), SIGNAL(meridianFlipStarted()), focusProcess.get(), SLOT(resetFrame()), Qt::UniqueConnection);
    }

    if (alignProcess.get() != nullptr)
    {
        // Alignment flag
        connect(alignProcess.get(), SIGNAL(newStatus(Ekos::AlignState)), captureProcess.get(),
                SLOT(setAlignStatus(Ekos::AlignState)), Qt::UniqueConnection);

        // Solver data
        connect(alignProcess.get(), SIGNAL(newSolverResults(double,double,double,double)), captureProcess.get(),
                SLOT(setAlignResults(double,double,double,double)), Qt::UniqueConnection);

        // Capture Status
        connect(captureProcess.get(), SIGNAL(newStatus(Ekos::CaptureState)), alignProcess.get(),
                SLOT(setCaptureStatus(Ekos::CaptureState)), Qt::UniqueConnection);
    }

    if (mountProcess.get() != nullptr)
    {
        // Meridian Flip
        connect(captureProcess.get(), SIGNAL(meridianFlipStarted()), mountProcess.get(), SLOT(disableAltLimits()),
                Qt::UniqueConnection);
        connect(captureProcess.get(), SIGNAL(meridianFlipCompleted()), mountProcess.get(), SLOT(enableAltLimits()),
                Qt::UniqueConnection);

        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), captureProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
    }

    if (managedDevices.contains(KSTARS_DOME))
    {
        captureProcess->setDome(managedDevices[KSTARS_DOME]);
    }
    if (managedDevices.contains(KSTARS_ROTATOR))
    {
        captureProcess->setRotator(managedDevices[KSTARS_ROTATOR]);
    }
}

void EkosManager::initAlign()
{
    if (alignProcess.get() != nullptr)
        return;

    alignProcess.reset(new Ekos::Align(currentProfile));

    double primaryScopeFL=0, primaryScopeAperture=0, guideScopeFL=0, guideScopeAperture=0;
    getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    alignProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);

    alignProcess->setEnabled(false);
    int index = toolsWidget->addTab(alignProcess.get(), QIcon(":/icons/ekos_align.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Align"));
    connect(alignProcess.get(), SIGNAL(newLog()), this, SLOT(updateLog()));
    if (Options::ekosLeftIcons())
    {
        QTransform trans;
        trans.rotate(90);
        QIcon icon  = toolsWidget->tabIcon(index);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(index, icon);
    }

    alignProcess->setFilterManager(filterManager);

    if (captureProcess.get() != nullptr)
    {
        // Align Status
        connect(alignProcess.get(), SIGNAL(newStatus(Ekos::AlignState)), captureProcess.get(),
                SLOT(setAlignStatus(Ekos::AlignState)), Qt::UniqueConnection);

        // Solver data
        connect(alignProcess.get(), SIGNAL(newSolverResults(double,double,double,double)), captureProcess.get(),
                SLOT(setAlignResults(double,double,double,double)), Qt::UniqueConnection);

        // Capture Status
        connect(captureProcess.get(), SIGNAL(newStatus(Ekos::CaptureState)), alignProcess.get(),
                SLOT(setCaptureStatus(Ekos::CaptureState)), Qt::UniqueConnection);
    }

    if (focusProcess.get() != nullptr)
    {
        connect(focusProcess.get(), SIGNAL(newStatus(Ekos::FocusState)), alignProcess.get(), SLOT(setFocusStatus(Ekos::FocusState)),
                Qt::UniqueConnection);
    }

    if (mountProcess.get() != nullptr)
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), alignProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);

    if (ekosLiveClient.get() != nullptr)
    {
        connect(alignProcess.get(), &Ekos::Align::newStatus, ekosLiveClient.get()->message(), &EkosLive::Message::setAlignStatus);
        connect(alignProcess.get(), &Ekos::Align::newSolution, ekosLiveClient.get()->message(), &EkosLive::Message::setAlignSolution);
        connect(alignProcess.get(), &Ekos::Align::newPAHStage, ekosLiveClient.get()->message(), &EkosLive::Message::setPAHStage);
        connect(alignProcess.get(), &Ekos::Align::newPAHMessage, ekosLiveClient.get()->message(), &EkosLive::Message::setPAHMessage);
        connect(alignProcess.get(), &Ekos::Align::PAHEnabled, ekosLiveClient.get()->message(), &EkosLive::Message::setPAHEnabled);
        //connect(alignProcess.get(), &Ekos::Align::newFOVTelescopeType, ekosLiveClient.get()->message(), &EkosLive::Message::setFOVTelescopeType);

        connect(alignProcess.get(), &Ekos::Align::newImage, [&](FITSView *view)
        {
            ekosLiveClient.get()->media()->sendPreviewImage(view, QString());
        });
        connect(alignProcess.get(), &Ekos::Align::newFrame, ekosLiveClient.get()->media(), &EkosLive::Media::sendUpdatedFrame);

        connect(alignProcess.get(), &Ekos::Align::polarResultUpdated, ekosLiveClient.get()->message(), &EkosLive::Message::setPolarResults);        
        connect(alignProcess.get(), &Ekos::Align::settingsUpdated, ekosLiveClient.get()->message(), &EkosLive::Message::sendAlignSettings);

        connect(alignProcess.get(), &Ekos::Align::newCorrectionVector, ekosLiveClient.get()->media(), &EkosLive::Media::setCorrectionVector);

    }

    if (managedDevices.contains(KSTARS_DOME))
    {
        alignProcess->setDome(managedDevices[KSTARS_DOME]);
    }
    if (managedDevices.contains(KSTARS_ROTATOR))
    {
        alignProcess->setRotator(managedDevices[KSTARS_ROTATOR]);
    }
}

void EkosManager::initFocus()
{
    if (focusProcess.get() != nullptr)
        return;

    focusProcess.reset(new Ekos::Focus());
    int index    = toolsWidget->addTab(focusProcess.get(), QIcon(":/icons/ekos_focus.png"), "");

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Focus"));
    connect(focusProcess.get(), SIGNAL(newLog()), this, SLOT(updateLog()));
    connect(focusProcess.get(), SIGNAL(newStatus(Ekos::FocusState)), this, SLOT(setFocusStatus(Ekos::FocusState)));
    connect(focusProcess.get(), SIGNAL(newStarPixmap(QPixmap&)), this, SLOT(updateFocusStarPixmap(QPixmap&)));
    connect(focusProcess.get(), SIGNAL(newProfilePixmap(QPixmap&)), this, SLOT(updateFocusProfilePixmap(QPixmap&)));
    connect(focusProcess.get(), SIGNAL(newHFR(double,int)), this, SLOT(updateCurrentHFR(double,int)));

    focusProcess->setFilterManager(filterManager);
    connect(filterManager.data(), SIGNAL(checkFocus(double)), focusProcess.get(), SLOT(checkFocus(double)), Qt::UniqueConnection);
    connect(focusProcess.get(), SIGNAL(newStatus(Ekos::FocusState)), filterManager.data(), SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
    connect(filterManager.data(), SIGNAL(newFocusOffset(int,bool)), focusProcess.get(), SLOT(adjustFocusOffset(int,bool)),
            Qt::UniqueConnection);
    connect(focusProcess.get(), SIGNAL(focusPositionAdjusted()), filterManager.data(), SLOT(setFocusOffsetComplete()),
            Qt::UniqueConnection);
    connect(focusProcess.get(), SIGNAL(absolutePositionChanged(int)), filterManager.data(), SLOT(setFocusAbsolutePosition(int)),
            Qt::UniqueConnection);

    if (Options::ekosLeftIcons())
    {
        QTransform trans;
        trans.rotate(90);
        QIcon icon  = toolsWidget->tabIcon(index);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(index, icon);
    }

    focusGroup->setEnabled(true);

    if (!focusPI)
    {
        focusPI = new QProgressIndicator(focusProcess.get());
        focusStatusLayout->insertWidget(0, focusPI);
    }

    if (captureProcess.get() != nullptr)
    {
        // Autofocus
        connect(captureProcess.get(), SIGNAL(checkFocus(double)), focusProcess.get(), SLOT(checkFocus(double)),
                Qt::UniqueConnection);
        connect(focusProcess.get(), SIGNAL(newStatus(Ekos::FocusState)), captureProcess.get(),
                SLOT(setFocusStatus(Ekos::FocusState)), Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::newHFR, captureProcess.get(), &Ekos::Capture::setHFR, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess.get(), SIGNAL(meridianFlipStarted()), focusProcess.get(), SLOT(resetFrame()), Qt::UniqueConnection);
    }

    if (guideProcess.get() != nullptr)
    {
        // Suspend
        connect(focusProcess.get(), SIGNAL(suspendGuiding()), guideProcess.get(), SLOT(suspend()), Qt::UniqueConnection);
        connect(focusProcess.get(), SIGNAL(resumeGuiding()), guideProcess.get(), SLOT(resume()), Qt::UniqueConnection);
    }

    if (alignProcess.get() != nullptr)
    {
        connect(focusProcess.get(), SIGNAL(newStatus(Ekos::FocusState)), alignProcess.get(), SLOT(setFocusStatus(Ekos::FocusState)),
                Qt::UniqueConnection);
    }

    if (mountProcess.get() != nullptr)
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), focusProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
}

void EkosManager::updateCurrentHFR(double newHFR, int position)
{
    currentHFR->setText(QString("%1").arg(newHFR, 0, 'f', 2) + " px");

    QJsonObject cStatus = {
      {"hfr", newHFR},
      {"pos", position}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
}

void EkosManager::updateSigmas(double ra, double de)
{
    errRA->setText(QString::number(ra, 'f', 2) + "\"");
    errDEC->setText(QString::number(de, 'f', 2) + "\"");

    QJsonObject cStatus = { {"rarms", ra}, {"derms", de} };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
}

void EkosManager::initMount()
{
    if (mountProcess.get() != nullptr)
        return;

    mountProcess.reset(new Ekos::Mount());
    int index    = toolsWidget->addTab(mountProcess.get(), QIcon(":/icons/ekos_mount.png"), "");

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Mount"));
    connect(mountProcess.get(), SIGNAL(newLog()), this, SLOT(updateLog()));
    connect(mountProcess.get(), SIGNAL(newCoords(QString,QString,QString,QString)), this,
            SLOT(updateMountCoords(QString,QString,QString,QString)));
    connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), this,
            SLOT(updateMountStatus(ISD::Telescope::TelescopeStatus)));
    //connect(mountProcess.get(), SIGNAL(newTarget(QString)), mountTarget, SLOT(setText(QString)));
    connect(mountProcess.get(), &Ekos::Mount::newTarget, [&](const QString &target) {
        mountTarget->setText(target);
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", target}}));
    });
    connect(mountProcess.get(), &Ekos::Mount::slewRateChanged, [&](int slewRate) {
        QJsonObject status = { { "slewRate", slewRate} };
        ekosLiveClient.get()->message()->updateMountStatus(status);
    }
    );

    foreach (ISD::GDInterface *device, findDevices(KSTARS_AUXILIARY))
    {
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::GPS_INTERFACE)
            mountProcess->setGPS(device);
    }

    if (Options::ekosLeftIcons())
    {
        QTransform trans;
        trans.rotate(90);
        QIcon icon  = toolsWidget->tabIcon(index);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(index, icon);
    }

    if (!mountPI)
    {
        mountPI = new QProgressIndicator(mountProcess.get());
        mountStatusLayout->insertWidget(0, mountPI);
    }

    mountGroup->setEnabled(true);

    if (captureProcess.get() != nullptr)
    {
        // Meridian Flip
        connect(captureProcess.get(), SIGNAL(meridianFlipStarted()), mountProcess.get(), SLOT(disableAltLimits()),
                Qt::UniqueConnection);
        connect(captureProcess.get(), SIGNAL(meridianFlipCompleted()), mountProcess.get(), SLOT(enableAltLimits()),
                Qt::UniqueConnection);

        // Mount Status
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), captureProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
    }

    if (alignProcess.get() != nullptr)
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), alignProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);

    if (focusProcess.get() != nullptr)
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), focusProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);

    if (guideProcess.get() != nullptr)
    {
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), guideProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
    }
}

void EkosManager::initGuide()
{
    if (guideProcess.get() == nullptr)
    {
        guideProcess.reset(new Ekos::Guide());

        double primaryScopeFL=0, primaryScopeAperture=0, guideScopeFL=0, guideScopeAperture=0;
        getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
        // Save telescope info in mount driver
        guideProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    }

    //if ( (haveGuider || ccdCount > 1 || useGuideHead) && useST4 && toolsWidget->indexOf(guideProcess) == -1)
    if ((findDevices(KSTARS_CCD).isEmpty() == false || useGuideHead) && useST4 &&
            toolsWidget->indexOf(guideProcess.get()) == -1)
    {
        //if (mount && mount->isConnected())
        if (managedDevices.contains(KSTARS_TELESCOPE) && managedDevices[KSTARS_TELESCOPE]->isConnected())
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        int index = toolsWidget->addTab(guideProcess.get(), QIcon(":/icons/ekos_guide.png"), "");
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Guide"));
        connect(guideProcess.get(), SIGNAL(newLog()), this, SLOT(updateLog()));
        guideGroup->setEnabled(true);

        if (!guidePI)
        {
            guidePI = new QProgressIndicator(guideProcess.get());
            guideStatusLayout->insertWidget(0, guidePI);
        }

        connect(guideProcess.get(), SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(updateGuideStatus(Ekos::GuideState)));
        connect(guideProcess.get(), SIGNAL(newStarPixmap(QPixmap&)), this, SLOT(updateGuideStarPixmap(QPixmap&)));
        connect(guideProcess.get(), SIGNAL(newProfilePixmap(QPixmap&)), this, SLOT(updateGuideProfilePixmap(QPixmap&)));
        connect(guideProcess.get(), SIGNAL(sigmasUpdated(double,double)), this, SLOT(updateSigmas(double,double)));

        if (Options::ekosLeftIcons())
        {
            QTransform trans;
            trans.rotate(90);
            QIcon icon  = toolsWidget->tabIcon(index);
            QPixmap pix = icon.pixmap(QSize(48, 48));
            icon        = QIcon(pix.transformed(trans));
            toolsWidget->setTabIcon(index, icon);
        }
    }

    if (captureProcess.get() != nullptr)
    {
        guideProcess->disconnect(captureProcess.get());
        captureProcess->disconnect(guideProcess.get());

        // Guide Limits
        //connect(guideProcess.get(), SIGNAL(guideReady()), captureProcess, SLOT(enableGuideLimits()));
        connect(guideProcess.get(), SIGNAL(newStatus(Ekos::GuideState)), captureProcess.get(),
                SLOT(setGuideStatus(Ekos::GuideState)));
        connect(guideProcess.get(), SIGNAL(newAxisDelta(double,double)), captureProcess.get(),
                SLOT(setGuideDeviation(double,double)));

        // Dithering
        connect(captureProcess.get(), SIGNAL(newStatus(Ekos::CaptureState)), guideProcess.get(),
                SLOT(setCaptureStatus(Ekos::CaptureState)), Qt::UniqueConnection);

        // Guide Head
        connect(captureProcess.get(), SIGNAL(suspendGuiding()), guideProcess.get(), SLOT(suspend()));
        connect(captureProcess.get(), SIGNAL(resumeGuiding()), guideProcess.get(), SLOT(resume()));
        connect(guideProcess.get(), SIGNAL(guideChipUpdated(ISD::CCDChip*)), captureProcess.get(),
                SLOT(setGuideChip(ISD::CCDChip*)));

        // Meridian Flip
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted, guideProcess.get(), &Ekos::Guide::abort);
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipCompleted, guideProcess.get(), [&]() {
            if (Options::resetGuideCalibration())
                guideProcess->clearCalibration();
            guideProcess->guide();
        });
    }

    if (mountProcess.get() != nullptr)
    {
        mountProcess.get()->disconnect(guideProcess.get());

        // Parking
        connect(mountProcess.get(), SIGNAL(newStatus(ISD::Telescope::TelescopeStatus)), guideProcess.get(),
                SLOT(setMountStatus(ISD::Telescope::TelescopeStatus)), Qt::UniqueConnection);
    }

    if (focusProcess.get() != nullptr)
    {
        focusProcess->disconnect(guideProcess.get());

        // Suspend
        connect(focusProcess.get(), SIGNAL(suspendGuiding()), guideProcess.get(), SLOT(suspend()), Qt::UniqueConnection);
        connect(focusProcess.get(), SIGNAL(resumeGuiding()), guideProcess.get(), SLOT(resume()), Qt::UniqueConnection);
    }
}

void EkosManager::initDome()
{
    if (domeProcess.get() != nullptr)
        return;

    domeProcess.reset(new Ekos::Dome());
}

void EkosManager::initWeather()
{
    if (weatherProcess.get() != nullptr)
        return;

    weatherProcess.reset(new Ekos::Weather());
}

void EkosManager::initDustCap()
{
    if (dustCapProcess.get() != nullptr)
        return;

    dustCapProcess.reset(new Ekos::DustCap());
}

void EkosManager::setST4(ISD::ST4 *st4Driver)
{
    appendLogText(i18n("Guider port from %1 is ready.", st4Driver->getDeviceName()));

    useST4 = true;

    initGuide();

    guideProcess->addST4(st4Driver);

    if (Options::defaultST4Driver().isEmpty() == false)
        guideProcess->setST4(Options::defaultST4Driver());
}

void EkosManager::removeTabs()
{
    disconnect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    for (int i = 2; i < toolsWidget->count(); i++)
        toolsWidget->removeTab(i);

    alignProcess.reset();
    captureProcess.reset();
    focusProcess.reset();
    guideProcess.reset();
    mountProcess.reset();
    domeProcess.reset();
    weatherProcess.reset();
    dustCapProcess.reset();

    managedDevices.clear();

    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));
}

bool EkosManager::isRunning(const QString &process)
{
    QProcess ps;
#ifdef Q_OS_OSX
    ps.start("pgrep", QStringList() << process);
    ps.waitForFinished();
    QString output = ps.readAllStandardOutput();
    return output.length()>0;
#else
    ps.start("ps", QStringList() << "-o"
             << "comm"
             << "--no-headers"
             << "-C" << process);
    ps.waitForFinished();
    QString output = ps.readAllStandardOutput();
    return output.contains(process);
#endif   
}

void EkosManager::addObjectToScheduler(SkyObject *object)
{
    if (schedulerProcess.get() != nullptr)
        schedulerProcess->addObject(object);
}

QString EkosManager::getCurrentJobName()
{
    return schedulerProcess->getCurrentJobName();
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

    for (int i = 0; i < profileCombo->count(); i++)
        profiles << profileCombo->itemText(i);

    return profiles;
}

void EkosManager::addProfile()
{
    ProfileEditor editor(this);

    if (editor.exec() == QDialog::Accepted)
    {
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(profileCombo->count() - 1);
    }

    currentProfile = getCurrentProfile();
}

void EkosManager::editProfile()
{
    ProfileEditor editor(this);

    currentProfile = getCurrentProfile();

    editor.setPi(currentProfile);

    if (editor.exec() == QDialog::Accepted)
    {
        int currentIndex = profileCombo->currentIndex();

        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(currentIndex);
    }

    currentProfile = getCurrentProfile();
}

void EkosManager::deleteProfile()
{
    currentProfile = getCurrentProfile();

    if (currentProfile->name == "Simulators")
        return;

    if (KMessageBox::questionYesNo(this, i18n("Are you sure you want to delete the profile?"),
                                   i18n("Confirm Delete")) == KMessageBox::No)
        return;

    KStarsData::Instance()->userdb()->DeleteProfile(currentProfile);

    profiles.clear();
    loadProfiles();
    currentProfile = getCurrentProfile();
}

void EkosManager::wizardProfile()
{
    ProfileWizard wz;
    if (wz.exec() != QDialog::Accepted)
        return;

    ProfileEditor editor(this);

    editor.setProfileName(wz.profileName);
    editor.setAuxDrivers(wz.selectedAuxDrivers());
    if (wz.useInternalServer == false)
        editor.setHostPort(wz.host, wz.port);
    editor.setWebManager(wz.useWebManager);
    editor.setGuiderType(wz.selectedExternalGuider());
    // Disable connection options
    editor.setConnectionOptionsEnabled(false);

    if (editor.exec() == QDialog::Accepted)
    {
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(profileCombo->count() - 1);
    }

    currentProfile = getCurrentProfile();
}

ProfileInfo *EkosManager::getCurrentProfile()
{
    ProfileInfo *currProfile = nullptr;

    // Get current profile
    for (auto& pi : profiles)
    {
        if (profileCombo->currentText() == pi->name)
        {
            currProfile = pi.get();
            break;
        }
    }

    return currProfile;
}

void EkosManager::updateProfileLocation(ProfileInfo *pi)
{
    if (pi->city.isEmpty() == false)
    {
        bool cityFound = KStars::Instance()->setGeoLocation(pi->city, pi->province, pi->country);
        if (cityFound)
            appendLogText(i18n("Site location updated to %1.", KStarsData::Instance()->geo()->fullName()));
        else
            appendLogText(i18n("Failed to update site location to %1. City not found.",
                               KStarsData::Instance()->geo()->fullName()));
    }
}

void EkosManager::updateMountStatus(ISD::Telescope::TelescopeStatus status)
{
    static ISD::Telescope::TelescopeStatus lastStatus = ISD::Telescope::MOUNT_IDLE;

    if (status == lastStatus)
        return;

    lastStatus = status;

    mountStatus->setText(dynamic_cast<ISD::Telescope *>(managedDevices[KSTARS_TELESCOPE])->getStatusString(status));

    switch (status)
    {
    case ISD::Telescope::MOUNT_PARKING:
    case ISD::Telescope::MOUNT_SLEWING:
    case ISD::Telescope::MOUNT_MOVING:
        mountPI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
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

    QJsonObject cStatus = {
      {"status", mountStatus->text()}
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus);
}

void EkosManager::updateMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt)
{
    raOUT->setText(ra);
    decOUT->setText(dec);
    azOUT->setText(az);
    altOUT->setText(alt);

    QJsonObject cStatus = {
      {"ra", dms::fromString(ra, false).Degrees()},
      {"de", dms::fromString(dec, true).Degrees()},
      {"az", dms::fromString(az, true).Degrees()},
      {"at", dms::fromString(alt, true).Degrees()},
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus);
}

void EkosManager::updateCaptureStatus(Ekos::CaptureState status)
{
    captureStatus->setText(Ekos::getCaptureStatusString(status));
    captureProgress->setValue(captureProcess->getProgressPercentage());

    overallCountDown.setHMS(0, 0, 0);
    overallCountDown = overallCountDown.addSecs(captureProcess->getOverallRemainingTime());

    sequenceCountDown.setHMS(0, 0, 0);
    sequenceCountDown = sequenceCountDown.addSecs(captureProcess->getActiveJobRemainingTime());

    if (status != Ekos::CAPTURE_ABORTED && status != Ekos::CAPTURE_COMPLETE && status != Ekos::CAPTURE_IDLE)
    {
        if (status == Ekos::CAPTURE_CAPTURING)
            capturePI->setColor(Qt::darkGreen);
        else
            capturePI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
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

            if (focusStatus->text() == "Complete")
            {
                if (focusPI->isAnimated())
                    focusPI->stopAnimation();
            }

            imageProgress->setValue(0);
            sequenceLabel->setText(i18n("Sequence"));
            imageRemainingTime->setText("--:--:--");
            overallRemainingTime->setText("--:--:--");
            sequenceRemainingTime->setText("--:--:--");
        }
    }

    QJsonObject cStatus = {
        {"status", captureStatus->text()},
        {"seqt", sequenceRemainingTime->text()},
        {"ovt", overallRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(cStatus);
}

void EkosManager::updateCaptureProgress(QImage *image, Ekos::SequenceJob *job)
{
    // Image is set to nullptr only on initial capture start up
    int completed = 0;
    if (job->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        completed = job->getCompleted() + 1;
    else
        completed = (image == nullptr) ? job->getCompleted() : job->getCompleted() + 1;

    if (job->isPreview() == false)
    {
        sequenceLabel->setText(QString("Job # %1/%2 %3 (%4/%5)")
                               .arg(captureProcess->getActiveJobID() + 1)
                               .arg(captureProcess->getJobCount())
                               .arg(job->getFullPrefix())
                               .arg(completed)
                               .arg(job->getCount()));
    }
    else
        sequenceLabel->setText(i18n("Preview"));

    sequenceProgress->setRange(0, job->getCount());
    sequenceProgress->setValue(completed);

    QJsonObject status = {
        {"seqv", completed},
        {"seqr", job->getCount()},
        {"seql", sequenceLabel->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void EkosManager::updateExposureProgress(Ekos::SequenceJob *job)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(job->getExposeLeft());
    if (imageCountDown.hour() == 23)
        imageCountDown.setHMS(0, 0, 0);

    imageProgress->setRange(0, job->getExposure());
    imageProgress->setValue(job->getExposeLeft());

    imageRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));

    QJsonObject status {
        {"expv", job->getExposeLeft()},
        {"expr", job->getExposure()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void EkosManager::updateCaptureCountDown()
{
    overallCountDown = overallCountDown.addSecs(-1);
    if (overallCountDown.hour() == 23)
        overallCountDown.setHMS(0, 0, 0);

    sequenceCountDown = sequenceCountDown.addSecs(-1);
    if (sequenceCountDown.hour() == 23)
        sequenceCountDown.setHMS(0, 0, 0);

    overallRemainingTime->setText(overallCountDown.toString("hh:mm:ss"));
    sequenceRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));

    QJsonObject status = {
      {"seqt", sequenceRemainingTime->text()},
      {"ovt", overallRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void EkosManager::updateFocusStarPixmap(QPixmap &starPixmap)
{
    if (starPixmap.isNull())
        return;

    focusStarPixmap.reset(new QPixmap(starPixmap));
    focusStarImage->setPixmap(focusStarPixmap->scaled(focusStarImage->width(), focusStarImage->height(),
                                                      Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EkosManager::updateFocusProfilePixmap(QPixmap &profilePixmap)
{
    if (profilePixmap.isNull())
        return;

    focusProfileImage->setPixmap(profilePixmap);
}

void EkosManager::setFocusStatus(Ekos::FocusState status)
{
    focusStatus->setText(Ekos::getFocusStatusString(status));

    if (status >= Ekos::FOCUS_PROGRESS)
    {
        focusPI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
        if (focusPI->isAnimated() == false)
            focusPI->startAnimation();
    }
    else if (status == Ekos::FOCUS_COMPLETE && Options::enforceAutofocus() && captureProcess->getActiveJobID() != -1)
    {
        focusPI->setColor(Qt::darkGreen);
        if (focusPI->isAnimated() == false)
            focusPI->startAnimation();
    }
    else
    {
        if (focusPI->isAnimated())
            focusPI->stopAnimation();
    }

    QJsonObject cStatus = {
      {"status", focusStatus->text()}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
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
        guidePI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
        if (guidePI->isAnimated() == false)
            guidePI->startAnimation();
        break;
    case Ekos::GUIDE_GUIDING:
        guidePI->setColor(Qt::darkGreen);
        if (guidePI->isAnimated() == false)
            guidePI->startAnimation();
        break;
    case Ekos::GUIDE_DITHERING:
        guidePI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
        if (guidePI->isAnimated() == false)
            guidePI->startAnimation();
        break;
    case Ekos::GUIDE_DITHERING_SUCCESS:
        guidePI->setColor(Qt::darkGreen);
        if (guidePI->isAnimated() == false)
            guidePI->startAnimation();
        break;

    default:
        if (guidePI->isAnimated())
            guidePI->stopAnimation();
        break;
    }

    QJsonObject cStatus = {
      {"status", guideStatus->text()}
    };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
}

void EkosManager::updateGuideStarPixmap(QPixmap &starPix)
{
    if (starPix.isNull())
        return;

    guideStarPixmap.reset(new QPixmap(starPix));
    guideStarImage->setPixmap(guideStarPixmap->scaled(guideStarImage->width(), guideStarImage->height(),
                                                      Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EkosManager::updateGuideProfilePixmap(QPixmap &profilePix)
{
    if (profilePix.isNull())
        return;

    guideProfileImage->setPixmap(profilePix);
}

void EkosManager::setTarget(SkyObject *o)
{
    mountTarget->setText(o->name());    
    ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", o->name()}}));
}

void EkosManager::showEkosOptions()
{
    QWidget *currentWidget = toolsWidget->currentWidget();

    if (alignProcess.get() && alignProcess.get() == currentWidget)
    {
        KConfigDialog *alignSettings = KConfigDialog::exists("alignsettings");
        if (alignSettings)
        {
            alignSettings->setEnabled(true);
            alignSettings->show();
        }
        return;
    }

    if (guideProcess.get() && guideProcess.get() == currentWidget)
    {
        KConfigDialog::showDialog("guidesettings");
        return;
    }

    if (ekosOptionsWidget == nullptr)
    {
        optionsB->click();
    }
    else if (KConfigDialog::showDialog("settings"))
    {
        KConfigDialog *cDialog = KConfigDialog::exists("settings");
        cDialog->setCurrentPage(ekosOptionsWidget);
    }
}

void EkosManager::getCurrentProfileTelescopeInfo(double &primaryFocalLength, double &primaryAperture, double &guideFocalLength, double &guideAperture)
{
    ProfileInfo *pi = getCurrentProfile();
    if (pi)
    {
        int primaryScopeID=0, guideScopeID=0;
        primaryScopeID=pi->primaryscope;
        guideScopeID=pi->guidescope;
        if (primaryScopeID > 0 || guideScopeID > 0)
        {
            // Get all OAL equipment filter list
            QList<OAL::Scope*> m_scopeList;
            KStarsData::Instance()->userdb()->GetAllScopes(m_scopeList);
            foreach(OAL::Scope *oneScope, m_scopeList)
            {
                if (oneScope->id().toInt() == primaryScopeID)
                {
                    primaryFocalLength = oneScope->focalLength();
                    primaryAperture = oneScope->aperture();
                }

                if (oneScope->id().toInt() == guideScopeID)
                {
                    guideFocalLength = oneScope->focalLength();
                    guideAperture = oneScope->aperture();
                }
            }
        }
    }
}

void EkosManager::updateDebugInterfaces()
{
    KSUtils::Logging::SyncFilterRules();

    for (ISD::GDInterface *device : genericDevices)
    {
        INDI::Property *debugProp = device->getProperty("DEBUG");
        ISwitchVectorProperty *debugSP = nullptr;
        if (debugProp)
            debugSP = debugProp->getSwitch();
        else
            continue;

        // Check if the debug interface matches the driver device class
        if ( ( opsLogs->getINDIDebugInterface() & device->getBaseDevice()->getDriverInterface() ) &&
             debugSP->sp[0].s != ISS_ON)
        {
            debugSP->sp[0].s = ISS_ON;
            debugSP->sp[1].s = ISS_OFF;
            device->getDriverInfo()->getClientManager()->sendNewSwitch(debugSP);
            appendLogText(i18n("Enabling debug logging for %1...", device->getDeviceName()));
        }
        else if ( !( opsLogs->getINDIDebugInterface() & device->getBaseDevice()->getDriverInterface() ) &&
                  debugSP->sp[0].s != ISS_OFF)
        {
            debugSP->sp[0].s = ISS_OFF;
            debugSP->sp[1].s = ISS_ON;
            device->getDriverInfo()->getClientManager()->sendNewSwitch(debugSP);
            appendLogText(i18n("Disabling debug logging for %1...", device->getDeviceName()));
        }

        if (opsLogs->isINDISettingsChanged())
            device->setConfig(SAVE_CONFIG);
    }
}

void EkosManager::watchDebugProperty(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "DEBUG"))
    {
        ISD::GenericDevice *deviceInterface = qobject_cast<ISD::GenericDevice *>(sender());

        // We don't process pure general interfaces
        if (deviceInterface->getBaseDevice()->getDriverInterface() == INDI::BaseDevice::GENERAL_INTERFACE)
            return;

        // If debug was turned off, but our logging policy requires it then turn it back on.
        // We turn on debug logging if AT LEAST one driver interface is selected by the logging settings
        if (svp->s == IPS_OK && svp->sp[0].s == ISS_OFF &&
                (opsLogs->getINDIDebugInterface() & deviceInterface->getBaseDevice()->getDriverInterface()))
        {
            svp->sp[0].s = ISS_ON;
            svp->sp[1].s = ISS_OFF;
            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(svp);
            appendLogText(i18n("Re-enabling debug logging for %1...", deviceInterface->getDeviceName()));
        }
        // To turn off debug logging, NONE of the driver interfaces should be enabled in logging settings.
        // For example, if we have CCD+FilterWheel device and CCD + Filter Wheel logging was turned on in
        // the log settings, then if the user turns off only CCD logging, the debug logging is NOT
        // turned off until he turns off Filter Wheel logging as well.
        else if (svp->s == IPS_OK && svp->sp[0].s == ISS_ON && !(opsLogs->getINDIDebugInterface() & deviceInterface->getBaseDevice()->getDriverInterface()))
        {
            svp->sp[0].s = ISS_OFF;
            svp->sp[1].s = ISS_ON;
            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(svp);
            appendLogText(i18n("Re-disabling debug logging for %1...", deviceInterface->getDeviceName()));
        }
    }
}

void EkosManager::announceEvent(const QString &message, KSNotification::EventType event)
{
    ekosLiveClient.get()->message()->sendEvent(message, event);
}
