/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikartech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "manager.h"

#include "analyze/analyze.h"
#include "capture/capture.h"
#include "scheduler/scheduler.h"
#include "scheduler/schedulerprocess.h"
#include "scheduler/schedulermodulestate.h"
#include "focus/focus.h"
#include "focus/focusmodule.h"
#include "align/align.h"
#include "guide/guide.h"
#include "mount/mount.h"
#include "observatory/observatory.h"

#include "opsekos.h"
#include "ekosadaptor.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "ekos/capture/rotatorsettings.h"
#include "profileeditor.h"
#include "profilewizard.h"
#include "auxiliary/darklibrary.h"
#include "auxiliary/ksmessagebox.h"
#include "auxiliary/profilesettings.h"
#include "capture/sequencejob.h"
#include "capture/cameraprocess.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/guimanager.h"
#include "indi/indilistener.h"
#include "auxiliary/opticaltrainmanager.h"
#include "auxiliary/opticaltrainsettings.h"
#include "indi/indiwebmanager.h"
#include "indi/indigps.h"
#include "indi/indiguider.h"
#include "indi/indirotator.h"
#include "mount/meridianflipstatuswidget.h"
#include "ekos/auxiliary/rotatorutils.h"

#include "ekoslive/ekosliveclient.h"
#include "ekoslive/message.h"
#include "ekoslive/media.h"

#include <basedevice.h>

#include <KConfigDialog>
#include <KMessageBox>
#include <KActionCollection>
#include <knotification.h>

#include <QFutureWatcher>
#include <QComboBox>
#include <QDesktopServices>

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
    ProfileSettings::release();
    OpticalTrainManager::release();
    OpticalTrainSettings::release();
    RotatorUtils::release();
    delete _Manager;
}

