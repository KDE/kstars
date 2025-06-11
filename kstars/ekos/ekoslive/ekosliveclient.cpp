/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Options.h"

#include "ekosliveclient.h"
#include "ekos/manager.h"

#include "kspaths.h"
#include "Options.h"
#include "ekos_debug.h"
#include "QProgressIndicator.h"

#include <config-kstars.h>

#ifdef HAVE_KEYCHAIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <qt6keychain/keychain.h>
#else
#include <qt5keychain/keychain.h>
#endif
#endif

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFormLayout>

namespace EkosLive
{
Client::Client(Ekos::Manager *manager) : QDialog(manager), m_Manager(manager)
{
    setupUi(this);

    connect(closeB, &QPushButton::clicked, this, &Client::close);

    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::AppLocalDataLocation, "ekoslive.png")))
        leftBanner->setPixmap(im);

    pi = new QProgressIndicator(this);
    bottomLayout->insertWidget(1, pi);

    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));

    username->setText(Options::ekosLiveUsername());
    connect(username, &QLineEdit::editingFinished, this, [this]()
    {
        Options::setEkosLiveUsername(username->text());
    });

    // Initialize node managers
    QSharedPointer<NodeManager> onlineManager(new NodeManager(NodeManager::Message | NodeManager::Media | NodeManager::Cloud));
    connect(onlineManager.get(), &NodeManager::authenticationError, this, [this, onlineManager](const QString & message)
    {
        onlineLabel->setToolTip(message);
        if (pi && pi->isAnimated()) pi->stopAnimation();
        onlineManager->setIsReauthenticating(false);
        onDisconnected(); // Refresh overall UI state potentially
    });
    connect(onlineManager.get(), &NodeManager::connected, this, &Client::onConnected);
    connect(onlineManager.get(), &NodeManager::disconnected, this, &Client::onDisconnected);

    QSharedPointer<NodeManager> offlineManager(new NodeManager(NodeManager::Message | NodeManager::Media));

    connect(offlineManager.get(), &NodeManager::authenticationError, this, [this, offlineManager](const QString & message)
    {
        offlineLabel->setToolTip(message);
        if (pi && pi->isAnimated()) pi->stopAnimation();
        offlineManager->setIsReauthenticating(false);
        onDisconnected(); // Refresh overall UI state potentially
    });
    connect(offlineManager.get(), &NodeManager::connected, this, &Client::onConnected);
    connect(offlineManager.get(), &NodeManager::disconnected, this, &Client::onDisconnected);

    m_NodeManagers.append(std::move(onlineManager));
    m_NodeManagers.append(std::move(offlineManager));
    syncURLs();

    connect(selectServersB, &QPushButton::clicked, this, &Client::showSelectServersDialog);
    connect(connectB, &QPushButton::clicked, this, [this]()
    {
        if (m_isConnected) // If fully connected, then this button is "Disconnect"
        {
            m_userRequestedDisconnect = true; // User is clicking "Disconnect"
            for (auto &oneManager : m_NodeManagers)
                oneManager->disconnectNodes();
        }
        else // Button is "Connect"
        {
            m_userRequestedDisconnect = false; // User is clicking "Connect"
            // pi->startAnimation() will be handled by checkAndTriggerAuth if auth is attempted
            checkAndTriggerAuth(true); // Force re-authentication
        }
    });

    connect(password, &QLineEdit::returnPressed, this, [this]()
    {
        if (!m_isConnected)
        {
            // pi->startAnimation() will be handled by checkAndTriggerAuth if auth is attempted
            checkAndTriggerAuth(true); // Force re-authentication
        }
    });

    rememberCredentialsCheck->setChecked(Options::rememberCredentials());
    connect(rememberCredentialsCheck, &QCheckBox::toggled, [ = ](bool toggled)
    {
        Options::setRememberCredentials(toggled);
    });
    autoStartCheck->setChecked(Options::autoStartEkosLive());
    connect(autoStartCheck, &QCheckBox::toggled, [ = ](bool toggled)
    {
        Options::setAutoStartEkosLive(toggled);
    });

