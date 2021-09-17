/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Options.h"

#include "ekosliveclient.h"
#include "ekos_debug.h"
#include "ekos/manager.h"
#include "ekos/capture/capture.h"
#include "ekos/mount/mount.h"
#include "ekos/focus/focus.h"

#include "kspaths.h"
#include "kstarsdata.h"
#include "filedownloader.h"
#include "QProgressIndicator.h"

#include "indi/indilistener.h"
#include "indi/indiccd.h"
#include "indi/indifilter.h"

#include <config-kstars.h>

#ifdef HAVE_KEYCHAIN
#include <qt5keychain/keychain.h>
#endif

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace EkosLive
{
Client::Client(Ekos::Manager *manager) : QDialog(manager), m_Manager(manager)
{
    setupUi(this);

    connect(closeB, SIGNAL(clicked()), this, SLOT(close()));

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::AppDataLocation, "ekoslive.png")))
        leftBanner->setPixmap(im);

    pi = new QProgressIndicator(this);
    bottomLayout->insertWidget(1, pi);

    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));

    username->setText(Options::ekosLiveUsername());
    connect(username, &QLineEdit::editingFinished, [ = ]()
    {
        Options::setEkosLiveUsername(username->text());
    });

    connect(connectB, &QPushButton::clicked, [ = ]()
    {
        if (m_isConnected)
            disconnectAuthServer();
        else
            connectAuthServer();
    });

    connect(password, &QLineEdit::returnPressed, [ = ]()
    {
        if (!m_isConnected)
            connectAuthServer();
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

    m_serviceURL.setUrl("https://live.stellarmate.com");
    m_wsURL.setUrl("wss://live.stellarmate.com");

    if (Options::ekosLiveOnline())
        ekosLiveOnlineR->setChecked(true);
    else
        ekosLiveOfflineR->setChecked(true);

    connect(ekosLiveOnlineR, &QRadioButton::toggled, [&](bool toggled)
    {
        Options::setEkosLiveOnline(toggled);
        if (toggled)
        {
            m_serviceURL.setUrl("https://live.stellarmate.com");
            m_wsURL.setUrl("wss://live.stellarmate.com");
            m_Message->setURL(m_wsURL);
            m_Media->setURL(m_wsURL);
            m_Cloud->setURL(m_wsURL);
        }
        else
        {
            m_serviceURL.setUrl("http://localhost:3000");
            m_wsURL.setUrl("ws://localhost:3000");
            m_Message->setURL(m_wsURL);
            m_Media->setURL(m_wsURL);
            m_Cloud->setURL(m_wsURL);
        }
    }
           );

    if (Options::ekosLiveOnline() == false)
    {
        m_serviceURL.setUrl("http://localhost:3000");
        m_wsURL.setUrl("ws://localhost:3000");
    }

#ifdef HAVE_KEYCHAIN
    QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(QLatin1String("kstars"));
    job->setAutoDelete(false);
    job->setKey(QLatin1String("ekoslive"));
    connect(job, &QKeychain::Job::finished, [&](QKeychain::Job * job)
    {
        if (job->error() == false)
        {
            //QJsonObject data = QJsonDocument::fromJson(dynamic_cast<QKeychain::ReadPasswordJob*>(job)->textData().toLatin1()).object();
            //const QString usernameText = data["username"].toString();
            //const QString passwordText = data["password"].toString();

            const auto passwordText = dynamic_cast<QKeychain::ReadPasswordJob*>(job)->textData().toLatin1();

            // Only set and attempt connection if the data is not empty
            //if (usernameText.isEmpty() == false && passwordText.isEmpty() == false)
            if (passwordText.isEmpty() == false && username->text().isEmpty() == false)
            {
                //username->setText(usernameText);
                password->setText(passwordText);

                if (autoStartCheck->isChecked())
                    connectAuthServer();
            }

        }
        job->deleteLater();
    });
    job->start();
#endif

    m_Message = new Message(m_Manager);
    m_Message->setURL(m_wsURL);
    connect(m_Message, &Message::connected, this, &Client::onConnected);
    connect(m_Message, &Message::disconnected, this, &Client::onDisconnected);
    connect(m_Message, &Message::expired, [&]()
    {
        // If token expired, disconnect and reconnect again.
        disconnectAuthServer();
        connectAuthServer();
    });

    m_Media = new Media(m_Manager);
    connect(m_Message, &Message::optionsChanged, m_Media, &Media::setOptions);
    m_Media->setURL(m_wsURL);

    m_Cloud = new Cloud(m_Manager);
    connect(m_Message, &Message::optionsChanged, m_Cloud, &Cloud::setOptions);
    m_Cloud->setURL(m_wsURL);
}

Client::~Client()
{
    m_Message->disconnectServer();
    m_Media->disconnectServer();
    m_Cloud->disconnectServer();
}

void Client::onConnected()
{
    pi->stopAnimation();

    m_isConnected = true;

    connectB->setText(i18n("Disconnect"));
    connectionState->setPixmap(QIcon::fromTheme("state-ok").pixmap(QSize(64, 64)));

    if (rememberCredentialsCheck->isChecked())
    {
#ifdef HAVE_KEYCHAIN
        //        QJsonObject credentials =
        //        {
        //            {"username", username->text()},
        //            {"password", password->text()}
        //        };

        QKeychain::WritePasswordJob *job = new QKeychain::WritePasswordJob(QLatin1String("kstars"));
        job->setAutoDelete(true);
        job->setKey(QLatin1String("ekoslive"));
        //job->setTextData(QJsonDocument(credentials).toJson());
        job->setTextData(password->text());
        job->start();
#endif
    }
}

void Client::onDisconnected()
{
    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));
    m_isConnected = false;
    connectB->setText(i18n("Connect"));
}