Manager::Manager(QWidget * parent) : QDialog(parent), m_networkManager(this)
{
#ifdef Q_OS_MACOS

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
    // do not show empty targets
    capturePreview->targetLabel->setVisible(false);
    capturePreview->mountTarget->setVisible(false);

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

    m_CountdownTimer.setInterval(1000);
    connect(&m_CountdownTimer, &QTimer::timeout, this, &Ekos::Manager::updateCaptureCountDown);

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

    // Init EkosLive client
    ekosLiveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    ekosLiveClient.reset(new EkosLive::Client(this));
    connect(ekosLiveClient.get(), &EkosLive::Client::connected, this, [this]()
    {
        emit ekosLiveStatusChanged(true);
    });
    connect(ekosLiveClient.get(), &EkosLive::Client::disconnected, this, [this]()
    {
        emit ekosLiveStatusChanged(false);
    });

    // INDI Control Panel
    //connect(controlPanelB, &QPushButton::clicked, GUIManager::Instance(), SLOT(show()));
    connect(ekosLiveB, &QPushButton::clicked, this, [&]()
    {
        ekosLiveClient.get()->show();
        ekosLiveClient.get()->raise();
    });

    connect(this, &Manager::ekosStatusChanged, ekosLiveClient.get()->message(), &EkosLive::Message::setEkosStatingStatus);
    connect(this, &Manager::indiStatusChanged, ekosLiveClient.get()->message(), &EkosLive::Message::setINDIStatus);
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::connected, this, [&]()
    {
        ekosLiveB->setIcon(QIcon(":/icons/cloud-online.svg"));
    });
    connect(ekosLiveClient.get()->message(), &EkosLive::Message::disconnected, this, [&]()
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
        if (m_PortSelector && m_CurrentProfile->portSelector)
        {
            if (m_PortSelector->shouldShow())
            {
                m_PortSelector->show();
                m_PortSelector->raise();

                ekosLiveClient.get()->message()->requestPortSelection(true);
            }
            // If port selector is enabled, but we have zero ports to work with, let's proceed to connecting if it is enabled.
            else if (m_CurrentProfile->autoConnect)
                setPortSelectionComplete();
        }
        else if (m_CurrentProfile->autoConnect)
            setPortSelectionComplete();
    });
    connect(portSelectorB, &QPushButton::clicked, this, [&]()
    {
        if (m_PortSelector)
        {
            m_PortSelector->show();
            m_PortSelector->raise();
        }
    });

    connect(this, &Ekos::Manager::ekosStatusChanged, this, [&](Ekos::CommunicationStatus status)
    {
        indiControlPanelB->setEnabled(status == Ekos::Success);
        connectB->setEnabled(false);
        disconnectB->setEnabled(false);
        extensionB->setEnabled(false);
        extensionCombo->setEnabled(false);
        profileGroup->setEnabled(status == Ekos::Idle || status == Ekos::Error);
        m_isStarted = (status == Ekos::Success || status == Ekos::Pending);
        if (status == Ekos::Success)
        {
            processINDIB->setIcon(QIcon::fromTheme("media-playback-stop"));
            processINDIB->setToolTip(i18n("Stop"));
            setWindowTitle(i18nc("@title:window", "Ekos - %1 Profile", m_CurrentProfile->name));
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
    connect(indiControlPanelB, &QPushButton::clicked, this, [&]()
    {
        KStars::Instance()->actionCollection()->action("show_control_panel")->trigger();
    });
    connect(optionsB, &QPushButton::clicked, this, [&]()
    {
        KStars::Instance()->actionCollection()->action("configure")->trigger();
    });
    // Save as above, but it appears in all modules
    connect(ekosOptionsB, &QPushButton::clicked, this, &Ekos::Manager::showEkosOptions);

    connect(helpB, &QPushButton::clicked, this, &Ekos::Manager::help);
    helpB->setIcon(QIcon::fromTheme("help-about"));

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
    connect(profileCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentTextChanged), this,
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
    connect(&settleTimer, &QTimer::timeout, this, [&]()
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
    int index = addModuleTab(EkosModule::Scheduler, schedulerModule(), QIcon(":/icons/ekos_scheduler.png"));
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Scheduler"));
    capturePreview->shareSchedulerModuleState(schedulerModule()->moduleState());
    connect(schedulerModule()->process().data(), &SchedulerProcess::newLog, this, &Ekos::Manager::updateLog);
    connect(schedulerModule(), &Ekos::Scheduler::newTarget, this, &Manager::setTarget);
    // Scheduler <---> EkosLive connections
    connect(schedulerModule(), &Ekos::Scheduler::jobsUpdated, ekosLiveClient.get()->message(),
            &EkosLive::Message::sendSchedulerJobs, Qt::UniqueConnection);
    connect(schedulerModule(), &Ekos::Scheduler::settingsUpdated, ekosLiveClient.get()->message(),
            &EkosLive::Message::sendSchedulerSettings, Qt::UniqueConnection);
    connect(schedulerModule()->process().data(), &SchedulerProcess::newLog, ekosLiveClient.get()->message(),
            [this]()
    {
        QJsonObject cStatus =
        {
            {"log", schedulerModule()->moduleState()->getLogText()}
        };

        ekosLiveClient.get()->message()->sendSchedulerStatus(cStatus);
    });
    connect(schedulerModule(), &Ekos::Scheduler::newStatus, ekosLiveClient.get()->message(),
            [this](Ekos::SchedulerState state)
    {
        QJsonObject cStatus =
        {
            {"status", state}
        };

        ekosLiveClient.get()->message()->sendSchedulerStatus(cStatus);
    });

    // Initialize Ekos Analyze Module
    analyzeProcess.reset(new Ekos::Analyze());
    connect(analyzeProcess.get(), &Ekos::Analyze::newLog, this, &Ekos::Manager::updateLog);

    index = addModuleTab(EkosModule::Analyze, analyzeProcess.get(), QIcon(":/icons/ekos_analyze.png"));
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Analyze"));

    numPermanentTabs = index + 1;

    // Extensions
    extensionTimer.setSingleShot(true);
    groupBox_4->setHidden(true);
    extensionB->setIcon(QIcon::fromTheme("media-playback-start"));
    connect(extensionB, &QPushButton::clicked, this, [this]
    {
        if (extensionB->icon().name() == "media-playback-start")
        {
            extensionTimer.setInterval(1000);
            connect(&extensionTimer, &QTimer::timeout, this, [this]
            {
                appendLogText(i18n("Extension '%1' failed to start, aborting", extensionCombo->currentText()));
                m_extensions.kill();
            });
            extensionTimer.start();
            extensionAbort = false;
            m_extensions.run(extensionCombo->currentText());
        }
        else if (extensionB->icon().name() == "media-playback-stop")
        {
            if (!extensionAbort)
            {
                extensionTimer.setInterval(10000);
                connect(&extensionTimer, &QTimer::timeout, this, [this]
                {
                    appendLogText(i18n("Extension '%1' failed to stop, abort enabled", extensionCombo->currentText()));
                    extensionB->setEnabled(true);
                    extensionAbort = true;
                });
                extensionTimer.start();
                m_extensions.stop();
            }
            else
            {
                appendLogText(i18n("Extension '%1' aborting", extensionCombo->currentText()));
                m_extensions.kill();
            }
        }
    });
    connect(&m_extensions, &extensions::extensionStateChanged, this, [this](Ekos::ExtensionState state)
    {
        switch (state)
        {
            case EXTENSION_START_REQUESTED:
                appendLogText(i18n("Extension '%1' start requested", extensionCombo->currentText()));
                extensionB->setEnabled(false);
                extensionCombo->setEnabled(false);
                break;
            case EXTENSION_STARTED:
                appendLogText(i18n("Extension '%1' started", extensionCombo->currentText()));
                extensionB->setIcon(QIcon::fromTheme("media-playback-stop"));
                extensionB->setEnabled(true);
                extensionCombo->setEnabled(false);
                extensionTimer.stop();
                disconnect(&extensionTimer, &QTimer::timeout, this, nullptr);
                break;
            case EXTENSION_STOP_REQUESTED:
                appendLogText(i18n("Extension '%1' stop requested", extensionCombo->currentText()));
                extensionB->setEnabled(false);
                extensionCombo->setEnabled(false);
                break;
            case EXTENSION_STOPPED:
                appendLogText(i18n("Extension '%1' stopped", extensionCombo->currentText()));
                extensionB->setIcon(QIcon::fromTheme("media-playback-start"));
                extensionB->setEnabled(true);
                extensionCombo->setEnabled(true);
                extensionTimer.stop();
                disconnect(&extensionTimer, &QTimer::timeout, this, nullptr);
        }
        m_extensionStatus = state;
        emit extensionStatusChanged();
    });
    connect(extensionCombo, &QComboBox::currentTextChanged, this, [this] (QString text)
    {
        extensionCombo->setToolTip(m_extensions.getTooltip(text));
    });
    connect(&m_extensions, &extensions::extensionOutput, this, [this] (QString message)
    {
        appendLogText(QString(i18n("Extension '%1': %2", extensionCombo->currentText(), message.trimmed())));
    });

    // Temporary fix. Not sure how to resize Ekos Dialog to fit contents of the various tabs in the QScrollArea which are added
    // dynamically. I used setMinimumSize() but it doesn't appear to make any difference.
    // Also set Layout policy to SetMinAndMaxSize as well. Any idea how to fix this?
    // FIXME
    //resize(1000,750);

    m_SummaryView.reset(new SummaryFITSView(capturePreview->previewWidget));
    m_SummaryView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // sterne-jaeger 2021-08-08: Do not set base size here, otherwise the zoom will be incorrect
    // summaryPreview->setBaseSize(capturePreview->previewWidget->size());
    m_SummaryView->createFloatingToolBar();
    m_SummaryView->setCursorMode(FITSView::dragCursor);
    m_SummaryView->showProcessInfo(false);
    capturePreview->setSummaryFITSView(m_SummaryView);
    mountStatusLayout->setAlignment(Qt::AlignVCenter);

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


    if (qgetenv("KDE_FULL_SESSION") != "true")
    {
        if (!Options::ekosGeometry().isEmpty())
            restoreGeometry(QByteArray::fromBase64(Options::ekosGeometry().toLatin1()));
        else
            resize(600, 600);
    }
    else
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

    if (qgetenv("KDE_FULL_SESSION") != "true")
        Options::setEkosGeometry(QString::fromLatin1(saveGeometry().toBase64()));

    // 2019-02-14 JM: Close event, for some reason, make all the children disappear
    // when the widget is shown again. Applying a workaround here

    event->ignore();
    hide();
}

void Manager::hideEvent(QHideEvent * /*event*/)
{
    if (qgetenv("KDE_FULL_SESSION") != "true")
        Options::setEkosGeometry(QString::fromLatin1(saveGeometry().toBase64()));
    else
    {
        Options::setEkosWindowWidth(width());
        Options::setEkosWindowHeight(height());
    }

    QAction * a = KStars::Instance()->actionCollection()->action("show_ekos");
    a->setChecked(false);
}

// Returns true if the url will result in a successful get.
// Times out after 3 seconds.
bool Manager::checkIfPageExists(const QString &urlString)
{
    if (urlString.isEmpty())
        return false;

    QUrl url(urlString);
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager.get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(3000); // 3 seconds timeout

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    timer.stop();

    if (reply->error() == QNetworkReply::NoError)
    {
        reply->deleteLater();
        return true;
    }
    else if (timer.isActive() )
    {
        reply->deleteLater();
        return false;
    }
    else
    {
        reply->deleteLater();
        return false;
    }
}

void Manager::help()
{
    QString urlStr("https://kstars-docs.kde.org/%1/user_manual/ekos.html");
    QWidget *widget = toolsWidget->currentWidget();
    if (widget)
    {
        if (widget == alignModule())
            urlStr = "https://kstars-docs.kde.org/%1/user_manual/ekos-align.html";
        else if (widget == captureModule())
            urlStr = "https://kstars-docs.kde.org/%1/user_manual/ekos-capture.html";
        else if (widget == focusModule())
            urlStr = "https://kstars-docs.kde.org/%1/user_manual/ekos-focus.html";
        else if (widget == guideModule())
            urlStr = "https://kstars-docs.kde.org/%1/user_manual/ekos-guide.html";
        //else if (widget == mountModule())
        //    url = "https://kstars-docs.kde.org/%1/user_manual/ekos-mount.html";
        else if (widget == schedulerModule())
            urlStr = "https://kstars-docs.kde.org/%1/user_manual/ekos-scheduler.html";
        //else if (widget == observatoryProcess.get())
        //    url = "https://kstars-docs.kde.org/%1/user_manual/ekos-observatory.html";
        else if (widget == analyzeProcess.get())
            urlStr = "https://kstars-docs.kde.org/%1/user_manual/ekos-analyze.html";
    }
    QLocale locale;
    QString fullStr = QString(urlStr).arg(locale.name());
    if (!checkIfPageExists(fullStr))
    {
        const int underscoreIndex = locale.name().indexOf('_');
        QString firstPart = locale.name().mid(0, underscoreIndex);
        fullStr = QString(urlStr).arg(firstPart);
        if (!checkIfPageExists(fullStr))
            fullStr = QString(urlStr).arg("en");
    }
    if (!fullStr.isEmpty())
        QDesktopServices::openUrl(QUrl(fullStr));
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
    if (qgetenv("KDE_FULL_SESSION") != "true")
        Options::setEkosGeometry(QString::fromLatin1(saveGeometry().toBase64()));

    focusProgressWidget->updateFocusDetailView();
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

    ProfileSettings::release();
    OpticalTrainManager::release();
    OpticalTrainSettings::release();
    RotatorUtils::release();

    m_DriverDevicesCount = 0;

    removeTabs();

    captureProcess.reset();
    focusProcess.reset();
    guideProcess.reset();
    alignProcess.reset();
    mountProcess.reset();
    observatoryProcess.reset();

    for (auto &oneManger : m_FilterManagers)
        oneManger.reset();
    m_FilterManagers.clear();

    for (auto &oneController : m_RotatorControllers)
        oneController.reset();
    m_RotatorControllers.clear();

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
    extensionB->setEnabled(false);
    extensionCombo->setEnabled(false);
    //controlPanelB->setEnabled(false);
    processINDIB->setEnabled(true);

    mountGroup->setEnabled(false);
    capturePreview->setEnabled(false);
    capturePreview->reset();
    mountStatus->setStatus(i18n("Idle"), Qt::gray);
    mountStatus->setStyleSheet(QString());
    focusProgressWidget->reset();
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
    m_CountdownTimer.stop();
    portSelectorB->setEnabled(false);

    profileGroup->setEnabled(true);

    setWindowTitle(i18nc("@title:window", "Ekos"));

    // Clear extensions list ready for rediscovery if start is called again
    extensionCombo->clear();
    m_extensions.found->clear();
    groupBox_4->setHidden(true);
}

void Manager::start()
{
    if (analyzeProcess && Options::analyzeRestartWithEkos())
        analyzeProcess->restart();

    // Don't start if it is already started before
    if (m_ekosStatus == Ekos::Pending || m_ekosStatus == Ekos::Success)
    {
        qCWarning(KSTARS_EKOS) << "Ekos Manager start called but current Ekos Status is" << m_ekosStatus << "Ignoring request.";
        return;
    }

    managedDrivers.clear();

    // Set clock to realtime mode
    if (!Options::dontSyncToRealTime())
        KStarsData::Instance()->clock()->setRealTime(true);

    // Reset Ekos Manager
    reset();

    // Get Current Profile
    getCurrentProfile(m_CurrentProfile);
    m_LocalMode      = m_CurrentProfile->isLocal();

    ProfileSettings::Instance()->setProfile(m_CurrentProfile);

    // Load profile location if one exists
    updateProfileLocation(m_CurrentProfile);

    bool haveCamera = false;

    // If external guide is specified in the profile, set the
    // corresponding options
    if (m_CurrentProfile->guidertype == Ekos::Guide::GUIDE_PHD2)
    {
        Options::setPHD2Host(m_CurrentProfile->guiderhost);
        Options::setPHD2Port(m_CurrentProfile->guiderport);
    }
    else if (m_CurrentProfile->guidertype == Ekos::Guide::GUIDE_LINGUIDER)
    {
        Options::setLinGuiderHost(m_CurrentProfile->guiderhost);
        Options::setLinGuiderPort(m_CurrentProfile->guiderport);
    }

    // Parse script, if any
    QJsonParseError jsonError;
    QJsonArray profileScripts;
    QJsonDocument doc = QJsonDocument::fromJson(m_CurrentProfile->scripts, &jsonError);

    if (jsonError.error == QJsonParseError::NoError)
        profileScripts = doc.array();

    ekosLiveClient->message()->setPendingPropertiesEnabled(true);

    // For locally running INDI server
    if (m_LocalMode)
    {
        for (auto it = m_CurrentProfile->drivers.constBegin(); it != m_CurrentProfile->drivers.constEnd(); ++it)
        {
            for (const QString &driver : it.value())
            {
                auto drv = driversList.value(driver);
                if (!drv.isNull())
                {
                    if (it.key() == KSTARS_CCD)
                        haveCamera = true;
                    managedDrivers.append(drv->clone());
                }
            }
        }
        Options::setGuiderType(m_CurrentProfile->guidertype);

        // Add remote drivers if we have any
        if (m_CurrentProfile->remotedrivers.isEmpty() == false && m_CurrentProfile->remotedrivers.contains("@"))
        {
            for (auto remoteDriver : m_CurrentProfile->remotedrivers.split(","))
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

                QSharedPointer<DriverInfo> dv(new DriverInfo(name));
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


        if (haveCamera == false && m_CurrentProfile->remotedrivers.isEmpty())
        {
            KSNotification::error(i18n("Ekos requires at least one Camera to operate."));
            managedDrivers.clear();
            m_ekosStatus = Ekos::Error;
            emit ekosStatusChanged(m_ekosStatus);
            return;
        }

        m_DriverDevicesCount = managedDrivers.count();
    }
    else
    {
        QSharedPointer<DriverInfo> remote_indi(new DriverInfo(QString("Ekos Remote Host")));

        remote_indi->setHostParameters(m_CurrentProfile->host, m_CurrentProfile->port);

        remote_indi->setDriverSource(GENERATED_SOURCE);

        managedDrivers.append(remote_indi);

        haveCamera = m_CurrentProfile->drivers.contains(KSTARS_CCD);

        Options::setGuiderType(m_CurrentProfile->guidertype);

        if (m_CurrentProfile->drivers.empty() == false && haveCamera == false && m_CurrentProfile->remotedrivers.isEmpty())
        {
            KSNotification::error(i18n("Ekos requires at least one Camera to operate."));
            m_DriverDevicesCount = 0;
            m_ekosStatus = Ekos::Error;
            emit ekosStatusChanged(m_ekosStatus);
            return;
        }

        m_DriverDevicesCount = m_CurrentProfile->drivers.count();
    }


    // Prioritize profile script drivers over other drivers
    QList<QSharedPointer<DriverInfo >> sortedList;
    for (const auto &oneRule : qAsConst(profileScripts))
    {
        auto driver = oneRule.toObject()["Driver"].toString();
        auto matchingDriver = std::find_if(managedDrivers.begin(), managedDrivers.end(), [oneRule, driver](const auto & oneDriver)
        {
            // Account for both local and remote drivers
            return oneDriver->getLabel() == driver || (driver.startsWith("@") && !oneDriver->getRemoteHost().isEmpty());
        });

        if (matchingDriver != managedDrivers.end())
        {
            (*matchingDriver)->setStartupShutdownRule(oneRule.toObject());
            sortedList.append(*matchingDriver);
        }
    }

    // If we have any profile scripts drivers, let's re-sort managed drivers
    // so that profile script drivers
    if (!sortedList.isEmpty())
    {
        for (auto &oneDriver : managedDrivers)
        {
            if (sortedList.contains(oneDriver) == false)
                sortedList.append(oneDriver);
        }

        managedDrivers = sortedList;
    }

    connect(DriverManager::Instance(), &DriverManager::serverStarted, this,
            &Manager::setServerStarted, Qt::UniqueConnection);
    connect(DriverManager::Instance(), &DriverManager::serverFailed, this,
            &Manager::setServerFailed, Qt::UniqueConnection);
    connect(DriverManager::Instance(), &DriverManager::clientStarted, this,
            &Manager::setClientStarted, Qt::UniqueConnection);
    connect(DriverManager::Instance(), &DriverManager::clientFailed, this,
            &Manager::setClientFailed, Qt::UniqueConnection);
    connect(DriverManager::Instance(), &DriverManager::clientTerminated, this,
            &Manager::setClientTerminated, Qt::UniqueConnection);

    connect(INDIListener::Instance(), &INDIListener::newDevice, this, &Ekos::Manager::processNewDevice);
    connect(INDIListener::Instance(), &INDIListener::deviceRemoved, this, &Ekos::Manager::removeDevice, Qt::DirectConnection);


#ifdef Q_OS_MACOS
    if (m_LocalMode || m_CurrentProfile->host == "localhost")
    {
        if (isRunning("PTPCamera"))
        {
            if (KMessageBox::Continue == KMessageBox::warningContinueCancel(nullptr,
                    i18n("Ekos detected that PTP Camera is running and may prevent a Canon or Nikon camera from connecting to Ekos. Do you want to quit PTP Camera now?"),
                    i18n("PTP Camera")))
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

            m_ekosStatus = Ekos::Pending;
            emit ekosStatusChanged(m_ekosStatus);

            DriverManager::Instance()->startDevices(managedDrivers);
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
                const QString program = "pkill";
                QStringList arguments;
                arguments << "indiserver";
                p.start(program, arguments);
                p.waitForFinished();

                QTimer::singleShot(1000, this, executeStartINDIServices);
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
                i18n("Connecting to remote INDI server at %1 on port %2 ...", m_CurrentProfile->host, m_CurrentProfile->port));

            DriverManager::Instance()->connectRemoteHost(managedDrivers.first());
        };

        auto runProfile = [this, runConnection]()
        {
            // If it got cancelled by the user, return immediately.
            if (m_ekosStatus != Ekos::Pending)
                return;

            INDI::WebManager::syncCustomDrivers(m_CurrentProfile);
            INDI::WebManager::checkVersion(m_CurrentProfile);

            if (INDI::WebManager::areDriversRunning(m_CurrentProfile) == false)
            {
                INDI::WebManager::stopProfile(m_CurrentProfile);

                if (INDI::WebManager::startProfile(m_CurrentProfile) == false)
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
        if (m_CurrentProfile->INDIWebManagerPort > 0)
        {
            appendLogText(i18n("Establishing communication with remote INDI Web Manager..."));
            m_RemoteManagerStart = false;
            QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
            connect(watcher, &QFutureWatcher<bool>::finished, this, [this, runConnection, runProfile, watcher]()
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

            QFuture<bool> result = INDI::AsyncWebManager::isOnline(m_CurrentProfile);
            watcher->setFuture(result);
        }
        else
        {
            runConnection();
        }
    }

    // Search for extensions
    if (m_extensions.discover())
    {
        foreach (QString extension, m_extensions.found->keys())
        {
            extensions::extDetails m_ext = m_extensions.found->value(extension);
            extensionCombo->addItem(m_ext.icon, extension);
        }
    }
    if (extensionCombo->count() > 0)
    {
        groupBox_4->setHidden(false);
    }
}

void Manager::setClientStarted(const QString &host, int port)
{
    if (managedDrivers.size() > 0)
    {
        if (m_LocalMode)
        {
            if (m_CurrentProfile->autoConnect)
                appendLogText(i18n("INDI services started on port %1.", port));
            else
                appendLogText(
                    i18n("INDI services started on port %1. Please connect devices.", port));
        }
        else
        {
            appendLogText(
                i18n("INDI services started. Connection to remote INDI server %1:%2 is successful. Waiting for devices...", host, port));
        }
    }

    auto maxTimeout = MAX_LOCAL_INDI_TIMEOUT;

    // Parse script, if any
    QJsonParseError jsonError;
    QJsonArray profileScripts;
    QJsonDocument doc = QJsonDocument::fromJson(m_CurrentProfile->scripts, &jsonError);

    // If we have any rules that delay startup of drivers, we need to take that into account
    // otherwise Ekos would prematurely declare that drivers failed to connect.
    if (jsonError.error == QJsonParseError::NoError)
    {
        profileScripts = doc.array();
        for (const auto &oneRule : qAsConst(profileScripts))
        {
            const auto &oneRuleObj = oneRule.toObject();
            auto totalDelay = (oneRuleObj["PreDelay"].toDouble(0) + oneRuleObj["PostDelay"].toDouble(0)) * 1000;
            if (totalDelay >= maxTimeout)
                maxTimeout = totalDelay + MAX_LOCAL_INDI_TIMEOUT;
        }
    }

    QTimer::singleShot(maxTimeout, this, &Ekos::Manager::checkINDITimeout);
}

void Manager::setClientFailed(const QString &host, int port, const QString &errorMessage)
{
    if (m_LocalMode)
        appendLogText(i18n("Failed to connect to local INDI server %1:%2", host, port));
    else
        appendLogText(i18n("Failed to connect to remote INDI server %1:%2", host, port));

    //INDIListener::Instance()->disconnect(this);
    //    qDeleteAll(managedDrivers);
    //    managedDrivers.clear();
    m_ekosStatus = Ekos::Error;
    emit ekosStatusChanged(m_ekosStatus);
    KSNotification::error(errorMessage, i18n("Error"), 15);
}

void Manager::setClientTerminated(const QString &host, int port, const QString &errorMessage)
{
    if (m_LocalMode)
        appendLogText(i18n("Lost connection to local INDI server %1:%2", host, port));
    else
        appendLogText(i18n("Lost connection to remote INDI server %1:%2", host, port));

    //INDIListener::Instance()->disconnect(this);
    //    qDeleteAll(managedDrivers);
    //    managedDrivers.clear();
    m_ekosStatus = Ekos::Error;
    emit ekosStatusChanged(m_ekosStatus);
    KSNotification::error(errorMessage, i18n("Error"), 15);
}

void Manager::setServerStarted(const QString &host, int port)
{
    Q_UNUSED(host)
    Q_UNUSED(port)
}

void Manager::setServerFailed(const QString &host, int port, const QString &message)
{
    Q_UNUSED(host)
    Q_UNUSED(port)
    managedDrivers.clear();
    m_ekosStatus = Ekos::Error;
    emit ekosStatusChanged(m_ekosStatus);
    KSNotification::error(message, i18n("Error"), 15);
}

//void Manager::setServerTerminated(const QString &host, int port, const QString &message)
//{
//    if ((m_LocalMode && managedDrivers.first()->getPort() == port) ||
//            (currentProfile->host == host && currentProfile->port == port))
//    {
//        cleanDevices(false);
//        if (indiHubAgent)
//            indiHubAgent->terminate();
//    }

//    INDIListener::Instance()->disconnect(this);
//    qDeleteAll(managedDrivers);
//    managedDrivers.clear();
//    m_ekosStatus = Ekos::Error;
//    emit ekosStatusChanged(m_ekosStatus);
//    KSNotification::error(message, i18n("Error"), 15);
//}

void Manager::checkINDITimeout()
{
    // Don't check anything unless we're still pending
    if (m_ekosStatus != Ekos::Pending)
    {
        // All devices are connected already, nothing to do.
        if (m_indiStatus != Ekos::Pending || m_CurrentProfile->portSelector || m_CurrentProfile->autoConnect == false)
            return;

        QStringList disconnectedDevices;
        for (auto &oneDevice : INDIListener::devices())
        {
            if (oneDevice->isConnected() == false)
                disconnectedDevices << oneDevice->getDeviceName();
        }

        QString message;

        if (disconnectedDevices.count() == 1)
            message = i18n("Failed to connect to %1. Please ensure device is connected and powered on.", disconnectedDevices.first());
        else
            message = i18n("Failed to connect to \n%1\nPlease ensure each device is connected and powered on.",
                           disconnectedDevices.join("\n"));

        appendLogText(message);
        KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::General, KSNotification::Warn);
        return;
    }


    if (m_DriverDevicesCount <= 0)
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
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::General, KSNotification::Warn);
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            QString message = i18n("Unable to establish the following devices:\n%1\nPlease ensure each device is connected "
                                   "and powered on.", remainingDevices.join("\n"));
            appendLogText(message);
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::General, KSNotification::Warn);
            KNotification::beep(i18n("Ekos startup error"));
        }
    }
    else
    {
        QStringList remainingDevices;

        for (auto &drivers : m_CurrentProfile->drivers)
        {
            for (auto &driver : drivers)
            {
                bool driverFound = false;

                for (auto &device : INDIListener::devices())
                {
                    if (device->getBaseDevice().getDriverName() == driver)
                    {
                        driverFound = true;
                        break;
                    }
                }

                if (driverFound == false)
                    remainingDevices << QString("+ %1").arg(driver);
            }
        }

        if (remainingDevices.count() == 1)
        {
            QString message = i18n("Unable to remotely establish:\n%1\nPlease ensure the device is connected and powered on.",
                                   remainingDevices.at(0));
            appendLogText(message);
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::General, KSNotification::Warn);
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            QString message = i18n("Unable to remotely establish the following devices:\n%1\nPlease ensure each device is connected "
                                   "and powered on.", remainingDevices.join("\n"));
            appendLogText(message);
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::General, KSNotification::Warn);
            KNotification::beep(i18n("Ekos startup error"));
        }
    }

    m_ekosStatus = Ekos::Error;
}

