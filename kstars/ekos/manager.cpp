/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikartech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "manager.h"

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
#include "auxiliary/ksmessagebox.h"
#include "capture/sequencejob.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
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

namespace Ekos
{

Manager::Manager(QWidget * parent) : QDialog(parent)
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
#else
    //    if (Options::independentWindowEkos())
    //        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
#endif
    setupUi(this);

    qRegisterMetaType<Ekos::CommunicationStatus>("Ekos::CommunicationStatus");
    qDBusRegisterMetaType<Ekos::CommunicationStatus>();

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
    connect(&countdownTimer, &QTimer::timeout, this, &Ekos::Manager::updateCaptureCountDown);

    toolsWidget->setIconSize(QSize(48, 48));
    connect(toolsWidget, &QTabWidget::currentChanged, this, &Ekos::Manager::processTabChange, Qt::UniqueConnection);

    // Enable scheduler Tab
    toolsWidget->setTabEnabled(1, false);

    // Start/Stop INDI Server
    connect(processINDIB, &QPushButton::clicked, this, &Ekos::Manager::processINDI);
    processINDIB->setIcon(QIcon::fromTheme("media-playback-start"));
    processINDIB->setToolTip(i18n("Start"));

    // Connect/Disconnect INDI devices
    connect(connectB, &QPushButton::clicked, this, &Ekos::Manager::connectDevices);
    connect(disconnectB, &QPushButton::clicked, this, &Ekos::Manager::disconnectDevices);

    ekosLiveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ekosLiveClient.reset(new EkosLive::Client(this));
    connect(ekosLiveClient.get(), &EkosLive::Client::connected, [this]()
    {
        emit ekosLiveStatusChanged(true);
    });
    connect(ekosLiveClient.get(), &EkosLive::Client::disconnected, [this]()
    {
        emit ekosLiveStatusChanged(false);
    });

    // INDI Control Panel
    //connect(controlPanelB, &QPushButton::clicked, GUIManager::Instance(), SLOT(show()));
    connect(ekosLiveB, &QPushButton::clicked, [&]()
    {
        ekosLiveClient.get()->show();
        ekosLiveClient.get()->raise();
    });

