/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikartech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "manager.h"

#include "ekosadaptor.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "opsekos.h"
#include "Options.h"
#include "profileeditor.h"
#include "profilewizard.h"
#include "indihub.h"
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

#include <QFutureWatcher>
#include <QComboBox>

#include <ekos_debug.h>

#define MAX_REMOTE_INDI_TIMEOUT 15000
#define MAX_LOCAL_INDI_TIMEOUT  10000

namespace Ekos
{

Manager *Manager::_Manager = nullptr;

Manager *Manager::Instance()
{
    if (_Manager == nullptr)
        _Manager = new Manager(Options::independentWindowEkos() ? nullptr : KStars::Instance());

    return _Manager;
}

void Manager::release()
{
    delete _Manager;
}

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
    if (Options::independentWindowEkos())
        //setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        setWindowFlags(Qt::Window);
#endif
    setupUi(this);

    // position the vertical splitter by 2/3
    deviceSplitter->setSizes(QList<int>({20000, 10000}));

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

    countdownTimer.setInterval(1000);
    connect(&countdownTimer, &QTimer::timeout, this, &Ekos::Manager::updateCaptureCountDown);

    toolsWidget->setIconSize(QSize(48, 48));
    connect(toolsWidget, &QTabWidget::currentChanged, this, &Ekos::Manager::processTabChange, Qt::UniqueConnection);

    // Enable scheduler Tab
    toolsWidget->setTabEnabled(1, false);