bool Manager::isINDIReady()
{
    // Check if already connected
    int nConnected = 0;

    Ekos::CommunicationStatus previousStatus = m_indiStatus;

    auto devices = INDIListener::devices();
    for (auto &device : devices)
    {
        // Make sure we're not only connected, but also ready (i.e. all properties have already been defined).
        if (device->isConnected() && device->isReady())
            nConnected++;
    }
    if (devices.count() == nConnected)
    {
        m_indiStatus = Ekos::Success;
        emit indiStatusChanged(m_indiStatus);
        return true;
    }

    m_indiStatus = Ekos::Pending;
    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

    return false;
}

void Manager::connectDevices()
{
    if (isINDIReady())
        return;

    auto devices = INDIListener::devices();

    for (auto &device : devices)
    {
        qCDebug(KSTARS_EKOS) << "Connecting " << device->getDeviceName();
        device->Connect();
    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);
    extensionCombo->setEnabled(true);
    if (extensionCombo->currentText() != "")
        extensionB->setEnabled(true);

    appendLogText(i18n("Connecting INDI devices..."));
}

void Manager::disconnectDevices()
{
    for (auto &device : INDIListener::devices())
    {
        qCDebug(KSTARS_EKOS) << "Disconnecting " << device->getDeviceName();
        device->Disconnect();
    }

    appendLogText(i18n("Disconnecting INDI devices..."));
}

void Manager::cleanDevices(bool stopDrivers)
{
    if (m_ekosStatus == Ekos::Idle)
        return;

    if (mountModule())
        mountModule()->stopTimers();

    ekosLiveClient->message()->setPendingPropertiesEnabled(false);
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

                if (m_RemoteManagerStart && m_CurrentProfile->INDIWebManagerPort != -1)
                    INDI::WebManager::stopProfile(m_CurrentProfile);
            }
            m_RemoteManagerStart = false;
        }
    }

    reset();

    profileGroup->setEnabled(true);

    appendLogText(i18n("INDI services stopped."));
}