#ifdef HAVE_KEYCHAIN
    QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(QLatin1String("kstars"));
    job->setAutoDelete(false);
    job->setKey(QLatin1String("ekoslive"));
    connect(job, &QKeychain::Job::finished, this, [&](QKeychain::Job * job)
    {
        if (job->error() == false)
        {
            const auto passwordText = dynamic_cast<QKeychain::ReadPasswordJob*>(job)->textData().toLatin1();

            // Only set and attempt connection if the data is not empty
            if (passwordText.isEmpty() == false && username->text().isEmpty() == false)
            {
                password->setText(passwordText);
                if (autoStartCheck->isChecked())
                {
                    qCInfo(KSTARS_EKOS) << "Attempting auto-start EkosLive connection.";
                    // pi->startAnimation() will be handled by checkAndTriggerAuth if auth is attempted
                    checkAndTriggerAuth();
                }
            }
        }
        else
        {
            if (autoStartCheck->isChecked())
                qCWarning(KSTARS_EKOS) << "Failed to automatically connect due to missing EkosLive credentials:" << job->errorString();
        }
        job->deleteLater();
    });
    job->start();
#endif

    m_Message = new Message(m_Manager, m_NodeManagers);
    connect(m_Message, &Message::connected, this, &Client::onConnected);
    connect(m_Message, &Message::disconnected, this, &Client::onDisconnected);

    // Connect the new global logout trigger signal
    connect(m_Message, &Message::globalLogoutTriggered, this, &Client::processGlobalLogoutTrigger);

    m_Media = new Media(m_Manager, m_NodeManagers);
    connect(m_Media, &Media::connected, this, &Client::onConnected);
    connect(m_Media, &Media::disconnected, this, &Client::onDisconnected); // Assuming Media also needs disconnect handling
    connect(m_Media, &Media::globalLogoutTriggered, this, &Client::processGlobalLogoutTrigger); // Connect Media's signal

    m_Cloud = new Cloud(m_Manager, m_NodeManagers);
    connect(m_Cloud, &Cloud::connected, this, &Client::onConnected);
    connect(m_Cloud, &Cloud::disconnected, this, &Client::onDisconnected); // Assuming Cloud also needs disconnect handling
    connect(m_Cloud, &Cloud::globalLogoutTriggered, this, &Client::processGlobalLogoutTrigger); // Connect Cloud's signal
}

void Client::checkAndTriggerAuth(bool force)
{
    bool needsReAuthForAllManagers = false;
    int managersAttemptingAuth = 0;

    for (auto &oneManager : m_NodeManagers)
    {
        // Using a buffer of 5 minutes (300 seconds) for "nearly expired"
        if (oneManager->isTokenNearlyExpired(300))
        {
            qCInfo(KSTARS_EKOS) << "Token for NodeManager" << oneManager->property("serviceURL").toUrl().toDisplayString()
                                << "is nearly expired or missing. Triggering re-authentication.";
            needsReAuthForAllManagers = true; // If one needs it, assume we want to refresh all for simplicity here
            break;
        }
    }

    if (needsReAuthForAllManagers || force)
    {
        qCInfo(KSTARS_EKOS) <<
        "One or more tokens are nearly expired/missing or connection is forced. Re-authenticating all managers.";
        for (auto &oneManager : m_NodeManagers)
        {
            if (oneManager->isReauthenticating() && !force)
            {
                qCInfo(KSTARS_EKOS) << "NodeManager" << oneManager->property("serviceURL").toUrl().toDisplayString() <<
                                       "is already re-authenticating (and not forced). Skipping.";
                managersAttemptingAuth++; // Count it as an attempt in progress
                continue;
            }

            if (force && oneManager->isReauthenticating())
            {
                qCInfo(KSTARS_EKOS) << "Forcing re-authentication for NodeManager"
                                    << oneManager->property("serviceURL").toUrl().toDisplayString()
                                    << "which was already re-authenticating. Resetting state.";
                oneManager->setIsReauthenticating(false); // Reset the state to allow new attempt
            }

            oneManager->clearAuthentication();
            oneManager->setCredentials(username->text(), password->text());
            oneManager->authenticate();
            managersAttemptingAuth++;
        }
    }
    else // Tokens are fine and not forcing connection
    {
        // Tokens are likely fine, but user might be clicking "Connect" because not all nodes are up.
        // Trigger authenticate on any manager that isn't fully connected and isn't already trying.
        qCInfo(KSTARS_EKOS) <<
        "Tokens seem valid and connection not forced. Checking for disconnected managers to trigger authentication.";
        for (auto &oneManager : m_NodeManagers)
        {
            if (!oneManager->isConnected() && !oneManager->isReauthenticating())
            {
                qCInfo(KSTARS_EKOS) << "NodeManager" << oneManager->property("serviceURL").toUrl().toDisplayString()
                                    << "is not connected and not re-authenticating. Triggering authenticate.";
                oneManager->clearAuthentication(); // Ensure clean state
                oneManager->setCredentials(username->text(), password->text());
                oneManager->authenticate();
                managersAttemptingAuth++;
            }
            else if (oneManager->isReauthenticating())
            {
                managersAttemptingAuth++; // Already in progress
            }
        }
    }

    if (managersAttemptingAuth == 0 && !m_isConnected)
    {
        // No auth was triggered (e.g. all tokens fine, all managers connected, but m_isConnected is false somehow)
        // or all managers were already connected and tokens fine.
        // This case might mean we don't need to animate, or UI state is inconsistent.
        // For safety, if nothing is happening and we are not connected, stop animation.
        if (pi && pi->isAnimated()) pi->stopAnimation();
        qCInfo(KSTARS_EKOS) <<
        "checkAndTriggerAuth: No authentication attempts started and client not marked as connected. PI stopped if running.";
    }
    else if (managersAttemptingAuth > 0 && pi && !pi->isAnimated())
    {
        // If any auth attempt started, ensure PI is running.
        pi->startAnimation();
    }
}