    connect(this, &Manager::ekosStatusChanged, ekosLiveClient.get()->message(), &EkosLive::Message::setEkosStatingStatus);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::connected, [&]()
    {
        ekosLiveB->setIcon(QIcon(":/icons/cloud-online.svg"));
    });
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::disconnected, [&]()
    {
        ekosLiveB->setIcon(QIcon::fromTheme("folder-cloud"));
    });
    connect(ekosLiveClient.get()->media(), &EkosLive::Media::newBoundingRect, ekosLiveClient.get()->message(), &EkosLive::Message::setBoundingRect);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::resetPolarView, ekosLiveClient.get()->media(), &EkosLive::Media::resetPolarView);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::previewJPEGGenerated, ekosLiveClient.get()->media(), &EkosLive::Media::sendPreviewJPEG);
    connect(KSMessageBox::Instance(), &KSMessageBox::newMessage, ekosLiveClient.get()->message(), &EkosLive::Message::sendDialog);

    // Serial Port Assistant
    connect(serialPortAssistantB, &QPushButton::clicked, [&]()
    {
        serialPortAssistant->show();
        serialPortAssistant->raise();
    });

    connect(this, &Ekos::Manager::ekosStatusChanged, [&](Ekos::CommunicationStatus status)
    {
        indiControlPanelB->setEnabled(status == Ekos::Success);
    });
    connect(indiControlPanelB, &QPushButton::clicked, [&]()
    {
        KStars::Instance()->actionCollection()->action("show_control_panel")->trigger();
    });
    connect(optionsB, &QPushButton::clicked, [&]()
    {
        KStars::Instance()->actionCollection()->action("configure")->trigger();
    });
    // Save as above, but it appears in all modules
    connect(ekosOptionsB, &QPushButton::clicked, this, &Ekos::Manager::showEkosOptions);

    // Clear Ekos Log
    connect(clearB, &QPushButton::clicked, this, &Ekos::Manager::clearLog);

    // Logs
    KConfigDialog * dialog = new KConfigDialog(this, "logssettings", Options::self());
    opsLogs = new Ekos::OpsLogs();
    KPageWidgetItem * page = dialog->addPage(opsLogs, i18n("Logging"));
    page->setIcon(QIcon::fromTheme("configure"));
    connect(logsB, &QPushButton::clicked, dialog, &KConfigDialog::show);
    connect(dialog->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &Ekos::Manager::updateDebugInterfaces);
    connect(dialog->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &Ekos::Manager::updateDebugInterfaces);

    // Summary
    // previewPixmap = new QPixmap(QPixmap(":/images/noimage.png"));

    // Profiles
    connect(addProfileB, &QPushButton::clicked, this, &Ekos::Manager::addProfile);
    connect(editProfileB, &QPushButton::clicked, this, &Ekos::Manager::editProfile);
    connect(deleteProfileB, &QPushButton::clicked, this, &Ekos::Manager::deleteProfile);
    connect(profileCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged),
            [ = ](const QString & text)
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
    connect(wizardProfileB, &QPushButton::clicked, this, &Ekos::Manager::wizardProfile);

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
    connect(schedulerProcess.get(), &Scheduler::newLog, this, &Ekos::Manager::updateLog);
    //connect(schedulerProcess.get(), SIGNAL(newTarget(QString)), mountTarget, SLOT(setText(QString)));
    connect(schedulerProcess.get(), &Ekos::Scheduler::newTarget, [&](const QString & target)
    {
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
    QVBoxLayout * vlayout = new QVBoxLayout();
    vlayout->addWidget(summaryPreview.get());
    previewWidget->setLayout(vlayout);

    // JM 2019-01-19: Why cloud images depend on summary preview?
    //    connect(summaryPreview.get(), &FITSView::loaded, [&]()
    //    {
    //        // UUID binds the cloud & preview frames by a common key
    //        QString uuid = QUuid::createUuid().toString();
    //        uuid = uuid.remove(QRegularExpression("[-{}]"));
    //        //ekosLiveClient.get()->media()->sendPreviewImage(summaryPreview.get(), uuid);
    //        ekosLiveClient.get()->cloud()->sendPreviewImage(summaryPreview.get(), uuid);
    //    });

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

void Manager::changeAlwaysOnTop(Qt::ApplicationState state)
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

Manager::~Manager()
{
    toolsWidget->disconnect(this);
    //delete previewPixmap;
}

void Manager::closeEvent(QCloseEvent * event)
{
    //    QAction * a = KStars::Instance()->actionCollection()->action("show_ekos");
    //    a->setChecked(false);

    // 2019-02-14 JM: Close event, for some reason, make all the children disappear
    // when the widget is shown again. Applying a workaround here

    event->ignore();
    hide();
}

void Manager::hideEvent(QHideEvent * /*event*/)
{
    Options::setEkosWindowWidth(width());
    Options::setEkosWindowHeight(height());

    QAction * a = KStars::Instance()->actionCollection()->action("show_ekos");
    a->setChecked(false);
}

void Manager::showEvent(QShowEvent * /*event*/)
{
    QAction * a = KStars::Instance()->actionCollection()->action("show_ekos");
    a->setChecked(true);

    // Just show the profile wizard ONCE per session
    if (profileWizardLaunched == false && profiles.count() == 1)
    {
        profileWizardLaunched = true;
        wizardProfile();
    }
}

void Manager::resizeEvent(QResizeEvent *)
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

void Manager::loadProfiles()
{
    profiles.clear();
    KStarsData::Instance()->userdb()->GetAllProfiles(profiles);

    profileModel->clear();

    for (auto &pi : profiles)
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

void Manager::loadDrivers()
{
    foreach (DriverInfo * dv, DriverManager::Instance()->getDrivers())
    {
        if (dv->getDriverSource() != HOST_SOURCE)
            driversList[dv->getLabel()] = dv;
    }
}

void Manager::reset()
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
    observatoryProcess.reset();
    dustCapProcess.reset();

    DarkLibrary::Instance()->reset();

    Ekos::CommunicationStatus previousStatus = m_ekosStatus;
    m_ekosStatus   = Ekos::Idle;
    if (previousStatus != m_ekosStatus)
        emit ekosStatusChanged(m_ekosStatus);

    previousStatus = m_indiStatus;
    m_indiStatus = Ekos::Idle;
    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

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
    mountStatus->setStyleSheet(QString());
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

    m_isStarted = false;

    //processINDIB->setText(i18n("Start INDI"));
    processINDIB->setIcon(QIcon::fromTheme("media-playback-start"));
    processINDIB->setToolTip(i18n("Start"));
}

void Manager::processINDI()
{
    if (m_isStarted == false)
        start();
    else
        stop();
}

bool Manager::stop()
{
    cleanDevices();

    serialPortAssistant.reset();
    serialPortAssistantB->setEnabled(false);

    profileGroup->setEnabled(true);

    setWindowTitle(i18n("Ekos"));

    return true;
}

bool Manager::start()
{
    // Don't start if it is already started before
    if (m_ekosStatus == Ekos::Pending || m_ekosStatus == Ekos::Success)
    {
        qCDebug(KSTARS_EKOS) << "Ekos Manager start called but current Ekos Status is" << m_ekosStatus << "Ignoring request.";
        return true;
    }

    if (m_LocalMode)
        qDeleteAll(managedDrivers);
    managedDrivers.clear();

    // If clock was paused, unpaused it and sync time
    if (KStarsData::Instance()->clock()->isActive() == false)
    {
        KStarsData::Instance()->changeDateTime(KStarsDateTime::currentDateTimeUtc());
        KStarsData::Instance()->clock()->start();
    }

    reset();

    //    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    //    {
    //        QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
    //        qDebug() << "Dialog was accepted!";
    //    });
    //    KSMessageBox::Instance()->questionYesNo("Do you want to proceed?", "Confirm", 21);


    currentProfile = getCurrentProfile();
    m_LocalMode      = currentProfile->isLocal();

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

    if (m_LocalMode)
    {
        DriverInfo * drv = driversList.value(currentProfile->mount());

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
                if (checkUniqueBinaryDriver( driversList.value(currentProfile->ccd()), drv))
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
        {
            if (!checkUniqueBinaryDriver(driversList.value(currentProfile->ccd()), drv) &&
                    !checkUniqueBinaryDriver(driversList.value(currentProfile->guider()), drv))
                managedDrivers.append(drv->clone());
        }
        drv = driversList.value(currentProfile->aux2());
        if (drv != nullptr)
        {
            if (!checkUniqueBinaryDriver(driversList.value(currentProfile->ccd()), drv) &&
                    !checkUniqueBinaryDriver(driversList.value(currentProfile->guider()), drv))
                managedDrivers.append(drv->clone());
        }

        drv = driversList.value(currentProfile->aux3());
        if (drv != nullptr)
        {
            if (!checkUniqueBinaryDriver(driversList.value(currentProfile->ccd()), drv) &&
                    !checkUniqueBinaryDriver(driversList.value(currentProfile->guider()), drv))
                managedDrivers.append(drv->clone());
        }

        drv = driversList.value(currentProfile->aux4());
        if (drv != nullptr)
        {
            if (!checkUniqueBinaryDriver(driversList.value(currentProfile->ccd()), drv) &&
                    !checkUniqueBinaryDriver(driversList.value(currentProfile->guider()), drv))
                managedDrivers.append(drv->clone());
        }

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

                DriverInfo * dv = new DriverInfo(name);
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
            KSNotification::error(i18n("Ekos requires at least one CCD or Guider to operate."));
            managedDrivers.clear();
            return false;
        }

        nDevices = managedDrivers.count();
    }
    else
    {
        DriverInfo * remote_indi = new DriverInfo(QString("Ekos Remote Host"));

        remote_indi->setHostParameters(currentProfile->host, QString::number(currentProfile->port));

        remote_indi->setDriverSource(GENERATED_SOURCE);

        managedDrivers.append(remote_indi);

        haveCCD    = currentProfile->drivers.contains("CCD");
        haveGuider = currentProfile->drivers.contains("Guider");

        Options::setGuiderType(currentProfile->guidertype);

        if (haveCCD == false && haveGuider == false)
        {
            KSNotification::error(i18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices = 0;
            return false;
        }

        nDevices = currentProfile->drivers.count();
    }

    connect(INDIListener::Instance(), &INDIListener::newDevice, this, &Ekos::Manager::processNewDevice);
    connect(INDIListener::Instance(), &INDIListener::newTelescope, this, &Ekos::Manager::setTelescope);
    connect(INDIListener::Instance(), &INDIListener::newCCD, this, &Ekos::Manager::setCCD);
    connect(INDIListener::Instance(), &INDIListener::newFilter, this, &Ekos::Manager::setFilter);
    connect(INDIListener::Instance(), &INDIListener::newFocuser, this, &Ekos::Manager::setFocuser);
    connect(INDIListener::Instance(), &INDIListener::newDome, this, &Ekos::Manager::setDome);
    connect(INDIListener::Instance(), &INDIListener::newWeather, this, &Ekos::Manager::setWeather);
    connect(INDIListener::Instance(), &INDIListener::newDustCap, this, &Ekos::Manager::setDustCap);
    connect(INDIListener::Instance(), &INDIListener::newLightBox, this, &Ekos::Manager::setLightBox);
    connect(INDIListener::Instance(), &INDIListener::newST4, this, &Ekos::Manager::setST4);
    connect(INDIListener::Instance(), &INDIListener::deviceRemoved, this, &Ekos::Manager::removeDevice, Qt::DirectConnection);

#ifdef Q_OS_OSX
    if (m_LocalMode || currentProfile->host == "localhost")
    {
        if (isRunning("PTPCamera"))
        {
            if (KMessageBox::Yes ==
                    (KMessageBox::questionYesNo(nullptr,
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
    if (m_LocalMode)
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
            m_ekosStatus = Ekos::Error;
            emit ekosStatusChanged(m_ekosStatus);
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString, QString)), this,
                SLOT(processServerTermination(QString, QString)));

        m_ekosStatus = Ekos::Pending;
        emit ekosStatusChanged(m_ekosStatus);

        if (currentProfile->autoConnect)
            appendLogText(i18n("INDI services started on port %1.", managedDrivers.first()->getPort()));
        else
            appendLogText(
                i18n("INDI services started on port %1. Please connect devices.", managedDrivers.first()->getPort()));

        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, &Ekos::Manager::checkINDITimeout);
    }
    else
    {
        // If we need to use INDI Web Manager
        if (currentProfile->INDIWebManagerPort > 0)
        {
            appendLogText(i18n("Establishing communication with remote INDI Web Manager..."));
            m_RemoteManagerStart = false;
            if (INDI::WebManager::isOnline(currentProfile))
            {
                INDI::WebManager::syncCustomDrivers(currentProfile);

                currentProfile->isStellarMate = INDI::WebManager::isStellarMate(currentProfile);

                if (INDI::WebManager::areDriversRunning(currentProfile) == false)
                {
                    INDI::WebManager::stopProfile(currentProfile);

                    if (INDI::WebManager::startProfile(currentProfile) == false)
                    {
                        appendLogText(i18n("Failed to start profile on remote INDI Web Manager."));
                        return false;
                    }

                    appendLogText(i18n("Starting profile on remote INDI Web Manager..."));
                    m_RemoteManagerStart = true;
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
            m_ekosStatus = Ekos::Error;
            emit ekosStatusChanged(m_ekosStatus);
            QApplication::restoreOverrideCursor();
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString, QString)), this,
                SLOT(processServerTermination(QString, QString)));

        QApplication::restoreOverrideCursor();
        m_ekosStatus = Ekos::Pending;
        emit ekosStatusChanged(m_ekosStatus);

        appendLogText(
            i18n("INDI services started. Connection to remote INDI server is successful. Waiting for devices..."));

        QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, &Ekos::Manager::checkINDITimeout);
    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    //controlPanelB->setEnabled(false);

    profileGroup->setEnabled(false);

    m_isStarted = true;
    //processINDIB->setText(i18n("Stop INDI"));
    processINDIB->setIcon(QIcon::fromTheme("media-playback-stop"));
    processINDIB->setToolTip(i18n("Stop"));

    setWindowTitle(i18n("Ekos - %1 Profile", currentProfile->name));

    return true;
}

void Manager::checkINDITimeout()
{
    // Don't check anything unless we're still pending
    if (m_ekosStatus != Ekos::Pending)
        return;

    if (nDevices <= 0)
    {
        m_ekosStatus = Ekos::Success;
        emit ekosStatusChanged(m_ekosStatus);
        return;
    }

    if (m_LocalMode)
    {
        QStringList remainingDevices;
        foreach (DriverInfo * drv, managedDrivers)
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

    m_ekosStatus = Ekos::Error;
}

void Manager::connectDevices()
{
    // Check if already connected
    int nConnected = 0;

    Ekos::CommunicationStatus previousStatus = m_indiStatus;

    for (auto &device : genericDevices)
    {
        if (device->isConnected())
            nConnected++;
    }
    if (genericDevices.count() == nConnected)
    {
        m_indiStatus = Ekos::Success;
        emit indiStatusChanged(m_indiStatus);
        return;
    }

    m_indiStatus = Ekos::Pending;
    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

    for (auto &device : genericDevices)
    {
        qCDebug(KSTARS_EKOS) << "Connecting " << device->getDeviceName();
        device->Connect();
    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText(i18n("Connecting INDI devices..."));
}

void Manager::disconnectDevices()
{
    for (auto &device : genericDevices)
    {
        qCDebug(KSTARS_EKOS) << "Disconnecting " << device->getDeviceName();
        device->Disconnect();
    }

    appendLogText(i18n("Disconnecting INDI devices..."));
}

void Manager::processServerTermination(const QString &host, const QString &port)
{
    if ((m_LocalMode && managedDrivers.first()->getPort() == port) ||
            (currentProfile->host == host && currentProfile->port == port.toInt()))
    {
        cleanDevices(false);
    }
}

void Manager::cleanDevices(bool stopDrivers)
{
    if (m_ekosStatus == Ekos::Idle)
        return;

    INDIListener::Instance()->disconnect(this);
    DriverManager::Instance()->disconnect(this);

    if (managedDrivers.isEmpty() == false)
    {
        if (m_LocalMode)
        {
            if (stopDrivers)
                DriverManager::Instance()->stopDevices(managedDrivers);
        }
        else
        {
            if (stopDrivers)
                DriverManager::Instance()->disconnectRemoteHost(managedDrivers.first());

            if (m_RemoteManagerStart && currentProfile->INDIWebManagerPort != -1)
            {
                INDI::WebManager::stopProfile(currentProfile);
                m_RemoteManagerStart = false;
            }
        }
    }

    reset();

    profileGroup->setEnabled(true);

    appendLogText(i18n("INDI services stopped."));
}

void Manager::processNewDevice(ISD::GDInterface * devInterface)
{
    qCInfo(KSTARS_EKOS) << "Ekos received a new device: " << devInterface->getDeviceName();

    Ekos::CommunicationStatus previousStatus = m_indiStatus;

    for(auto &device : genericDevices)
    {
        if (!strcmp(device->getDeviceName(), devInterface->getDeviceName()))
        {
            qCWarning(KSTARS_EKOS) << "Found duplicate device, ignoring...";
            return;
        }
    }

    // Always reset INDI Connection status if we receive a new device
    m_indiStatus = Ekos::Idle;
    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

    genericDevices.append(devInterface);

    nDevices--;

    connect(devInterface, &ISD::GDInterface::Connected, this, &Ekos::Manager::deviceConnected);
    connect(devInterface, &ISD::GDInterface::Disconnected, this, &Ekos::Manager::deviceDisconnected);
    connect(devInterface, &ISD::GDInterface::propertyDefined, this, &Ekos::Manager::processNewProperty);
    connect(devInterface, &ISD::GDInterface::interfaceDefined, this, &Ekos::Manager::syncActiveDevices);

    if (currentProfile->isStellarMate)
    {
        connect(devInterface, &ISD::GDInterface::systemPortDetected, [this, devInterface]()
        {
            if (!serialPortAssistant)
            {
                serialPortAssistant.reset(new SerialPortAssistant(currentProfile, this));
                serialPortAssistantB->setEnabled(true);
            }

            uint32_t driverInterface = devInterface->getDriverInterface();
            // Ignore CCD interface
            if (driverInterface & INDI::BaseDevice::CCD_INTERFACE)
                return;

            if (driverInterface & INDI::BaseDevice::TELESCOPE_INTERFACE ||
                    driverInterface & INDI::BaseDevice::FOCUSER_INTERFACE   ||
                    driverInterface & INDI::BaseDevice::FILTER_INTERFACE    ||
                    driverInterface & INDI::BaseDevice::AUX_INTERFACE       ||
                    driverInterface & INDI::BaseDevice::GPS_INTERFACE)
                serialPortAssistant->addDevice(devInterface);

            if (Options::autoLoadSerialAssistant())
                serialPortAssistant->show();
        });
    }

    if (nDevices <= 0)
    {
        m_ekosStatus = Ekos::Success;
        emit ekosStatusChanged(m_ekosStatus);

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        //controlPanelB->setEnabled(true);

        if (m_LocalMode == false && nDevices == 0)
        {
            if (currentProfile->autoConnect)
                appendLogText(i18n("Remote devices established."));
            else
                appendLogText(i18n("Remote devices established. Please connect devices."));
        }
    }
}

void Manager::deviceConnected()
{
    connectB->setEnabled(false);
    disconnectB->setEnabled(true);
    processINDIB->setEnabled(false);

    Ekos::CommunicationStatus previousStatus = m_indiStatus;

    if (Options::verboseLogging())
    {
        ISD::GDInterface * device = qobject_cast<ISD::GDInterface *>(sender());
        qCInfo(KSTARS_EKOS) << device->getDeviceName()
                            << "Version:" << device->getDriverVersion()
                            << "Interface:" << device->getDriverInterface()
                            << "is connected.";
    }

    int nConnectedDevices = 0;

    foreach (ISD::GDInterface * device, genericDevices)
    {
        if (device->isConnected())
            nConnectedDevices++;
    }

    qCDebug(KSTARS_EKOS) << nConnectedDevices << " devices connected out of " << genericDevices.count();

    if (nConnectedDevices >= currentProfile->drivers.count())
        //if (nConnectedDevices >= genericDevices.count())
    {
        m_indiStatus = Ekos::Success;
        qCInfo(KSTARS_EKOS) << "All INDI devices are now connected.";
    }
    else
        m_indiStatus = Ekos::Pending;

    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

    ISD::GDInterface * dev = static_cast<ISD::GDInterface *>(sender());

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

    foreach (ISD::GDInterface * device, genericDevices)
    {
        if (device == dev)
        {
            connect(dev, &ISD::GDInterface::switchUpdated, this, &Ekos::Manager::watchDebugProperty);

            ISwitchVectorProperty * configProp = device->getBaseDevice()->getSwitch("CONFIG_PROCESS");
            if (configProp && configProp->s == IPS_IDLE)
                device->setConfig(tConfig);
            break;
        }
    }
}

void Manager::deviceDisconnected()
{
    ISD::GDInterface * dev = static_cast<ISD::GDInterface *>(sender());

    Ekos::CommunicationStatus previousStatus = m_indiStatus;

    if (dev != nullptr)
    {
        if (dev->getState("CONNECTION") == IPS_ALERT)
            m_indiStatus = Ekos::Error;
        else if (dev->getState("CONNECTION") == IPS_BUSY)
            m_indiStatus = Ekos::Pending;
        else
            m_indiStatus = Ekos::Idle;

        if (Options::verboseLogging())
            qCDebug(KSTARS_EKOS) << dev->getDeviceName() << " is disconnected.";

        appendLogText(i18n("%1 is disconnected.", dev->getDeviceName()));
    }
    else
        m_indiStatus = Ekos::Idle;

    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

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

void Manager::setTelescope(ISD::GDInterface * scopeDevice)
{
    //mount = scopeDevice;

    managedDevices[KSTARS_TELESCOPE] = scopeDevice;

    appendLogText(i18n("%1 is online.", scopeDevice->getDeviceName()));

    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
            SLOT(processNewNumber(INumberVectorProperty *)), Qt::UniqueConnection);

    initMount();

    mountProcess->setTelescope(scopeDevice);

    double primaryScopeFL = 0, primaryScopeAperture = 0, guideScopeFL = 0, guideScopeAperture = 0;
    getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    // Save telescope info in mount driver
    mountProcess->setTelescopeInfo(QList<double>() << primaryScopeFL << primaryScopeAperture << guideScopeFL << guideScopeAperture);

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

    //    if (domeProcess.get() != nullptr)
    //        domeProcess->setTelescope(scopeDevice);

    ekosLiveClient->message()->sendMounts();
    ekosLiveClient->message()->sendScopes();
}

void Manager::setCCD(ISD::GDInterface * ccdDevice)
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
        foreach (ISD::GDInterface * device, findDevices(KSTARS_CCD))
        {
            if (QString(device->getDeviceName()).startsWith(currentProfile->ccd(), Qt::CaseInsensitive))
                primaryCCD = QString(device->getDeviceName());
            else if (QString(device->getDeviceName()).startsWith(currentProfile->guider(), Qt::CaseInsensitive))
                guiderCCD = QString(device->getDeviceName());
        }
    }

    bool rc = false;
    if (Options::defaultCaptureCCD().isEmpty() == false)
        rc = captureProcess->setCamera(Options::defaultCaptureCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        captureProcess->setCamera(primaryCCD);

    initFocus();

    focusProcess->addCCD(ccdDevice);

    rc = false;
    if (Options::defaultFocusCCD().isEmpty() == false)
        rc = focusProcess->setCamera(Options::defaultFocusCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        focusProcess->setCamera(primaryCCD);

    initAlign();

    alignProcess->addCCD(ccdDevice);

    rc = false;
    if (Options::defaultAlignCCD().isEmpty() == false)
        rc = alignProcess->setCamera(Options::defaultAlignCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        alignProcess->setCamera(primaryCCD);

    initGuide();

    guideProcess->addCamera(ccdDevice);

    rc = false;
    if (Options::defaultGuideCCD().isEmpty() == false)
        rc = guideProcess->setCamera(Options::defaultGuideCCD());
    if (rc == false && guiderCCD.isEmpty() == false)
        guideProcess->setCamera(guiderCCD);

    appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
            SLOT(processNewNumber(INumberVectorProperty *)), Qt::UniqueConnection);

    if (managedDevices.contains(KSTARS_TELESCOPE))
    {
        alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
        captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
        guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
    }
}

void Manager::setFilter(ISD::GDInterface * filterDevice)
{
    // No duplicates
    if (findDevices(KSTARS_FILTER).contains(filterDevice) == false)
        managedDevices.insertMulti(KSTARS_FILTER, filterDevice);

    appendLogText(i18n("%1 filter is online.", filterDevice->getDeviceName()));

    initCapture();

    connect(filterDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
            SLOT(processNewNumber(INumberVectorProperty *)), Qt::UniqueConnection);
    connect(filterDevice, SIGNAL(textUpdated(ITextVectorProperty *)), this,
            SLOT(processNewText(ITextVectorProperty *)), Qt::UniqueConnection);

    captureProcess->addFilter(filterDevice);

    initFocus();

    focusProcess->addFilter(filterDevice);

    initAlign();

    alignProcess->addFilter(filterDevice);
}

void Manager::setFocuser(ISD::GDInterface * focuserDevice)
{
    // No duplicates
    if (findDevices(KSTARS_FOCUSER).contains(focuserDevice) == false)
        managedDevices.insertMulti(KSTARS_FOCUSER, focuserDevice);

    initCapture();

    initFocus();

    focusProcess->addFocuser(focuserDevice);

    if (Options::defaultFocusFocuser().isEmpty() == false)
        focusProcess->setFocuser(Options::defaultFocusFocuser());

    appendLogText(i18n("%1 focuser is online.", focuserDevice->getDeviceName()));
}

void Manager::setDome(ISD::GDInterface * domeDevice)
{
    managedDevices[KSTARS_DOME] = domeDevice;

    initDome();

    domeProcess->setDome(domeDevice);

    if (captureProcess.get() != nullptr)
        captureProcess->setDome(domeDevice);

    if (alignProcess.get() != nullptr)
        alignProcess->setDome(domeDevice);

    //    if (managedDevices.contains(KSTARS_TELESCOPE))
    //        domeProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

    appendLogText(i18n("%1 is online.", domeDevice->getDeviceName()));
}

void Manager::setWeather(ISD::GDInterface * weatherDevice)
{
    managedDevices[KSTARS_WEATHER] = weatherDevice;

    initWeather();

    weatherProcess->setWeather(weatherDevice);

    appendLogText(i18n("%1 is online.", weatherDevice->getDeviceName()));
}

void Manager::setDustCap(ISD::GDInterface * dustCapDevice)
{
    // No duplicates
    if (findDevices(KSTARS_AUXILIARY).contains(dustCapDevice) == false)
        managedDevices.insertMulti(KSTARS_AUXILIARY, dustCapDevice);

    initDustCap();

    dustCapProcess->setDustCap(dustCapDevice);

    appendLogText(i18n("%1 is online.", dustCapDevice->getDeviceName()));

    if (captureProcess.get() != nullptr)
        captureProcess->setDustCap(dustCapDevice);

    DarkLibrary::Instance()->setRemoteCap(dustCapDevice);

}

void Manager::setLightBox(ISD::GDInterface * lightBoxDevice)
{

    // No duplicates
    if (findDevices(KSTARS_AUXILIARY).contains(lightBoxDevice) == false)
        managedDevices.insertMulti(KSTARS_AUXILIARY, lightBoxDevice);

    if (captureProcess.get() != nullptr)
        captureProcess->setLightBox(lightBoxDevice);
}

void Manager::removeDevice(ISD::GDInterface * devInterface)
{
    //    switch (devInterface->getType())
    //    {
    //        case KSTARS_CCD:
    //            removeTabs();
    //            break;
    //        default:
    //            break;
    //    }

    if (alignProcess)
        alignProcess->removeDevice(devInterface);
    if (captureProcess)
        captureProcess->removeDevice(devInterface);
    if (focusProcess)
        focusProcess->removeDevice(devInterface);
    if (mountProcess)
        mountProcess->removeDevice(devInterface);
    if (guideProcess)
        guideProcess->removeDevice(devInterface);
    if (domeProcess)
        domeProcess->removeDevice(devInterface);
    if (weatherProcess)
        weatherProcess->removeDevice(devInterface);
    if (dustCapProcess)
    {
        dustCapProcess->removeDevice(devInterface);
        DarkLibrary::Instance()->removeDevice(devInterface);
    }

    appendLogText(i18n("%1 is offline.", devInterface->getDeviceName()));

    // #1 Remove from Generic Devices
    // Generic devices are ALL the devices we receive from INDI server
    // Whether Ekos cares about them (i.e. selected equipment) or extra devices we
    // do not care about
    for (auto &device : genericDevices)
    {
        if (!strcmp(device->getDeviceName(), devInterface->getDeviceName()))
        {
            genericDevices.removeOne(device);
        }
    }

    // #2 Remove from Ekos Managed Device
    // Managed devices are devices selected by the user in the device profile
    for (auto &device : managedDevices.values())
    {
        if (!strcmp(device->getDeviceName(), devInterface->getDeviceName()))
        {
            managedDevices.remove(managedDevices.key(device));

            if (managedDevices.count() == 0)
                cleanDevices();

            //break;
        }
    }

    if (managedDevices.isEmpty())
        removeTabs();
}

void Manager::processNewText(ITextVectorProperty * tvp)
{
    if (!strcmp(tvp->name, "FILTER_NAME"))
    {
        ekosLiveClient.get()->message()->sendFilterWheels();
    }

    if (!strcmp(tvp->name, "ACTIVE_DEVICES"))
    {
        syncActiveDevices();
    }
}

void Manager::processNewNumber(INumberVectorProperty * nvp)
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

void Manager::processNewProperty(INDI::Property * prop)
{
    ISD::GenericDevice * deviceInterface = qobject_cast<ISD::GenericDevice *>(sender());

    if (!strcmp(prop->getName(), "CONNECTION") && currentProfile->autoConnect)
    {
        // Check if we need to do any mappings
        const QString port = m_ProfileMapping.value(QString(deviceInterface->getDeviceName())).toString();
        // If we don't have port mapping, then we connect immediately.
        if (port.isEmpty())
            deviceInterface->Connect();
        return;
    }

    if (!strcmp(prop->getName(), "DEVICE_PORT"))
    {
        // Check if we need to do any mappings
        const QString port = m_ProfileMapping.value(QString(deviceInterface->getDeviceName())).toString();
        if (!port.isEmpty())
        {
            ITextVectorProperty *tvp = prop->getText();
            IUSaveText(&(tvp->tp[0]), port.toLatin1().data());
            deviceInterface->getDriverInfo()->getClientManager()->sendNewText(tvp);
            // Now connect if we need to.
            if (currentProfile->autoConnect)
                deviceInterface->Connect();
            return;
        }
    }

    // Check if we need to turn on DEBUG for logging purposes
    if (!strcmp(prop->getName(), "DEBUG"))
    {
        uint16_t interface = deviceInterface->getBaseDevice()->getDriverInterface();
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Check if we need to enable debug logging for the INDI drivers.
            ISwitchVectorProperty * debugSP = prop->getSwitch();
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
            ISwitchVectorProperty * debugLevel = prop->getSwitch();
            for (int i = 0; i < debugLevel->nsp; i++)
                debugLevel->sp[i].s = ISS_ON;

            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(debugLevel);
        }
    }

    if (!strcmp(prop->getName(), "ACTIVE_DEVICES"))
    {
        // Reset ACTIVE_FILTER aux0 and aux1 since we use them for overrides.
        ITextVectorProperty *tvp = prop->getText();
        IText *tp = IUFindText(tvp, "ACTIVE_FILTER");
        if (tp)
        {
            tp->aux0 = tp->aux1 = nullptr;
        }

        if (deviceInterface->getDriverInterface() > 0)
            syncActiveDevices();
    }

    if (!strcmp(prop->getName(), "TELESCOPE_INFO") || !strcmp(prop->getName(), "TELESCOPE_SLEW_RATE")
            || !strcmp(prop->getName(), "TELESCOPE_PARK"))
    {
        ekosLiveClient.get()->message()->sendMounts();
        ekosLiveClient.get()->message()->sendScopes();
    }

    if (!strcmp(prop->getName(), "CCD_INFO") || !strcmp(prop->getName(), "CCD_TEMPERATURE") || !strcmp(prop->getName(), "CCD_ISO") ||
            !strcmp(prop->getName(), "CCD_GAIN") || !strcmp(prop->getName(), "CCD_CONTROLS"))
    {
        ekosLiveClient.get()->message()->sendCameras();
        ekosLiveClient.get()->media()->registerCameras();
    }

    if (!strcmp(prop->getName(), "ABS_DOME_POSITION") || !strcmp(prop->getName(), "DOME_ABORT_MOTION") ||
            !strcmp(prop->getName(), "DOME_PARK"))
    {
        ekosLiveClient.get()->message()->sendDomes();
    }

    if (!strcmp(prop->getName(), "CAP_PARK") || !strcmp(prop->getName(), "FLAT_LIGHT_CONTROL"))
    {
        ekosLiveClient.get()->message()->sendCaps();
    }

    if (!strcmp(prop->getName(), "FILTER_NAME"))
        ekosLiveClient.get()->message()->sendFilterWheels();

    if (!strcmp(prop->getName(), "FILTER_NAME"))
        filterManager.data()->initFilterProperties();

    if (!strcmp(prop->getName(), "CONFIRM_FILTER_SET"))
        filterManager.data()->initFilterProperties();

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
        foreach (ISD::GDInterface * device, findDevices(KSTARS_CCD))
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
                    rc = guideProcess->setCamera(Options::defaultGuideCCD());
                if (rc == false)
                    guideProcess->setCamera(QString(device->getDeviceName()) + QString(" Guider"));
                return;
            }
        }

        return;
    }

    if (!strcmp(prop->getName(), "CCD_FRAME_TYPE"))
    {
        if (captureProcess.get() != nullptr)
        {
            foreach (ISD::GDInterface * device, findDevices(KSTARS_CCD))
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

    if (!strcmp(prop->getName(), "CCD_ISO"))
    {
        if (captureProcess.get() != nullptr)
            captureProcess->checkCCD();

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
        foreach (ISD::GDInterface * device, genericDevices)
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

QList<ISD::GDInterface *> Manager::getAllDevices()
{
    QList<ISD::GDInterface *> deviceList;

    QMapIterator<DeviceFamily, ISD::GDInterface *> i(managedDevices);
    while (i.hasNext())
    {
        i.next();
        deviceList.append(i.value());
    }

    return deviceList;
}

QList<ISD::GDInterface *> Manager::findDevices(DeviceFamily type)
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

QList<ISD::GDInterface *> Manager::findDevicesByInterface(uint32_t interface)
{
    QList<ISD::GDInterface *> deviceList;

    for (const auto dev : genericDevices)
    {
        uint32_t devInterface = dev->getDriverInterface();
        if (devInterface & interface)
            deviceList.append(dev);
    }

    return deviceList;
}

void Manager::processTabChange()
{
    QWidget * currentWidget = toolsWidget->currentWidget();

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
                else if (Options::solverBackend() == Ekos::Align::SOLVER_ASTROMETRYNET)
                {
                    // If current setting is remote astrometry and profile doesn't contain
                    // remote astrometry, then we switch to online solver. Otherwise, the whole align
                    // module remains disabled and the user cannot change re-enable it since he cannot select online solver
                    ProfileInfo * pi = getCurrentProfile();
                    if (Options::astrometrySolverType() == Ekos::Align::SOLVER_REMOTE && pi->aux1() != "Astrometry" && pi->aux2() != "Astrometry"
                            && pi->aux3() != "Astrometry" && pi->aux4() != "Astrometry")
                    {

                        Options::setAstrometrySolverType(Ekos::Align::SOLVER_ONLINE);
                        alignModule()->setAstrometrySolverType(Ekos::Align::SOLVER_ONLINE);
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

void Manager::updateLog()
{
    //if (enableLoggingCheck->isChecked() == false)
    //return;

    QWidget * currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
        ekosLogOut->setPlainText(m_LogText.join("\n"));
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
    else if (currentWidget == schedulerProcess.get())
        ekosLogOut->setPlainText(schedulerProcess->getLogText());
    else if (currentWidget == observatoryProcess.get())
        ekosLogOut->setPlainText(observatoryProcess->getLogText());

#ifdef Q_OS_OSX
    repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif

}

void Manager::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS) << text;

    emit newLog(text);

    updateLog();
}

void Manager::clearLog()
{
    QWidget * currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
    {
        m_LogText.clear();
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
    else if (currentWidget == observatoryProcess.get())
        observatoryProcess->clearLog();
}

void Manager::initCapture()
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
    connect(captureProcess.get(), &Ekos::Capture::newLog, this, &Ekos::Manager::updateLog);
    connect(captureProcess.get(), &Ekos::Capture::newStatus, this, &Ekos::Manager::updateCaptureStatus);
    connect(captureProcess.get(), &Ekos::Capture::newImage, this, &Ekos::Manager::updateCaptureProgress);
    connect(captureProcess.get(), &Ekos::Capture::newSequenceImage, [&](const QString & filename, const QString & previewFITS)
    {
        if (Options::useSummaryPreview() && QFile::exists(filename))
        {
            if (Options::autoImageToFITS())
            {
                if (previewFITS.isEmpty() == false)
                    summaryPreview->loadFITS(previewFITS);
            }
            else
                summaryPreview->loadFITS(filename);
        }
    });
    connect(captureProcess.get(), &Ekos::Capture::newDownloadProgress, this, &Ekos::Manager::updateDownloadProgress);
    connect(captureProcess.get(), &Ekos::Capture::newExposureProgress, this, &Ekos::Manager::updateExposureProgress);
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

    foreach (ISD::GDInterface * device, findDevices(KSTARS_AUXILIARY))
    {
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
            captureProcess->setDustCap(device);
        if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::LIGHTBOX_INTERFACE)
            captureProcess->setLightBox(device);
    }

    if (managedDevices.contains(KSTARS_DOME))
    {
        captureProcess->setDome(managedDevices[KSTARS_DOME]);
    }
    if (managedDevices.contains(KSTARS_ROTATOR))
    {
        captureProcess->setRotator(managedDevices[KSTARS_ROTATOR]);
    }

    connectModules();

    emit newModule("Capture");
}

void Manager::initAlign()
{
    if (alignProcess.get() != nullptr)
        return;

    alignProcess.reset(new Ekos::Align(currentProfile));

    double primaryScopeFL = 0, primaryScopeAperture = 0, guideScopeFL = 0, guideScopeAperture = 0;
    getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    alignProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);

    alignProcess->setEnabled(false);
    int index = toolsWidget->addTab(alignProcess.get(), QIcon(":/icons/ekos_align.png"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Align"));
    connect(alignProcess.get(), &Ekos::Align::newLog, this, &Ekos::Manager::updateLog);
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

    if (managedDevices.contains(KSTARS_DOME))
    {
        alignProcess->setDome(managedDevices[KSTARS_DOME]);
    }
    if (managedDevices.contains(KSTARS_ROTATOR))
    {
        alignProcess->setRotator(managedDevices[KSTARS_ROTATOR]);
    }

    connectModules();

    emit newModule("Align");
}

void Manager::initFocus()
{
    if (focusProcess.get() != nullptr)
        return;

    focusProcess.reset(new Ekos::Focus());
    int index    = toolsWidget->addTab(focusProcess.get(), QIcon(":/icons/ekos_focus.png"), "");

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Focus"));

    // Focus <---> Manager connections
    connect(focusProcess.get(), &Ekos::Focus::newLog, this, &Ekos::Manager::updateLog);
    connect(focusProcess.get(), &Ekos::Focus::newStatus, this, &Ekos::Manager::setFocusStatus);
    connect(focusProcess.get(), &Ekos::Focus::newStarPixmap, this, &Ekos::Manager::updateFocusStarPixmap);
    connect(focusProcess.get(), &Ekos::Focus::newProfilePixmap, this, &Ekos::Manager::updateFocusProfilePixmap);
    connect(focusProcess.get(), &Ekos::Focus::newHFR, this, &Ekos::Manager::updateCurrentHFR);

    // Focus <---> Filter Manager connections
    focusProcess->setFilterManager(filterManager);
    connect(filterManager.data(), &Ekos::FilterManager::checkFocus, focusProcess.get(), &Ekos::Focus::checkFocus, Qt::UniqueConnection);
    connect(focusProcess.get(), &Ekos::Focus::newStatus, filterManager.data(), &Ekos::FilterManager::setFocusStatus, Qt::UniqueConnection);
    connect(filterManager.data(), &Ekos::FilterManager::newFocusOffset, focusProcess.get(), &Ekos::Focus::adjustFocusOffset, Qt::UniqueConnection);
    connect(focusProcess.get(), &Ekos::Focus::focusPositionAdjusted, filterManager.data(), &Ekos::FilterManager::setFocusOffsetComplete, Qt::UniqueConnection);
    connect(focusProcess.get(), &Ekos::Focus::absolutePositionChanged, filterManager.data(), &Ekos::FilterManager::setFocusAbsolutePosition, Qt::UniqueConnection);

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

    connectModules();

    emit newModule("Focus");
}

void Manager::updateCurrentHFR(double newHFR, int position)
{
    currentHFR->setText(QString("%1").arg(newHFR, 0, 'f', 2) + " px");

    QJsonObject cStatus =
    {
        {"hfr", newHFR},
        {"pos", position}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
}

void Manager::updateSigmas(double ra, double de)
{
    errRA->setText(QString::number(ra, 'f', 2) + "\"");
    errDEC->setText(QString::number(de, 'f', 2) + "\"");

    QJsonObject cStatus = { {"rarms", ra}, {"derms", de} };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
}

void Manager::initMount()
{
    if (mountProcess.get() != nullptr)
        return;

    mountProcess.reset(new Ekos::Mount());
    int index    = toolsWidget->addTab(mountProcess.get(), QIcon(":/icons/ekos_mount.png"), "");

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Mount"));
    connect(mountProcess.get(), &Ekos::Mount::newLog, this, &Ekos::Manager::updateLog);
    connect(mountProcess.get(), &Ekos::Mount::newCoords, this, &Ekos::Manager::updateMountCoords);
    connect(mountProcess.get(), &Ekos::Mount::newStatus, this, &Ekos::Manager::updateMountStatus);
    connect(mountProcess.get(), &Ekos::Mount::newTarget, [&](const QString & target)
    {
        mountTarget->setText(target);
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", target}}));
    });
    connect(mountProcess.get(), &Ekos::Mount::slewRateChanged, [&](int slewRate)
    {
        QJsonObject status = { { "slewRate", slewRate} };
        ekosLiveClient.get()->message()->updateMountStatus(status);
    }
           );

    foreach (ISD::GDInterface * device, findDevices(KSTARS_AUXILIARY))
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

    connectModules();

    emit newModule("Mount");
}

void Manager::initGuide()
{
    if (guideProcess.get() == nullptr)
    {
        guideProcess.reset(new Ekos::Guide());

        double primaryScopeFL = 0, primaryScopeAperture = 0, guideScopeFL = 0, guideScopeAperture = 0;
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
        connect(guideProcess.get(), &Ekos::Guide::newLog, this, &Ekos::Manager::updateLog);
        guideGroup->setEnabled(true);

        if (!guidePI)
        {
            guidePI = new QProgressIndicator(guideProcess.get());
            guideStatusLayout->insertWidget(0, guidePI);
        }

        connect(guideProcess.get(), &Ekos::Guide::newStatus, this, &Ekos::Manager::updateGuideStatus);
        connect(guideProcess.get(), &Ekos::Guide::newStarPixmap, this, &Ekos::Manager::updateGuideStarPixmap);
        connect(guideProcess.get(), &Ekos::Guide::newProfilePixmap, this, &Ekos::Manager::updateGuideProfilePixmap);
        connect(guideProcess.get(), &Ekos::Guide::newAxisSigma, this, &Ekos::Manager::updateSigmas);

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

    connectModules();

    emit newModule("Guide");
}

void Manager::initDome()
{
    if (domeProcess.get() != nullptr)
        return;

    domeProcess.reset(new Ekos::Dome());

    connect(domeProcess.get(), &Ekos::Dome::newStatus, [&](ISD::Dome::Status newStatus)
    {
        // For roll-off domes
        // cw ---> unparking
        // ccw --> parking
        if (domeProcess->isRolloffRoof() &&
                (newStatus == ISD::Dome::DOME_MOVING_CW || newStatus == ISD::Dome::DOME_MOVING_CCW))
        {
            newStatus = (newStatus == ISD::Dome::DOME_MOVING_CW) ? ISD::Dome::DOME_UNPARKING :
                        ISD::Dome::DOME_PARKING;
        }
        QJsonObject status = { { "status", ISD::Dome::getStatusString(newStatus)} };
        ekosLiveClient.get()->message()->updateDomeStatus(status);
    });

    connect(domeProcess.get(), &Ekos::Dome::azimuthPositionChanged, [&](double pos)
    {
        QJsonObject status = { { "az", pos} };
        ekosLiveClient.get()->message()->updateDomeStatus(status);
    });

    initObservatory(nullptr, domeProcess.get());
    emit newModule("Dome");

    ekosLiveClient->message()->sendDomes();
}

void Manager::initWeather()
{
    if (weatherProcess.get() != nullptr)
        return;

    weatherProcess.reset(new Ekos::Weather());
    initObservatory(weatherProcess.get(), nullptr);

    emit newModule("Weather");
}

void Manager::initObservatory(Weather *weather, Dome *dome)
{
    if (observatoryProcess.get() == nullptr)
    {
        // Initialize the Observatory Module
        observatoryProcess.reset(new Ekos::Observatory());
        int index = toolsWidget->addTab(observatoryProcess.get(), QIcon(":/icons/ekos_observatory.png"), "");
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Observatory"));
        connect(observatoryProcess.get(), &Ekos::Observatory::newLog, this, &Ekos::Manager::updateLog);
    }

    Observatory *obs = observatoryProcess.get();
    if (weather != nullptr)
        obs->getWeatherModel()->initModel(weather);
    if (dome != nullptr)
        obs->getDomeModel()->initModel(dome);

    emit newModule("Observatory");

}

void Manager::initDustCap()
{
    if (dustCapProcess.get() != nullptr)
        return;

    dustCapProcess.reset(new Ekos::DustCap());

    connect(dustCapProcess.get(), &Ekos::DustCap::newStatus, [&](ISD::DustCap::Status newStatus)
    {
        QJsonObject status = { { "status", ISD::DustCap::getStatusString(newStatus)} };
        ekosLiveClient.get()->message()->updateCapStatus(status);
    });
    connect(dustCapProcess.get(), &Ekos::DustCap::lightToggled, [&](bool enabled)
    {
        QJsonObject status = { { "lightS", enabled} };
        ekosLiveClient.get()->message()->updateCapStatus(status);
    });
    connect(dustCapProcess.get(), &Ekos::DustCap::lightIntensityChanged, [&](uint16_t value)
    {
        QJsonObject status = { { "lightB", value} };
        ekosLiveClient.get()->message()->updateCapStatus(status);
    });

    emit newModule("DustCap");

    ekosLiveClient->message()->sendCaps();
}

void Manager::setST4(ISD::ST4 * st4Driver)
{
    appendLogText(i18n("Guider port from %1 is ready.", st4Driver->getDeviceName()));

    useST4 = true;

    initGuide();

    guideProcess->addST4(st4Driver);

    if (Options::defaultST4Driver().isEmpty() == false)
        guideProcess->setST4(Options::defaultST4Driver());
}

void Manager::removeTabs()
{
    disconnect(toolsWidget, &QTabWidget::currentChanged, this, &Ekos::Manager::processTabChange);

    for (int i = 2; i < toolsWidget->count(); i++)
        toolsWidget->removeTab(i);

    alignProcess.reset();
    captureProcess.reset();
    focusProcess.reset();
    guideProcess.reset();
    mountProcess.reset();
    domeProcess.reset();
    weatherProcess.reset();
    observatoryProcess.reset();
    dustCapProcess.reset();

    managedDevices.clear();

    connect(toolsWidget, &QTabWidget::currentChanged, this, &Ekos::Manager::processTabChange, Qt::UniqueConnection);
}

bool Manager::isRunning(const QString &process)
{
    QProcess ps;
#ifdef Q_OS_OSX
    ps.start("pgrep", QStringList() << process);
    ps.waitForFinished();
    QString output = ps.readAllStandardOutput();
    return output.length() > 0;
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

void Manager::addObjectToScheduler(SkyObject * object)
{
    if (schedulerProcess.get() != nullptr)
        schedulerProcess->addObject(object);
}

QString Manager::getCurrentJobName()
{
    return schedulerProcess->getCurrentJobName();
}

bool Manager::setProfile(const QString &profileName)
{
    int index = profileCombo->findText(profileName);

    if (index < 0)
        return false;

    profileCombo->setCurrentIndex(index);

    return true;
}

void Manager::editNamedProfile(const QJsonObject &profileInfo)
{
    ProfileEditor editor(this);
    setProfile(profileInfo["name"].toString());
    currentProfile = getCurrentProfile();
    editor.setPi(currentProfile);
    editor.setSettings(profileInfo);
    editor.saveProfile();
}

void Manager::addNamedProfile(const QJsonObject &profileInfo)
{
    ProfileEditor editor(this);

    editor.setSettings(profileInfo);
    editor.saveProfile();
    profiles.clear();
    loadProfiles();
    profileCombo->setCurrentIndex(profileCombo->count() - 1);
    currentProfile = getCurrentProfile();
}

void Manager::deleteNamedProfile(const QString &name)
{
    currentProfile = getCurrentProfile();

    for (auto &pi : profiles)
    {
        // Do not delete an actively running profile
        // Do not delete simulator profile
        if (pi->name == "Simulators" || pi->name != name || (pi.get() == currentProfile && ekosStatus() != Idle))
            continue;

        KStarsData::Instance()->userdb()->DeleteProfile(pi.get());

        profiles.clear();
        loadProfiles();
        currentProfile = getCurrentProfile();
        return;
    }
}

QJsonObject Manager::getNamedProfile(const QString &name)
{
    QJsonObject profileInfo;

    // Get current profile
    for (auto &pi : profiles)
    {
        if (name == pi->name)
            return pi->toJson();
    }

    return QJsonObject();
}

QStringList Manager::getProfiles()
{
    QStringList profiles;

    for (int i = 0; i < profileCombo->count(); i++)
        profiles << profileCombo->itemText(i);

    return profiles;
}

void Manager::addProfile()
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

void Manager::editProfile()
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

void Manager::deleteProfile()
{
    currentProfile = getCurrentProfile();

    if (currentProfile->name == "Simulators")
        return;

    auto executeDeleteProfile = [&]()
    {
        KStarsData::Instance()->userdb()->DeleteProfile(currentProfile);
        profiles.clear();
        loadProfiles();
        currentProfile = getCurrentProfile();
    };

    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executeDeleteProfile]()
    {
        //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
        KSMessageBox::Instance()->disconnect(this);
        executeDeleteProfile();
    });

    KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to delete the profile?"),
                                            i18n("Confirm Delete"));

}

void Manager::wizardProfile()
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

ProfileInfo * Manager::getCurrentProfile()
{
    ProfileInfo * currProfile = nullptr;

    // Get current profile
    for (auto &pi : profiles)
    {
        if (profileCombo->currentText() == pi->name)
        {
            currProfile = pi.get();
            break;
        }
    }

    return currProfile;
}

void Manager::updateProfileLocation(ProfileInfo * pi)
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

void Manager::updateMountStatus(ISD::Telescope::Status status)
{
    static ISD::Telescope::Status lastStatus = ISD::Telescope::MOUNT_IDLE;

    if (status == lastStatus)
        return;

    lastStatus = status;

    mountStatus->setText(dynamic_cast<ISD::Telescope *>(managedDevices[KSTARS_TELESCOPE])->getStatusString(status));
    mountStatus->setStyleSheet(QString());

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

        case ISD::Telescope::MOUNT_PARKED:
            mountStatus->setStyleSheet("font-weight:bold;background-color:red;border:2px solid black;");
            if (mountPI->isAnimated())
                mountPI->stopAnimation();
            break;

        default:
            if (mountPI->isAnimated())
                mountPI->stopAnimation();
    }

    QJsonObject cStatus =
    {
        {"status", mountStatus->text()}
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus);
}

void Manager::updateMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt)
{
    raOUT->setText(ra);
    decOUT->setText(dec);
    azOUT->setText(az);
    altOUT->setText(alt);

    QJsonObject cStatus =
    {
        {"ra", dms::fromString(ra, false).Degrees()},
        {"de", dms::fromString(dec, true).Degrees()},
        {"az", dms::fromString(az, true).Degrees()},
        {"at", dms::fromString(alt, true).Degrees()},
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus);
}

void Manager::updateCaptureStatus(Ekos::CaptureState status)
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

    QJsonObject cStatus =
    {
        {"status", captureStatus->text()},
        {"seqt", sequenceRemainingTime->text()},
        {"ovt", overallRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(cStatus);
}

void Manager::updateCaptureProgress(Ekos::SequenceJob * job)
{
    // Image is set to nullptr only on initial capture start up
    int completed = job->getCompleted();
    //    if (job->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    //        completed = job->getCompleted() + 1;
    //    else
    //        completed = job->isPreview() ? job->getCompleted() : job->getCompleted() + 1;

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

    QJsonObject status =
    {
        {"seqv", completed},
        {"seqr", job->getCount()},
        {"seql", sequenceLabel->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
    if (job->getStatus() == SequenceJob::JOB_BUSY)
    {
        QString uuid = QUuid::createUuid().toString();
        uuid = uuid.remove(QRegularExpression("[-{}]"));
        //        FITSView *image = job->getActiveChip()->getImageView(FITS_NORMAL);
        //        ekosLiveClient.get()->media()->sendPreviewImage(image, uuid);
        //        ekosLiveClient.get()->cloud()->sendPreviewImage(image, uuid);
        QString filename = job->property("filename").toString();
        ekosLiveClient.get()->media()->sendPreviewImage(filename, uuid);
        if (job->isPreview() == false)
            ekosLiveClient.get()->cloud()->sendPreviewImage(filename, uuid);

    }
}

void Manager::updateDownloadProgress(double timeLeft)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(timeLeft);
    imageRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));
}

void Manager::updateExposureProgress(Ekos::SequenceJob * job)
{
    imageCountDown.setHMS(0, 0, 0);
    imageCountDown = imageCountDown.addSecs(job->getExposeLeft() + captureProcess->getEstimatedDownloadTime());
    if (imageCountDown.hour() == 23)
        imageCountDown.setHMS(0, 0, 0);

    imageProgress->setRange(0, job->getExposure());
    imageProgress->setValue(job->getExposeLeft());

    imageRemainingTime->setText(imageCountDown.toString("hh:mm:ss"));

    QJsonObject status
    {
        {"expv", job->getExposeLeft()},
        {"expr", job->getExposure()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void Manager::updateCaptureCountDown()
{
    overallCountDown = overallCountDown.addSecs(-1);
    if (overallCountDown.hour() == 23)
        overallCountDown.setHMS(0, 0, 0);

    sequenceCountDown = sequenceCountDown.addSecs(-1);
    if (sequenceCountDown.hour() == 23)
        sequenceCountDown.setHMS(0, 0, 0);

    overallRemainingTime->setText(overallCountDown.toString("hh:mm:ss"));
    sequenceRemainingTime->setText(sequenceCountDown.toString("hh:mm:ss"));

    QJsonObject status =
    {
        {"seqt", sequenceRemainingTime->text()},
        {"ovt", overallRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void Manager::updateFocusStarPixmap(QPixmap &starPixmap)
{
    if (starPixmap.isNull())
        return;

    focusStarPixmap.reset(new QPixmap(starPixmap));
    focusStarImage->setPixmap(focusStarPixmap->scaled(focusStarImage->width(), focusStarImage->height(),
                              Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Manager::updateFocusProfilePixmap(QPixmap &profilePixmap)
{
    if (profilePixmap.isNull())
        return;

    focusProfileImage->setPixmap(profilePixmap);
}

void Manager::setFocusStatus(Ekos::FocusState status)
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

    QJsonObject cStatus =
    {
        {"status", focusStatus->text()}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
}

void Manager::updateGuideStatus(Ekos::GuideState status)
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

    QJsonObject cStatus =
    {
        {"status", guideStatus->text()}
    };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
}

void Manager::updateGuideStarPixmap(QPixmap &starPix)
{
    if (starPix.isNull())
        return;

    guideStarPixmap.reset(new QPixmap(starPix));
    guideStarImage->setPixmap(guideStarPixmap->scaled(guideStarImage->width(), guideStarImage->height(),
                              Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Manager::updateGuideProfilePixmap(QPixmap &profilePix)
{
    if (profilePix.isNull())
        return;

    guideProfileImage->setPixmap(profilePix);
}

void Manager::setTarget(SkyObject * o)
{
    mountTarget->setText(o->name());
    ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", o->name()}}));
}

void Manager::showEkosOptions()
{
    QWidget * currentWidget = toolsWidget->currentWidget();

    if (alignProcess.get() && alignProcess.get() == currentWidget)
    {
        KConfigDialog * alignSettings = KConfigDialog::exists("alignsettings");
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
        KConfigDialog * cDialog = KConfigDialog::exists("settings");
        cDialog->setCurrentPage(ekosOptionsWidget);
    }
}

void Manager::getCurrentProfileTelescopeInfo(double &primaryFocalLength, double &primaryAperture, double &guideFocalLength, double &guideAperture)
{
    ProfileInfo * pi = getCurrentProfile();
    if (pi)
    {
        int primaryScopeID = 0, guideScopeID = 0;
        primaryScopeID = pi->primaryscope;
        guideScopeID = pi->guidescope;
        if (primaryScopeID > 0 || guideScopeID > 0)
        {
            // Get all OAL equipment filter list
            QList<OAL::Scope *> m_scopeList;
            KStarsData::Instance()->userdb()->GetAllScopes(m_scopeList);
            for(auto oneScope : m_scopeList)
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

            qDeleteAll(m_scopeList);
        }
    }
}

void Manager::updateDebugInterfaces()
{
    KSUtils::Logging::SyncFilterRules();

    for (ISD::GDInterface * device : genericDevices)
    {
        INDI::Property * debugProp = device->getProperty("DEBUG");
        ISwitchVectorProperty * debugSP = nullptr;
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

void Manager::watchDebugProperty(ISwitchVectorProperty * svp)
{
    if (!strcmp(svp->name, "DEBUG"))
    {
        ISD::GenericDevice * deviceInterface = qobject_cast<ISD::GenericDevice *>(sender());

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

void Manager::announceEvent(const QString &message, KSNotification::EventType event)
{
    ekosLiveClient.get()->message()->sendEvent(message, event);
}

void Manager::connectModules()
{
    // Guide <---> Capture connections
    if (captureProcess.get() && guideProcess.get())
    {
        captureProcess.get()->disconnect(guideProcess.get());
        guideProcess.get()->disconnect(captureProcess.get());

        // Guide Limits
        connect(guideProcess.get(), &Ekos::Guide::newStatus, captureProcess.get(), &Ekos::Capture::setGuideStatus, Qt::UniqueConnection);
        connect(guideProcess.get(), &Ekos::Guide::newAxisDelta, captureProcess.get(), &Ekos::Capture::setGuideDeviation);

        // Dithering
        connect(captureProcess.get(), &Ekos::Capture::newStatus, guideProcess.get(), &Ekos::Guide::setCaptureStatus, Qt::UniqueConnection);

        // Guide Head
        connect(captureProcess.get(), &Ekos::Capture::suspendGuiding, guideProcess.get(), &Ekos::Guide::suspend, Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::resumeGuiding, guideProcess.get(), &Ekos::Guide::resume, Qt::UniqueConnection);
        connect(guideProcess.get(), &Ekos::Guide::guideChipUpdated, captureProcess.get(), &Ekos::Capture::setGuideChip, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted, guideProcess.get(), &Ekos::Guide::abort, Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipCompleted, guideProcess.get(), &Ekos::Guide::guideAfterMeridianFlip, Qt::UniqueConnection);
    }

    // Guide <---> Mount connections
    if (guideProcess.get() && mountProcess.get())
    {
        // Parking
        connect(mountProcess.get(), &Ekos::Mount::newStatus, guideProcess.get(), &Ekos::Guide::setMountStatus, Qt::UniqueConnection);
    }

    // Focus <---> Guide connections
    if (guideProcess.get() && focusProcess.get())
    {
        // Suspend
        connect(focusProcess.get(), &Ekos::Focus::suspendGuiding, guideProcess.get(), &Ekos::Guide::suspend, Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::resumeGuiding, guideProcess.get(), &Ekos::Guide::resume, Qt::UniqueConnection);
    }

    // Capture <---> Focus connections
    if (captureProcess.get() && focusProcess.get())
    {
        // Check focus HFR value
        connect(captureProcess.get(), &Ekos::Capture::checkFocus, focusProcess.get(), &Ekos::Focus::checkFocus, Qt::UniqueConnection);
        // Reset Focus
        connect(captureProcess.get(), &Ekos::Capture::resetFocus, focusProcess.get(), &Ekos::Focus::resetFrame, Qt::UniqueConnection);

        // New Focus Status
        connect(focusProcess.get(), &Ekos::Focus::newStatus, captureProcess.get(), &Ekos::Capture::setFocusStatus, Qt::UniqueConnection);
        // New Focus HFR
        connect(focusProcess.get(), &Ekos::Focus::newHFR, captureProcess.get(), &Ekos::Capture::setHFR, Qt::UniqueConnection);
    }

    // Capture <---> Align connections
    if (captureProcess.get() && alignProcess.get())
    {
        // Alignment flag
        connect(alignProcess.get(), &Ekos::Align::newStatus, captureProcess.get(), &Ekos::Capture::setAlignStatus, Qt::UniqueConnection);
        // Solver data
        connect(alignProcess.get(), &Ekos::Align::newSolverResults, captureProcess.get(),  &Ekos::Capture::setAlignResults, Qt::UniqueConnection);
        // Capture Status
        connect(captureProcess.get(), &Ekos::Capture::newStatus, alignProcess.get(), &Ekos::Align::setCaptureStatus, Qt::UniqueConnection);
    }

    // Capture <---> Mount connections
    if (captureProcess.get() && mountProcess.get())
    {
        // Meridian Flip states
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted, mountProcess.get(), &Ekos::Mount::disableAltLimits, Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipCompleted, mountProcess.get(), &Ekos::Mount::enableAltLimits, Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::newMeridianFlipStatus, mountProcess.get(), &Ekos::Mount::meridianFlipStatusChanged, Qt::UniqueConnection);
        connect(mountProcess.get(), &Ekos::Mount::newMeridianFlipStatus, captureProcess.get(), &Ekos::Capture::meridianFlipStatusChanged, Qt::UniqueConnection);

        // Mount Status
        connect(mountProcess.get(), &Ekos::Mount::newStatus, captureProcess.get(), &Ekos::Capture::setMountStatus, Qt::UniqueConnection);
    }

    // Capture <---> EkosLive connections
    if (captureProcess.get() && ekosLiveClient.get())
    {
        captureProcess.get()->disconnect(ekosLiveClient.get()->message());

        connect(captureProcess.get(), &Ekos::Capture::dslrInfoRequested, ekosLiveClient.get()->message(), &EkosLive::Message::requestDSLRInfo);
        connect(captureProcess.get(), &Ekos::Capture::sequenceChanged, ekosLiveClient.get()->message(), &EkosLive::Message::sendCaptureSequence);
        connect(captureProcess.get(), &Ekos::Capture::settingsUpdated, ekosLiveClient.get()->message(), &EkosLive::Message::sendCaptureSettings);
    }

    // Focus <---> Align connections
    if (focusProcess.get() && alignProcess.get())
    {
        connect(focusProcess.get(), &Ekos::Focus::newStatus, alignProcess.get(), &Ekos::Align::setFocusStatus, Qt::UniqueConnection);
    }

    // Focus <---> Mount connections
    if (focusProcess.get() && mountProcess.get())
    {
        connect(mountProcess.get(), &Ekos::Mount::newStatus, focusProcess.get(), &Ekos::Focus::setMountStatus, Qt::UniqueConnection);
    }

    // Mount <---> Align connections
    if (mountProcess.get() && alignProcess.get())
    {
        connect(mountProcess.get(), &Ekos::Mount::newStatus, alignProcess.get(), &Ekos::Align::setMountStatus, Qt::UniqueConnection);
    }

    // Mount <---> Guide connections
    if (mountProcess.get() && guideProcess.get())
    {
        connect(mountProcess.get(), &Ekos::Mount::pierSideChanged, guideProcess.get(), &Ekos::Guide::setPierSide, Qt::UniqueConnection);
    }

    // Focus <---> Align connections
    if (focusProcess.get() && alignProcess.get())
    {
        connect(focusProcess.get(), &Ekos::Focus::newStatus, alignProcess.get(), &Ekos::Align::setFocusStatus, Qt::UniqueConnection);
    }

    // Align <--> EkosLive connections
    if (alignProcess.get() && ekosLiveClient.get())
    {
        alignProcess.get()->disconnect(ekosLiveClient.get()->message());
        alignProcess.get()->disconnect(ekosLiveClient.get()->media());

        connect(alignProcess.get(), &Ekos::Align::newStatus, ekosLiveClient.get()->message(), &EkosLive::Message::setAlignStatus);
        connect(alignProcess.get(), &Ekos::Align::newSolution, ekosLiveClient.get()->message(), &EkosLive::Message::setAlignSolution);
        connect(alignProcess.get(), &Ekos::Align::newPAHStage, ekosLiveClient.get()->message(), &EkosLive::Message::setPAHStage);
        connect(alignProcess.get(), &Ekos::Align::newPAHMessage, ekosLiveClient.get()->message(), &EkosLive::Message::setPAHMessage);
        connect(alignProcess.get(), &Ekos::Align::PAHEnabled, ekosLiveClient.get()->message(), &EkosLive::Message::setPAHEnabled);

        connect(alignProcess.get(), &Ekos::Align::newImage, [&](FITSView * view)
        {
            ekosLiveClient.get()->media()->sendPreviewImage(view, QString());
        });
        connect(alignProcess.get(), &Ekos::Align::newFrame, ekosLiveClient.get()->media(), &EkosLive::Media::sendUpdatedFrame);

        connect(alignProcess.get(), &Ekos::Align::polarResultUpdated, ekosLiveClient.get()->message(), &EkosLive::Message::setPolarResults);
        connect(alignProcess.get(), &Ekos::Align::settingsUpdated, ekosLiveClient.get()->message(), &EkosLive::Message::sendAlignSettings);

        connect(alignProcess.get(), &Ekos::Align::newCorrectionVector, ekosLiveClient.get()->media(), &EkosLive::Media::setCorrectionVector);
    }
}

void Manager::setEkosLiveConnected(bool enabled)
{
    ekosLiveClient.get()->setConnected(enabled);
}

void Manager::setEkosLiveConfig(bool onlineService, bool rememberCredentials, bool autoConnect)
{
    ekosLiveClient.get()->setConfig(onlineService, rememberCredentials, autoConnect);
}

void Manager::setEkosLiveUser(const QString &username, const QString &password)
{
    ekosLiveClient.get()->setUser(username, password);
}

bool Manager::ekosLiveStatus()
{
    return ekosLiveClient.get()->isConnected();
}

void Manager::syncActiveDevices()
{
    for (auto oneDevice : genericDevices)
    {
        // Find out what ACTIVE_DEVICES properties this driver needs
        // and update it from the the existing drivers.
        ITextVectorProperty *tvp = oneDevice->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (tvp)
        {
            bool propertyUpdated = false;

            for (int i = 0; i < tvp->ntp; i++)
            {
                QList<ISD::GDInterface *> devs;
                if (!strcmp(tvp->tp[i].name, "ACTIVE_TELESCOPE"))
                {
                    devs = findDevicesByInterface(INDI::BaseDevice::TELESCOPE_INTERFACE);
                }
                else if (!strcmp(tvp->tp[i].name, "ACTIVE_DOME"))
                {
                    devs = findDevicesByInterface(INDI::BaseDevice::DOME_INTERFACE);
                }
                else if (!strcmp(tvp->tp[i].name, "ACTIVE_GPS"))
                {
                    devs = findDevicesByInterface(INDI::BaseDevice::GPS_INTERFACE);
                }
                else if (!strcmp(tvp->tp[i].name, "ACTIVE_FILTER"))
                {
                    if (tvp->tp[i].aux0 != nullptr)
                    {
                        bool *override = static_cast<bool *>(tvp->tp[i].aux0);
                        if (override && *override)
                            continue;
                    }
                    devs = findDevicesByInterface(INDI::BaseDevice::FILTER_INTERFACE);
                }
                else if (!strcmp(tvp->tp[i].name, "ACTIVE_WEATHER"))
                {
                    devs = findDevicesByInterface(INDI::BaseDevice::WEATHER_INTERFACE);
                }

                if (!devs.empty())
                {
                    if (strcmp(tvp->tp[i].text, devs.first()->getDeviceName()))
                    {
                        propertyUpdated = true;
                        IUSaveText(&tvp->tp[i], devs.first()->getDeviceName());
                        oneDevice->getDriverInfo()->getClientManager()->sendNewText(tvp);
                    }
                }
            }

            // Save configuration
            if (propertyUpdated)
                oneDevice->setConfig(SAVE_CONFIG);
        }
    }
}

bool Manager::checkUniqueBinaryDriver(DriverInfo *primaryDriver, DriverInfo *secondaryDriver)
{
    if (!primaryDriver || !secondaryDriver)
        return false;

    return (primaryDriver->getExecutable() == secondaryDriver->getExecutable() &&
            primaryDriver->getAuxInfo().value("mdpd", false).toBool() == true);
}

}