void Manager::processNewDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    qCInfo(KSTARS_EKOS) << "Ekos received a new device: " << device->getDeviceName();

    Ekos::CommunicationStatus previousStatus = m_indiStatus;

    //    for(auto &oneDevice : INDIListener::devices())
    //    {
    //        if (oneDevice->getDeviceName() == device->getDeviceName())
    //        {
    //            qCWarning(KSTARS_EKOS) << "Found duplicate device, ignoring...";
    //            return;
    //        }
    //    }

    // Always reset INDI Connection status if we receive a new device
    m_indiStatus = Ekos::Idle;
    if (previousStatus != m_indiStatus)
        emit indiStatusChanged(m_indiStatus);

    m_DriverDevicesCount--;

    connect(device.get(), &ISD::GenericDevice::ready, this, &Ekos::Manager::setDeviceReady, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newMount, this, &Ekos::Manager::addMount, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newCamera, this, &Ekos::Manager::addCamera, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newGuider, this, &Ekos::Manager::addGuider, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newFilterWheel, this, &Ekos::Manager::addFilterWheel, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newFocuser, this, &Ekos::Manager::addFocuser, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newDome, this, &Ekos::Manager::addDome, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newRotator, this, &Ekos::Manager::addRotator, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newWeather, this, &Ekos::Manager::addWeather, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newDustCap, this, &Ekos::Manager::addDustCap, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newLightBox, this, &Ekos::Manager::addLightBox, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::newGPS, this, &Ekos::Manager::addGPS, Qt::UniqueConnection);

    connect(device.get(), &ISD::GenericDevice::Connected, this, &Ekos::Manager::deviceConnected, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::Disconnected, this, &Ekos::Manager::deviceDisconnected, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::propertyDefined, this, &Ekos::Manager::processNewProperty, Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::propertyDeleted, this, &Ekos::Manager::processDeleteProperty,
            Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::propertyUpdated, this, &Ekos::Manager::processUpdateProperty,
            Qt::UniqueConnection);
    connect(device.get(), &ISD::GenericDevice::messageUpdated, this, &Ekos::Manager::processMessage, Qt::UniqueConnection);

    if (m_DriverDevicesCount <= 0)
    {
        m_ekosStatus = Ekos::Success;
        emit ekosStatusChanged(m_ekosStatus);

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        extensionCombo->setEnabled(false);
        extensionB->setEnabled(false);

        if (m_LocalMode == false && m_DriverDevicesCount == 0)
        {
            if (m_CurrentProfile->autoConnect)
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
    extensionCombo->setEnabled(true);
    if (extensionCombo->currentText() != "")
        extensionB->setEnabled(true);

    auto device = qobject_cast<ISD::GenericDevice *>(sender());

    if (Options::verboseLogging())
    {
        qCInfo(KSTARS_EKOS) << device->getDeviceName()
                            << "Version:" << device->getDriverVersion()
                            << "Interface:" << device->getDriverInterface()
                            << "is connected.";
    }

    if (Options::neverLoadConfig() == false && device && device->wasConfigLoaded() == false)
    {
        INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

        for (auto &oneDevice : INDIListener::devices())
        {
            if (oneDevice == device)
            {
                connect(device, &ISD::GenericDevice::propertyUpdated, this, &Ekos::Manager::watchDebugProperty, Qt::UniqueConnection);

                auto configProp = device->getBaseDevice().getSwitch("CONFIG_PROCESS");
                if (configProp && configProp.getState() == IPS_IDLE)
                    device->setConfig(tConfig);
                break;
            }
        }
    }
}

void Manager::deviceDisconnected()
{
    ISD::GenericDevice * dev = static_cast<ISD::GenericDevice *>(sender());

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
            KSNotification::event(QLatin1String("IndiServerMessage"), message, KSNotification::General, KSNotification::Warn);
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
    extensionCombo->setEnabled(false);
    extensionB->setEnabled(false);
}

void Manager::addMount(ISD::Mount *device)
{
    ekosLiveClient->message()->sendScopes();

    appendLogText(i18n("%1 is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addCamera(ISD::Camera * device)
{
    ekosLiveClient.get()->media()->registerCameras();

    appendLogText(i18n("%1 is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addFilterWheel(ISD::FilterWheel * device)
{
    QString name = device->getDeviceName();
    appendLogText(i18n("%1 filter is online.", name));

    createFilterManager(device);

    emit newDevice(name, device->getDriverInterface());
}

void Manager::addFocuser(ISD::Focuser *device)
{
    appendLogText(i18n("%1 focuser is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addRotator(ISD::Rotator *device)
{
    appendLogText(i18n("Rotator %1 is online.", device->getDeviceName()));

    // createRotatorControl(device);

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addDome(ISD::Dome * device)
{
    appendLogText(i18n("%1 is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addWeather(ISD::Weather * device)
{
    appendLogText(i18n("%1 Weather is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addGPS(ISD::GPS * device)
{
    appendLogText(i18n("%1 GPS is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addDustCap(ISD::DustCap * device)
{
    OpticalTrainManager::Instance()->syncDevices();

    appendLogText(i18n("%1 Dust cap is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::addLightBox(ISD::LightBox * device)
{
    appendLogText(i18n("%1 Light box is online.", device->getDeviceName()));

    emit newDevice(device->getDeviceName(), device->getDriverInterface());
}

void Manager::syncGenericDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    qCDebug(KSTARS_EKOS) << "Syncing Generic Device" << device->getDeviceName();

    createModules(device);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Cameras
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    auto camera = device->getCamera();
    if (camera)
    {
        // Focus Module
        if (focusProcess)
        {
            if (camera->hasCooler())
            {
                QSharedPointer<ISD::GenericDevice> generic;
                if (INDIListener::findDevice(camera->getDeviceName(), generic))
                    focusModule()->addTemperatureSource(generic);
            }
        }

    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Mount
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    auto mount = device->getMount();
    if (mount)
    {
        if (mountProcess)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(mount->getDeviceName(), generic))
            {
                mountModule()->addTimeSource(generic);
                mountModule()->addLocationSource(generic);
            }
        }

    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Focuser
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    auto focuser = device->getFocuser();
    if (focuser)
    {
        if (focusProcess)
        {
            // Temperature sources.
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(focuser->getDeviceName(), generic))
                focusModule()->addTemperatureSource(generic);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Filter Wheel
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Rotators
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Domes
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    auto dome = device->getDome();
    if (dome)
    {
        if (captureProcess)
            captureProcess->setDome(dome);
        if (alignProcess)
            alignProcess->setDome(dome);
        if (observatoryProcess)
            observatoryProcess->setDome(dome);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Weather
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    auto weather = device->getWeather();
    if (weather)
    {
        if (observatoryProcess)
            observatoryProcess->addWeatherSource(weather);

        if (focusProcess)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(weather->getDeviceName(), generic))
                focusModule()->addTemperatureSource(generic);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GPS
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    auto gps = device->getGPS();
    if (gps)
    {
        if (mountProcess)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(gps->getDeviceName(), generic))
            {
                mountModule()->addTimeSource(generic);
                mountModule()->addLocationSource(generic);
            }
        }

    }
}

void Manager::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (alignProcess)
        alignModule()->removeDevice(device);
    if (captureProcess)
        captureProcess->removeDevice(device);
    if (focusProcess)
        focusModule()->removeDevice(device);
    if (mountProcess)
        mountModule()->removeDevice(device);
    if (guideProcess)
        guideProcess->removeDevice(device);
    if (observatoryProcess)
        observatoryProcess->removeDevice(device);
    if (m_PortSelector)
        m_PortSelector->removeDevice(device->getDeviceName());

    DarkLibrary::Instance()->removeDevice(device);

    // Remove from filter managers
    for (auto &oneManager : m_FilterManagers)
    {
        oneManager->removeDevice(device);
    }

    // Remove from rotator controllers
    for (auto &oneController : m_RotatorControllers)
    {
        oneController->close();
    }

    appendLogText(i18n("%1 is offline.", device->getDeviceName()));


    if (INDIListener::devices().isEmpty())
    {
        cleanDevices();
        removeTabs();
    }
}

void Manager::processDeleteProperty(INDI::Property prop)
{
    ekosLiveClient.get()->message()->processDeleteProperty(prop);
}

void Manager::processMessage(int id)
{
    auto origin = static_cast<ISD::GenericDevice *>(sender());
    // Shouldn't happen
    if (!origin)
        return;
    QSharedPointer<ISD::GenericDevice> device;
    if (!INDIListener::findDevice(origin->getDeviceName(), device))
        return;

    ekosLiveClient.get()->message()->processMessage(device, id);
}

void Manager::processUpdateProperty(INDI::Property prop)
{
    ekosLiveClient.get()->message()->processUpdateProperty(prop);

    if (prop.isNameMatch("CCD_INFO") ||
            prop.isNameMatch("GUIDER_INFO") ||
            prop.isNameMatch("CCD_FRAME") ||
            prop.isNameMatch("GUIDER_FRAME"))
    {
        if (focusModule() != nullptr)
            focusModule()->syncCameraInfo(prop.getDeviceName());

        if (guideModule() != nullptr && guideModule()->camera() == prop.getDeviceName())
            guideModule()->syncCameraInfo();

        if (alignModule() != nullptr && alignModule()->camera() == prop.getDeviceName())
            alignModule()->syncCameraInfo();

        return;
    }
}

void Manager::processNewProperty(INDI::Property prop)
{
    QSharedPointer<ISD::GenericDevice> device;
    if (!INDIListener::findDevice(prop.getDeviceName(), device))
        return;

    settleTimer.start();

    ekosLiveClient.get()->message()->processNewProperty(prop);

    if (prop.isNameMatch("DEVICE_PORT_SCAN") || prop.isNameMatch("CONNECTION_TYPE"))
    {
        if (!m_PortSelector)
        {
            m_PortSelector.reset(new Selector::Dialog(KStars::Instance()));
            connect(m_PortSelector.get(), &Selector::Dialog::accepted, this, &Manager::setPortSelectionComplete);
        }
        m_PortSelectorTimer.start();
        portSelectorB->setEnabled(true);
        m_PortSelector->addDevice(device);
        return;
    }

    // Check if we need to turn on DEBUG for logging purposes
    if (prop.isNameMatch("DEBUG"))
    {
        uint16_t interface = device->getDriverInterface();
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Check if we need to enable debug logging for the INDI drivers.
            auto debugSP = prop.getSwitch();
            debugSP->at(0)->setState(ISS_ON);
            debugSP->at(1)->setState(ISS_OFF);
            device->sendNewProperty(debugSP);
        }
        return;
    }

    // Handle debug levels for logging purposes
    if (prop.isNameMatch("DEBUG_LEVEL"))
    {
        uint16_t interface = device->getDriverInterface();
        // Check if the logging option for the specific device class is on and if the device interface matches it.
        if ( opsLogs->getINDIDebugInterface() & interface )
        {
            // Turn on everything
            auto debugLevel = prop.getSwitch();
            for (auto &it : *debugLevel)
                it.setState(ISS_ON);

            device->sendNewProperty(debugLevel);
        }
        return;
    }

    if (prop.isNameMatch("ASTROMETRY_SOLVER"))
    {
        for (auto &oneDevice : INDIListener::devices())
        {
            if (oneDevice->getDeviceName() == prop.getDeviceName())
            {
                initAlign();
                alignModule()->setAstrometryDevice(oneDevice);
                break;
            }
        }

        return;
    }

    if (focusModule() != nullptr && strstr(prop.getName(), "FOCUS_"))
    {
        focusModule()->checkFocusers();
        return;
    }
}

void Manager::processTabChange()
{
    auto currentWidget = toolsWidget->currentWidget();

    if (alignProcess && alignModule() == currentWidget)
    {
        auto alignReady = alignModule()->isEnabled() == false && alignModule()->isParserOK();
        auto captureReady = captureProcess && captureModule()->isEnabled();
        auto mountReady = mountProcess && mountModule()->isEnabled();
        if (alignReady && captureReady && mountReady)
            alignModule()->setEnabled(true);

        alignModule()->checkCamera();
    }
    else if (captureProcess && currentWidget == captureModule())
    {
        captureModule()->process()->checkCamera();
    }
    else if (focusProcess && currentWidget == focusModule())
    {
        focusModule()->checkCameras();
    }
    else if (guideProcess && currentWidget == guideModule())
    {
        guideModule()->checkCamera();
    }

    updateLog();
}

void Manager::updateLog()
{
    QWidget * currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
        ekosLogOut->setPlainText(m_LogText.join("\n"));
    else if (currentWidget == alignModule())
        ekosLogOut->setPlainText(alignModule()->getLogText());
    else if (currentWidget == captureModule())
        ekosLogOut->setPlainText(captureModule()->getLogText());
    else if (currentWidget == focusModule())
        ekosLogOut->setPlainText(focusModule()->getLogText());
    else if (currentWidget == guideModule())
        ekosLogOut->setPlainText(guideModule()->getLogText());
    else if (currentWidget == mountModule())
        ekosLogOut->setPlainText(mountModule()->getLogText());
    else if (currentWidget == schedulerModule())
        ekosLogOut->setPlainText(schedulerModule()->moduleState()->getLogText());
    else if (currentWidget == observatoryProcess.get())
        ekosLogOut->setPlainText(observatoryProcess->getLogText());
    else if (currentWidget == analyzeProcess.get())
        ekosLogOut->setPlainText(analyzeProcess->getLogText());

#ifdef Q_OS_MACOS
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
    else if (currentWidget == alignModule())
        alignModule()->clearLog();
    else if (currentWidget == captureModule())
        captureModule()->clearLog();
    else if (currentWidget == focusModule())
        focusModule()->clearLog();
    else if (currentWidget == guideModule())
        guideModule()->clearLog();
    else if (currentWidget == mountModule())
        mountModule()->clearLog();
    else if (currentWidget == schedulerModule())
        schedulerModule()->process()->clearLog();
    else if (currentWidget == observatoryProcess.get())
        observatoryProcess->clearLog();
    else if (currentWidget == analyzeProcess.get())
        analyzeProcess->clearLog();
}

void Manager::initCapture()
{
    if (captureModule() != nullptr)
        return;

    captureProcess.reset(new Capture());

    emit newModule("Capture");

    // retrieve the meridian flip state machine from the mount module if the module is already present
    if (mountModule() != nullptr)
        captureModule()->setMeridianFlipState(mountModule()->getMeridianFlipState());

    capturePreview->shareCaptureModule(captureModule());
    int index = addModuleTab(EkosModule::Capture, captureModule(), QIcon(":/icons/ekos_ccd.png"));
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
    connect(captureModule(), &Ekos::Capture::newLog, this, &Ekos::Manager::updateLog);
    connect(captureModule(), &Ekos::Capture::newLog, this, [this]()
    {
        QJsonObject cStatus =
        {
            {"log", captureModule()->getLogText()}
        };

        ekosLiveClient.get()->message()->updateCaptureStatus(cStatus);
    });
    connect(captureModule(), &Ekos::Capture::newStatus, this, &Ekos::Manager::updateCaptureStatus);
    connect(captureModule(), &Ekos::Capture::newImage, this, &Ekos::Manager::updateCaptureProgress);
    connect(captureModule(), &Ekos::Capture::driverTimedout, this, &Ekos::Manager::restartDriver);
    connect(captureModule(), &Ekos::Capture::newExposureProgress, this, &Ekos::Manager::updateExposureProgress);
    capturePreview->setEnabled(true);

    // display capture status changes
    connect(captureModule(), &Ekos::Capture::newFilterStatus, capturePreview->captureStatusWidget,
            &LedStatusWidget::setFilterState);

    // display target drift
    connect(schedulerModule(), &Ekos::Scheduler::targetDistance,
            captureModule(), &Ekos::Capture::updateTargetDistance,  Qt::UniqueConnection);
    connect(schedulerModule(), &Ekos::Scheduler::targetDistance, this, [this](double distance)
    {
        capturePreview->updateTargetDistance(distance);
    });


    connectModules();
}

void Manager::initAlign()
{
    if (alignModule() != nullptr)
        return;

    alignProcess.reset(new Ekos::Align(m_CurrentProfile));

    emit newModule("Align");

    int index = addModuleTab(EkosModule::Align, alignModule(), QIcon(":/icons/ekos_align.png"));
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Align"));
    connect(alignModule(), &Ekos::Align::newLog, this, &Ekos::Manager::updateLog);
    connect(alignModule(), &Ekos::Align::newLog, this, [this]()
    {
        QJsonObject cStatus =
        {
            {"log", alignModule()->getLogText()}
        };

        ekosLiveClient.get()->message()->updateAlignStatus(cStatus);
    });
    connect(alignModule(), &Ekos::Align::newDownloadProgress, this, [this](QString info)
    {
        QJsonObject cStatus =
        {
            {"downloadProgress", info}
        };

        ekosLiveClient.get()->message()->updateAlignStatus(cStatus);
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

    connectModules();
}

void Manager::initFocus()
{
    if (focusModule() != nullptr)
        return;

    focusProcess.reset(new Ekos::FocusModule());

    emit newModule("Focus");

    int index    = addModuleTab(EkosModule::Focus, focusModule(), QIcon(":/icons/ekos_focus.png"));

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Focus"));

    // Focus <---> Manager connections (restricted to the main focuser)
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newStatus, this, &Ekos::Manager::updateFocusStatus);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newStarPixmap, focusProgressWidget,
            &Ekos::FocusProgressWidget::updateFocusStarPixmap);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newHFR, this, &Ekos::Manager::updateCurrentHFR);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::focuserTimedout, this, &Ekos::Manager::restartDriver);
    connect(focusModule(), &Ekos::FocusModule::newLog, this, [this]()
    {
        // update the logging in the client
        updateLog();

        QJsonObject cStatus =
        {
            {"log", focusModule()->getLogText()}
        };

        ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
    });
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newFocusAdvisorMessage, this, [this](const QString & message)
    {
        QJsonObject cStatus =
        {
            {"focusAdvisorMessage", message}
        };

        ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
    });
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newFocusAdvisorStage, ekosLiveClient.get()->message(),
            [this](int stage)
    {
        QJsonObject cStatus =
        {
            {"focusAdvisorStage", stage}
        };

        ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
    });


    // connect HFR plot widget
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::initHFRPlot, [this](QString str, double starUnits, bool minimum,
            bool useWeights,
            bool showPosition)
    {
        focusProgressWidget->hfrVPlot->init(str, starUnits, minimum, useWeights, showPosition);
        QJsonObject cStatus =
        {
            {"focusinitHFRPlot", true}
        };

        ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
    });

    // Update title
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::setTitle, [this](const QString & title, bool plot)
    {
        focusProgressWidget->hfrVPlot->setTitle(title, plot);
        QJsonObject cStatus =
        {
            {"title", title}
        };

        ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
    });
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::setTitle, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::setTitle);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::redrawHFRPlot, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::redraw);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newHFRPlotPosition, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::addPosition);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::drawPolynomial, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::drawPolynomial);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::finalUpdates, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::finalUpdates);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::minimumFound, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::drawMinimum);
    // setup signal/slots for Linear 1 Pass focus algo
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::drawCurve, focusProgressWidget->hfrVPlot,
            &FocusHFRVPlot::drawCurve);
    connect(focusModule()->mainFocuser().get(), &Ekos::Focus::drawCFZ, focusProgressWidget->hfrVPlot, &FocusHFRVPlot::drawCFZ);

    if (Options::ekosLeftIcons())
    {
        QTransform trans;
        trans.rotate(90);
        QIcon icon  = toolsWidget->tabIcon(index);
        QPixmap pix = icon.pixmap(QSize(48, 48));
        icon        = QIcon(pix.transformed(trans));
        toolsWidget->setTabIcon(index, icon);
    }

    focusProgressWidget->init();
    focusProgressWidget->setEnabled(true);

    for (auto &oneDevice : INDIListener::devices())
    {
        auto prop1  = oneDevice->getProperty("CCD_TEMPERATURE");
        auto prop2  = oneDevice->getProperty("FOCUSER_TEMPERATURE");
        auto prop3  = oneDevice->getProperty("WEATHER_PARAMETERS");
        if (prop1 || prop2 || prop3)
            focusModule()->addTemperatureSource(oneDevice);
    }

    connectModules();
}

void Manager::updateCurrentHFR(double newHFR, int position, bool inAutofocus)
{
    Q_UNUSED(inAutofocus);
    focusProgressWidget->updateCurrentHFR(newHFR);

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
    if (mountModule() != nullptr)
        return;

    mountProcess.reset(new Ekos::Mount());

    // share the meridian flip state with capture if the module is already present
    if (captureModule() != nullptr)
        captureModule()->setMeridianFlipState(mountModule()->getMeridianFlipState());

    emit newModule("Mount");

    int index    = addModuleTab(EkosModule::Mount, mountModule(), QIcon(":/icons/ekos_mount.png"));

    toolsWidget->tabBar()->setTabToolTip(index, i18n("Mount"));
    connect(mountModule(), &Ekos::Mount::newLog, this, &Ekos::Manager::updateLog);
    connect(mountModule(), &Ekos::Mount::newCoords, this, &Ekos::Manager::updateMountCoords);
    connect(mountModule(), &Ekos::Mount::newStatus, this, &Ekos::Manager::updateMountStatus);
    connect(mountModule(), &Ekos::Mount::newTargetName, this, [this](const QString & name)
    {
        setTarget(name);
    });
    connect(mountModule(), &Ekos::Mount::pierSideChanged, this, [&](ISD::Mount::PierSide side)
    {
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"pierSide", side}}));
    });
    connect(mountModule()->getMeridianFlipState().get(),
            &Ekos::MeridianFlipState::newMountMFStatus, [&](MeridianFlipState::MeridianFlipMountState status)
    {
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject(
        {
            {"meridianFlipStatus", status},
        }));
    });
    connect(mountModule()->getMeridianFlipState().get(),
            &Ekos::MeridianFlipState::newMeridianFlipMountStatusText, [&](const QString & text)
    {
        // Throttle this down
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject(
        {
            {"meridianFlipText", text},
        }), mountModule()->getMeridianFlipState()->getMeridianFlipMountState() == MeridianFlipState::MOUNT_FLIP_NONE);
        meridianFlipStatusWidget->setStatus(text);
    });
    connect(mountModule(), &Ekos::Mount::autoParkCountdownUpdated, this, [&](const QString & text)
    {
        ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"autoParkCountdown", text}}), true);
    });

    connect(mountModule(), &Ekos::Mount::trainChanged, ekosLiveClient.get()->message(),
            &EkosLive::Message::sendTrainProfiles, Qt::UniqueConnection);

    connect(mountModule(), &Ekos::Mount::slewRateChanged, this, [&](int slewRate)
    {
        QJsonObject status = { { "slewRate", slewRate} };
        ekosLiveClient.get()->message()->updateMountStatus(status);
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

    mountGroup->setEnabled(true);
    capturePreview->shareMountModule(mountModule());

    connectModules();
}

void Manager::initGuide()
{
    if (guideModule() == nullptr)
    {
        guideProcess.reset(new Ekos::Guide());

        emit newModule("Guide");
    }

    if (toolsWidget->indexOf(guideModule()) == -1)
    {
        // if (managedDevices.contains(KSTARS_TELESCOPE) && managedDevices.value(KSTARS_TELESCOPE)->isConnected())
        // guideProcess->addMount(managedDevices.value(KSTARS_TELESCOPE));

        int index = addModuleTab(EkosModule::Guide, guideModule(), QIcon(":/icons/ekos_guide.png"));
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Guide"));
        connect(guideModule(), &Ekos::Guide::newLog, this, &Ekos::Manager::updateLog);
        connect(guideModule(), &Ekos::Guide::driverTimedout, this, &Ekos::Manager::restartDriver);

        guideManager->setEnabled(true);

        connect(guideModule(), &Ekos::Guide::newStatus, this, &Ekos::Manager::updateGuideStatus);
        connect(guideModule(), &Ekos::Guide::newStarPixmap, guideManager, &Ekos::GuideManager::updateGuideStarPixmap);
        connect(guideModule(), &Ekos::Guide::newAxisSigma, this, &Ekos::Manager::updateSigmas);
        connect(guideModule(), &Ekos::Guide::newAxisDelta, [&](double ra, double de)
        {
            QJsonObject status = { { "drift_ra", ra}, {"drift_de", de} };
            ekosLiveClient.get()->message()->updateGuideStatus(status);
        });
        connect(guideModule(), &Ekos::Guide::newLog, ekosLiveClient.get()->message(),
                [this]()
        {
            QJsonObject cStatus =
            {
                {"log", guideModule()->getLogText()}
            };

            ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
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
        guideManager->init(guideModule());
    }

    connectModules();
}

void Manager::initObservatory()
{
    if (observatoryProcess.get() == nullptr)
    {
        // Initialize the Observatory Module
        observatoryProcess.reset(new Ekos::Observatory());

        emit newModule("Observatory");

        int index = addModuleTab(EkosModule::Observatory, observatoryProcess.get(), QIcon(":/icons/ekos_observatory.png"));
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Observatory"));
        connect(observatoryProcess.get(), &Ekos::Observatory::newLog, this, &Ekos::Manager::updateLog);

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
}

void Manager::addGuider(ISD::Guider * device)
{
    appendLogText(i18n("Guider port from %1 is ready.", device->getDeviceName()));
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
    observatoryProcess.reset();

    connect(toolsWidget, &QTabWidget::currentChanged, this, &Ekos::Manager::processTabChange, Qt::UniqueConnection);
}

bool Manager::isRunning(const QString &process)
{
    QProcess ps;
#ifdef Q_OS_MACOS
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
    if (schedulerModule() != nullptr)
        schedulerModule()->addObject(object);
}

QString Manager::getCurrentJobName()
{
    return schedulerModule()->getCurrentJobName();
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
    if (getCurrentProfile(m_CurrentProfile))
    {
        editor.setPi(m_CurrentProfile);
        editor.setSettings(profileInfo);
        editor.saveProfile();
    }
}

void Manager::addNamedProfile(const QJsonObject &profileInfo)
{
    ProfileEditor editor(this);

    editor.setSettings(profileInfo);
    editor.saveProfile();
    profiles.clear();
    loadProfiles();
    profileCombo->setCurrentIndex(profileCombo->count() - 1);
    getCurrentProfile(m_CurrentProfile);
}

void Manager::deleteNamedProfile(const QString &name)
{
    if (!getCurrentProfile(m_CurrentProfile))
        return;

    for (auto &pi : profiles)
    {
        // Do not delete an actively running profile
        // Do not delete simulator profile
        if (pi->name == "Simulators" || pi->name != name || (pi.get() == m_CurrentProfile && ekosStatus() != Idle))
            continue;

        KStarsData::Instance()->userdb()->PurgeProfile(pi);
        profiles.clear();
        loadProfiles();
        getCurrentProfile(m_CurrentProfile);
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

    getCurrentProfile(m_CurrentProfile);
}

void Manager::editProfile()
{
    ProfileEditor editor(this);

    if (getCurrentProfile(m_CurrentProfile))
    {

        editor.setPi(m_CurrentProfile);

        if (editor.exec() == QDialog::Accepted)
        {
            int currentIndex = profileCombo->currentIndex();

            profiles.clear();
            loadProfiles();
            profileCombo->setCurrentIndex(currentIndex);
        }

        getCurrentProfile(m_CurrentProfile);
    }
}

void Manager::deleteProfile()
{
    if (!getCurrentProfile(m_CurrentProfile))
        return;

    if (m_CurrentProfile->name == "Simulators")
        return;

    auto executeDeleteProfile = [&]()
    {
        KStarsData::Instance()->userdb()->PurgeProfile(m_CurrentProfile);
        profiles.clear();
        loadProfiles();
        getCurrentProfile(m_CurrentProfile);
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

    getCurrentProfile(m_CurrentProfile);
}

bool Manager::getCurrentProfile(QSharedPointer<ProfileInfo> &profile) const
{
    // Get current profile
    for (auto &pi : profiles)
    {
        if (profileCombo->currentText() == pi->name)
        {
            profile = pi;
            return true;
        }
    }

    return false;
}

void Manager::updateProfileLocation(const QSharedPointer<ProfileInfo> &profile)
{
    if (profile->city.isEmpty() == false)
    {
        bool cityFound = KStars::Instance()->setGeoLocation(profile->city, profile->province, profile->country);
        if (cityFound)
            appendLogText(i18n("Site location updated to %1.", KStarsData::Instance()->geo()->fullName()));
        else
            appendLogText(i18n("Failed to update site location to %1. City not found.",
                               KStarsData::Instance()->geo()->fullName()));
    }
}

void Manager::updateMountStatus(ISD::Mount::Status status)
{
    static ISD::Mount::Status lastStatus = ISD::Mount::MOUNT_IDLE;

    if (status == lastStatus)
        return;

    lastStatus = status;

    mountStatus->setMountState(mountModule()->statusString(), status);
    mountStatus->setStyleSheet(QString());

    QJsonObject cStatus =
    {
        {"status", mountModule()->statusString(false)}
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus);
}

void Manager::updateMountCoords(const SkyPoint position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    Q_UNUSED(pierSide)
    raOUT->setText(position.ra().toHMSString());
    decOUT->setText(position.dec().toDMSString());
    azOUT->setText(position.az().toDMSString());
    altOUT->setText(position.alt().toDMSString());

    QJsonObject cStatus =
    {
        {"ra", dms::fromString(raOUT->text(), false).Degrees()},
        {"de", dms::fromString(decOUT->text(), true).Degrees()},
        {"ra0", position.ra0().Degrees()},
        {"de0", position.dec0().Degrees()},
        {"az", dms::fromString(azOUT->text(), true).Degrees()},
        {"at", dms::fromString(altOUT->text(), true).Degrees()},
        {"ha", ha.Degrees()},
    };

    ekosLiveClient.get()->message()->updateMountStatus(cStatus, true);
}

void Manager::updateCaptureStatus(Ekos::CaptureState status, const QString &trainname)
{
    capturePreview->updateCaptureStatus(status, captureModule()->isActiveJobPreview(), trainname);

    switch (status)
    {
        case Ekos::CAPTURE_IDLE:
        /* Fall through */
        case Ekos::CAPTURE_ABORTED:
        /* Fall through */
        case Ekos::CAPTURE_COMPLETE:
            m_CountdownTimer.stop();
            break;
        case Ekos::CAPTURE_CAPTURING:
            m_CountdownTimer.start();
            break;
        default:
            break;
    }

    QJsonObject cStatus =
    {
        {"status", QString::fromLatin1(captureStates[status].untranslatedText())},
        {"seqt", capturePreview->captureCountsWidget->sequenceRemainingTime->text()},
        {"ovt", capturePreview->captureCountsWidget->overallRemainingTime->text()},
        {"train", trainname}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(cStatus);
}

void Manager::updateCaptureProgress(const QSharedPointer<SequenceJob> &job, const QSharedPointer<FITSData> &data,
                                    const QString &trainname)
{
    capturePreview->updateJobProgress(job, data, trainname);

    QJsonObject status =
    {
        {"seqv", job->getCompleted()},
        {"seqr", job->getCoreProperty(SequenceJob::SJ_Count).toInt()},
        {"seql", capturePreview->captureCountsWidget->sequenceRemainingTime->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);

    if (data && job->getStatus() == JOB_BUSY)
    {
        // Normally FITS Viewer would trigger an upload
        // If off, then rely on summary view or raw data
        if (Options::useFITSViewer() == false)
            ekosLiveClient.get()->media()->sendData(data, data->objectName());

        if (job->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            ekosLiveClient.get()->cloud()->sendData(data, data->objectName());
    }
}

void Manager::updateExposureProgress(const QSharedPointer<SequenceJob> &job, const QString &trainname)
{
    QJsonObject status
    {
        {"expv", job->getExposeLeft()},
        {"expr", job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble()},
        {"train", trainname}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}

void Manager::updateCaptureCountDown()
{
    capturePreview->updateCaptureCountDown(-1);

    QJsonObject status =
    {
        {"seqt", capturePreview->captureCountsWidget->sequenceRemainingTime->text()},
        {"ovt", capturePreview->captureCountsWidget->overallRemainingTime->text()},
        {"ovp", capturePreview->captureCountsWidget->gr_overallProgressBar->value()},
        {"ovl", capturePreview->captureCountsWidget->gr_overallLabel->text()}
    };

    ekosLiveClient.get()->message()->updateCaptureStatus(status);
}


void Manager::updateFocusStatus(Ekos::FocusState status)
{
    focusProgressWidget->updateFocusStatus(status);

    QJsonObject cStatus =
    {
        {"status", getFocusStatusString(status, false)}
    };

    ekosLiveClient.get()->message()->updateFocusStatus(cStatus);
}

void Manager::updateGuideStatus(Ekos::GuideState status)
{
    guideManager->updateGuideStatus(status);
    QJsonObject cStatus =
    {
        {"status", getGuideStatusString(status, false)}
    };

    ekosLiveClient.get()->message()->updateGuideStatus(cStatus);
}

void Manager::setTarget(const QString &name)
{
    capturePreview->targetLabel->setVisible(!name.isEmpty());
    capturePreview->mountTarget->setVisible(!name.isEmpty());
    capturePreview->mountTarget->setText(name);
    ekosLiveClient.get()->message()->updateMountStatus(QJsonObject({{"target", name}}));
    // forward it to the mount tab
    if (mountModule())
        mountModule()->setTargetName(name);
}

void Manager::showEkosOptions()
{
    QWidget * currentWidget = toolsWidget->currentWidget();

    if (alignModule() && alignModule() == currentWidget)
    {
        KConfigDialog * alignSettings = KConfigDialog::exists("alignsettings");
        if (alignSettings)
        {
            alignSettings->setEnabled(true);
            alignSettings->show();
        }
        return;
    }

    if (guideModule() && guideModule() == currentWidget)
    {
        KConfigDialog::showDialog("guidesettings");
        return;
    }

    if (focusModule() && focusModule() == currentWidget)
    {
        focusModule()->showOptions();
        return;
    }

    if (schedulerModule() == currentWidget)
    {
        KConfigDialog * cDialog = KConfigDialog::exists("schedulersettings");
        if(cDialog)
        {
            cDialog->show();
            cDialog->raise();  // for MacOS
            cDialog->activateWindow(); // for Windows
        }
        return;
    }

    if(captureModule() == currentWidget)
    {
        KConfigDialog * cDialog = KConfigDialog::exists("capturesettings");
        if(cDialog)
        {
            cDialog->show();
            cDialog->raise();  // for MacOS
            cDialog->activateWindow(); // for Windows
        }
        return;
    }

    const bool isAnalyze = (analyzeProcess.get() && analyzeProcess.get() == currentWidget);
    if (isAnalyze)
    {
        if (opsEkos)
        {
            int index = 0;
            if (isAnalyze) index = 1;
            opsEkos->setCurrentIndex(index);
        }
        KConfigDialog * cDialog = KConfigDialog::exists("settings");
        if (cDialog)
        {
            cDialog->setCurrentPage(ekosOptionsWidget);
            cDialog->show();
            cDialog->raise();  // for MacOS
            cDialog->activateWindow(); // for Windows
        }
        return;
    }

    if (ekosOptionsWidget == nullptr)
    {
        optionsB->click();
    }
    else if (KConfigDialog::showDialog("settings"))
    {
        KConfigDialog * cDialog = KConfigDialog::exists("settings");
        if (cDialog)
        {
            cDialog->setCurrentPage(ekosOptionsWidget);
            cDialog->show();
            cDialog->raise();  // for MacOS
            cDialog->activateWindow(); // for Windows
        }
    }
}

void Manager::updateDebugInterfaces()
{
    KSUtils::Logging::SyncFilterRules();

    for (auto &device : INDIListener::devices())
    {
        auto debugProp = device->getProperty("DEBUG");
        if (!debugProp)
            continue;

        auto debugSP = debugProp.getSwitch();

        // Check if the debug interface matches the driver device class
        if ( ( opsLogs->getINDIDebugInterface() & device->getDriverInterface() ) &&
                debugSP->sp[0].s != ISS_ON)
        {
            debugSP->at(0)->setState(ISS_ON);
            debugSP->at(1)->setState(ISS_OFF);
            device->sendNewProperty(debugSP);
            appendLogText(i18n("Enabling debug logging for %1...", device->getDeviceName()));
        }
        else if ( !( opsLogs->getINDIDebugInterface() & device->getDriverInterface() ) &&
                  debugSP->sp[0].s != ISS_OFF)
        {
            debugSP->at(0)->setState(ISS_OFF);
            debugSP->at(1)->setState(ISS_ON);
            device->sendNewProperty(debugSP);
            appendLogText(i18n("Disabling debug logging for %1...", device->getDeviceName()));
        }

        if (opsLogs->isINDISettingsChanged())
            device->setConfig(SAVE_CONFIG);
    }
}

void Manager::watchDebugProperty(INDI::Property prop)
{
    if (prop.isNameMatch("DEBUG"))
    {
        auto svp = prop.getSwitch();

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
            deviceInterface->sendNewProperty(svp);
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
            deviceInterface->sendNewProperty(svp);
            appendLogText(i18n("Re-disabling debug logging for %1...", deviceInterface->getDeviceName()));
        }
    }
}

void Manager::announceEvent(const QString &message, KSNotification::EventSource source, KSNotification::EventType event)
{
    ekosLiveClient.get()->message()->sendEvent(message, source, event);
}

void Manager::connectModules()
{
    // Dark Library
    connect(DarkLibrary::Instance(), &DarkLibrary::newImage, ekosLiveClient.get()->media(),
            &EkosLive::Media::sendDarkLibraryData, Qt::UniqueConnection);
    connect(DarkLibrary::Instance(), &DarkLibrary::trainChanged, ekosLiveClient.get()->message(),
            &EkosLive::Message::sendTrainProfiles, Qt::UniqueConnection);
    connect(DarkLibrary::Instance(), &DarkLibrary::newFrame, ekosLiveClient.get()->media(), &EkosLive::Media::sendModuleFrame,
            Qt::UniqueConnection);
    connect(DarkLibrary::Instance(), &DarkLibrary::settingsUpdated, ekosLiveClient.get()->message(),
            &EkosLive::Message::sendGuideSettings, Qt::UniqueConnection);

    // Guide <---> Capture connections
    if (captureProcess && guideProcess)
    {
        //        captureProcess.get()->disconnect(guideProcess.get());
        //        guideProcess.get()->disconnect(captureProcess.get());

        // Guide Limits
        connect(guideModule(), &Ekos::Guide::newStatus, captureModule(), &Ekos::Capture::setGuideStatus,
                Qt::UniqueConnection);
        connect(guideModule(), &Ekos::Guide::newAxisDelta, captureModule(), &Ekos::Capture::setGuideDeviation,
                Qt::UniqueConnection);

        // Dithering
        connect(captureModule(), &Ekos::Capture::dither, guideModule(), &Ekos::Guide::dither, Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::resetNonGuidedDither, guideModule(), &Ekos::Guide::resetNonGuidedDither,
                Qt::UniqueConnection);

        // Guide Head
        connect(captureModule(), &Ekos::Capture::suspendGuiding, guideModule(), &Ekos::Guide::suspend,
                Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::resumeGuiding, guideModule(), &Ekos::Guide::resume,
                Qt::UniqueConnection);
        connect(guideModule(), &Ekos::Guide::guideChipUpdated, captureModule(), &Ekos::Capture::setGuideChip,
                Qt::UniqueConnection);

        // Meridian Flip
        connect(captureModule(), &Ekos::Capture::meridianFlipStarted, guideModule(), &Ekos::Guide::abort,
                Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::guideAfterMeridianFlip, guideModule(),
                &Ekos::Guide::guideAfterMeridianFlip, Qt::UniqueConnection);
    }

    // Guide <---> Mount connections
    if (guideProcess && mountProcess)
    {
        // Parking
        connect(mountModule(), &Ekos::Mount::newStatus, guideModule(), &Ekos::Guide::setMountStatus,
                Qt::UniqueConnection);
        connect(mountModule(), &Ekos::Mount::newCoords, guideModule(), &Ekos::Guide::setMountCoords,
                Qt::UniqueConnection);

    }

    // Focus <---> Guide connections
    if (guideProcess && focusProcess)
    {
        // Suspend
        connect(focusModule(), &Ekos::FocusModule::suspendGuiding, guideModule(), &Ekos::Guide::suspend,
                Qt::UniqueConnection);
        connect(focusModule(), &Ekos::FocusModule::resumeGuiding, guideModule(), &Ekos::Guide::resume,
                Qt::UniqueConnection);
    }

    // Capture <---> Focus connections
    if (captureProcess && focusProcess)
    {
        // Check focus HFR value and if above threshold parameter, run autoFocus
        connect(captureModule(), &Ekos::Capture::checkFocus, focusModule(), &Ekos::FocusModule::checkFocus,
                Qt::UniqueConnection);

        // Run autoFocus
        connect(captureProcess.get(), &Ekos::Capture::runAutoFocus, focusModule(), &Ekos::FocusModule::runAutoFocus,
                Qt::UniqueConnection);

        // Reset Frame
        connect(captureModule(), &Ekos::Capture::resetFocusFrame, focusModule(), &Ekos::FocusModule::resetFrame,
                Qt::UniqueConnection);

        // Abort Focus
        connect(captureModule(), &Ekos::Capture::abortFocus, focusModule(), &Ekos::FocusModule::abort, Qt::UniqueConnection);

        // New Focus Status
        connect(focusModule(), &Ekos::FocusModule::newStatus, captureModule(), &Ekos::Capture::setFocusStatus,
                Qt::UniqueConnection);

        // Perform adaptive focus
        connect(captureModule(), &Ekos::Capture::adaptiveFocus, focusModule(), &Ekos::FocusModule::adaptiveFocus,
                Qt::UniqueConnection);

        // New Adaptive Focus Status
        connect(focusModule(), &Ekos::FocusModule::focusAdaptiveComplete, captureModule(), &Ekos::Capture::focusAdaptiveComplete,
                Qt::UniqueConnection);

        // New Focus HFR
        connect(focusModule(), &Ekos::FocusModule::newHFR, captureModule(), &Ekos::Capture::setHFR, Qt::UniqueConnection);

        // New Focus temperature delta
        connect(focusModule(), &Ekos::FocusModule::newFocusTemperatureDelta, captureModule(),
                &Ekos::Capture::setFocusTemperatureDelta, Qt::UniqueConnection);

        // User requested AF
        connect(focusModule(), &Ekos::FocusModule::inSequenceAF, captureModule(),
                &Ekos::Capture::inSequenceAFRequested, Qt::UniqueConnection);

        // Meridian Flip
        connect(captureModule(), &Ekos::Capture::meridianFlipStarted, focusModule(), &Ekos::FocusModule::meridianFlipStarted,
                Qt::UniqueConnection);
    }

    // Capture <---> Align connections
    if (captureProcess && alignProcess)
    {
        // Alignment flag
        connect(alignModule(), &Ekos::Align::newStatus, captureModule(), &Ekos::Capture::setAlignStatus,
                Qt::UniqueConnection);
        // Solver data
        connect(alignModule(), &Ekos::Align::newSolverResults, captureModule(),  &Ekos::Capture::setAlignResults,
                Qt::UniqueConnection);
        // Capture Status
        connect(captureModule(), &Ekos::Capture::newStatus, alignModule(), &Ekos::Align::setCaptureStatus,
                Qt::UniqueConnection);
    }

    // Capture <---> Mount connections
    if (captureProcess && mountProcess)
    {
        // Register both modules since both are now created and ready
        // In case one module misses the DBus signal, then it will be correctly initialized.
        captureModule()->registerNewModule("Mount");
        mountModule()->registerNewModule("Capture");

        // Meridian Flip states
        connect(captureModule(), &Ekos::Capture::meridianFlipStarted, mountModule(), &Ekos::Mount::suspendAltLimits,
                Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::guideAfterMeridianFlip, mountModule(), &Ekos::Mount::resumeAltLimits,
                Qt::UniqueConnection);

        // Mount Status
        connect(mountModule(), &Ekos::Mount::newStatus, captureModule(), &Ekos::Capture::setMountStatus,
                Qt::UniqueConnection);
    }

    // Optical Train Manager ---> EkosLive connections
    if (ekosLiveClient)
    {
        connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, ekosLiveClient->message(),
                &EkosLive::Message::sendTrains, Qt::UniqueConnection);
        connect(OpticalTrainManager::Instance(), &OpticalTrainManager::configurationRequested, ekosLiveClient->message(),
                &EkosLive::Message::requestOpticalTrains, Qt::UniqueConnection);
    }

    // Capture <---> EkosLive connections
    if (captureProcess && ekosLiveClient)
    {
        //captureProcess.get()->disconnect(ekosLiveClient.get()->message());

        connect(captureModule(), &Ekos::Capture::dslrInfoRequested, ekosLiveClient.get()->message(),
                &EkosLive::Message::requestDSLRInfo, Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::sequenceChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendCaptureSequence, Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendCaptureSettings, Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::newLocalPreview, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendPreviewLabel, Qt::UniqueConnection);
        connect(captureModule(), &Ekos::Capture::trainChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendTrainProfiles, Qt::UniqueConnection);
    }

    // Focus <---> Align connections
    if (focusProcess && alignProcess)
    {
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newStatus, alignModule(), &Ekos::Align::setFocusStatus,
                Qt::UniqueConnection);
    }

    // Focus <---> Mount connections
    if (focusProcess && mountProcess)
    {
        connect(mountModule(), &Ekos::Mount::newStatus, focusModule(), &Ekos::FocusModule::setMountStatus, Qt::UniqueConnection);
        connect(mountModule(), &Ekos::Mount::newCoords, focusModule(), &Ekos::FocusModule::setMountCoords, Qt::UniqueConnection);
    }

    // Mount <---> Align connections
    if (mountProcess && alignProcess)
    {
        connect(mountModule(), &Ekos::Mount::newStatus, alignModule(), &Ekos::Align::setMountStatus,
                Qt::UniqueConnection);
        connect(mountModule(), &Ekos::Mount::newTarget,  alignModule(), &Ekos::Align::setTarget,
                Qt::UniqueConnection);
        connect(mountModule(), &Ekos::Mount::newCoords,  alignModule(), &Ekos::Align::setTelescopeCoordinates,
                Qt::UniqueConnection);
        connect(alignModule(), &Ekos::Align::newPAAStage, mountModule(), &Ekos::Mount::paaStageChanged,
                Qt::UniqueConnection);
    }

    // Mount <---> Guide connections
    if (mountProcess && guideProcess)
    {
        connect(mountModule(), &Ekos::Mount::pierSideChanged, guideModule(), &Ekos::Guide::setPierSide,
                Qt::UniqueConnection);
    }

    // Align <--> EkosLive connections
    if (alignProcess && ekosLiveClient)
    {
        //        alignProcess.get()->disconnect(ekosLiveClient.get()->message());
        //        alignProcess.get()->disconnect(ekosLiveClient.get()->media());

        connect(alignModule(), &Ekos::Align::newStatus, ekosLiveClient.get()->message(), &EkosLive::Message::setAlignStatus,
                Qt::UniqueConnection);
        connect(alignModule(), &Ekos::Align::newSolution, ekosLiveClient.get()->message(),
                &EkosLive::Message::setAlignSolution, Qt::UniqueConnection);
        connect(alignModule()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::newPAHStage,
                ekosLiveClient.get()->message(), &EkosLive::Message::setPAHStage,
                Qt::UniqueConnection);
        connect(alignModule()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::newPAHMessage,
                ekosLiveClient.get()->message(),
                &EkosLive::Message::setPAHMessage, Qt::UniqueConnection);
        connect(alignModule()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::PAHEnabled,
                ekosLiveClient.get()->message(), &EkosLive::Message::setPAHEnabled,
                Qt::UniqueConnection);
        connect(alignModule()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::polarResultUpdated,
                ekosLiveClient.get()->message(),
                &EkosLive::Message::setPolarResults, Qt::UniqueConnection);
        connect(alignModule()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::updatedErrorsChanged,
                ekosLiveClient.get()->message(),
                &EkosLive::Message::setUpdatedErrors, Qt::UniqueConnection);
        connect(alignModule()->polarAlignmentAssistant(), &Ekos::PolarAlignmentAssistant::newCorrectionVector,
                ekosLiveClient.get()->media(),
                &EkosLive::Media::setCorrectionVector, Qt::UniqueConnection);

        connect(alignModule(), &Ekos::Align::newImage, ekosLiveClient.get()->media(), &EkosLive::Media::sendModuleFrame,
                Qt::UniqueConnection);
        connect(alignModule(), &Ekos::Align::newFrame, ekosLiveClient.get()->media(), &EkosLive::Media::sendUpdatedFrame,
                Qt::UniqueConnection);

        connect(alignModule(), &Ekos::Align::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendAlignSettings, Qt::UniqueConnection);

        connect(alignModule(), &Ekos::Align::trainChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendTrainProfiles, Qt::UniqueConnection);

        connect(alignModule(), &Ekos::Align::manualRotatorChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendManualRotatorStatus, Qt::UniqueConnection);
    }

    // Focus <--> EkosLive Connections
    if (focusProcess && ekosLiveClient)
    {
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendFocusSettings, Qt::UniqueConnection);

        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newImage, ekosLiveClient.get()->media(),
                &EkosLive::Media::sendModuleFrame,
                Qt::UniqueConnection);

        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::trainChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendTrainProfiles,
                Qt::UniqueConnection);

        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::autofocusAborted,
                ekosLiveClient.get()->message(), &EkosLive::Message::autofocusAborted, Qt::UniqueConnection);
    }

    // Guide <--> EkosLive Connections
    if (guideProcess && ekosLiveClient)
    {
        connect(guideModule(), &Ekos::Guide::settingsUpdated, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendGuideSettings, Qt::UniqueConnection);

        connect(guideModule(), &Ekos::Guide::trainChanged, ekosLiveClient.get()->message(),
                &EkosLive::Message::sendTrainProfiles, Qt::UniqueConnection);

        connect(guideModule(), &Ekos::Guide::newImage, ekosLiveClient.get()->media(), &EkosLive::Media::sendModuleFrame,
                Qt::UniqueConnection);
    }

    // Analyze connections.
    if (analyzeProcess)
    {
        // Scheduler <---> Analyze
        connect(schedulerModule(), &Ekos::Scheduler::jobStarted,
                analyzeProcess.get(), &Ekos::Analyze::schedulerJobStarted, Qt::UniqueConnection);
        connect(schedulerModule(), &Ekos::Scheduler::jobEnded,
                analyzeProcess.get(), &Ekos::Analyze::schedulerJobEnded, Qt::UniqueConnection);
        connect(schedulerModule(), &Ekos::Scheduler::targetDistance,
                analyzeProcess.get(), &Ekos::Analyze::newTargetDistance,  Qt::UniqueConnection);

        // Capture <---> Analyze
        if (captureProcess)
        {
            connect(captureModule(), &Ekos::Capture::captureComplete,
                    analyzeProcess.get(), &Ekos::Analyze::captureComplete, Qt::UniqueConnection);
            connect(captureModule(), &Ekos::Capture::captureStarting,
                    analyzeProcess.get(), &Ekos::Analyze::captureStarting, Qt::UniqueConnection);
            connect(captureModule(), &Ekos::Capture::captureAborted,
                    analyzeProcess.get(), &Ekos::Analyze::captureAborted, Qt::UniqueConnection);
#if 0
            // Meridian Flip
            connect(captureModule(), &Ekos::Capture::meridianFlipStarted,
                    analyzeProcess.get(), &Ekos::Analyze::meridianFlipStarted, Qt::UniqueConnection);
            connect(captureModule(), &Ekos::Capture::meridianFlipCompleted,
                    analyzeProcess.get(), &Ekos::Analyze::meridianFlipComplete, Qt::UniqueConnection);
#endif
        }

        // Guide <---> Analyze
        if (guideProcess)
        {
            connect(guideModule(), &Ekos::Guide::newStatus,
                    analyzeProcess.get(), &Ekos::Analyze::guideState, Qt::UniqueConnection);

            connect(guideModule(), &Ekos::Guide::guideStats,
                    analyzeProcess.get(), &Ekos::Analyze::guideStats, Qt::UniqueConnection);
        }
    }


    // Focus <---> Analyze connections
    if (focusProcess && analyzeProcess)
    {
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::autofocusComplete,
                analyzeProcess.get(), &Ekos::Analyze::autofocusComplete, Qt::UniqueConnection);
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::adaptiveFocusComplete,
                analyzeProcess.get(), &Ekos::Analyze::adaptiveFocusComplete, Qt::UniqueConnection);
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::autofocusStarting,
                analyzeProcess.get(), &Ekos::Analyze::autofocusStarting, Qt::UniqueConnection);
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::autofocusAborted,
                analyzeProcess.get(), &Ekos::Analyze::autofocusAborted, Qt::UniqueConnection);
        connect(focusModule()->mainFocuser().get(), &Ekos::Focus::newFocusTemperatureDelta,
                analyzeProcess.get(), &Ekos::Analyze::newTemperature, Qt::UniqueConnection);
    }

    // Align <---> Analyze connections
    if (alignProcess && analyzeProcess)
    {
        connect(alignModule(), &Ekos::Align::newStatus,
                analyzeProcess.get(), &Ekos::Analyze::alignState, Qt::UniqueConnection);

    }

    // Mount <---> Analyze connections
    if (mountProcess && analyzeProcess)
    {
        connect(mountModule(), &Ekos::Mount::newStatus,
                analyzeProcess.get(), &Ekos::Analyze::mountState, Qt::UniqueConnection);
        connect(mountModule(), &Ekos::Mount::newCoords,
                analyzeProcess.get(), &Ekos::Analyze::mountCoords, Qt::UniqueConnection);
        connect(mountModule()->getMeridianFlipState().get(), &Ekos::MeridianFlipState::newMountMFStatus,
                analyzeProcess.get(), &Ekos::Analyze::mountFlipStatus, Qt::UniqueConnection);
    }
}