Client::~Client()
{
    for (auto &oneManager : m_NodeManagers)
        oneManager->disconnectNodes();
}

void Client::processGlobalLogoutTrigger(const QUrl &url)
{
    qCInfo(KSTARS_EKOS) << "Processing global logout trigger for URL:" << url.toDisplayString();

    bool urlIsEmpty = url.isEmpty();
    bool processedAny = false;

    for (auto &oneManager : m_NodeManagers)
    {
        QUrl managerUrl = oneManager->property("websocketURL").toUrl(); // Assuming this property exists

        bool shouldProcessThisManager = false;
        if (urlIsEmpty) // Should not happen with current emitters, but handle defensively
        {
            shouldProcessThisManager = true;
        }
        else
        {
            // Check if the received URL matches any of the node URLs within this manager
            if (oneManager->message() && oneManager->message()->url() == url) shouldProcessThisManager = true;
            if (!shouldProcessThisManager && oneManager->media() && oneManager->media()->url() == url) shouldProcessThisManager = true;
            if (!shouldProcessThisManager && oneManager->cloud() && oneManager->cloud()->url() == url) shouldProcessThisManager = true;
        }

        if (shouldProcessThisManager)
        {
            if (oneManager->isReauthenticating())
            {
                qCInfo(KSTARS_EKOS) << "NodeManager for URL" << (managerUrl.isValid() ? managerUrl.toDisplayString() : "Unknown") <<
                                       "is already re-authenticating. Skipping trigger.";
                continue; // Skip this manager if already processing
            }

            qCInfo(KSTARS_EKOS) << "EkosLiveClient: Disconnecting and re-authenticating NodeManager triggered by global logout for URL:"
                                <<
                                (oneManager->property("websocketURL").toUrl().isValid() ? oneManager->property("websocketURL").toUrl().toDisplayString() :
            "related to " + url.toDisplayString());
            oneManager->clearAuthentication(); // Clear old token before re-auth
            oneManager->disconnectNodes(); // Disconnect first
            // Consider a small delay if disconnect is not immediate, though authenticate should be fine
            oneManager->setCredentials(username->text(), password->text());
            oneManager->authenticate();
            processedAny = true;
        }
    } // End of for loop

    if (!processedAny && !urlIsEmpty)
    {
        qCWarning(KSTARS_EKOS) << "Global logout trigger for URL" << url.toDisplayString() <<
                                  "did not match any active NodeManager.";
    }
}