void Client::connectAuthServer()
{
    if (username->text().isEmpty() || password->text().isEmpty())
    {
        KSNotification::error(i18n("Username or password is missing."));
        return;
    }

    pi->startAnimation();
    authenticate();
}

void Client::disconnectAuthServer()
{
    token.clear();

    m_Message->disconnectServer();
    m_Media->disconnectServer();
    m_Cloud->disconnectServer();

    modeLabel->setEnabled(true);
    ekosLiveOnlineR->setEnabled(true);
    ekosLiveOfflineR->setEnabled(true);
}

void Client::authenticate()
{
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl authURL(m_serviceURL);
    authURL.setPath("/api/authenticate");

    request.setUrl(authURL);

    QJsonObject json = { {"username", username->text()},
        {"password", password->text()}
    };

    auto postData = QJsonDocument(json).toJson(QJsonDocument::Compact);

    networkManager->post(request, postData);
}

void Client::onResult(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        // If connection refused, retry up to 3 times
        if (reply->error() == QNetworkReply::ConnectionRefusedError && m_AuthReconnectTries++ < RECONNECT_MAX_TRIES)
        {
            reply->deleteLater();
            QTimer::singleShot(RECONNECT_INTERVAL, this, &Client::connectAuthServer);
            return;
        }

        m_AuthReconnectTries = 0;
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(i18n("Error authentication with Ekos Live server: %1", reply->errorString()));
        reply->deleteLater();
        return;
    }

    m_AuthReconnectTries = 0;
    QJsonParseError error;
    auto response = QJsonDocument::fromJson(reply->readAll(), &error);

    if (error.error != QJsonParseError::NoError)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(i18n("Error parsing server response: %1", error.errorString()));
        reply->deleteLater();
        return;
    }

    authResponse = response.object();

    if (authResponse["success"].toBool() == false)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(authResponse["message"].toString());
        reply->deleteLater();
        return;
    }

    token = authResponse["token"].toString();

    m_Message->setAuthResponse(authResponse);
    m_Message->connectServer();

    m_Media->setAuthResponse(authResponse);
    m_Media->connectServer();

    // If we are using EkosLive Offline
    // We need to check for internet connection before we connect to the online web server
    if (ekosLiveOnlineR->isChecked() || (ekosLiveOfflineR->isChecked() &&
                                         networkManager->networkAccessible() == QNetworkAccessManager::Accessible))
    {
        m_Cloud->setAuthResponse(authResponse);
        m_Cloud->connectServer();
    }

    modeLabel->setEnabled(false);
    ekosLiveOnlineR->setEnabled(false);
    ekosLiveOfflineR->setEnabled(false);

    reply->deleteLater();
}

void Client::setConnected(bool enabled)
{
    // Return if there is no change.
    if (enabled == m_isConnected)
        return;

    connectB->click();
}

void Client::setConfig(bool onlineService, bool rememberCredentials, bool autoConnect)
{
    ekosLiveOnlineR->setChecked(onlineService);
    ekosLiveOfflineR->setChecked(!onlineService);

    rememberCredentialsCheck->setChecked(rememberCredentials);

    autoStartCheck->setChecked(autoConnect);
}

void Client::setUser(const QString &user, const QString &pass)
{
    username->setText(user);
    Options::setEkosLiveUsername(user);

    password->setText(pass);
}

}