void Manager::setEkosLiveConnected(bool enabled)
{
    ekosLiveClient.get()->setConnected(enabled);
}

void Manager::setEkosLiveConfig(bool rememberCredentials, bool autoConnect)
{
    ekosLiveClient.get()->setConfig(rememberCredentials, autoConnect);
}

void Manager::setEkosLiveUser(const QString &username, const QString &password)
{
    ekosLiveClient.get()->setUser(username, password);
}

bool Manager::ekosLiveStatus()
{
    return ekosLiveClient.get()->isConnected();
}

bool Manager::checkUniqueBinaryDriver(const QSharedPointer<DriverInfo> &primaryDriver,
                                      const QSharedPointer<DriverInfo> &secondaryDriver)
{
    if (!primaryDriver || !secondaryDriver)
        return false;

    return (primaryDriver->getExecutable() == secondaryDriver->getExecutable() &&
            primaryDriver->getAuxInfo().value("mdpd", false).toBool() == true);
}

void Manager::restartDriver(const QString &deviceName)
{
    qCInfo(KSTARS_EKOS) << "Restarting driver" << deviceName;
    if (m_LocalMode)
    {
        for (auto &oneDevice : INDIListener::devices())
        {
            if (oneDevice->getDeviceName() == deviceName)
            {
                DriverManager::Instance()->restartDriver(oneDevice->getDriverInfo());
                break;
            }
        }
    }
    else
        INDI::WebManager::restartDriver(m_CurrentProfile, deviceName);
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
    if (m_CurrentProfile->portSelector)
    {
        // Turn off port selector
        m_CurrentProfile->portSelector = false;
        KStarsData::Instance()->userdb()->SaveProfile(m_CurrentProfile);
    }

    if (m_CurrentProfile->autoConnect)
        connectDevices();
}