void Client::onConnected()
{
    // This slot might be called multiple times as each node connects.
    // We only consider the client fully connected when ALL nodes of ALL managers are connected.
    // However, the NodeManager::isConnected checks if all *its* nodes are connected.
    // We need a check across all managers.

    bool allManagersConnected = true;
    for (const auto &manager : m_NodeManagers)
    {
        if (!manager->isConnected())
        {
            allManagersConnected = false;
            break;
        }
    }

    // Only update UI to fully connected state if all managers report connected
    // and if we weren't already in the fully connected state.
    if (allManagersConnected && !m_isConnected)
    {
        if (pi && pi->isAnimated()) pi->stopAnimation();
        m_isConnected = true;
        connectB->setText(i18n("Disconnect"));
        connectionState->setPixmap(QIcon::fromTheme("state-ok").pixmap(QSize(64, 64)));
        selectServersB->setEnabled(false);

        if (rememberCredentialsCheck->isChecked())
        {
#ifdef HAVE_KEYCHAIN
            QKeychain::WritePasswordJob *job = new QKeychain::WritePasswordJob(QLatin1String("kstars"));
            job->setAutoDelete(true);
            job->setKey(QLatin1String("ekoslive"));
            job->setTextData(password->text());
            job->start();
#endif
        }
        emit connected(); // Emit client connected signal
    }

    // Update individual manager status icons regardless
    auto disconnectedIcon = QIcon(":/icons/AlignFailure.svg").pixmap(QSize(32, 32));
    auto connectedIcon = QIcon(":/icons/AlignSuccess.svg").pixmap(QSize(32, 32));

    // Assuming m_NodeManagers[0] is Online and m_NodeManagers[1] is Offline
    if (m_NodeManagers.size() > 0)
    {
        onlineLabel->setStyleSheet(m_NodeManagers[Online]->isConnected() ? "color:white" : "color:gray");
        onlineIcon->setPixmap(m_NodeManagers[Online]->isConnected() ? connectedIcon : disconnectedIcon);
        if (m_NodeManagers[Online]->isConnected())
            onlineLabel->setToolTip(QString());
    }
    if (m_NodeManagers.size() > 1)
    {
        offlineLabel->setStyleSheet(m_NodeManagers[Offline]->isConnected() ? "color:white" : "color:gray");
        offlineIcon->setPixmap(m_NodeManagers[Offline]->isConnected() ? connectedIcon : disconnectedIcon);
        if (m_NodeManagers[Offline]->isConnected())
            offlineLabel->setToolTip(QString());
    }
}

void Client::onDisconnected()
{
    // This slot might be called multiple times.
    // We only consider the client fully disconnected if ANY manager is not fully connected.

    bool anyManagerDisconnected = false;
    for (const auto &manager : m_NodeManagers)
    {
        if (!manager->isConnected())
        {
            anyManagerDisconnected = true;
            break;
        }
    }

    // If any manager is disconnected and we previously thought we were connected, update state.
    if (anyManagerDisconnected && m_isConnected)
    {
        m_isConnected = false; // Now considered disconnected overall
        connectB->setText(i18n("Connect"));
        connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));
        selectServersB->setEnabled(true);
        emit disconnected(); // Emit client disconnected signal

        bool wasUserDisconnect = m_userRequestedDisconnect;

        if (!wasUserDisconnect)
        {
            qCInfo(KSTARS_EKOS) <<
            "Client::onDisconnected: Overall connection lost (unexpected). Attempting to re-authenticate all managers.";
            checkAndTriggerAuth(); // Proactively attempt to re-establish the session
        }
        else
        {
            qCInfo(KSTARS_EKOS) << "Client::onDisconnected: User-initiated disconnect processed. Auto-reconnect skipped.";
            m_userRequestedDisconnect = false; // Reset flag after processing user disconnect
            // Ensure progress indicator is stopped if it was running due to a previous attempt
            if (pi && pi->isAnimated())
                pi->stopAnimation();
        }

        // If we are transitioning to a fully disconnected state (no manager is connected or trying to connect), ensure PI is stopped.
        // Note: checkAndTriggerAuth() will start PI if it initiates authentication.
        // This check remains useful if checkAndTriggerAuth decides no auth is needed but we are still disconnected.
        bool anyManagerStillActive = false;
        for (const auto &manager : m_NodeManagers)
        {
            if (manager->isConnected() || manager->isReauthenticating())
            {
                anyManagerStillActive = true;
                break;
            }
        }
        if (!anyManagerStillActive && pi && pi->isAnimated())
        {
            pi->stopAnimation();
        }
    }

    // Update individual manager status icons regardless
    auto disconnectedIcon = QIcon(":/icons/AlignFailure.svg").pixmap(QSize(32, 32));
    auto connectedIcon = QIcon(":/icons/AlignSuccess.svg").pixmap(QSize(32, 32));

    // Assuming m_NodeManagers[0] is Online and m_NodeManagers[1] is Offline
    if (m_NodeManagers.size() > 0)
    {
        onlineLabel->setStyleSheet(m_NodeManagers[Online]->isConnected() ? "color:white" : "color:gray");
        onlineIcon->setPixmap(m_NodeManagers[Online]->isConnected() ? connectedIcon : disconnectedIcon);
    }
    if (m_NodeManagers.size() > 1)
    {
        offlineLabel->setStyleSheet(m_NodeManagers[Offline]->isConnected() ? "color:white" : "color:gray");
        offlineIcon->setPixmap(m_NodeManagers[Offline]->isConnected() ? connectedIcon : disconnectedIcon);
    }
}

