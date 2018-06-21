/*  Ekos Live Client
    Copyright (C) 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "Options.h"

#include "ekosliveclient.h"
#include "ekos_debug.h"
#include "ekos/ekosmanager.h"
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
Client::Client(EkosManager *manager) : QDialog(manager), m_Manager(manager)
{
    setupUi(this);

    connect(closeB, SIGNAL(clicked()), this, SLOT(close()));

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "ekoslive.png")))
        leftBanner->setPixmap(im);

    pi = new QProgressIndicator(this);
    bottomLayout->insertWidget(1, pi);

    connectionState->setPixmap(QIcon::fromTheme("state-offline").pixmap(QSize(64, 64)));

    connect(connectB, &QPushButton::clicked, [=]()
    {
        if (m_isConnected)
            disconnectAuthServer();
        else
            connectAuthServer();
    });

    rememberCredentialsCheck->setChecked(Options::rememberCredentials());
    connect(rememberCredentialsCheck, &QCheckBox::toggled, [=](bool toggled) { Options::setRememberCredentials(toggled);});


    m_serviceURL.setUrl("https://live.stellarmate.com");

    QUrl m_wsURL("wss://live.stellarmate.com");

    if (getenv("LOCAL_EKOSLIVE_SERVER"))
    {
        m_serviceURL.setUrl("http://localhost:3000");
        m_wsURL.setUrl("ws://localhost:3000");
    }

    #ifdef HAVE_KEYCHAIN
    QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(QLatin1String("kstars"));
    job->setAutoDelete(false);
    job->setKey(QLatin1String("ekoslive"));
    connect(job, &QKeychain::Job::finished, [&](QKeychain::Job* job) {
        if (job->error() == false)
        {
            QJsonObject data = QJsonDocument::fromJson(dynamic_cast<QKeychain::ReadPasswordJob*>(job)->textData().toLatin1()).object();
            username->setText(data["username"].toString());
            password->setText(data["password"].toString());
        }
        job->deleteLater();
    });
    job->start();
    #endif

    m_Message = new Message(m_Manager);
    m_Message->setURL(m_wsURL);
    connect(m_Message, &Message::connected, this, &Client::onConnected);
    connect(m_Message, &Message::disconnected, this, &Client::onDisconnected);
    connect(m_Message, &Message::expired, [&]() {
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
        QJsonObject credentials = {
            {"username", username->text()},
            {"password", password->text()}
        };

        QKeychain::WritePasswordJob *job = new QKeychain::WritePasswordJob(QLatin1String("kstars"));
        job->setAutoDelete(true);
        job->setKey(QLatin1String("ekoslive"));
        job->setTextData(QJsonDocument(credentials).toJson());
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
}

void Client::authenticate()
{
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl authURL(m_serviceURL);
    authURL.setPath("/api/authenticate");

    request.setUrl(authURL);

    QJsonObject json = { {"username" , username->text()},
                         {"password" , password->text()}};

    auto postData = QJsonDocument(json).toJson(QJsonDocument::Compact);

    networkManager->post(request, postData);
}

void Client::onResult(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(i18n("Error authentication with Ekos Live server: %1", reply->errorString()));
        return;
    }

    QJsonParseError error;
    auto response = QJsonDocument::fromJson(reply->readAll(), &error);

    if (error.error != QJsonParseError::NoError)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(i18n("Error parsing server response: %1", error.errorString()));
        return;
    }

    authResponse = response.object();

    if (authResponse["success"].toBool() == false)
    {
        pi->stopAnimation();
        connectionState->setPixmap(QIcon::fromTheme("state-error").pixmap(QSize(64, 64)));
        KSNotification::error(authResponse["message"].toString());
        return;
    }

    token = authResponse["token"].toString();

    m_Message->setAuthResponse(authResponse);
    m_Message->connectServer();

    m_Media->setAuthResponse(authResponse);
    m_Media->connectServer();

    m_Cloud->setAuthResponse(authResponse);
    m_Cloud->connectServer();
}

}