void Manager::activateModule(const QString &name, bool popup)
{
    auto child = toolsWidget->findChild<QWidget *>(name);
    if (child)
    {
        toolsWidget->setCurrentWidget(child);
        if (popup)
        {
            raise();
            activateWindow();
            showNormal();
        }
    }
}

void Manager::createModules(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device->isConnected())
    {
        if (device->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
        {
            initCapture();
            initFocus();
            initAlign();
            initGuide();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::FILTER_INTERFACE)
        {
            initCapture();
            initFocus();
            initAlign();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
            initFocus();
        if (device->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
        {
            initCapture();
            initAlign();
            initGuide();
            initMount();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::ROTATOR_INTERFACE)
        {
            initCapture();
            initAlign();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::DOME_INTERFACE)
        {
            initCapture();
            initAlign();
            initObservatory();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::WEATHER_INTERFACE)
        {
            initFocus();
            initObservatory();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
        {
            initCapture();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::LIGHTBOX_INTERFACE)
        {
            initCapture();
        }
        if (device->getDriverInterface() & INDI::BaseDevice::GPS_INTERFACE)
        {
            initMount();
        }
    }
}

void Manager::setDeviceReady()
{
    auto device = static_cast<ISD::GenericDevice*>(sender());

    // Check if ALL our devices are ready.
    // Ready indicates that all properties have been defined.
    if (isINDIReady() == false)
    {
        if (device)
        {

            if (device->isConnected() == false && m_CurrentProfile->autoConnect)
            {
                // Do we have port selector checked?
                if (m_CurrentProfile->portSelector)
                {
                    // If port selector was not initialized, kick off the timer
                    // so we can check if all devices should be connected.
                    // Otherwise, if port selector is started, then let user
                    // select ports first and then manually connect time.
                    if (!m_PortSelector)
                        m_PortSelectorTimer.start();
                }
                else
                {
                    qCInfo(KSTARS_EKOS) << "Connecting to" << device->getDeviceName();
                    device->Connect();
                }
            }
            else
                qCInfo(KSTARS_EKOS) << device->getDeviceName() << "is connected and ready.";
        }

        if (m_ekosStatus != Ekos::Success)
            return;
    }
    // Some device (e.g. Toupcam) can take some time after conncting to full define
    // all their properties, so we need to wait until all properties are defined before
    // we issue a load config
    else if (device && Options::neverLoadConfig() == false)
    {
        // If device is ready, and is connected, and config was not loaded, then load it now.
        if (device && device->isConnected() && device->wasConfigLoaded() == false)
        {
            INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

            for (auto &oneDevice : INDIListener::devices())
            {
                if (oneDevice == device)
                {
                    connect(device, &ISD::GenericDevice::propertyUpdated, this, &Ekos::Manager::watchDebugProperty, Qt::UniqueConnection);

                    auto configProp = device->getBaseDevice().getSwitch("CONFIG_PROCESS");
                    if (configProp && configProp.getState() == IPS_IDLE)
                        device->setConfig(tConfig);
                    break;
                }
            }
        }
    }

    // If port selector is active, then do not show optical train dialog unless it is dismissed first.
    if (m_DriverDevicesCount <= 0 && (m_CurrentProfile->portSelector == false || !m_PortSelector))
    {
        for (auto &oneDevice : INDIListener::devices())
        {
            // Fix issue reported by Ed Lee (#238) as we only need to sync once per device
            if (oneDevice->getDeviceName() == device->getDeviceName())
                syncGenericDevice(oneDevice);
        }
        OpticalTrainManager::Instance()->setProfile(m_CurrentProfile);
    }
}

void Manager::createFilterManager(ISD::FilterWheel *device)
{
    auto name = device->getDeviceName();
    if (m_FilterManagers.contains(name) == false)
    {
        QSharedPointer<FilterManager> newFM(new FilterManager(this));
        newFM->setFilterWheel(device);
        m_FilterManagers.insert(name, std::move(newFM));
    }
    else
        m_FilterManagers[name]->setFilterWheel(device);

}

bool Manager::getFilterManager(const QString &name, QSharedPointer<FilterManager> &fm)
{
    if (m_FilterManagers.contains(name))
    {
        fm = m_FilterManagers[name];
        return true;
    }
    return false;
}

bool Manager::getFilterManager(QSharedPointer<FilterManager> &fm)
{
    if (m_FilterManagers.size() > 0)
    {
        fm = m_FilterManagers.values()[0];
        return true;
    }
    return false;
}

void Manager::createRotatorController(ISD::Rotator *device)
{
    auto Name = device->getDeviceName();
    if (m_RotatorControllers.contains(Name) == false)
    {
        QSharedPointer<RotatorSettings> newRC(new RotatorSettings(this));
        // Properties are fetched in RotatorSettings::initRotator!
        m_RotatorControllers[Name] = newRC;
    }
}

bool Manager::getRotatorController(const QString &Name, QSharedPointer<RotatorSettings> &rs)
{
    if (m_RotatorControllers.contains(Name))
    {
        rs = m_RotatorControllers[Name];
        return true;
    }
    return false;
}

bool Manager::existRotatorController()
{
    return (!m_RotatorControllers.empty());
}

void Manager::setFITSfromFile(bool previewFromFile)
{
    if (previewFromFile && !FITSfromFile)
    {
        // Prevent preview from Capture module
        QObject::disconnect(captureModule(), &Ekos::Capture::newImage, this, &Ekos::Manager::updateCaptureProgress);
        FITSfromFile = previewFromFile;
        appendLogText(i18n("Preview source set to external"));
    }
    else if (!previewFromFile && FITSfromFile)
    {
        // Reset preview from Capture module
        QObject::connect(captureModule(), &Ekos::Capture::newImage, this, &Ekos::Manager::updateCaptureProgress);
        FITSfromFile = previewFromFile;
        appendLogText(i18n("Preview source reset to internal"));
    }
}

void Manager::previewFile(QString filePath)
{
    capturePreview->updateJobPreview(filePath);
    appendLogText(i18n("Received external preview file"));
}
}