void Client::setConnected(bool enabled)
{
    // Return if there is no change.
    if (enabled == m_isConnected)
        return;

    connectB->click();
}

void Client::setConfig(bool rememberCredentials, bool autoConnect)
{
    rememberCredentialsCheck->setChecked(rememberCredentials);
    autoStartCheck->setChecked(autoConnect);
}

void Client::setUser(const QString &user, const QString &pass)
{
    username->setText(user);
    Options::setEkosLiveUsername(user);

    password->setText(pass);
}

void Client::showSelectServersDialog()
{
    QDialog dialog(this);
    dialog.setMinimumWidth(300);
    dialog.setWindowTitle(i18nc("@title:window", "Select EkosLive Servers"));

    QLabel observatory(i18n("Observatory:"));
    QLabel offline(i18n("Offline:"));
    QLabel online(i18n("Online:"));
    QLabel id(i18n("Machine ID:"));

    QLineEdit observatoryEdit(&dialog);
    QLineEdit offlineEdit(&dialog);
    QLineEdit onlineEdit(&dialog);
    QLineEdit machineID(&dialog);
    machineID.setReadOnly(true);
    machineID.setText(KSUtils::getMachineID());

    observatoryEdit.setText(Options::ekosLiveObservatory());
    offlineEdit.setText(Options::ekosLiveOfflineServer());
    onlineEdit.setText(Options::ekosLiveOnlineServer());

    QFormLayout * layout = new QFormLayout;
    layout->addRow(&observatory, &observatoryEdit);
    layout->addRow(&offline, &offlineEdit);
    layout->addRow(&online, &onlineEdit);
    layout->addRow(&id, &machineID);
    dialog.setLayout(layout);
    dialog.exec();

    Options::setEkosLiveObservatory(observatoryEdit.text());
    Options::setEkosLiveOfflineServer(offlineEdit.text());
    Options::setEkosLiveOnlineServer(onlineEdit.text());

    syncURLs();
}

void Client::syncURLs()
{
    auto onlineSSL = QUrl(Options::ekosLiveOnlineServer()).scheme() == "https";
    auto onlineURL = QUrl(Options::ekosLiveOnlineServer()).url(QUrl::RemoveScheme);
    if (onlineSSL)
        m_NodeManagers[Online]->setURLs(QUrl("https:" + onlineURL), QUrl("wss:" + onlineURL));
    else
        m_NodeManagers[Online]->setURLs(QUrl("http:" + onlineURL), QUrl("ws:" + onlineURL));

    auto offlineSSL = QUrl(Options::ekosLiveOfflineServer()).scheme() == "https";
    auto offlineURL = QUrl(Options::ekosLiveOfflineServer()).url(QUrl::RemoveScheme);
    if (offlineSSL)
        m_NodeManagers[Offline]->setURLs(QUrl("https:" + offlineURL), QUrl("wss:" + offlineURL));
    else
        m_NodeManagers[Offline]->setURLs(QUrl("http:" + offlineURL), QUrl("ws:" + offlineURL));
}
}