    // Enable analyze Tab
    toolsWidget->setTabEnabled(2, false);

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
    connect(ekosLiveClient.get()->media(), &EkosLive::Media::newBoundingRect, ekosLiveClient.get()->message(),
            &EkosLive::Message::setBoundingRect);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::resetPolarView, ekosLiveClient.get()->media(),
            &EkosLive::Media::resetPolarView);
    connect(KSMessageBox::Instance(), &KSMessageBox::newMessage, ekosLiveClient.get()->message(),
            &EkosLive::Message::sendDialog);

    // Port Selector
    m_PortSelectorTimer.setInterval(500);
    m_PortSelectorTimer.setSingleShot(true);
    connect(&m_PortSelectorTimer, &QTimer::timeout, this, [this]()
    {
        if (m_PortSelector && currentProfile->portSelector)
        {
            if (m_PortSelector->shouldShow())
            {
                m_PortSelector->show();
                m_PortSelector->raise();

                ekosLiveClient.get()->message()->requestPortSelection(true);
            }
            // If port selector is enabled, but we have zero ports to work with, let's proceed to connecting if it is enabled.
            else if (currentProfile->autoConnect)
                setPortSelectionComplete();
        }
    });
    connect(portSelectorB, &QPushButton::clicked, [&]()
    {
        if (m_PortSelector)
        {
            m_PortSelector->show();
            m_PortSelector->raise();
        }
    });

    connect(this, &Ekos::Manager::ekosStatusChanged, [&](Ekos::CommunicationStatus status)
    {
        indiControlPanelB->setEnabled(status == Ekos::Success);
        connectB->setEnabled(false);
        disconnectB->setEnabled(false);
        profileGroup->setEnabled(status == Ekos::Idle || status == Ekos::Error);
        m_isStarted = (status == Ekos::Success || status == Ekos::Pending);
        if (status == Ekos::Success)
        {
            processINDIB->setIcon(QIcon::fromTheme("media-playback-stop"));
            processINDIB->setToolTip(i18n("Stop"));
            setWindowTitle(i18nc("@title:window", "Ekos - %1 Profile", currentProfile->name));
        }
        else if (status == Ekos::Error || status == Ekos::Idle)
        {
            processINDIB->setIcon(QIcon::fromTheme("media-playback-start"));
            processINDIB->setToolTip(i18n("Start"));
        }
        else
        {
            processINDIB->setIcon(QIcon::fromTheme("call-stop"));
            processINDIB->setToolTip(i18n("Connection in progress. Click to abort."));
        }
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

    // Settle timer
    // Debounce until property stream settles down for a second.
    settleTimer.setInterval(1000);
    connect(&settleTimer, &QTimer::timeout, [&]()
    {
        if (m_settleStatus != Ekos::Success)
        {
            m_settleStatus = Ekos::Success;
            emit settleStatusChanged(m_settleStatus);
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
    schedulerProcess.reset(new Scheduler());
    int index = addModuleTab(EkosModule::Scheduler, schedulerProcess.get(), QIcon(":/icons/ekos_scheduler.png"));
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Scheduler"));
    captureCountsWidget->shareSchedulerProcess(schedulerProcess.get());
    connect(schedulerProcess.get(), &Scheduler::newLog, this, &Ekos::Manager::updateLog);
    //connect(schedulerProcess.get(), SIGNAL(newTarget(QString)), mountTarget, SLOT(setText(QString)));
    connect(schedulerProcess.get(), &Ekos::Scheduler::newTarget, [&](const QString & target)
    {
        mountTarget->setText(target);
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", target}}));
    });

    // Initialize Ekos Analyze Module
    analyzeProcess.reset(new Ekos::Analyze());
    index = addModuleTab(EkosModule::Analyze, analyzeProcess.get(), QIcon(":/icons/ekos_analyze.png"));
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Analyze"));

    numPermanentTabs = index + 1;

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
    vlayout->setContentsMargins(0, 0, 0, 0);
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

        for (int i = 0; i < numPermanentTabs; ++i)
        {
            QIcon icon  = toolsWidget->tabIcon(i);
            QPixmap pix = icon.pixmap(QSize(48, 48));
            icon        = QIcon(pix.transformed(trans));
            toolsWidget->setTabIcon(i, icon);
        }
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
    focusManager->updateFocusDetailView();
    guideManager->updateGuideDetailView();
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

int Manager::addModuleTab(Manager::EkosModule module, QWidget *tab, const QIcon &icon)
{
    int index = 0;
    switch(module)
    {
        case EkosModule::Observatory:
            index += guideProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Guide:
            index += alignProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Align:
            index += mountProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Mount:
            index += focusProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Focus:
            index += captureProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Capture:
            index += analyzeProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Analyze:
            index += schedulerProcess ? 1 : 0; /* FALLTHRU */
        case EkosModule::Scheduler:
            index += 1; /* FALLTHRU */
        case EkosModule::Setup:
            // do nothing
            break;
        default:
            index = toolsWidget->count();
            break;
    }

    toolsWidget->insertTab(index, tab, icon, "");
    return index;
}

void Manager::loadDrivers()
{
    for (auto &dv : DriverManager::Instance()->getDrivers())
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

    if (proxyDevices.empty() == false)
    {
        qDeleteAll(proxyDevices);
        proxyDevices.clear();
    }

    captureProcess.reset();
    focusProcess.reset();
    guideProcess.reset();
    domeProcess.reset();
    alignProcess.reset();
    mountProcess.reset();
    weatherProcess.reset();
    observatoryProcess.reset();
    dustCapProcess.reset();

    DarkLibrary::Release();
    m_PortSelector.reset();
    m_PortSelectorTimer.stop();

    Ekos::CommunicationStatus previousStatus;

    previousStatus = m_settleStatus;
    m_settleStatus = Ekos::Idle;
    if (previousStatus != m_settleStatus)
        emit settleStatusChanged(m_settleStatus);

    previousStatus = m_ekosStatus;
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
    captureGroup->setEnabled(false);
    captureCountsWidget->reset();
    mountStatus->setText(i18n("Idle"));
    mountStatus->setStyleSheet(QString());
    captureStatus->setText(i18n("Idle"));
    if (capturePI)
        capturePI->stopAnimation();
    if (mountPI)
        mountPI->stopAnimation();
    focusManager->reset();
    guideManager->reset();

    m_isStarted = false;

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

void Manager::stop()
{
    cleanDevices();
    m_PortSelector.reset();
    m_PortSelectorTimer.stop();
    portSelectorB->setEnabled(false);

    if (indiHubAgent)
        indiHubAgent->terminate();

    profileGroup->setEnabled(true);

    setWindowTitle(i18nc("@title:window", "Ekos"));
}

void Manager::start()
{
    // Don't start if it is already started before
    if (m_ekosStatus == Ekos::Pending || m_ekosStatus == Ekos::Success)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Manager start called but current Ekos Status is" << m_ekosStatus << "Ignoring request.";
        return;
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

    // Reset Ekos Manager
    reset();

    // Get Current Profile
    currentProfile = getCurrentProfile();
    m_LocalMode      = currentProfile->isLocal();

    // Load profile location if one exists
    updateProfileLocation(currentProfile);

    bool haveCCD = false, haveGuider = false;

    // If external guide is specified in the profile, set the
    // corresponding options
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

    // For locally running INDI server
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
                QString name, label, host("localhost"), port("7624"), hostport(host + ':' + port);

                // Possible configurations:
                // - device
                // - device@host
                // - device@host:port
                // - @host
                // - @host:port

                {
                    QStringList device_location = remoteDriver.split('@');

                    // device or device@host:port
                    if (device_location.length() > 0)
                        name = device_location[0];

                    // device@host:port or @host:port
                    if (device_location.length() > 1)
                        hostport = device_location[1];
                }

                {
                    QStringList location = hostport.split(':');

                    // host or host:port
                    if (location.length() > 0)
                        host = location[0];

                    // host:port
                    if (location.length() > 1)
                        port = location[1];
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


        if (haveCCD == false && haveGuider == false && currentProfile->remotedrivers.isEmpty())
        {
            KSNotification::error(i18n("Ekos requires at least one CCD or Guider to operate."));
            managedDrivers.clear();
            m_ekosStatus = Ekos::Error;
            emit ekosStatusChanged(m_ekosStatus);
            return;
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

        if (haveCCD == false && haveGuider == false && currentProfile->remotedrivers.isEmpty())
        {
            KSNotification::error(i18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices = 0;
            m_ekosStatus = Ekos::Error;
            emit ekosStatusChanged(m_ekosStatus);
            return;
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
        auto executeStartINDIServices = [this]()
        {
            appendLogText(i18n("Starting INDI services..."));

            connect(DriverManager::Instance(), &DriverManager::serverStarted, this,
                    &Manager::processServerStarted, Qt::UniqueConnection);
            connect(DriverManager::Instance(), &DriverManager::serverTerminated, this,
                    &Manager::processServerTerminated, Qt::UniqueConnection);

            if (DriverManager::Instance()->startDevices(managedDrivers) == false)
            {
                INDIListener::Instance()->disconnect(this);
                qDeleteAll(managedDrivers);
                managedDrivers.clear();
                m_ekosStatus = Ekos::Error;
                emit ekosStatusChanged(m_ekosStatus);
            }



            m_ekosStatus = Ekos::Pending;
            emit ekosStatusChanged(m_ekosStatus);

            if (managedDrivers.size() > 0)
            {
                if (currentProfile->autoConnect)
                    appendLogText(i18n("INDI services started on port %1.", managedDrivers.first()->getPort()));
                else
                    appendLogText(
                        i18n("INDI services started on port %1. Please connect devices.", managedDrivers.first()->getPort()));
            }

            QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, &Ekos::Manager::checkINDITimeout);
        };

        // If INDI server is already running, let's see if we need to shut it down first
        if (isRunning("indiserver"))
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executeStartINDIServices]()
            {
                KSMessageBox::Instance()->disconnect(this);
                DriverManager::Instance()->stopAllDevices();
                //TODO is there a better way to do this.
                QProcess p;
                p.start("pkill indiserver");
                p.waitForFinished();

                QTimer::singleShot(500, executeStartINDIServices);
            });
            connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this, executeStartINDIServices]()
            {
                KSMessageBox::Instance()->disconnect(this);
                executeStartINDIServices();
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Ekos detected an instance of INDI server running. Do you wish to "
                                                    "shut down the existing instance before starting a new one?"),
                                                    i18n("INDI Server"), 5);
        }
        else
            executeStartINDIServices();

    }
    else
    {
        auto runConnection = [this]()
        {
            // If it got cancelled by the user, return immediately.
            if (m_ekosStatus != Ekos::Pending)
                return;

            appendLogText(
                i18n("Connecting to remote INDI server at %1 on port %2 ...", currentProfile->host, currentProfile->port));

            if (DriverManager::Instance()->connectRemoteHost(managedDrivers.first()))
            {
                connect(DriverManager::Instance(), &DriverManager::serverTerminated, this,
                        &Manager::processServerTerminated, Qt::UniqueConnection);

                appendLogText(
                    i18n("INDI services started. Connection to remote INDI server is successful. Waiting for devices..."));

                QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, &Ekos::Manager::checkINDITimeout);
            }
            else
            {
                appendLogText(i18n("Failed to connect to remote INDI server."));
                INDIListener::Instance()->disconnect(this);
                qDeleteAll(managedDrivers);
                managedDrivers.clear();
                m_ekosStatus = Ekos::Error;
                emit ekosStatusChanged(m_ekosStatus);
                return;
            }
        };

        auto runProfile = [this, runConnection]()
        {
            // If it got cancelled by the user, return immediately.
            if (m_ekosStatus != Ekos::Pending)
                return;

            INDI::WebManager::syncCustomDrivers(currentProfile);
            currentProfile->isStellarMate = INDI::WebManager::isStellarMate(currentProfile);

            if (INDI::WebManager::areDriversRunning(currentProfile) == false)
            {
                INDI::WebManager::stopProfile(currentProfile);

                if (INDI::WebManager::startProfile(currentProfile) == false)
                {
                    appendLogText(i18n("Failed to start profile on remote INDI Web Manager."));
                    return;
                }

                appendLogText(i18n("Starting profile on remote INDI Web Manager..."));
                m_RemoteManagerStart = true;
            }

            runConnection();
        };

        m_ekosStatus = Ekos::Pending;
        emit ekosStatusChanged(m_ekosStatus);

        // If we need to use INDI Web Manager
        if (currentProfile->INDIWebManagerPort > 0)
        {
            appendLogText(i18n("Establishing communication with remote INDI Web Manager..."));
            m_RemoteManagerStart = false;
            QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
            connect(watcher, &QFutureWatcher<bool>::finished, [this, runConnection, runProfile, watcher]()
            {
                watcher->deleteLater();

                // If it got cancelled by the user, return immediately.
                if (m_ekosStatus != Ekos::Pending)
                    return;

                // If web manager is online, try to run the profile in it
                if (watcher->result())
                {
                    runProfile();
                }
                // Else, try to connect directly to INDI server as there could be a chance
                // that it is already running.
                else
                {
                    appendLogText(i18n("Warning: INDI Web Manager is not online."));
                    runConnection();
                }

            });

            QFuture<bool> result = INDI::AsyncWebManager::isOnline(currentProfile);
            watcher->setFuture(result);
        }
        else
        {
            runConnection();
        }
    }
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
        for (auto &drv : managedDrivers)
        {
            if (drv->getDevices().count() == 0)
                remainingDevices << QString("+ %1").arg(
                                     drv->getUniqueLabel().isEmpty() == false ? drv->getUniqueLabel() : drv->getName());
        }

        if (remainingDevices.count() == 1)
        {
            QString message = i18n("Unable to establish:\n%1\nPlease ensure the device is connected and powered on.",
                                   remainingDevices.at(0));
            appendLogText(message);
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::EVENT_WARN);
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            QString message = i18n("Unable to establish the following devices:\n%1\nPlease ensure each device is connected "
                                   "and powered on.", remainingDevices.join("\n"));
            appendLogText(message);
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::EVENT_WARN);
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

void Manager::processServerTerminated(const QString &host, const QString &port)
{
    if ((m_LocalMode && managedDrivers.first()->getPort() == port) ||
            (currentProfile->host == host && currentProfile->port == port.toInt()))
    {
        cleanDevices(false);
        if (indiHubAgent)
            indiHubAgent->terminate();
    }
}

void Manager::processServerStarted(const QString &host, const QString &port)
{
    if (m_LocalMode && currentProfile->indihub != INDIHub::None)
    {
        if (QFile(Options::iNDIHubAgent()).exists())
        {
            indiHubAgent = new QProcess();
            QStringList args;

            args << "--indi-server" << QString("%1:%2").arg(host).arg(port);
            if (currentProfile->guidertype == Ekos::Guide::GUIDE_PHD2)
                args << "--phd2-server" << QString("%1:%2").arg(currentProfile->guiderhost).arg(currentProfile->guiderport);
            args << "--mode" << INDIHub::toString(currentProfile->indihub);
            indiHubAgent->start(Options::iNDIHubAgent(), args);

            qCDebug(KSTARS_EKOS) << "Started INDIHub agent.";
        }
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
            {
                DriverManager::Instance()->disconnectRemoteHost(managedDrivers.first());

                if (m_RemoteManagerStart && currentProfile->INDIWebManagerPort != -1)
                    INDI::WebManager::stopProfile(currentProfile);
            }
            m_RemoteManagerStart = false;
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
        if (device->getDeviceName() == devInterface->getDeviceName())
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
    connect(devInterface, &ISD::GDInterface::propertyDeleted, this, &Ekos::Manager::processDeleteProperty);
    connect(devInterface, &ISD::GDInterface::interfaceDefined, this, &Ekos::Manager::syncActiveDevices);

    connect(devInterface, &ISD::GDInterface::numberUpdated, this, &Ekos::Manager::processNewNumber);
    connect(devInterface, &ISD::GDInterface::switchUpdated, this, &Ekos::Manager::processNewSwitch);
    connect(devInterface, &ISD::GDInterface::textUpdated, this, &Ekos::Manager::processNewText);
    connect(devInterface, &ISD::GDInterface::lightUpdated, this, &Ekos::Manager::processNewLight);
    connect(devInterface, &ISD::GDInterface::BLOBUpdated, this, &Ekos::Manager::processNewBLOB);

    if (nDevices <= 0)
    {
        m_ekosStatus = Ekos::Success;
        emit ekosStatusChanged(m_ekosStatus);

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);

        if (m_LocalMode == false && nDevices == 0)
        {
            if (currentProfile->autoConnect)
                appendLogText(i18n("Remote devices established."));
            else
                appendLogText(i18n("Remote devices established. Please connect devices."));
        }

        if (!m_PortSelector)
        {
            portSelectorB->setEnabled(false);
            m_PortSelector.reset(new Selector::Dialog(KStars::Instance()));
            connect(m_PortSelector.get(), &Selector::Dialog::accepted, this, &Manager::setPortSelectionComplete);
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

    for (auto &device : genericDevices)
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

    if (dev->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        if (mountProcess.get() != nullptr)
        {
            mountProcess->setEnabled(true);
            if (alignProcess.get() != nullptr)
                alignProcess->setEnabled(true);
        }
    }
    else if (dev->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess.get() != nullptr)
        {
            captureProcess->setEnabled(true);
            captureCountsWidget->setEnabled(true);
        }
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
    else if (dev->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess.get() != nullptr)
            focusProcess->setEnabled(true);
    }

    if (Options::neverLoadConfig())
        return;

    INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

    for (auto &device : genericDevices)
    {
        if (device == dev)
        {
            connect(dev, &ISD::GDInterface::switchUpdated, this, &Ekos::Manager::watchDebugProperty);

            auto configProp = device->getBaseDevice()->getSwitch("CONFIG_PROCESS");
            if (configProp && configProp->getState() == IPS_IDLE)
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

        // In case a device fails to connect, display and log a useful message for the user.
        if (m_indiStatus == Ekos::Error)
        {
            QString message = i18n("%1 failed to connect.\nPlease ensure the device is connected and powered on.",
                                   dev->getDeviceName());
            appendLogText(message);
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::EVENT_WARN);
        }
        else if (m_indiStatus == Ekos::Idle)
        {
            QString message = i18n("%1 is disconnected.", dev->getDeviceName());
            appendLogText(message);
        }
    }
    else
        m_indiStatus = Ekos::Idle;

    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);
    processINDIB->setEnabled(true);

    if (dev != nullptr && dev->getBaseDevice() &&
            (dev->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE))
    {
        if (mountProcess.get() != nullptr)
            mountProcess->setEnabled(false);
    }
}

void Manager::setTelescope(ISD::GDInterface * scopeDevice)
{
    //mount = scopeDevice;

    managedDevices[KSTARS_TELESCOPE] = scopeDevice;

    appendLogText(i18n("%1 is online.", scopeDevice->getDeviceName()));

    //    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
    //            SLOT(processNewNumber(INumberVectorProperty *)), Qt::UniqueConnection);

    initMount();

    mountProcess->setTelescope(scopeDevice);

    double primaryScopeFL = 0, primaryScopeAperture = 0, guideScopeFL = 0, guideScopeAperture = 0;
    getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    // Save telescope info in mount driver
    mountProcess->setTelescopeInfo(QList<double>() << primaryScopeFL << primaryScopeAperture << guideScopeFL <<
                                   guideScopeAperture);

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
    captureCountsWidget->setEnabled(true);
    captureProcess->addCCD(ccdDevice);

    QString primaryCCD, guiderCCD;

    // Only look for primary & guider CCDs if we can tell a difference between them
    // otherwise rely on saved options
    if (currentProfile->ccd() != currentProfile->guider())
    {
        for (auto &device : findDevices(KSTARS_CCD))
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
    if (dynamic_cast<ISD::CCD*>(ccdDevice)->hasCooler())
        focusProcess->addTemperatureSource(ccdDevice);

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

    //    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
    //            SLOT(processNewNumber(INumberVectorProperty *)), Qt::UniqueConnection);

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

    //    connect(filterDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this,
    //            SLOT(processNewNumber(INumberVectorProperty *)), Qt::UniqueConnection);
    //    connect(filterDevice, SIGNAL(textUpdated(ITextVectorProperty *)), this,
    //            SLOT(processNewText(ITextVectorProperty *)), Qt::UniqueConnection);

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

    focusProcess->addTemperatureSource(focuserDevice);

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

    if (weatherDevice->getType() != KSTARS_WEATHER)
    {
        ISD::Weather *source = new ISD::Weather(weatherDevice);
        weatherProcess->setWeather(source);
        proxyDevices.append(source);
    }
    else
        weatherProcess->setWeather(weatherDevice);

    if (focusProcess)
    {
        focusProcess->addTemperatureSource(weatherDevice);
    }

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

    //DarkLibrary::Instance()->setRemoteCap(dustCapDevice);

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
    }

    if (devInterface->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
        DarkLibrary::Instance()->removeCamera(devInterface);

    appendLogText(i18n("%1 is offline.", devInterface->getDeviceName()));

    // #1 Remove from Generic Devices
    // Generic devices are ALL the devices we receive from INDI server
    // Whether Ekos cares about them (i.e. selected equipment) or extra devices we
    // do not care about
    for (auto &device : genericDevices)
    {
        if (device->getDeviceName() == devInterface->getDeviceName())
        {
            genericDevices.removeOne(device);
        }
    }

    // #2 Remove from Proxy device
    // Proxy devices are ALL the devices we specifically create in Ekos Manager to handle specific interfaces
    for (auto &device : proxyDevices)
    {
        if (device->getDeviceName() == devInterface->getDeviceName())
        {
            proxyDevices.removeOne(device);
            device->deleteLater();
        }
    }

    // #3 Remove from Ekos Managed Device
    // Managed devices are devices selected by the user in the device profile
    for (auto it = managedDevices.begin(); it != managedDevices.end();)
    {
        if (it.value()->getDeviceName() == devInterface->getDeviceName())
        {
            it = managedDevices.erase(it);
        }
        else
            ++it;
    }

    if (managedDevices.isEmpty() && genericDevices.isEmpty() && proxyDevices.isEmpty())
    {
        cleanDevices();
        removeTabs();
    }
}

void Manager::processNewText(ITextVectorProperty * tvp)
{
    ekosLiveClient.get()->message()->processNewText(tvp);

    if (!strcmp(tvp->name, "FILTER_NAME"))
    {
        ekosLiveClient.get()->message()->sendFilterWheels();
    }

    if (!strcmp(tvp->name, "ACTIVE_DEVICES"))
    {
        syncActiveDevices();
    }
}

void Manager::processNewSwitch(ISwitchVectorProperty * svp)
{
    ekosLiveClient.get()->message()->processNewSwitch(svp);
}

void Manager::processNewLight(ILightVectorProperty * lvp)
{
    ekosLiveClient.get()->message()->processNewLight(lvp);
}

void Manager::processNewBLOB(IBLOB * bp)
{
    ekosLiveClient.get()->media()->processNewBLOB(bp);
}

void Manager::processNewNumber(INumberVectorProperty * nvp)
{
    ekosLiveClient.get()->message()->processNewNumber(nvp);

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

void Manager::processDeleteProperty(const QString &name)
{
    ISD::GenericDevice * deviceInterface = qobject_cast<ISD::GenericDevice *>(sender());
    ekosLiveClient.get()->message()->processDeleteProperty(deviceInterface->getDeviceName(), name);
}

void Manager::processNewProperty(INDI::Property prop)
{
    ISD::GenericDevice * deviceInterface = qobject_cast<ISD::GenericDevice *>(sender());

    settleTimer.start();

    ekosLiveClient.get()->message()->processNewProperty(prop);

    if (prop->isNameMatch("CONNECTION") && currentProfile->autoConnect && currentProfile->portSelector == false)
    {
        deviceInterface->Connect();
        return;
    }

    if (prop->isNameMatch("DEVICE_PORT_SCAN") || prop->isNameMatch("CONNECTION_TYPE"))
    {
        if (!m_PortSelector)
        {
            m_PortSelector.reset(new Selector::Dialog(KStars::Instance()));
            connect(m_PortSelector.get(), &Selector::Dialog::accepted, this, &Manager::setPortSelectionComplete);
        }
        m_PortSelectorTimer.start();
        portSelectorB->setEnabled(true);
        m_PortSelector->addDevice(deviceInterface);
    }

    // Check if we need to turn on DEBUG for logging purposes
    if (prop->isNameMatch("DEBUG"))
    {
        uint16_t interface = deviceInterface->getDriverInterface();
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Check if we need to enable debug logging for the INDI drivers.
            auto debugSP = prop->getSwitch();
            debugSP->at(0)->setState(ISS_ON);
            debugSP->at(1)->setState(ISS_OFF);
            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(debugSP);
        }
    }

    // Handle debug levels for logging purposes
    if (prop->isNameMatch("DEBUG_LEVEL"))
    {
        uint16_t interface = deviceInterface->getDriverInterface();
        // Check if the logging option for the specific device class is on and if the device interface matches it.
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Turn on everything
            auto debugLevel = prop->getSwitch();
            for (auto &it : *debugLevel)
                it.setState(ISS_ON);

            deviceInterface->getDriverInfo()->getClientManager()->sendNewSwitch(debugLevel);
        }
    }

    if (prop->isNameMatch("ACTIVE_DEVICES"))
    {
        if (deviceInterface->getDriverInterface() > 0)
            syncActiveDevices();
    }

    if (prop->isNameMatch("TELESCOPE_INFO") || prop->isNameMatch("TELESCOPE_SLEW_RATE")
            || prop->isNameMatch("TELESCOPE_PARK"))
    {
        ekosLiveClient.get()->message()->sendMounts();
        ekosLiveClient.get()->message()->sendScopes();
    }

    if (prop->isNameMatch("CCD_INFO") || prop->isNameMatch("CCD_TEMPERATURE")
            || prop->isNameMatch("CCD_ISO") ||
            prop->isNameMatch("CCD_GAIN") || prop->isNameMatch("CCD_CONTROLS"))
    {
        ekosLiveClient.get()->message()->sendCameras();
        ekosLiveClient.get()->media()->registerCameras();
    }

    if (prop->isNameMatch("CCD_TEMPERATURE") || prop->isNameMatch("FOCUSER_TEMPERATURE")
            || prop->isNameMatch("WEATHER_PARAMETERS"))
    {
        if (focusProcess)
        {
            focusProcess->addTemperatureSource(deviceInterface);
        }
    }

    if (prop->isNameMatch("ABS_DOME_POSITION") || prop->isNameMatch("DOME_ABORT_MOTION") ||
            prop->isNameMatch("DOME_PARK"))
    {
        ekosLiveClient.get()->message()->sendDomes();
    }

    if (prop->isNameMatch("CAP_PARK") || prop->isNameMatch("FLAT_LIGHT_CONTROL"))
    {
        ekosLiveClient.get()->message()->sendCaps();
    }

    if (prop->isNameMatch("FILTER_NAME"))
        ekosLiveClient.get()->message()->sendFilterWheels();

    if (prop->isNameMatch("FILTER_NAME"))
        filterManager.data()->initFilterProperties();

    if (prop->isNameMatch("CONFIRM_FILTER_SET"))
        filterManager.data()->initFilterProperties();

    if (prop->isNameMatch("CCD_INFO") || prop->isNameMatch("GUIDER_INFO"))
    {
        if (focusProcess.get() != nullptr)
            focusProcess->syncCCDInfo();

        if (guideProcess.get() != nullptr)
            guideProcess->syncCCDInfo();

        if (alignProcess.get() != nullptr)
            alignProcess->syncCCDInfo();

        return;
    }

    if (prop->isNameMatch("TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
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

    if (prop->isNameMatch("GUIDER_EXPOSURE"))
    {
        for (auto &device : findDevices(KSTARS_CCD))
        {
            if (device->getDeviceName() == prop->getDeviceName())
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

    if (prop->isNameMatch("CCD_FRAME_TYPE"))
    {
        if (captureProcess.get() != nullptr)
        {
            for (auto &device : findDevices(KSTARS_CCD))
            {
                if (device->getDeviceName() == prop->getDeviceName())
                {
                    captureProcess->syncFrameType(device);
                    return;
                }
            }
        }

        return;
    }

    if (prop->isNameMatch("CCD_ISO"))
    {
        if (captureProcess.get() != nullptr)
            captureProcess->checkCCD();

        return;
    }

    if (prop->isNameMatch("TELESCOPE_PARK") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (captureProcess.get() != nullptr)
            captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        if (mountProcess.get() != nullptr)
            mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

        return;
    }

    /*
    if (prop->isNameMatch("FILTER_NAME"))
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

    if (prop->isNameMatch("ASTROMETRY_SOLVER"))
    {
        for (auto &device : genericDevices)
        {
            if (device->getDeviceName() == prop->getDeviceName())
            {
                initAlign();
                alignProcess->setAstrometryDevice(device);
                break;
            }
        }
    }

    if (prop->isNameMatch("ABS_ROTATOR_ANGLE"))
    {
        managedDevices[KSTARS_ROTATOR] = deviceInterface;
        if (captureProcess.get() != nullptr)
            captureProcess->setRotator(deviceInterface);
        if (alignProcess.get() != nullptr)
            alignProcess->setRotator(deviceInterface);
    }

    if (prop->isNameMatch("GPS_REFRESH"))
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

const QList<ISD::GDInterface *> &Manager::getAllDevices() const
{
    return genericDevices;
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
                /** else if (Options::solverBackend() == Ekos::Align::SOLVER_ASTROMETRYNET)
                 {
                     // If current setting is remote astrometry and profile doesn't contain
                     // remote astrometry, then we switch to online solver. Otherwise, the whole align
                     // module remains disabled and the user cannot change re-enable it since he cannot select online solver
                     ProfileInfo * pi = getCurrentProfile();
                     if (Options::astrometrySolverType() == Ekos::Align::SOLVER_REMOTE && pi->aux1() != "Astrometry"
                             && pi->aux2() != "Astrometry"
                             && pi->aux3() != "Astrometry" && pi->aux4() != "Astrometry")
                     {

                         Options::setAstrometrySolverType(Ekos::Align::SOLVER_ONLINE);
                         //alignModule()->setAstrometrySolverType(Ekos::Align::SOLVER_ONLINE);
                         alignProcess->setEnabled(true);
                     }
                 }
                 **/
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

    captureProcess.reset(new Capture());
    emit newModule("Capture");


    captureProcess->setEnabled(false);
    captureCountsWidget->shareCaptureProcess(captureProcess.get());
    int index = addModuleTab(EkosModule::Capture, captureProcess.get(), QIcon(":/icons/ekos_ccd.png"));
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
    connect(captureProcess.get(), &Ekos::Capture::driverTimedout, this, &Ekos::Manager::restartDriver);
    connect(captureProcess.get(), &Ekos::Capture::newExposureProgress, this, &Ekos::Manager::updateExposureProgress);
    captureGroup->setEnabled(true);
    captureCountsWidget->setEnabled(true);

    captureProcess->setFilterManager(filterManager);

    for (auto &device : findDevices(KSTARS_AUXILIARY))
    {
        if (device->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
            captureProcess->setDustCap(device);
        if (device->getDriverInterface() & INDI::BaseDevice::LIGHTBOX_INTERFACE)
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
}

void Manager::initAlign()
{
    if (alignProcess.get() != nullptr)
        return;

    alignProcess.reset(new Ekos::Align(currentProfile));

    emit newModule("Align");

    double primaryScopeFL = 0, primaryScopeAperture = 0, guideScopeFL = 0, guideScopeAperture = 0;
    getCurrentProfileTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);
    alignProcess->setTelescopeInfo(primaryScopeFL, primaryScopeAperture, guideScopeFL, guideScopeAperture);

    alignProcess->setEnabled(false);
    int index = addModuleTab(EkosModule::Align, alignProcess.get(), QIcon(":/icons/ekos_align.png"));
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
}

void Manager::initFocus()
{
    if (focusProcess.get() != nullptr)
        return;

    focusProcess.reset(new Ekos::Focus());

    emit newModule("Focus");

    int index    = addModuleTab(EkosModule::Focus, focusProcess.get(), QIcon(":/icons/ekos_focus.png"));

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Focus"));

    // Focus <---> Manager connections
    connect(focusProcess.get(), &Ekos::Focus::newLog, this, &Ekos::Manager::updateLog);
    connect(focusProcess.get(), &Ekos::Focus::newStatus, this, &Ekos::Manager::updateFocusStatus);
    connect(focusProcess.get(), &Ekos::Focus::newStarPixmap, focusManager, &Ekos::FocusManager::updateFocusStarPixmap);
    connect(focusProcess.get(), &Ekos::Focus::newHFR, this, &Ekos::Manager::updateCurrentHFR);

    // connect HFR plot widget
    connect(focusProcess.get(), &Ekos::Focus::initHFRPlot, focusManager->hfrVPlot, &FocusHFRVPlot::init);
    connect(focusProcess.get(), &Ekos::Focus::redrawHFRPlot, focusManager->hfrVPlot, &FocusHFRVPlot::redraw);
    connect(focusProcess.get(), &Ekos::Focus::newHFRPlotPosition, focusManager->hfrVPlot, &FocusHFRVPlot::addPosition);
    connect(focusProcess.get(), &Ekos::Focus::drawPolynomial, focusManager->hfrVPlot, &FocusHFRVPlot::drawPolynomial);
    connect(focusProcess.get(), &Ekos::Focus::minimumFound, focusManager->hfrVPlot, &FocusHFRVPlot::drawMinimum);


    // Focus <---> Filter Manager connections
    focusProcess->setFilterManager(filterManager);
    connect(filterManager.data(), &Ekos::FilterManager::checkFocus, focusProcess.get(), &Ekos::Focus::checkFocus,
            Qt::UniqueConnection);
    connect(focusProcess.get(), &Ekos::Focus::newStatus, filterManager.data(), &Ekos::FilterManager::setFocusStatus,
            Qt::UniqueConnection);
    connect(filterManager.data(), &Ekos::FilterManager::newFocusOffset, focusProcess.get(), &Ekos::Focus::adjustFocusOffset,
            Qt::UniqueConnection);
    connect(focusProcess.get(), &Ekos::Focus::focusPositionAdjusted, filterManager.data(),
            &Ekos::FilterManager::setFocusOffsetComplete, Qt::UniqueConnection);
    connect(focusProcess.get(), &Ekos::Focus::absolutePositionChanged, filterManager.data(),
            &Ekos::FilterManager::setFocusAbsolutePosition, Qt::UniqueConnection);

    if (Options::ekosLeftIcons())
    {
        QTransform trans;
        trans.rotate(90);
        QIcon icon  = toolsWidget->tabIcon(index);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(index, icon);
    }

    focusManager->init(focusProcess.get());
    focusManager->setEnabled(true);

    // Check for weather device to snoop temperature
    for (auto &device : findDevices(KSTARS_WEATHER))
    {
        focusProcess->addTemperatureSource(device);
    }

    connectModules();
}

void Manager::updateCurrentHFR(double newHFR, int position)
{
    focusManager->updateCurrentHFR(newHFR);

    QJsonObject cStatus =
    {
        {"hfr", newHFR},
        {"pos", position}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
}

void Manager::updateSigmas(double ra, double de)
{
    guideManager->updateSigmas(ra, de);

    QJsonObject cStatus = { {"rarms", ra}, {"derms", de} };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
}

void Manager::initMount()
{
    if (mountProcess.get() != nullptr)
        return;

    mountProcess.reset(new Ekos::Mount());

    emit newModule("Mount");

    int index    = addModuleTab(EkosModule::Mount, mountProcess.get(), QIcon(":/icons/ekos_mount.png"));

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Mount"));
    connect(mountProcess.get(), &Ekos::Mount::newLog, this, &Ekos::Manager::updateLog);
    connect(mountProcess.get(), &Ekos::Mount::newCoords, this, &Ekos::Manager::updateMountCoords);
    connect(mountProcess.get(), &Ekos::Mount::newStatus, this, &Ekos::Manager::updateMountStatus);
    connect(mountProcess.get(), &Ekos::Mount::newTarget, [&](const QString & target)
    {
        mountTarget->setText(target);
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", target}}));
    });
    connect(mountProcess.get(), &Ekos::Mount::pierSideChanged, [&](ISD::Telescope::PierSide side)
    {
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"pierSide", side}}));
    });
    connect(mountProcess.get(), &Ekos::Mount::newMeridianFlipStatus, [&](Mount::MeridianFlipStatus status)
    {
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject(
        {
            {"meridianFlipStatus", status},
        }));
    });
    connect(mountProcess.get(), &Ekos::Mount::newMeridianFlipText, [&](const QString & text)
    {
        // Throttle this down
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject(
        {
            {"meridianFlipText", text},
        }), mountProcess->meridianFlipStatus() == Mount::FLIP_NONE);
    });


    connect(mountProcess.get(), &Ekos::Mount::slewRateChanged, [&](int slewRate)
    {
        QJsonObject status = { { "slewRate", slewRate} };
        ekosLiveClient.get()->message()->updateMountStatus(status);
    });

    for (auto &device : findDevices(KSTARS_AUXILIARY))
    {
        if (device->getDriverInterface() & INDI::BaseDevice::GPS_INTERFACE)
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

    mountGroup->setEnabled(true);

    connectModules();
}

void Manager::initGuide()
{
    if (guideProcess.get() == nullptr)
    {
        guideProcess.reset(new Ekos::Guide());

        emit newModule("Guide");

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

        int index = addModuleTab(EkosModule::Guide, guideProcess.get(), QIcon(":/icons/ekos_guide.png"));
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Guide"));
        connect(guideProcess.get(), &Ekos::Guide::newLog, this, &Ekos::Manager::updateLog);
        connect(guideProcess.get(), &Ekos::Guide::driverTimedout, this, &Ekos::Manager::restartDriver);

        guideManager->setEnabled(true);

        connect(guideProcess.get(), &Ekos::Guide::newStatus, this, &Ekos::Manager::updateGuideStatus);
        connect(guideProcess.get(), &Ekos::Guide::newStarPixmap, guideManager, &Ekos::GuideManager::updateGuideStarPixmap);
        connect(guideProcess.get(), &Ekos::Guide::newAxisSigma, this, &Ekos::Manager::updateSigmas);
        connect(guideProcess.get(), &Ekos::Guide::newAxisDelta, [&](double ra, double de)
        {
            QJsonObject status = { { "drift_ra", ra}, {"drift_de", de} };
            ekosLiveClient.get()->message()->updateGuideStatus(status);
        });

        if (Options::ekosLeftIcons())
        {
            QTransform trans;
            trans.rotate(90);
            QIcon icon  = toolsWidget->tabIcon(index);
            QPixmap pix = icon.pixmap(QSize(48, 48));
            icon        = QIcon(pix.transformed(trans));
            toolsWidget->setTabIcon(index, icon);
        }
        guideManager->init(guideProcess.get());
    }

    connectModules();
}

void Manager::initDome()
{
    if (domeProcess.get() != nullptr)
        return;

    domeProcess.reset(new Ekos::Dome());

    emit newModule("Dome");

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
    ekosLiveClient->message()->sendDomes();
}

void Manager::initWeather()
{
    if (weatherProcess.get() != nullptr)
        return;

    weatherProcess.reset(new Ekos::Weather());
    emit newModule("Weather");

    initObservatory(weatherProcess.get(), nullptr);
}

void Manager::initObservatory(Weather *weather, Dome *dome)
{
    if (observatoryProcess.get() == nullptr)
    {
        // Initialize the Observatory Module
        observatoryProcess.reset(new Ekos::Observatory());

        emit newModule("Observatory");

        int index = addModuleTab(EkosModule::Observatory, observatoryProcess.get(), QIcon(":/icons/ekos_observatory.png"));
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Observatory"));
        connect(observatoryProcess.get(), &Ekos::Observatory::newLog, this, &Ekos::Manager::updateLog);
    }

    Observatory *obs = observatoryProcess.get();
    if (weather != nullptr)
        obs->getWeatherModel()->initModel(weather);
    if (dome != nullptr)
        obs->getDomeModel()->initModel(dome);

}

void Manager::initDustCap()
{
    if (dustCapProcess.get() != nullptr)
        return;

    dustCapProcess.reset(new Ekos::DustCap());

    emit newModule("DustCap");

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

    for (int i = numPermanentTabs; i < toolsWidget->count(); i++)
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

void Manager::updateMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt,
                                int pierSide, const QString &ha)
{
    Q_UNUSED(pierSide);
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
        {"ha", dms::fromString(ha, false).Degrees()},
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus);
}

void Manager::updateCaptureStatus(Ekos::CaptureState status)
{
    captureStatus->setText(Ekos::getCaptureStatusString(status));
    captureCountsWidget->updateCaptureStatus(status);

    switch (status)
    {
        case Ekos::CAPTURE_IDLE:
        /* Fall through */
        case Ekos::CAPTURE_ABORTED:
        /* Fall through */
        case Ekos::CAPTURE_COMPLETE:
            if (capturePI->isAnimated())
            {
                capturePI->stopAnimation();
                countdownTimer.stop();

                if (getFocusStatusText() == "Complete")
                    focusManager->stopAnimation();
            }
            break;
        default:
            if (status == Ekos::CAPTURE_CAPTURING)
                capturePI->setColor(Qt::darkGreen);
            else
                capturePI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
            if (capturePI->isAnimated() == false)
            {
                capturePI->startAnimation();
                countdownTimer.start();
            }
            break;
    }

    QJsonObject cStatus =
    {
        {"status", captureStatus->text()},
        {"seqt", captureCountsWidget->sequenceRemainingTime->text()},
        {"ovt", captureCountsWidget->overallRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(cStatus);
}

void Manager::updateCaptureProgress(Ekos::SequenceJob * job, const QSharedPointer<FITSData> &data)
{
    captureCountsWidget->updateCaptureProgress(job);

    QJsonObject status =
    {
        {"seqv", job->getCompleted()},
        {"seqr", job->getCount()},
        {"seql", captureCountsWidget->sequenceLabel->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);

    //const QString filename = ;
    //if (!filename.isEmpty() && job->getStatus() == SequenceJob::JOB_BUSY)
    if (data && job->getStatus() == SequenceJob::JOB_BUSY)
    {
        QString uuid = QUuid::createUuid().toString();
        uuid = uuid.remove(QRegularExpression("[-{}]"));

        if (Options::useSummaryPreview())
        {
            summaryPreview->loadData(data);
            ekosLiveClient.get()->media()->sendPreviewImage(summaryPreview.get(), uuid);
        }
        else
            ekosLiveClient.get()->media()->sendPreviewImage(data, uuid);

        if (job->isPreview() == false)
            ekosLiveClient.get()->cloud()->upload(data, uuid);

    }
}

void Manager::updateExposureProgress(Ekos::SequenceJob * job)
{
    QJsonObject status
    {
        {"expv", job->getExposeLeft()},
        {"expr", job->getExposure()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void Manager::updateCaptureCountDown()
{
    captureCountsWidget->updateCaptureCountDown(-1);

    QJsonObject status =
    {
        {"seqt", captureCountsWidget->sequenceRemainingTime->text()},
        {"ovt", captureCountsWidget->overallRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}


void Manager::updateFocusStatus(Ekos::FocusState status)
{
    focusManager->updateFocusStatus(status);

    QJsonObject cStatus =
    {
        {"status", getFocusStatusText()}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
}

void Manager::updateGuideStatus(Ekos::GuideState status)
{
    guideManager->updateGuideStatus(status);
    QJsonObject cStatus =
    {
        {"status", getGuideStatusText()}
    };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
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

void Manager::getCurrentProfileTelescopeInfo(double &primaryFocalLength, double &primaryAperture, double &guideFocalLength,
        double &guideAperture)
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
        auto debugProp = device->getProperty("DEBUG");
        if (!debugProp)
            continue;

        auto debugSP = debugProp->getSwitch();

        // Check if the debug interface matches the driver device class
        if ( ( opsLogs->getINDIDebugInterface() & device->getDriverInterface() ) &&
                debugSP->sp[0].s != ISS_ON)
        {
            debugSP->at(0)->setState(ISS_ON);
            debugSP->at(1)->setState(ISS_OFF);
            device->getDriverInfo()->getClientManager()->sendNewSwitch(debugSP);
            appendLogText(i18n("Enabling debug logging for %1...", device->getDeviceName()));
        }
        else if ( !( opsLogs->getINDIDebugInterface() & device->getDriverInterface() ) &&
                  debugSP->sp[0].s != ISS_OFF)
        {
            debugSP->at(0)->setState(ISS_OFF);
            debugSP->at(1)->setState(ISS_ON);
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
        if (deviceInterface->getDriverInterface() == INDI::BaseDevice::GENERAL_INTERFACE)
            return;

        // If debug was turned off, but our logging policy requires it then turn it back on.
        // We turn on debug logging if AT LEAST one driver interface is selected by the logging settings
        if (svp->s == IPS_OK && svp->sp[0].s == ISS_OFF &&
                (opsLogs->getINDIDebugInterface() & deviceInterface->getDriverInterface()))
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
        else if (svp->s == IPS_OK && svp->sp[0].s == ISS_ON
                 && !(opsLogs->getINDIDebugInterface() & deviceInterface->getDriverInterface()))
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
        //        captureProcess.get()->disconnect(guideProcess.get());
        //        guideProcess.get()->disconnect(captureProcess.get());

        // Guide Limits
        connect(guideProcess.get(), &Ekos::Guide::newStatus, captureProcess.get(), &Ekos::Capture::setGuideStatus,
                Qt::UniqueConnection);
        connect(guideProcess.get(), &Ekos::Guide::newAxisDelta, captureProcess.get(), &Ekos::Capture::setGuideDeviation,
                Qt::UniqueConnection);

        // Dithering
        connect(captureProcess.get(), &Ekos::Capture::newStatus, guideProcess.get(), &Ekos::Guide::setCaptureStatus,
                Qt::UniqueConnection);

        // Guide Head
        connect(captureProcess.get(), &Ekos::Capture::suspendGuiding, guideProcess.get(), &Ekos::Guide::suspend,
                Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::resumeGuiding, guideProcess.get(), &Ekos::Guide::resume,
                Qt::UniqueConnection);
        connect(guideProcess.get(), &Ekos::Guide::guideChipUpdated, captureProcess.get(), &Ekos::Capture::setGuideChip,
                Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted, guideProcess.get(), &Ekos::Guide::abort,
                Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipCompleted, guideProcess.get(),
                &Ekos::Guide::guideAfterMeridianFlip, Qt::UniqueConnection);
    }

    // Guide <---> Mount connections
    if (guideProcess.get() && mountProcess.get())
    {
        // Parking
        connect(mountProcess.get(), &Ekos::Mount::newStatus, guideProcess.get(), &Ekos::Guide::setMountStatus,
                Qt::UniqueConnection);
        connect(mountProcess.get(), &Ekos::Mount::newCoords, guideProcess.get(), &Ekos::Guide::setMountCoords,
                Qt::UniqueConnection);

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
        connect(captureProcess.get(), &Ekos::Capture::checkFocus, focusProcess.get(), &Ekos::Focus::checkFocus,
                Qt::UniqueConnection);

        // Reset Focus
        connect(captureProcess.get(), &Ekos::Capture::resetFocus, focusProcess.get(), &Ekos::Focus::resetFrame,
                Qt::UniqueConnection);

        // Abort Focus
        connect(captureProcess.get(), &Ekos::Capture::abortFocus, focusProcess.get(), &Ekos::Focus::abort,
                Qt::UniqueConnection);

        // New Focus Status
        connect(focusProcess.get(), &Ekos::Focus::newStatus, captureProcess.get(), &Ekos::Capture::setFocusStatus,
                Qt::UniqueConnection);

        // New Focus HFR
        connect(focusProcess.get(), &Ekos::Focus::newHFR, captureProcess.get(), &Ekos::Capture::setHFR, Qt::UniqueConnection);

        // New Focus temperature delta
        connect(focusProcess.get(), &Ekos::Focus::newFocusTemperatureDelta, captureProcess.get(),
                &Ekos::Capture::setFocusTemperatureDelta, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted, focusProcess.get(), &Ekos::Focus::meridianFlipStarted,
                Qt::UniqueConnection);
    }

    // Capture <---> Align connections
    if (captureProcess.get() && alignProcess.get())
    {
        // Alignment flag
        connect(alignProcess.get(), &Ekos::Align::newStatus, captureProcess.get(), &Ekos::Capture::setAlignStatus,
                Qt::UniqueConnection);
        // Solver data
        connect(alignProcess.get(), &Ekos::Align::newSolverResults, captureProcess.get(),  &Ekos::Capture::setAlignResults,
                Qt::UniqueConnection);
        // Capture Status
        connect(captureProcess.get(), &Ekos::Capture::newStatus, alignProcess.get(), &Ekos::Align::setCaptureStatus,
                Qt::UniqueConnection);
    }

    // Capture <---> Mount connections
    if (captureProcess.get() && mountProcess.get())
    {
        // Register both modules since both are now created and ready
        // In case one module misses the DBus signal, then it will be correctly initialized.
        captureProcess->registerNewModule("Mount");
        mountProcess->registerNewModule("Capture");

        // Meridian Flip states
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted, mountProcess.get(), &Ekos::Mount::disableAltLimits,
                Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::meridianFlipCompleted, mountProcess.get(), &Ekos::Mount::enableAltLimits,
                Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::newMeridianFlipStatus, mountProcess.get(),
                &Ekos::Mount::meridianFlipStatusChanged, Qt::UniqueConnection);
        connect(mountProcess.get(), &Ekos::Mount::newMeridianFlipStatus, captureProcess.get(),
                &Ekos::Capture::meridianFlipStatusChanged, Qt::UniqueConnection);

        // Mount Status
        connect(mountProcess.get(), &Ekos::Mount::newStatus, captureProcess.get(), &Ekos::Capture::setMountStatus,
                Qt::UniqueConnection);
    }

    // Capture <---> EkosLive connections
    if (captureProcess.get() && ekosLiveClient.get())
    {
        //captureProcess.get()->disconnect(ekosLiveClient.get()->message());

        connect(captureProcess.get(), &Ekos::Capture::dslrInfoRequested, ekosLiveClient.get()->message(),
                &EkosLive::Message::requestDSLRInfo, Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::sequenceChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendCaptureSequence, Qt::UniqueConnection);
        connect(captureProcess.get(), &Ekos::Capture::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendCaptureSettings, Qt::UniqueConnection);
    }

    // Focus <---> Align connections
    if (focusProcess.get() && alignProcess.get())
    {
        connect(focusProcess.get(), &Ekos::Focus::newStatus, alignProcess.get(), &Ekos::Align::setFocusStatus,
                Qt::UniqueConnection);
    }

    // Focus <---> Mount connections
    if (focusProcess.get() && mountProcess.get())
    {
        connect(mountProcess.get(), &Ekos::Mount::newStatus, focusProcess.get(), &Ekos::Focus::setMountStatus,
                Qt::UniqueConnection);
        connect(mountProcess.get(), &Ekos::Mount::newCoords, focusProcess.get(), &Ekos::Focus::setMountCoords,
                Qt::UniqueConnection);
    }

    // Focus <---> Observatory connections
    //    if (focusProcess.get() && observatoryProcess.get())
    //    {
    //        connect(observatoryProcess.get(), &Ekos::Observatory::newWeatherData, focusProcess.get(), &Ekos::Focus::setWeatherData,
    //                Qt::UniqueConnection);
    //    }

    // Mount <---> Align connections
    if (mountProcess.get() && alignProcess.get())
    {
        connect(mountProcess.get(), &Ekos::Mount::newStatus, alignProcess.get(), &Ekos::Align::setMountStatus,
                Qt::UniqueConnection);
        //        connect(mountProcess.get(), &Ekos::Mount::newCoords, alignProcess.get(), &Ekos::Align::setMountCoords,
        //                Qt::UniqueConnection);
    }

    // Mount <---> Guide connections
    if (mountProcess.get() && guideProcess.get())
    {
        connect(mountProcess.get(), &Ekos::Mount::pierSideChanged, guideProcess.get(), &Ekos::Guide::setPierSide,
                Qt::UniqueConnection);
    }

    // Align <--> EkosLive connections
    if (alignProcess.get() && ekosLiveClient.get())
    {
        //        alignProcess.get()->disconnect(ekosLiveClient.get()->message());
        //        alignProcess.get()->disconnect(ekosLiveClient.get()->media());

        connect(alignProcess.get(), &Ekos::Align::newStatus, ekosLiveClient.get()->message(), &EkosLive::Message::setAlignStatus,
                Qt::UniqueConnection);
        connect(alignProcess.get(), &Ekos::Align::newSolution, ekosLiveClient.get()->message(),
                &EkosLive::Message::setAlignSolution, Qt::UniqueConnection);
        connect(alignProcess.get()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::newPAHStage,
                ekosLiveClient.get()->message(), &EkosLive::Message::setPAHStage,
                Qt::UniqueConnection);
        connect(alignProcess.get()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::newPAHMessage,
                ekosLiveClient.get()->message(),
                &EkosLive::Message::setPAHMessage, Qt::UniqueConnection);
        connect(alignProcess.get()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::PAHEnabled,
                ekosLiveClient.get()->message(), &EkosLive::Message::setPAHEnabled,
                Qt::UniqueConnection);
        connect(alignProcess.get()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::polarResultUpdated,
                ekosLiveClient.get()->message(),
                &EkosLive::Message::setPolarResults, Qt::UniqueConnection);
        connect(alignProcess.get()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::newCorrectionVector,
                ekosLiveClient.get()->media(),
                &EkosLive::Media::setCorrectionVector, Qt::UniqueConnection);

        connect(alignProcess.get(), &Ekos::Align::newImage, ekosLiveClient.get()->media(), &EkosLive::Media::sendModuleFrame,
                Qt::UniqueConnection);
        connect(alignProcess.get(), &Ekos::Align::newFrame, ekosLiveClient.get()->media(), &EkosLive::Media::sendUpdatedFrame,
                Qt::UniqueConnection);

        connect(alignProcess.get(), &Ekos::Align::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendAlignSettings, Qt::UniqueConnection);
    }

    // Focus <--> EkosLive Connections
    if (focusProcess.get() && ekosLiveClient.get())
    {
        connect(focusProcess.get(), &Ekos::Focus::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendFocusSettings, Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::newImage, ekosLiveClient.get()->media(), &EkosLive::Media::sendModuleFrame,
                Qt::UniqueConnection);
    }

    // Guide <--> EkosLive Connections
    if (guideProcess.get() && ekosLiveClient.get())
    {
        connect(guideProcess.get(), &Ekos::Guide::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendGuideSettings, Qt::UniqueConnection);
        connect(guideProcess.get(), &Ekos::Guide::newImage, ekosLiveClient.get()->media(), &EkosLive::Media::sendModuleFrame,
                Qt::UniqueConnection);
    }

    // Analyze connections.
    if (analyzeProcess.get())
    {
        if (captureProcess.get())
        {
            connect(captureProcess.get(), &Ekos::Capture::captureComplete,
                    analyzeProcess.get(), &Ekos::Analyze::captureComplete, Qt::UniqueConnection);
            connect(captureProcess.get(), &Ekos::Capture::captureStarting,
                    analyzeProcess.get(), &Ekos::Analyze::captureStarting, Qt::UniqueConnection);
            connect(captureProcess.get(), &Ekos::Capture::captureAborted,
                    analyzeProcess.get(), &Ekos::Analyze::captureAborted, Qt::UniqueConnection);
#if 0
            // Meridian Flip
            connect(captureProcess.get(), &Ekos::Capture::meridianFlipStarted,
                    analyzeProcess.get(), &Ekos::Analyze::meridianFlipStarted, Qt::UniqueConnection);
            connect(captureProcess.get(), &Ekos::Capture::meridianFlipCompleted,
                    analyzeProcess.get(), &Ekos::Analyze::meridianFlipComplete, Qt::UniqueConnection);
#endif
        }
        if (guideProcess.get())
        {
            connect(guideProcess.get(), &Ekos::Guide::newStatus,
                    analyzeProcess.get(), &Ekos::Analyze::guideState, Qt::UniqueConnection);

            connect(guideProcess.get(), &Ekos::Guide::guideStats,
                    analyzeProcess.get(), &Ekos::Analyze::guideStats, Qt::UniqueConnection);
        }
    }
    if (focusProcess.get())
    {
        connect(focusProcess.get(), &Ekos::Focus::autofocusComplete,
                analyzeProcess.get(), &Ekos::Analyze::autofocusComplete, Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::autofocusStarting,
                analyzeProcess.get(), &Ekos::Analyze::autofocusStarting, Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::autofocusAborted,
                analyzeProcess.get(), &Ekos::Analyze::autofocusAborted, Qt::UniqueConnection);
        connect(focusProcess.get(), &Ekos::Focus::newFocusTemperatureDelta,
                analyzeProcess.get(), &Ekos::Analyze::newTemperature, Qt::UniqueConnection);
    }
    if (alignProcess.get())
    {
        connect(alignProcess.get(), &Ekos::Align::newStatus,
                analyzeProcess.get(), &Ekos::Analyze::alignState, Qt::UniqueConnection);

    }
    if (mountProcess.get())
    {
        // void newStatus(ISD::Telescope::Status status);
        connect(mountProcess.get(), &Ekos::Mount::newStatus,
                analyzeProcess.get(), &Ekos::Analyze::mountState, Qt::UniqueConnection);
        //void newCoords(const QString &ra, const QString &dec,
        //               const QString &az, const QString &alt, int pierSide);
        connect(mountProcess.get(), &Ekos::Mount::newCoords,
                analyzeProcess.get(), &Ekos::Analyze::mountCoords, Qt::UniqueConnection);
        // void newMeridianFlipStatus(MeridianFlipStatus status);
        connect(mountProcess.get(), &Ekos::Mount::newMeridianFlipStatus,
                analyzeProcess.get(), &Ekos::Analyze::mountFlipStatus, Qt::UniqueConnection);
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
        // and update it from the existing drivers.
        auto tvp = oneDevice->getBaseDevice()->getText("ACTIVE_DEVICES");
        if (!tvp)
            continue;

        //bool propertyUpdated = false;

        for (auto &it : *tvp)
        {
            QList<ISD::GDInterface *> devs;
            if (it.isNameMatch("ACTIVE_TELESCOPE"))
            {
                devs = findDevicesByInterface(INDI::BaseDevice::TELESCOPE_INTERFACE);
            }
            else if (it.isNameMatch("ACTIVE_DOME"))
            {
                devs = findDevicesByInterface(INDI::BaseDevice::DOME_INTERFACE);
            }
            else if (it.isNameMatch("ACTIVE_GPS"))
            {
                devs = findDevicesByInterface(INDI::BaseDevice::GPS_INTERFACE);
            }
            else if (it.isNameMatch("ACTIVE_FILTER"))
            {
                devs = findDevicesByInterface(INDI::BaseDevice::FILTER_INTERFACE);
                // Active filter wheel should be set to whatever the user selects in capture module
                const QString defaultFilterWheel = Options::defaultCaptureFilterWheel();
                // Does defaultFilterWheel exist in devices?
                if (defaultFilterWheel == "--")
                {
                    // If already empty, do not update it.
                    if (!QString(it.getText()).isEmpty())
                    {
                        it.setText("");
                        oneDevice->getDriverInfo()->getClientManager()->sendNewText(tvp);
                    }
                    continue;
                }
                else
                {
                    for (auto &oneDev : devs)
                    {
                        if (oneDev->getDeviceName() == defaultFilterWheel)
                        {
                            // TODO this should be profile specific
                            if (QString(it.getText()) != defaultFilterWheel)
                            {
                                it.setText(defaultFilterWheel.toLatin1().constData());
                                oneDevice->getDriverInfo()->getClientManager()->sendNewText(tvp);
                                break;
                            }
                        }
                    }
                    continue;
                }
                // If it does not exist, then continue and pick from available devs below.

            }
            // 2021.04.21 JM: There could be more than active weather device
            //                else if (it.isNameMatch("ACTIVE_WEATHER"))
            //                {
            //                    devs = findDevicesByInterface(INDI::BaseDevice::WEATHER_INTERFACE);
            //                }

            if (!devs.empty())
            {
                if (it.getText() != devs.first()->getDeviceName())
                {
                    it.setText(devs.first()->getDeviceName().toLatin1().constData());
                    oneDevice->getDriverInfo()->getClientManager()->sendNewText(tvp);
                }
            }
        }
    }
}

bool Manager::checkUniqueBinaryDriver(DriverInfo * primaryDriver, DriverInfo * secondaryDriver)
{
    if (!primaryDriver || !secondaryDriver)
        return false;

    return (primaryDriver->getExecutable() == secondaryDriver->getExecutable() &&
            primaryDriver->getAuxInfo().value("mdpd", false).toBool() == true);
}

void Manager::restartDriver(const QString &deviceName)
{
    if (m_LocalMode)
    {
        for (auto &device : managedDevices)
        {
            if (QString(device->getDeviceName()) == deviceName)
            {
                DriverManager::Instance()->restartDriver(device->getDriverInfo());
                break;
            }
        }
    }
    else
        INDI::WebManager::restartDriver(currentProfile, deviceName);
}

void Manager::setEkosLoggingEnabled(const QString &name, bool enabled)
{
    // LOGGING, FILE, DEFAULT are exclusive, so one of them must be SET to TRUE
    if (name == "LOGGING")
    {
        Options::setDisableLogging(!enabled);
        if (!enabled)
            KSUtils::Logging::Disable();
    }
    else if (name == "FILE")
    {
        Options::setLogToFile(enabled);
        if (enabled)
            KSUtils::Logging::UseFile();
    }
    else if (name == "DEFAULT")
    {
        Options::setLogToDefault(enabled);
        if (enabled)
            KSUtils::Logging::UseDefault();
    }
    // VERBOSE should be set to TRUE if INDI or Ekos logging is selected.
    else if (name == "VERBOSE")
    {
        Options::setVerboseLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    // Toggle INDI Logging
    else if (name == "INDI")
    {
        Options::setINDILogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "FITS")
    {
        Options::setFITSLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "CAPTURE")
    {
        Options::setCaptureLogging(enabled);
        Options::setINDICCDLogging(enabled);
        Options::setINDIFilterWheelLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "FOCUS")
    {
        Options::setFocusLogging(enabled);
        Options::setINDIFocuserLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "GUIDE")
    {
        Options::setGuideLogging(enabled);
        Options::setINDICCDLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "ALIGNMENT")
    {
        Options::setAlignmentLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "MOUNT")
    {
        Options::setMountLogging(enabled);
        Options::setINDIMountLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "SCHEDULER")
    {
        Options::setSchedulerLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
    else if (name == "OBSERVATORY")
    {
        Options::setObservatoryLogging(enabled);
        KSUtils::Logging::SyncFilterRules();
    }
}

void Manager::acceptPortSelection()
{
    if (m_PortSelector)
        m_PortSelector->accept();
}

void Manager::setPortSelectionComplete()
{
    if (currentProfile->portSelector)
    {
        // Turn off port selector
        currentProfile->portSelector = false;
        KStarsData::Instance()->userdb()->SaveProfile(currentProfile);
    }

    if (currentProfile->autoConnect)
        connectDevices();
}
}
